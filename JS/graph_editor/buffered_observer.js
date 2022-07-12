'use strict';
define( [ "underscore", "utils" ], function( _, utils ) { 

return function( observed_object, parent_notifier ) {

var changed_properties;
var callbacks = {};

var observer;

function _Reset() {
  changed_properties = { added:{}, changed:{}, deleted:{}, bChanged:false };
  if ( parent_notifier ) {
    parent_notifier( false );
  }
}
_Reset();

function do_callbacks( property, old_value, new_value ) {
  _.each( [ property, "" ], function( property_name ) {
    if ( _.has( callbacks, property_name ) ) {
      _.each( callbacks[ property_name ], function( callback ) {
        callback.do_callback( old_value, new_value, property );
      } );
    }
  } );
}

function on_change( model, value, options ) {
  var bChanged = false;
  _.each( model.changedAttributes(), function( value, property_name ) {
    var bNewUndefined = ( value === undefined );
    var bOldUndefined = ( model.previous( property_name ) === undefined );

    if ( !bNewUndefined && bOldUndefined ) {

      // add
      var bCurrentChanged = false;
      if ( property_name in changed_properties.deleted ) {
        bCurrentChanged = true;
        delete changed_properties.deleted[ property_name ];
        changed_properties.changed[ property_name ] = value;
      } else {
        bCurrentChanged = true;
        changed_properties.added[ property_name ] = value;
      }
      if ( bCurrentChanged ) {
        bChanged = true;
        do_callbacks( property_name, undefined, value );
      }

    } else if ( bNewUndefined && !bOldUndefined ) {

      // remove
      var bCurrentChanged = false;
      if ( property_name in changed_properties.changed ) {
        bCurrentChanged = true;
        delete changed_properties.changed[ property_name ];
      }
      if ( property_name in changed_properties.added ) {
        bCurrentChanged = true;
        delete changed_properties.added[ property_name ];
      } else {
        bCurrentChanged = true;
        changed_properties.deleted[ property_name ] = ( model.previousAttributes() )[ property_name ];
      }
      if ( bCurrentChanged ) {
        bChanged = true;
        do_callbacks( property_name, model.previous( property_name ), undefined );
      }

    } else if ( !bNewUndefined && !bOldUndefined ) {

      // change
      var bCurrentChanged = false;
      if ( property_name in changed_properties.added ) {
        bCurrentChanged = true;
        changed_properties.added[ property_name ] = value;
      } else {
        if ( model.previous( property_name ) != value ) {
          bCurrentChanged = true;
          changed_properties.changed[ property_name ] = value;
        }
      }
      if ( bCurrentChanged ) {
        bChanged = true;
        do_callbacks( property_name, model.previous( property_name ), value );
      }

    }
  } );
  if ( bChanged && !changed_properties.bChanged ) {
    changed_properties.bChanged = true;
  }
  if ( bChanged && parent_notifier ) {
    parent_notifier( true );
  }
}

var instance = {

  _GetChangedProperties : function() {
    return changed_properties;
  },

  _GetCallbacks : function() {
    return callbacks;
  },

  _GetParentNotifier : function() {
    return parent_notifier;
  },

  Reset : _Reset,

  ResetAsNew : function() {
    this.Reset();
    _.each( observed_object.attributes, function( value, key ) {
      changed_properties.added[ key ] = value;
    } );
  },

  EachAdded : function( f ) {
    _.each( changed_properties.added, f );
  },

  EachChanged : function( f ) {
    _.each( changed_properties.changed, f );
  },

  EachDeleted : function( f ) {
    _.each( changed_properties.deleted, f );
  },

  FlushAllObservers : function() {
  },

  ObservedObjectChildIsChanged : function( id ) {
    if ( observed_object.has( id ) ) {
      var notify_object = new Backbone.Model();
      notify_object.set( id, "" );
      notify_object.on( "change", on_change );
      notify_object.set( id, observed_object.get( id ) );
      notify_object.off( "change", on_change );
    }
  },

  Pause : function() {
    observed_object.off( "change", on_change );
  },

  Resume : function() {
    observed_object.on( "change", on_change );
  },

  IsChanged : function() {
    this.FlushAllObservers();
    return changed_properties.bChanged;
  },

  AddCallback : function( name, f /* ( old_value, new_value, property_name ) */ ) {
    callbacks[ name ] || ( callbacks[ name ] = [] );
    var callback_handle = { name: name, do_callback:f };
    callbacks[ name ].push( callback_handle );
    return callback_handle;
  },

  RemoveCallback : function( callback_handle ) {
    callbacks[ callback_handle.name ] || ( callbacks[ callback_handle.name ] = [] );
    utils.remove_item_from_array( callback_handle, callbacks[ callback_handle.name ] );
  },

  CopyFrom : function( another_buffered_observer ) {
    changed_properties = _.cloneDeep( another_buffered_observer._GetChangedProperties() );
    callbacks = _.cloneDeep( another_buffered_observer._GetCallbacks() );
    parent_notifier = another_buffered_observer._GetParentNotifier();
  },

z:0}; // instance

instance.Resume();

return instance;

} } );