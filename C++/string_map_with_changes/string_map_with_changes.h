#pragma once

#include "algorithm/string_map.h"

namespace application {

class string_map_with_changes;

class string_map_with_changes {
protected:
  string_map m_original;
  string_map m_changes;
  string_set m_deletions;
  string_map m_original_temp;
  bool m_bForcedChanges;
  void compact( string_map_with_changes & to ) const;
  bool do_is_changed( string_map_with_changes & to ) const;
  bool could_be_changed( const string_map_with_changes & to ) const;
  const string * do_get_const_value( const string & key ) const;
public:
  string_map_with_changes( void );
  string_map_with_changes( const string_map_with_changes & o );
  string_map_with_changes( const string_map & params );
  virtual ~string_map_with_changes( void );
  void clear( void );
  void load( const string_map & original );
  void apply( void );
  void touch_all( void );
  void reset( void );
  const string & get_const_value( const string & key ) const;
  string & get_value_to_update( const string & key );
  string_map & get_values_to_update( void );
  const string_map & get_const_values( void ) const;
  void unset_value( const string & key, const bool keep_nonexisting_key = false );
  bool value_exists( const string & key ) const;
  bool is_changed( void ) const;
  bool could_be_changed( void ) const;
  bool get_changes( const string_map * & changes, const string_set * & deletions ) const;
  string_map_with_changes & operator=( const string_map_with_changes & source );
  const string_map & get_original( void ) const;
  bool get_original_value( const string & key, string & value ) const;
  bool empty( void ) const;
};

} // namespace application