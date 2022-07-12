'use strict';
define( [ "underscore", "rng" ], function( _, rng ) { return function( params_ ) {

var params = $.extend( {}, params_ );
// params.graph

var left = 0;
var right = 0;
var top = 0;
var bottom = 0;
var selection_name = "in_rect_" + new String( new Date().getTime() ) + new String( rng() );

var bRemove = !!( params.view.GetDragMode() === "select_remove" );
var bAdd = !!( params.view.GetDragMode() === "select_add" );
var bSelect = !!( params.view.GetDragMode() === "select" );
var bSelectNodes = !!( params.view.GetSelectMode() === "node" || params.view.GetSelectMode() === "both" );
var bSelectLinks = !!( params.view.GetSelectMode() === "link" || params.view.GetSelectMode() === "both" );
var bSelectBoth = !!( params.view.GetSelectMode() === "both" );
var bInverseDrag = false;

var values_changed_by_this_instance = {};

function CheckNode( node, real_x, real_y ) {
  var x = ( real_x === undefined ) ? node.getx() : real_x;
  var y = ( real_y === undefined ) ? node.gety() : real_y;
  var bInRect = ( x > left && x < right && y > top && y < bottom );
  CheckObject( node, bInRect );
}
  
function CheckLink( link ) {
  var from_x = link.get( "from" ) ? link.get( "from" ).getx() : 0;
  var from_y = link.get( "from" ) ? link.get( "from" ).gety() : 0;
  var to_x = link.get( "to" ) ? link.get( "to" ).getx() : 0;
  var to_y = link.get( "to" ) ? link.get( "to" ).gety() : 0;
  var x = ( from_x + to_x ) / 2;
  var y = ( from_y + to_y ) / 2;
  var bInRect = ( x > left && x < right && y > top && y < bottom );
  CheckObject( link, bInRect );
}
  
function CheckObject( object, bInRect ) {
  if (
    (
      (
        !bInverseDrag
        &&
        (
          ( !bRemove && bInRect ) // add or select mode, non-selected object entering rect
          ||
          ( bRemove && !bInRect && values_changed_by_this_instance[ object.id ] === true ) // remove mode, selected-before-remove object exiting rect
        )
      )
      ||
      (
        bInverseDrag
        &&
        (
          ( bInRect && ( values_changed_by_this_instance[ object.id ] === undefined || values_changed_by_this_instance[ object.id ] === false ) ) // inverse mode, non-selected object entering rect
          ||
          ( !bInRect && values_changed_by_this_instance[ object.id ] === true ) // inverse mode, selected-before-invert object exiting rect
        )
      )
    )
    &&
    !object.selected
    &&
    ( 
      // only appropriate objects should be selected
      bSelectBoth
      ||
      ( bSelectNodes && params.graph.IsNode( object ) )
      ||
      ( bSelectLinks && !params.graph.IsNode( object ) )
    )
  ) {

    object.selected = true;
    if ( values_changed_by_this_instance[ object.id ] === undefined ) {
      values_changed_by_this_instance[ object.id ] = false;
    }
    if ( params.on_object_included ) {
      params.on_object_included( object );
    }

  } else if (
    (
      (
        !bInverseDrag
        &&
        (
          ( bRemove && bInRect ) // remove mode, selected object entering rect
          ||
          ( bSelect && !bInRect ) // select mode, selected object exiting rect
          ||
          ( bAdd && !bInRect && values_changed_by_this_instance[ object.id ] === false ) // add mode, non-selected-before-add object exiting rect
        )
      )
      ||
      (
        bInverseDrag
        &&
        (
          ( bInRect && ( values_changed_by_this_instance[ object.id ] === undefined || values_changed_by_this_instance[ object.id ] === true ) ) // inverse mode, selected object entering rect
          ||
          ( !bInRect && values_changed_by_this_instance[ object.id ] === false ) // inverse mode, non-selected-before-invert object exiting rect
        )
      )
    )
    &&
    object.selected
    &&
    (
      // only appropriate objects should be deselected
      // except for select mode - initial deselection should deselect all kinds of object
      bSelect
      ||
      bSelectBoth
      ||
      ( bSelectNodes && params.graph.IsNode( object ) )
      ||
      ( bSelectLinks && !params.graph.IsNode( object ) )
    )
  ) {

    object.selected = false;
    if ( values_changed_by_this_instance[ object.id ] === undefined ) {
      values_changed_by_this_instance[ object.id ] = true;
    }
    if ( params.on_object_excluded ) {
      params.on_object_excluded( object );
    }

  }
}

var CheckNode__ = _.ary( CheckNode, 1 );
var CheckLink__ = _.ary( CheckLink, 1 );

var inst = {

  UpdateBounds : function( new_left, new_top, new_right, new_bottom, bInverseDrag_ ) {
    var tmp;
    if ( new_left > new_right ) {
      tmp = new_left; new_left = new_right; new_right = tmp;
    }
    if ( new_top > new_bottom ) {
      tmp = new_top; new_top = new_bottom; new_bottom = tmp;
    }
    if ( new_left !== left || new_right !== right || new_top !== top || new_bottom !== bottom ) {
      left = new_left;
      right = new_right;
      top = new_top;
      bottom = new_bottom;
      bInverseDrag = bInverseDrag_;
      if ( bSelectNodes || bSelect ) {
        var nodes = params.graph.GetNodes();
        nodes.each( CheckNode__ );
      }
      if ( bSelectLinks || bSelect ) {
        var links = params.graph.GetLinks();
        links.each( CheckLink__ );
      }
    }

  },

  CheckNode : CheckNode,

z:0};

return inst;

} } );