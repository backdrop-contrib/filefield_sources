<?php

/**
 * @file
 * A FileField extension to allow referencing of existing files.
 *
 * The "hooks" in this file are not true hooks, they're called individually
 * from the main filefield_sources.module in the corresponding hook by the
 * same name. Any of these hooks could be broken out into a separate module.
 */

define('FILEFIELD_SOURCE_REFERENCE_HINT_TEXT', 'example.png [fid:123]');

/**
 * Implements hook_filefield_source_info().
 */
function filefield_source_reference_info() {
  $source = array();
  $source['reference'] = array(
    'name' => t('Autocomplete reference textfield'),
    'label' => t('Reference existing'),
    'description' => t('Reuse an existing file by entering its file name.'),
    'process' => 'filefield_source_reference_process',
    'value' => 'filefield_source_reference_value',
    'weight' => 1,
    'file' => 'sources/reference.inc',
  );
  return $source;
}

/**
 * Implements hook_menu().
 */
function filefield_source_reference_menu() {
  $items = array();

  $items['file/reference/%/%/%'] = array(
    'page callback' => 'filefield_source_reference_autocomplete',
    'page arguments' => array(2, 3, 4),
    'access callback' => '_filefield_sources_field_access',
    'access arguments' => array(2, 3, 4),
    'file' => 'sources/reference.inc',
    'type' => MENU_CALLBACK,
  );
  return $items;
}

/**
 * Implements hook_theme().
 */
function filefield_source_reference_theme() {
  return array(
    'filefield_source_reference_element' => array(
      'render element' => 'element',
      'file' => 'sources/reference.inc',
    ),
    'filefield_source_reference_autocomplete_item' => array(
      'variables' => array('file' => NULL),
      'file' => 'sources/reference.inc',
    ),
 );
}

/**
 * Implements hook_filefield_source_settings().
 */
function filefield_source_reference_settings($op, $instance) {
  $return = array();

  if ($op == 'form') {
    $settings = $instance['widget']['settings']['filefield_sources'];

    $return['source_reference'] = array(
      '#title' => t('Autocomplete reference options'),
      '#type' => 'fieldset',
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#states' => array(
        'visible' => array(
          ':input[name="instance[widget][settings][filefield_sources][filefield_sources][reference]"]' => array('checked' => TRUE),
        ),
      ),
    );

    $return['source_reference']['autocomplete'] = array(
      '#title' => t('Match file name'),
      '#options' => array(
        '0' => t('Starts with string'),
        '1' => t('Contains string'),
      ),
      '#type' => 'radios',
      '#default_value' => isset($settings['source_reference']['autocomplete']) ? $settings['source_reference']['autocomplete'] : '0',
    );

    $return['source_reference']['search_all_fields'] = array(
      '#title' => t('Files to search'),
      '#options' => array(
        '0' => t('This field'),
        '1' => t('All file fields'),
        '2' => t('All files'),
      ),
      '#type' => 'radios',
      '#default_value' => isset($settings['source_reference']['search_all_fields']) ? $settings['source_reference']['search_all_fields'] : '0',
    );
    $return['source_reference']['search_all_fields']['0']['#description'] = t('Only files uploaded within the same base field.');
    $return['source_reference']['search_all_fields']['1']['#description'] = t('Files uploaded across all file fields.');
    $return['source_reference']['search_all_fields']['2']['#description'] = t('All uploaded files, including from the !url.', array('!url' => l('Manage files screen', 'admin/content/files')));
  }
  elseif ($op == 'save') {
    $return['source_reference']['autocomplete'] = '0';
  }

  return $return;
}

/**
 * A #process callback to extend the filefield_widget element type.
 */
function filefield_source_reference_process($element, &$form_state, $form) {

  $element['filefield_reference'] = array(
    '#weight' => 100.5,
    '#theme' => 'filefield_source_reference_element',
    '#filefield_source' => TRUE, // Required for proper theming.
    '#filefield_sources_hint_text' => FILEFIELD_SOURCE_REFERENCE_HINT_TEXT,
  );

  $element['filefield_reference']['autocomplete'] = array(
    '#type' => 'textfield',
    '#autocomplete_path' => 'file/reference/' . $element['#entity_type'] . '/' . $element['#bundle'] . '/' . $element['#field_name'],
    '#description' => filefield_sources_element_validation_help($element['#upload_validators']),
  );

  $ajax_path = 'file/ajax/' . implode('/', $element['#array_parents']) . '/' . $form['form_build_id']['#value'];
  $token = backdrop_hmac_base64($ajax_path, backdrop_get_private_key() . backdrop_get_hash_salt());
  $element['filefield_reference']['select'] = array(
    '#name' => implode('_', $element['#array_parents']) . '_autocomplete_select',
    '#type' => 'submit',
    '#value' => t('Select'),
    '#validate' => array(),
    '#submit' => array('filefield_sources_field_submit'),
    '#name' => $element['#name'] . '[filefield_reference][button]',
    '#limit_validation_errors' => array($element['#parents']),
    '#ajax' => array(
      'path' => $ajax_path . '/' . $token,
      'wrapper' => $element['upload_button']['#ajax']['wrapper'],
      'effect' => 'fade',
    ),
  );

  return $element;
}

/**
 * A #filefield_value_callback function.
 */
function filefield_source_reference_value($element, &$item) {
  if (isset($item['filefield_reference']['autocomplete']) && strlen($item['filefield_reference']['autocomplete']) > 0 && $item['filefield_reference']['autocomplete'] != FILEFIELD_SOURCE_REFERENCE_HINT_TEXT) {
    $matches = array();
    if (preg_match('/\[fid:(\d+)\]/', $item['filefield_reference']['autocomplete'], $matches)) {
      $fid = $matches[1];
      if ($file = file_load($fid)) {

        // Remove file size restrictions, since the file already exists on disk.
        if (isset($element['#upload_validators']['file_validate_size'])) {
          unset($element['#upload_validators']['file_validate_size']);
        }

        // Check that the user has access to this file through hook_download().
        if (!filefield_sources_file_access($file->uri)) {
          form_error($element, t('You do not have permission to use the selected file.'));
        }
        elseif (filefield_sources_element_validate($element, (object) $file)) {
          $item = array_merge($item, (array) $file);
        }
      }
      else {
        form_error($element, t('The referenced file could not be used because the file does not exist in the database.'));
      }
    }
    // No matter what happens, clear the value from the autocomplete.
    $item['filefield_reference']['autocomplete'] = '';
  }
}

/**
 * Menu callback; autocomplete.js callback to return a list of files.
 */
function filefield_source_reference_autocomplete($entity_type, $bundle_name, $field_name, $filename) {
  $field = field_info_instance($entity_type, $field_name, $bundle_name);

  $items = array();
  if (!empty($field)) {
    $files = filefield_source_reference_get_files($filename, $field);
    foreach ($files as $fid => $file) {
      if (filefield_sources_file_access($file->uri)) {
        $items[$file->filename ." [fid:$fid]"] = theme('filefield_source_reference_autocomplete_item', array('file' => $file));
      }
    }
  }

  backdrop_json_output($items);
}

/**
 * Theme the output of a single item in the autocomplete list.
 */
function theme_filefield_source_reference_autocomplete_item($variables) {
  $file = $variables['file'];

  $output = '';
  $output .= '<div class="filefield-source-reference-item">';
  $output .= '<span class="filename">' . check_plain($file->filename) . '</span> <span class="filesize">(' . format_size($file->filesize) . ')</span>';
  $output .= '</div>';
  return $output;
}

/**
 * Theme the output of the autocomplete field.
 */
function theme_filefield_source_reference_element($variables) {
  $element = $variables['element'];

  $element['autocomplete']['#field_suffix'] = backdrop_render($element['select']);
  return '<div class="filefield-source filefield-source-reference clear-block">' . backdrop_render($element['autocomplete']) . '</div>';
}

/**
 * Get all the files used within a particular field (or all fields).
 *
 * @param $file_name
 *   The partial name of the file to retrieve.
 * @param $instance
 *   Optional. A CCK field array for which to filter returned files.
 */
function filefield_source_reference_get_files($filename, $instance = NULL) {
  if (isset($instance)) {
    // If we are looking at a single field, cache its settings, in case we want to search all fields.
    $setting_autocomplete = empty($instance['widget']['settings']['filefield_sources']['source_reference']['autocomplete']) ? 0 : 1;
    $setting_search_all_fields = empty($instance['widget']['settings']['filefield_sources']['source_reference']['search_all_fields']) ? 0 : $instance['widget']['settings']['filefield_sources']['source_reference']['search_all_fields'];
    $file_extensions = empty($instance['settings']['file_extensions']) ? '' : $instance['settings']['file_extensions'];
  }

  $instances = array();
  if (!isset($instance) || $setting_search_all_fields) {
    foreach (field_info_fields() as $instance) {
      if ($instance['type'] == 'file' || $instance['type'] == 'image') {
        $instances[] = $instance;
      }
    }
  }
  else {
    $instances = array($instance);
  }

  $files = array();

  // If searching all managed files skip loading individual fields and return
  // entire result.
  if ($setting_search_all_fields == 2) {
    $like = empty($setting_autocomplete) ? (db_like($filename) . '%') : ('%' . db_like($filename) . '%');
    $regex = '\.(' . preg_replace('/ +/', '|', preg_quote($file_extensions)) . ')$';

    $query = db_select('file_managed', 'f');
    $query->fields('f');
    $query->condition('f.status', 1);
    $query->condition('f.filename', $like, 'LIKE');
    $query->condition('f.filename', $regex, 'REGEXP');
    $query->orderBy('f.timestamp', 'DESC');
    $query->groupBy('f.fid');
    $query->range(0, 30);
    $query->addTag('filefield_source_reference_list');
    $result = $query->execute();

    foreach ($result as $file) {
      $files[$file->fid] = $file;
    }

    return $files;
  }

  // Limiting search to file field(s).
  foreach ($instances as $instance) {
    // Load the field data, which contains the schema information.
    $field = field_info_field($instance['field_name']);

    // We don't support fields that are not stored with SQL.
    if (!isset($field['storage']['details']['sql']['FIELD_LOAD_CURRENT'])) {
      continue;
    }

    // If we are searching all fields, use the autocomplete settings from the source field.
    if (!empty($setting_search_all_fields)) {
      $instance['widget']['settings']['filefield_sources']['source_reference']['autocomplete'] = empty($setting_autocomplete) ? 0 : 1;
    }

    // 1 == contains, 0 == starts with.
    $like = empty($instance['widget']['settings']['filefield_sources']['source_reference']['autocomplete']) ? (db_like($filename) . '%') : ('%' . db_like($filename) . '%');

    $table_info = reset($field['storage']['details']['sql']['FIELD_LOAD_CURRENT']);
    $table = key($field['storage']['details']['sql']['FIELD_LOAD_CURRENT']);
    $query = db_select($table, 'cf');
    $query->innerJoin('file_managed', 'f', 'f.fid = cf.' . $table_info['fid']);
    $query->fields('f');
    $query->condition('f.status', 1);
    $query->condition('f.filename', $like, 'LIKE');
    $query->orderBy('f.timestamp', 'DESC');
    $query->groupBy('f.fid');
    $query->range(0, 30);
    $query->addTag('filefield_source_reference_list');
    $result = $query->execute();

    foreach ($result as $file) {
      $files[$file->fid] = $file;
    }
  }

  return $files;
}
