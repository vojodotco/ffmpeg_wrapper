<?php

// $Id$

/**
 * @FILE This file provides the transcoding testing capabilities
 *       for ffmpeg_wrapper
 */


/* ************************************************** */
/* TEST CONVERSION FORM FUNCTIONS                     */
/* ************************************************** */


/**
 * This creates a form for testing files. If data is being passed back into this
 * form it will use that data to attempt to convert the file.
 *
 * @NOTE This is a LARGE potential security risk. Only admins
 *       should have access to this function.
 *
 * @param string $path
 *   path to the file to manipulate
 * @param string $type
 *   type of item passing file (node, media_mover, etc)
 * @param int $id
 *   $id related to the $type, used to pass item back
 * @return array
 *   drupal form array
 */
function ffmpeg_wrapper_ffmpeg_test_form($form_values = null) {
  // If we have incoming files that means we are probably
  // ready to process the files for conversion
  if ($form_values['post']['files']) {
    // run the conversion process with the settings
    $ffmpeg_object = ffmpeg_wrapper_ffmpeg_test_form_run($form_values['post']);
    // set messages- we do this instead of drupal_message()
    $form['messages'] = array(
      '#type' => 'markup',
      '#value' => $ffmpeg_object->errors ? t('There were errors during the conversion!') : t('Your file was successfully transcoded!'),
      '#prefix' => '<h3>',
      '#suffix' => '</h3>',
    );
    // display the conversion output
    $form['data'] = array(
      '#type' => 'fieldset',
      '#title' => t('Conversion data'),
    );
    if ($ffmpeg_object->errors){
      $form['data']['errors'] = array(
        '#type' => 'markup',
        '#value' => t('Errors found: !errors', array('!errors' => implode('<br />', $ffmpeg_object->errors))),
      );
    }
    $form['data']['command'] = array(
      '#type' => 'markup',
      '#value' => t('<b>Command run was:</b> !command', array('!command' => $ffmpeg_object->command)),
      '#suffix' => '<br />',
    );
    $form['data']['output_file'] = array(
      '#type' => 'markup',
      '#value' => t('<b>Output file is:</b> !command', array('!command' => l($ffmpeg_object->output_file, file_create_url($ffmpeg_object->output_file)))),
      '#suffix' => '<br />',
    );
    $form['data']['ffmpeg_output'] = array(
      '#type' => 'fieldset',
      '#title' => t('FFmpeg Output'),
      '#collapsible' => true,
      '#collapsed' => true,
    );
    $form['data']['ffmpeg_output']['output'] = array(
      '#type' => 'markup',
      '#value' => '<pre>'. $ffmpeg_object->output .'</pre>',
    );
  }

  // -------------------------------------------------
  // Start building the normal form
  $paths = array();
  // set the path to our directories
  $paths['test'] = variable_get('ffmpeg_wrapper_test_test', drupal_get_path('module', 'ffmpeg_wrapper') .'/test');
  $paths['out'] = variable_get('ffmpeg_wrapper_test_out', file_directory_path() .'/ffmpeg_wrapper_test_output');
  $paths['in'] = variable_get('ffmpeg_wrapper_test_in', file_directory_path() .'/ffmpeg_wrapper_test_input');

  // We need to check if there is a file path set in the URL and
  // make sure that we only copy this once. Use the cache to save the data.
  $file_path = mysql_escape_string($_GET['path']);
  // has this already been used?
  // get the cached file name
  $cache = cache_get('ffmpeg_wrapper_test_file', 'cache');
  // do we have this file already?
  if ($file_path == $cache->data) {
    unset($file_path);
  }
  else {
    cache_set('ffmpeg_wrapper_test_file', $file_path, 'cache', CACHE_TEMPORARY);
  }

  // If we have an incoming file copy that has not matched other files
  // copy it into the test directory
  if ($file_path) {
    if (! file_exists($file_path)) {
      drupal_set_message(t('Sorry, that file does not exist'), 'error');
    }
    else {
      // check against our list of files
      if (! $form['incoming_files']['#options'][$file_path]) {
        // save the old name before it can get renamed
        $old_file_path = $file_path;
        // make sure the copy directory exists
        file_check_directory($paths['in'], FILE_CREATE_DIRECTORY);
        // we make a copy of the file for testing
        if (file_copy($file_path, $paths['in']) ) {
          drupal_set_message(t('Copied your file into the test directory'));
        }
        else {
          drupal_set_message(t('Could not copy your file into the test directory'), 'error');
        }
      }
    }
  }

  $form['info'] = array(
    '#type' => 'markup',
    '#value' => t('This allows you to test a configuration on a file that you specify as the input file. Additional files can be added in the directory: <br /> %path', array('%path' =>$path)),
  );

  // display files that we currently have
  $form['files'] = array(
    '#theme' => 'ffmpeg_wrapper_files_checkboxes',
    '#tree' => TRUE,
    '#description' => t('Select a file to test conversions on.'),
  );

  // get a list of the files that are currently in the directories
  foreach ($paths as $path) {
    $files = file_scan_directory($path, '.*$');
    if (count($files)) {
      foreach ($files as $key => $file) {
        // build the form items out
        $form['files'][$key]['description'] = array('#type' => 'markup',  '#value' => basename($file->filename));
        $form['files'][$key]['size'] = array('#type' => 'markup',  '#value' => format_size(filesize($file->filename)));
        $form['files'][$key]['mime'] = array('#type' => 'markup',  '#value' => file_get_mimetype($file->filename));
        $form['files'][$key]['use'] = array('#type' => 'checkbox',  '#default_value' => null);
        $form['files'][$key]['attach'] = array('#type' => 'checkbox',  '#default_value' => null);
      }
    }
  }

  // did we get any files?
  if (! $files) {
    drupal_set_message(t('Sorry there are no files to test. Please place some files to test in !path', array('!path' =>$path)));
    return;
  }

  // set the nid
  $form['nid'] = array(
    '#type' => 'hidden',
    '#value' => $_GET['nid'] ? mysql_escape_string($_GET['nid']) : ($form_values['nid'] ? $form_values['nid'] : null),
  );

  // get the FFmpeg form
  $form['ffmpeg'] = ffmpeg_wrapper_configuration_form();

  // add a submit button
  $form['submit'] = array(
    '#value' => t('Submit'),
    '#type' => 'submit',
  );

  // This part is important!
  $form['#multistep'] = TRUE;
  $form['#redirect'] = FALSE;

  return $form;
}


/**
 * forces a value for the test output type\
 * @param string $form_id
 * @param array $form_values
 */
function ffmpeg_wrapper_ffmpeg_test_form_validate($form_id, &$form_state) {
  // first we need to go through files and figure out if there is more than one
  // file selected for process or attaching
  foreach ($form_state['values']['files'] as $id => $values) {
    // are we using files?
    if ($values['use']) { $use++; }
    // are we attaching files?
    if ($values['attach']) { $attach++; }
  }
  // Error for attach and use set at the same time
  if ($attach >= 1 && $use >= 1) {
    form_set_error('files', t('You can only attach or use a single file at a time. Please select either use or attach.'));
  }
  // Error for use files
  elseif ($use > 1) {
    form_set_error('files', t('You can only process a single file at a time. Please select one file.'));
  }
  // Error for attach files
  elseif ($attach > 1) {
    form_set_error('files', t('You can only attach a single file at a time. Please select one file.'));
  }

  // We do not want to invalidate if the user is attaching
  if ($form_state['values']['nid'] && $attach == 1) {
    return;
  }

  // make sure an output type is set
  if (! $form_state['values']['ffmpeg_output_type']) {
    form_set_error('ffmpeg_output_type', t('You must specify an output type'));
  }
}


/**
 * build the confirm form for removing all the files generated by the tests
 */
function ffmpeg_wrapper_test_cleanup_form() {
  $form['status'] = array(
    '#type' => 'markup',
    '#value' => t('When testing, FFmpeg Wrapper creates output files as well as copying input files. This allows you to remove them and reclaim disk space. '),
  );
  return confirm_form(
    $form,
    t('Are you sure you want to clean up the test directories? This will delete any of the test files that you have created.'),
    'admin/settings/ffmpeg_wrapper',
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}


/**
 * deletes the test directories
 * @param string $form_id
 * @param array $form_values
 */
function ffmpeg_wrapper_test_cleanup_form_submit($form_id, &$form_state) {
  // set the paths for removal
  $paths[] = file_directory_path() .'/ffmpeg_wrapper_test_output';
  $paths[] = file_directory_path() .'/ffmpeg_wrapper_test_input';

  // get a list of the files that are currently in the directories
  foreach ($paths as $path) {
    $files = file_scan_directory($path, '.*$');
    if (count($files)) {
      foreach ($files as $file) {
        file_delete($file->filename);
      }
    }
    // now delete the directory
    rmdir($path);
  }

  // clear the cached file
  cache_clear_all('ffmpeg_wrapper_test_file', 'cache');
  drupal_set_message(t('The test data has been deleted'));
  return 'admin/settings/ffmpeg_wrapper';
}


/* ************************************************** */
/* TEST CONVERSION FUNCTIONS                          */
/* ************************************************** */

/**
 * This runs FFmpeg based on the form data passed into it.
 * @param string $input_file
 *   path to the file to operate on
 * @param array $params
 *   configuration options in the format set in the ffmpeg_wrapper_configuration_form()
 * @param string $output_file_path
 *   where to place the file, assumes same dir as $input_file. No trailing slash
 * @param object $ffmpeg_object
 *   contains debug information that calling functions can utilize
 * @return string
 *
 */
function ffmpeg_wrapper_configuration_form_convert($input_file, $params, $output_file_path = null, &$ffmpeg_object) {
  // first error check, make sure that we can decode this kind of file
  if (! ffmpeg_wrapper_can_decode($input_file)) {
    $message = 'FFmpeg Wrapper can not decode this file: !file';
    $variables = array('!file' => l($input_file, file_create_url($input_file)));
    watchdog('media_mover', $message, $variables, WATCHDOG_ERROR);
    $ffmpeg_object->errors[] = $message;
    return false;
  }

  // build the output file path if we don't have one. Use the output type as the extension.
  $output_file = file_create_filename(basename($input_file) .'.'. $params['ffmpeg_output_type'], $output_file_path ? $output_file_path : dirname($input_file));

  // did the admin define a specific FFmpeg comand to run?
  //  we only run what the admin specified
  if ($params['ffmpeg_video_custom']) {
    $options[] = str_replace(array('%in_file', '%out_file'), array($input_file, $output_file), $params['ffmpeg_video_custom_command']);
  }
  // build a standard configuration
  else {
    // build the ffmpeg command structure out
    $options = array();

    // input file
    $options[] = "-i '". $input_file ."'";

    // build the watermark config
    if ($params['ffmpeg_video_wm']) {
      $options[] = "-vhook '". ffmpeg_wrapper_path_to_vhook('watermark.so') ." -f ". $params['ffmpeg_video_wm_file'] ."'";
    }

    // build the audio config
    if ($params['ffmpeg_audio_advanced']) {

      // use a specifc codec?
      if ($params['ffmpeg_audio_acodec']) {
        $options[] =  '-acodec '. $params['ffmpeg_audio_acodec'];
      }

      // use a specific sample rate?
      if ($params['ffmpeg_audio_ar'] ) {
        $options[] = '-ar '. $params['ffmpeg_audio_ar'];
      }

      // use a specific bit rate?
      if ($params['ffmpeg_audio_ab']) {
        $options[] = '-ab '. $params['ffmpeg_audio_ab'];
      }
    }

    // build the video config
    if ($params['ffmpeg_video_advanced']) {

      // is codec set?
      if ($params['ffmpeg_video_vcodec']) {
        $options[] = '-vcodec '. $params['ffmpeg_video_vcodec'];
      }

      // is frame size set?
      if ($params['ffmpeg_video_size']) {
        $options[] = '-s '. $params[$params['ffmpeg_video_size'] == 'other' ? 'ffmpeg_video_size_other' : 'ffmpeg_video_size'];
      }

      // is the bit rate set?
      if ($params['ffmpeg_video_br']) {
        $options[] = '-b '. $params['ffmpeg_video_br'];
      }

      // is frame rate set?
      if ($params['ffmpeg_video_fps']) {
        $options[] = '-r '. $params['ffmpeg_video_fps'];
      }
    }

    // implement truncating
    if ($params['ffmpeg_time_advanced']) {
      $options[] = '-t '. $params['ffmpeg_time'];
    }

    // add the output file
    $options[] = "'". $output_file ."'";
  }

  $ffmpeg_object->command = implode(" ", $options);

  // run ffmpeg with error checking
  if (! $success = ffmpeg_wrapper_run_command($ffmpeg_object->command, true, null, $ffmpeg_object)) {
    return;
  }

  // successful convert, make a note in the log
  $message = t('FFmpeg converted this file: @file', array('@file' => $output_file));
  $message .= '<br />'. t('FFmpeg ran this command: ') .'<br />'. $ffmpeg_object->command;
  watchdog('FFmpeg', $message, 'WATCHDOG_NOTICE');

  $ffmpeg_object->output_file = $output_file;

  // return the completed file path
  return $output_file;
}


/**
 * This function is called after the form is submitted and when the form data returns
 * to the form function
 * @param $form_values
 * @return array
 */
function ffmpeg_wrapper_ffmpeg_test_form_run($form_values) {
  // we need to find out what file is being used or if we need to attach
  // the converted file back to the incoming node
  foreach ($form_values['files'] as $key => $value) {
    if ($form_values['nid'] && $value['attach']) {
      $attach_file = $key;
    }
    if ($value['use']) {
      $file_path = $key;
      break;
    }
  }

  // Do we need to attach the file?
  if ($attach_file) {
    // load the node object
    $node = node_load($form_values['nid']);
    // build the file object
    $file = new stdClass();
    $file->new = true;
    $file->fid = 'upload';
    $file->uid = $node->uid;
    $file->filename = basename($attach_file);
    $file->filepath = $attach_file;
    $file->filemime = file_get_mimetype($attach_file);
    $file->filesize = filesize($attach_file);
    $file->timestamp = time();
    $file->list = variable_get('upload_list_default', 1);
    $file->status = 1;
    $source ? $file->source = $source : null;
    // add the file to the node object
    $node->files[$file->fid] = $file;
    // save the modified node
    upload_save($node);
    node_save($node);
    // alert the user
    drupal_set_message(t('Attached your transcoded file'));
    drupal_goto('node/'. $node->nid .'/edit');
  }

  $ffmpeg_object = new StdClass;
  // create a test directory if there is not already one
  $path = file_directory_path() .'/ffmpeg_wrapper_test_output';
  // make sure the directory exists
  file_check_directory($path, FILE_CREATE_DIRECTORY);
  // run ffmpeg on the file and configuration
  ffmpeg_wrapper_configuration_form_convert($file_path, $form_values, $path, $ffmpeg_object);

  return $ffmpeg_object;
}


/* ************************************************** */
/* TEST CONVERSION THEME FUNCTIONS                    */
/* ************************************************** */

/**
 * Displays the files that are being manipulated in the test setup
 * @param $form
 *   drupal form
 * @return unknown_type
 */
function theme_ffmpeg_wrapper_files_checkboxes(&$form) {
  $header = array(t('Use'), t('Attach'), t('Name'), t('Mime type'), t('Size'));
  foreach (element_children($form) as $key) {
    $row = array();
    $row[] = drupal_render($form[$key]['use']);
    $row[] = drupal_render($form[$key]['attach']);
    $row[] = drupal_render($form[$key]['description']);
    $row[] = drupal_render($form[$key]['mime']);
    $row[] = drupal_render($form[$key]['size']);
    $rows[] = $row;
  }
  $output = theme('table', $header, $rows);
  $output .= drupal_render($form);
  return $output;
}