// $Id$;

/**
 * @param string $prefix is the prefix of the elements
 * @param string $source is the source for the output type
 */
function ffmpeg_wrapper_update_options(prefix, source) {

  // get the path to the output settings set in ffmpeg_wrapper module
  var path = Drupal.settings.ffmpeg_wrapper.ffmpeg_wrapper_output_url;
  
  // get the output type
  var output = $('#'+prefix+source).val();
  
  // only look for the value if there is one selected
  if (output != 0) {
    // now get the values for this codec
    $.getJSON(path+output, function(json) {
    
      // now we need to itterate through each of the keys returned and 
      // limit the choices to the incoming values.
      var data = eval (json);
      
      // first break the configuration into the types of config data we have
      for (var type in data) {
        // catch the flag for default settings and don't do 
        // any form updating for this value
        if (data[type] != 'default') {
          // now get each of the items in the config      
          for (var key in data[type] ) {
           // build the element from the prefix value, ffmpeg, the kind of item it is, and the key 
           var element = '#'+prefix+'ffmpeg-'+type+'-'+key;
  
           // make sure element exists
           if ($(element)) {
             // remove existing items   
             $(element).html('');
             var html = '';
             for (var option in data[type][key]) {
               html += '<option value="'+data[type][key][option]+'">'+data[type][key][option]+'</option>';
             }
             $(element).html(html);
            }
          }
          // now turn on the advanced options so they are picked up
          $('#'+prefix+'ffmpeg-'+type+'-advanced').attr('checked', true);
        }            
        else {
          // is the configuration data being passed back was the default data?
          // turn off the advanced configuration so that nothing nasty 
          // happens without user action
          $('#'+prefix+'ffmpeg-audio-advanced').attr('checked', false);
          $('#'+prefix+'ffmpeg-video-advanced').attr('checked', false);        
        }
      }
    });
  }
}