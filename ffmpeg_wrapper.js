// $Id$;

function ffmpeg_wrapper_update_options(output, prefix) {
  
  // get the path to the output settings set in ffmpeg_wrapper module
  var path = Drupal.settings.ffmpeg_wrapper.ffmpeg_wrapper_output_url;
  
  // now get the values for this codec
  $.getJSON(path+output, function(json) {
  
    // now we need to itterate through each of the keys returned and 
    // limit the choices to the incoming values.
    var data = eval (json);
    
    // first break the configuration into the types of config data we have
    for (var type in data) {
      // now get each of the items in the config      
      for (var key in data[type] ) {
       // get the element
       var element = '#'+prefix+'-'+type+'-'+key;
       //var element = '#edit-process--mm-ffmpeg--1--ffmpeg-'+type+'-'+key;
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
    }
  });
}
  


}

$.getJSON('http://localhost/5.7js/ffmpeg_wrapper/codec/vlc',
  function(json){
    var data = eval (json);
    // first break the configuration
    // into the types of config data we have
    for (var type in data) {
      // now get each of the items in the config      
      for (var key in data[type] ) {
       // get the element
       // var element = '#'+prefix+'-'+type+'-'+key;
       var element = '#edit-process--mm-ffmpeg--1--ffmpeg-'+type+'-'+key;
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
    }
  }
);