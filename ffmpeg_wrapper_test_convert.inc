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
function ffmpeg_wrapper_ffmpeg_test_form($form_state) {
  // If we have incoming files we are ready to process the files for conversion
  if (! empty($form_state['values'])) {
    $ffmpeg_object = $form_state['ffmpeg_object'];
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
        '#suffix' => '<br />',
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

  // header at the top of the page
  $form['info'] = array(
    '#type' => 'markup',
    '#value' => t('This allows you to test a configuration on a file that you specify as the input file. Additional files can be added in the directory: <br /> %path', array('%path' =>$path)),
  );

  // ------------------------------------------
  // AHAH form building

  // make sure we have files before we build the file data
  if ($files = ffmpeg_wrapper_test_build_files()) {
    // build the radio form element that the admin can choose from to process a file
    // use option is tied to AHAH
    $form['files'] = array(
      '#theme' => 'ffmpeg_wrapper_files_radios',
      '#type' => 'radios',
      '#default_value' => $form_state['values']['files'],
      '#ahah' => array(
        'event' => 'change',
        'path' => 'ffmpeg_wrapper/file_data',
        'wrapper' => 'file_data_display_test',
        'method' => 'replace',
      ),
    );

    // build the form elements for attaching a file back to a node
    $form['files']['attach_files'] = array(
      '#type' => 'radios',
      '#default_value' => null,
    );

    // this is really ugly, but I'm not sure there is a better way
    // given that this has to POST to the JS
    $form['files_lookup'] = array(
      '#type' => 'hidden',
      '#value' => serialize($files),
    );

    // build the options for the files and sub elements
    foreach($files as $key => $file) {
      // build an option for this file with no name
      $form['files']['#options'][$key] = $file;
      // build the attach options
      $form['files']['attach_files']['#options'][] = $file;
      // build other table elements
      $form['files'][$file]['name'] = array('#type' => 'markup',  '#value' => basename($file));
      $form['files'][$file]['size'] = array('#type' => 'markup',  '#value' => format_size(filesize($file)));
      $form['files'][$file]['mime'] = array('#type' => 'markup',  '#value' => file_get_mimetype($file));
    }

    // placeholder for the AHAH data
    $form['file_data_display'] = array(
      '#type' => 'markup',
      '#value' => '<div id="file_data_display_test"></div>',
    );
  }

  // There were no files passed in, give the default path
  else {
    drupal_set_message(t('Sorry there are no files to test. Please place some files to test in !path',
      array('!path' => variable_get('ffmpeg_wrapper_test_test', drupal_get_path('module', 'ffmpeg_wrapper') .'/test'))));
    return;
  }

  // set the nid for the attach option
  $form['nid'] = array(
    '#type' => 'hidden',
    '#value' => $_GET['nid'] ? mysql_escape_string($_GET['nid']) : ($form_values['nid'] ? $form_values['nid'] : null),
  );

  // -----------------------------------------------------
  // FFmpeg form building

  // get the FFmpeg form
  $form['ffmpeg'] = ffmpeg_wrapper_configuration_form($form_state['values']);

  // add a submit button
  $form['submit'] = array(
    '#value' => t('Transcode'),
    '#type' => 'submit',
  );

  // add a submit button
  $form['attach'] = array(
    '#value' => t('Attach'),
    '#type' => 'submit',
    '#submit' => array('ffmpeg_wrapper_ffmpeg_test_file_attach'),
  );

  // This part is important!
  $form['#multistep'] = TRUE;
  $form['#redirect'] = FALSE;

  return $form;
}


/**
 * Checks to make sure data coming in is sane.
 * @param string $form_id
 * @param array $form_values
 */
function ffmpeg_wrapper_ffmpeg_test_form_validate($form_id, &$form_state) {
  // did the user transcode or attach the file?
  if ($form_state['values']['op'] == t('Transcode')) {
    // make sure an output type is set
    if (! $form_state['values']['ffmpeg_output_type']) {
      form_set_error('ffmpeg_output_type', t('You must specify an output type'));
    }
    // make sure a file was selected
    if (! isset($form_state['values']['files'])) {
      form_set_error('files', t('You must choose a file to transcode'));
    }
  }
  if ($form_state['clicked_button']['#post']['op'] == t('Attach')) {
    // get the path to the file
    $files_lookup = unserialize($form_state['values']['files_lookup']);
    $path = $files_lookup[$form_state['values']['attach_files']];

    // make sure we have an NID
    if (! $form_state['values']['nid']) {
      form_set_error('', t('Sorry, there is no node to attach this file back to.'));
    }
    // make sure a file was selected
    elseif (! isset($form_state['values']['attach_files'])) {
      form_set_error('attach', t('You must choose a file to attach'));
    }
    elseif(! file_exists($path)) {
      form_set_error('', t('The file you are trying to attach does not seem to exist. Maybe this was a bad transcode?'));
    }
    elseif(filesize($path) < 10) {
      form_set_error('', t('The file you are trying to attach a file does not seem to have data. Maybe this was a bad transcode?'));
    }
  }
}

function ffmpeg_wrapper_ffmpeg_test_form_submit($form, &$form_state) {
  // get the file from the lookup files data
  $files_lookup = unserialize($form_state['values']['files_lookup']);
  $path = $files_lookup[$form_state['values']['files']];
  // run the conversion process with the settings
  $form_state['ffmpeg_object'] = ffmpeg_wrapper_ffmpeg_test_form_run($form_state['values'], $path);
  // we will rebuild the form.
  $form_state['rebuild'] = TRUE;
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
  $paths = array(
    file_directory_path() .'/ffmpeg_wrapper_test_output',
    file_directory_path() .'/ffmpeg_wrapper_test_input',
  );

  // get a list of the files that are currently in the directories
  foreach ($paths as $path) {
    if (is_dir($path)) {
      $files = file_scan_directory($path, '.*$');
      if (count($files)) {
        foreach ($files as $file) {
          file_delete($file->filename);
        }
      }
      // now delete the directory
      rmdir($path);
    }
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
 * This function builds out the list of files used in the
 * landing form
 * @return array
 */
function ffmpeg_wrapper_test_build_files() {
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
  $files = array();
  // get a list of files that can be processed
  foreach ($paths as $path) {
    $files = array_merge($files, file_scan_directory($path, '.*$'));
  }
  if (count($files)) {
    // extract the filenames
    $return = array();
    foreach ($files as $file) {
      $return[] = $file->filename;
    }
    return $return;
  }
}


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
  $output_file = file_create_filename(basename($input_file) .'.'. $params['ffmpeg_output_type'], ($output_file_path ? $output_file_path : dirname($input_file)));

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
function ffmpeg_wrapper_ffmpeg_test_form_run($form_values, $file_path) {
  $ffmpeg_object = new StdClass;
  // create a test directory if there is not already one
  $path = file_directory_path() .'/ffmpeg_wrapper_test_output';
  // make sure the directory exists
  file_check_directory($path, FILE_CREATE_DIRECTORY);
  // run ffmpeg on the file and configuration
  ffmpeg_wrapper_configuration_form_convert($file_path, $form_values, $path, $ffmpeg_object);

  return $ffmpeg_object;
}


/**
 * Takes the file chosen for attachment and attaches it to a node
 * @param $form_values
 * @return unknown_type
 */
function ffmpeg_wrapper_ffmpeg_test_file_attach($form_id, &$form_state) {
  // get the file to attach from the data
  $files_lookup = unserialize($form_state['values']['files_lookup']);
  $attach_file = $files_lookup[$form_state['values']['attach_files']];
  // load the node object
  $node = node_load($form_state['values']['nid']);
  // build the file object
  $file = new stdClass();
  $file->new = true;
  $file->fid = 'new';
  $file->uid = $node->uid;
  $file->filename = basename($attach_file);
  $file->filepath = file_create_filename(basename($attach_file), file_directory_path());
  $file->filemime = file_get_mimetype($attach_file);
  $file->filesize = filesize($attach_file);
  $file->timestamp = time();
  $file->list = variable_get('upload_list_default', 1);
  $file->status = 1;
  // save the file
  drupal_write_record('files', $file);
  // add the file to the node object
  $node->files[$file->fid] = $file;
  // save the modified node
  node_save($node);
  // alert the user
  drupal_set_message(t('Attached your transcoded file'));
  drupal_goto('node/'. $node->nid .'/edit');
}


/**
 * returns a json array of file type data
 * @return string
 */
function ffmpeg_wrapper_file_type_js() {
  // loop through the files to find the active one
  if (isset($_POST['files'])) {
    $files_lookup = unserialize($_POST['files_lookup']);
    // set the path
    $path = $files_lookup[$_POST['files']];

    if (! file_exists($path)){
      drupal_json(t('File does not exist'));
      exit();
    }

    // get the file data
    $file = ffmpeg_wrapper_file_data($files_lookup[$_POST['files']]);

    // theme the output
    $meta = array(
      t('File: !file', array('!file' => $path)),
      t('Format: !format', array('!format' => $file['format'])),
      t('Duration: !duration (seconds)', array('!duration' => $file['duration'])),
      t('Total bitrate: !rate', array('!rate' => $file['bitrate'])),
    );

    $video = array(
      t('Video codec: !codec', array('!codec' => $file['video']['codec'])),
      t('Frame size: !s', array('!s' => $file['video']['s'])),
      t('Bit rate: !br', array('!br' => $file['video']['br'])),
      null,
    );

    $audio = array(
      t('Audio codec: !codec', array('!codec' => $file['audio']['codec'])),
      t('Sample rate: !ar', array('!ar' => $file['audio']['ar'])),
      t('Bit rate: !br', array('!br' => $file['audio']['ab'])),
      t('Chanels: !ac', array('!ac' => $file['audio']['ac'])),
    );

    // build the rows for the display
    $rows = array($meta, $video, $audio);
    $output = theme('table', array(t('File information')), $rows);

    // return the html in json format
    drupal_json($output);
    exit();
  }
  // no file path was found, return empty
  drupal_json(' ');
  exit();

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


/**
 * Themes the list of radio buttons and other form data
 * @param $form
 * @return unknown_type
 */
function theme_ffmpeg_wrapper_files_radios(&$form) {
  $rows = array();
  $header = array(t('File Name'), t('Use'), t('Attach'), t('Mime type'), t('Size'));
  // Each radio is stored as a numeric under $form[X], use $count to get each one
  $count = 0;
  // select each of the options from the $files form and build from the #options
  foreach ($form['#options'] as $id => $key) {
    $row = array();
    // name of the file
    $row[] = drupal_render($form[$key]['name']);
    // remove the element title
    $form[$id]['#title'] = null;
    // render the radio
    $row[] = drupal_render($form[$count]);
    // unset the title
    $form['attach_files'][$count]['#title'] = null;
    // render the radio
    $row[] = drupal_render($form['attach_files'][$count]);
    $row[] = drupal_render($form[$key]['mime']);
    $row[] = drupal_render($form[$key]['size']);
    $rows[] = $row;
    $count++;
  }
  $output = theme('table', $header, $rows);
  $output .= drupal_render($form);
  return $output;
}

