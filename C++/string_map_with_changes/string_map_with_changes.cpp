#include "common.h"

#include "string_map_with_changes.h"

namespace application {

string_map_with_changes::string_map_with_changes( void )
  : m_bForcedChanges( false )
{
}

string_map_with_changes::string_map_with_changes( const string_map_with_changes & o )
: m_original( o.m_original )
, m_changes( o.m_changes )
, m_deletions( o.m_deletions )
, m_bForcedChanges( o.m_bForcedChanges )
{
}

string_map_with_changes::string_map_with_changes( const string_map & params )
: m_original( params )
, m_bForcedChanges( false )
{
}

string_map_with_changes::~string_map_with_changes( void ) {
}

void string_map_with_changes::load( const string_map & original ) {
  reset(); // clear change log
  m_original = original;
}

void string_map_with_changes::clear( void ) {
  reset(); // clear change log
  m_original.clear();
}

void string_map_with_changes::apply( void ) {
  compact( *this ); // leave only relevant changes
  // update original values
  foreach( const string_map_item & item, m_changes ) {
    m_original[ item.first ] = item.second;
  }
  // remove original items if needed
  foreach( const string & key, m_deletions ) {
    m_original.erase( key );
  }
  reset(); // clear change log
}

void string_map_with_changes::touch_all( void ) {
  m_bForcedChanges = true;
  foreach( const string_map_item & item, m_original ) {
    string_set::iterator deleted_item( m_deletions.find( item.first ) ); // should not re-touch values already deleted
    if ( deleted_item == m_deletions.end() ) {
      string_map::iterator changed_item( m_changes.find( item.first ) );
      if ( changed_item == m_changes.end() ) { // should not re-touch values already changed
        m_changes.insert( item );
      }
    }
  }
}

void string_map_with_changes::reset( void ) {
  m_changes.clear();
  m_deletions.clear();
  m_bForcedChanges = false;
}

const string & string_map_with_changes::get_const_value( const string & key ) const {
  static const string empty_value;
  const string * result = do_get_const_value( key );
  return result ? *result : empty_value;
}

const string * string_map_with_changes::do_get_const_value( const string & key ) const {
  bool bIsDeleted = ( m_deletions.find( key ) != m_deletions.end() );
  string_map::const_iterator i( m_changes.find( key ) );
  if ( i == m_changes.end() ) {
    if ( bIsDeleted ) {
      // deleted value
      return NULL;
    } else {
      i = m_original.find( key );
      if ( i == m_original.end() ) {
        // value never existed
        return NULL;
      } else {
        // original value
        return &( i->second );
      }
    }
  } else {
    // changed value
    return &( i->second );
  }
}

bool string_map_with_changes::value_exists( const string & key ) const {
  return do_get_const_value( key ) != NULL;
}

string & string_map_with_changes::get_value_to_update( const string & key ) {
  bool bDeleted = false;
  // recreate deleted value
  string_set::iterator deleted( m_deletions.find( key ) );
  if ( deleted != m_deletions.end() ) {
    bDeleted = true;
    m_deletions.erase( deleted );
  }
  string_map::iterator i( m_changes.find( key ) );
  if ( i == m_changes.end() ) {
    if ( !bDeleted ) {
      // unchanged yet value
      i = m_original.find( key );
      if ( i != m_original.end() ) {
        // existing unchanged value
        m_changes[ key ] = i->second;
      }
    }
    return m_changes[ key ];
  } else {
    // already changed value
    return i->second;
  }
}

void string_map_with_changes::unset_value( const string & key, const bool keep_nonexisting_key ) {
  m_changes.erase( key );
  if ( keep_nonexisting_key || m_original.find( key ) != m_original.end() ) {
    m_deletions.insert( key );
  }
}

void string_map_with_changes::compact( string_map_with_changes & to ) const {
  if ( &to != this ) {
    to.m_changes = m_changes;
    to.m_deletions = m_deletions;
  }
  // m_changes have priority since it can be changed outside after get_values_to_update()
  foreach( const string_map_item & item, m_changes ) {
    to.m_deletions.erase( item.first );
  }
  if ( !m_bForcedChanges ) {
    // leave only relevant changes
    foreach( const string_map_item & item, m_original ) {
      string_map::iterator changed_item( to.m_changes.find( item.first ) );
      if ( changed_item != to.m_changes.end() && changed_item->second == item.second ) {
        to.m_changes.erase( changed_item );
      }
    }
  }
}

bool string_map_with_changes::could_be_changed( const string_map_with_changes & to ) const {
  return to.m_changes.size() > 0 || to.m_deletions.size() > 0;
}

bool string_map_with_changes::could_be_changed( void ) const {
  return could_be_changed( *this );
}

bool string_map_with_changes::do_is_changed( string_map_with_changes & to ) const {
  compact( to );
  return could_be_changed( to );
}

bool string_map_with_changes::is_changed( void ) const {
  // computed by compaction to the temporary value
  string_map_with_changes compacted;
  return do_is_changed( compacted );
}

string_map & string_map_with_changes::get_values_to_update( void ) {
  foreach( const string_map_item & item, m_original ) {
    string_map::iterator changed_item( m_changes.find( item.first ) );
    if ( changed_item == m_changes.end() ) {
      m_changes[ item.first ] = item.second;
    }
  }
  foreach( string key, m_deletions ) {
    m_changes.erase( key );
  }
  return m_changes;
}

bool string_map_with_changes::get_changes( const string_map * & changes, const string_set * & deletions ) const {
  bool bChanged = do_is_changed( *( const_cast < string_map_with_changes * > ( this ) ) ); // compact itself
  changes = &m_changes;
  deletions = &m_deletions;
  return bChanged;
}

const string_map & string_map_with_changes::get_const_values( void ) const {
  string_map_with_changes & this_ = *( const_cast < string_map_with_changes * > ( this ) );
  bool bChanged = do_is_changed( this_ ); // compact itself
  if ( bChanged ) {
    string_map_with_changes temp( *this );
    temp.apply();
    this_.m_original_temp = temp.m_original;
    return this_.m_original_temp;
  } else {
    return m_original;
  }
}

string_map_with_changes & string_map_with_changes::operator=( const string_map_with_changes & source ) {
  m_original = source.m_original;
  m_changes = source.m_changes;
  m_deletions = source.m_deletions;
  m_original_temp = source.m_original_temp;
  m_bForcedChanges = source.m_bForcedChanges;
  return *this;
}

const string_map & string_map_with_changes::get_original( void ) const {
  return m_original;
}

bool string_map_with_changes::get_original_value( const string & key, string & value ) const {
  bool bResult = false;
  string_map::const_iterator i( m_original.find( key ) );
  if ( i == m_original.end() ) {
    value.clear();
  } else {
    value = i->second;
    bResult = true;
  }
  return bResult;
}

bool string_map_with_changes::empty( void ) const {
  return get_const_values().size() == 0;
}

} // namespace application