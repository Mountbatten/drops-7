<?php

/**
 * @file
 * Processing functions for layers and behaviors
 * @ingroup openlayers
 */

/**
 * Initialize the layer array into an indexed array of layer objects
 *
 * @param $layers
 *   Array of layers to process
 * @param $map
 *   Map array
 * @return $layer_data
 *   Array of initialized layer objects
 */
function _openlayers_layers_process($layers = array(), &$map = array()) {
  $layer_data = array();

  // Load Layers and assign weights
  foreach ($layers as $key => $layer) {
    if ($layer_object = openlayers_layer_load($layer)) {
      $layers[$key] = $layer_object;
      if (!empty($map['layer_weight'][$key])) {
        $layers[$key]->weight = $map['layer_weight'][$key];
      }
      else $layers[$key]->weight = 0;
    }
    else unset($layers[$key]);
  }

  // Sort layers
  usort($layers, '_openlayers_layers_process_sort');

  // Process into array-based layer data for the map
  foreach ($layers as $type => $layer_object) {
    if (is_object($layer_object)) {
      $layer_object->render($map);
      $layer_object->data['title'] = $layer_object->title;
      $layer_object->data['weight'] = $layer_object->weight;
      $layer_data[$layer_object->name] = $layer_object->data;
    }
  }

  return $layer_data;
}

/**
 * Callback function for sorting
 *
 * @param $a
 *   Layer $a
 * @param $b
 *   Layer $b
 * @return $a_greater_b
 *   Return the weight different - allowing usort to sort
 */
function _openlayers_layers_process_sort($a, $b) {
  return intval($a->weight - $b->weight);
}

/**
 * Execute render() method for all enabled behaviors.
 *
 * @param $behaviors
 *   Array of behaviors to process
 * @param $map
 *   Map array
 * @return $rendered
 *  Indexed array of rendered behaviors
 */
function _openlayers_behaviors_render($behaviors = array(), &$map = array()) {
  $rendered = array();

  foreach (openlayers_behaviors() as $key => $plugin) {
    if (isset($behaviors[$key]) && $class = ctools_plugin_get_class($plugin, 'behavior')) {
      $behavior = new $class($behaviors[$key], $map);
      $rendered[$key] = $behavior->render($map);
    }
  }

  return $rendered;
}

/**
 * Process Styles
 *
 * Get full data for any styles.  The idea is that we load
 * all the needed styles into the ['styles'] key of the
 * map object, and keep a reference in ['layer_styles']
 * and ['layer_styles_select'] for layer specific styling.
 *
 * TODO: Overall, this is not a great approach to managing
 * styles.
 *
 * @param $styles
 *   Array of map styles ( <style_role> : <style_name> | <style_array> )
 * @param $layer_styles
 *   Array of layer styles ( <layer_name> : <style_name> )
 * @param $layer_styles_select
 *   Array of layer styles ( <layer_name> : <style_name> )
 * @param $map
 *   Map array
 * @return $processed
 *   Array of processed styles ( <style_name> => <style_array> )
 */
function _openlayers_styles_process($styles = array(),
  $layer_styles = array(), $layer_styles_select = array(), $layer_styles_temporary = array(), &$map = array()) {

  // Get styles info array
  $styles_info = openlayers_styles();

  // Go through styles
  $processed = array();
  foreach ($styles as $k => $style) {
    // Check if array, if array, just pass on
    if (is_array($style)) {
      $processed[$k] = $style;
    }
    elseif (!empty($styles_info[$style]) && $info = $styles_info[$style]->data) {
      $processed[$k] = $info;
    }
  }

  // Add layer styles
  foreach ($layer_styles as $key => $style) {
    if (!isset($processed[$style]) &&
        !empty($styles_info[$style]) &&
        $info = $styles_info[$style]->data) {
      $processed[$style] = $info;
    }
    if (empty($style)) {
      // If a particular layer's style is 0, set it to the appropriate default.
      $map['layer_styles'][$key] = 'default';
    }
  }

  // Add layer styles select
  foreach ($layer_styles_select as $key => $style) {
    if (!isset($processed[$style]) &&
        !empty($styles_info[$style]) &&
        $info = $styles_info[$style]->data) {
      $processed[$style] = $info;
    }
    if (empty($style)) {
      // If a particular layer's style is 0, set it to the appropriate default.
      $map['layer_styles_select'][$key] = 'select';
    }
  }

  // Add layer styles temporary
  foreach ($layer_styles_temporary as $key => $style) {
    if (!isset($processed[$style]) &&
      !empty($styles_info[$style]) &&
      $info = $styles_info[$style]->data) {
      $processed[$style] = $info;
    }
    if (empty($style)) {
      // If a particular layer's style is 0, set it to the appropriate default.
      $map['layer_styles_temporary'][$key] = 'temporary';
    }
  }

  // Update URLs to support different types of paths
  foreach ($processed as $k => $style) {
    $processed[$k] = openlayers_render_style($style);
  }

  // Run through theme function
  $processed = theme('openlayers_styles', array(
    'styles' => $processed,
    'map' => $map)
  );

  // Return processed
  return $processed;
}

/**
 * Render style array.
 *
 * At the moment, this only makes the external grpahics
 * relative.
 */
function openlayers_render_style($style = array()) {
  // Relative path conversion
  if (!empty($style['externalGraphic'])) {
    // Check full URL or absolute path
    if (!valid_url($style['externalGraphic'], TRUE)
      && strpos($style['externalGraphic'], '/') !== 0) {
      // Make full URL from Drupal path
      $style['externalGraphic'] = openlayers_style_path($style['externalGraphic']);
    }
  }

  return $style;
}

/**
 * Create Map ID
 *
 * Create a unique ID for any maps that are not assigned an ID
 *
 * @note
 *   Technically someone can assign a map ID identical
 *   to the one that is created
 * @return
 *   New map id
 */
function _openlayers_create_map_id() {
  return drupal_html_id('openlayers-map');
}

/**
 * URL Style
 *
 * Takes in a path and makes full URL for style.  Overall, this
 * can be handled by url(), but we have to avoid some encoding
 * for variable replacement.  Note that this is not perfect as
 * it will decode values that maybe not specifically part of the
 * attribute replacement.
 *
 * A value that is just a replacement value, ${value} should
 * not be run through the file_create_url() function.
 *
 * @param $path
 *   Path to process.
 * @return
 *   Processed path.
 */
function openlayers_style_path($path) {
  if (strpos($path, '${') !== 0) {
    $path = file_create_url($path);
    $path = str_replace('%24%7B', '${', $path);
    $path = str_replace('%7D', '}', $path);
  }
  return $path;
}

