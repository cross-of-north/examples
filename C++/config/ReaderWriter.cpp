#include "../common.h"
#pragma hdrstop

#include "ReaderWriter.h"

#include "ConfigTree.h"
#include "ConfigData.h"
#include "sdfiniter.h"
#include "tcaahead.h"
#include "sdf.h"
#include "sdftcaa.h"
#include "tcaadir.h"
#include "debtrap.h"
#include "inifile.h"
#include "GetOSVar.h"
//#include <inifiles.hpp>
#include <comctrls.hpp>

static const char * s_autoedit = "SDF_AUTOEDIT";
static const char * s_if = ".IF ";
static const char * s_sdf_ini = ".\\sdf.ini";
static const char * s_config_type = SDF_CONFIG_TYPE;
static const char * s_1 = "1";
static const char * s_edited_by_meta = "SDF_EDITED_BY_META";
static const char * s_admin = "SDF_ADMIN";
static const char * s_path_divider = ";";

static AnsiString GetSdfIniFileName( void ) {
  return AnsiString( GetTCAADir() ) + s_sdf_ini;
}

static AnsiString GetTCAANetIniFileName( void ) {
  return AnsiString( GetTCAADir() ) + "tcaanet.ini";
}

class TIniIterator {

  protected:

    std::string m_old_unexpanded_ini_path;
    std::string m_old_expanded_ini_path;
//    std::string m_sdf_path;
    const TConfigData * m_old_data;
    TIniFile * m_current_ini;
    const TConfigTree * m_meta_tree;
    typedef std::map < std::string, TIniFile * > ini_files;
    ini_files m_ini_files;

    AnsiString GetIniFileName( const TConfigNode * meta, const TConfigData * data, AnsiString & error );
    std::string BuildSectionName( const std::string & base, const TConfigNode * meta );
    std::string GetIniPath( const TConfigNode * meta );

  public:

    TIniIterator( const TConfigTree * meta_tree );
    ~TIniIterator();

    bool SwitchIni( const TConfigNode * meta_node, const TConfigData * data, AnsiString & error );
    TIniFile * GetIniFile( std::string path, AnsiString & error );

    enum IterateMode {
      imRead,
      imWrite,
      imWriteSDF
    };
    bool Iterate( TConfigData * data, const IterateMode im, AnsiString & error );

    static bool IterateFilter( TConfigPage * main_page, TConfigData * data,
      TIniFile * ini, AnsiString section_name, const std::string prefix, const TIniIterator::IterateMode im );

    bool Commit( AnsiString & error );

    static AnsiString ExpandValue( const TConfigData * data, const std::string & s );
    
//    void SetSDFPath( const std::string sdf_path );
};

TIniIterator::TIniIterator( const TConfigTree * meta_tree )
  : m_meta_tree( meta_tree )
  , m_old_data( NULL )
  , m_current_ini( NULL )
{
}

TIniIterator::~TIniIterator() {
  //delete m_ini;
  ini_files::iterator i = m_ini_files.begin();
  while ( i != m_ini_files.end() ) {

    // Commit() must be called explicitly
    i->second->DoNotFlush();

    delete i->second;
    i++;
  }
}

bool TIniIterator::Commit( AnsiString & error ) {
  bool bResult = true;
  ini_files::iterator i = m_ini_files.begin();
  while ( i != m_ini_files.end() ) {
    i->second->Flush();
    if ( i->second->IsError() ) {
      bResult = false;
      error = i->second->GetErrorString().c_str();
      break;
    }
    i++;
  }
  return bResult;
}

extern bool g_empty_userdomain_patch;
extern bool g_empty_computername_patch;

// static
AnsiString TIniIterator::ExpandValue( const TConfigData * data, const std::string & s ) {
  TReplaceFlags rf;
  rf = ( rf << rfReplaceAll );
  AnsiString result = StringReplace( c_str( s ), "/*NAME*/", c_str( data->GetName() ), rf );

  std::string computername = data->GetValue( s_main_tab, "COMPUTERNAME" );
  if ( g_empty_computername_patch && computername.empty() ) {
    computername = GetOSVariable( "COMPUTERNAME" );
  }
  result = StringReplace( result, "/*COMPUTERNAME*/", computername.c_str(), rf );

  result = StringReplace( result, "/*USERNAME*/", data->GetValue( s_main_tab, "USERNAME" ).c_str(), rf );

  std::string userdomain = data->GetValue( s_main_tab, "USERDOMAIN" );
  if ( g_empty_userdomain_patch && userdomain.empty() ) {
    userdomain = GetOSVariable( "USERDOMAIN" );
  }
  result = StringReplace( result, "/*USERDOMAIN*/", userdomain.c_str(), rf );

  return result;
}

void OverrideSDFVariable( const char * name, const TConfigData * data, TSDFVariables & sdf ) {
  std::string value = data->GetValue( s_sdf, name );
  if ( !value.empty() ) {
    sdf.Set( name, TSDFExpander( sdf ).Expand( value ) );
  }
}

std::string ExpandFileName( const std::string name, const std::string app, const TConfigData * data,
  AnsiString & error ) {
  
  std::string result = name;
  std::string version;
  std::string tap = data->GetValue( "tcaanet", "TA" );
  TSDFVariables sdf;
  std::string dummy;
  ITCAAStartInit si( NULL, dummy, dummy, dummy, dummy, dummy );
  TSDFVariablesTCAAIniter::Init( sdf, &si, app, version, tap );
  #define OVERRIDE_SDF(name) OverrideSDFVariable( name, data, sdf );
  int i = 0;
  while ( i < 2 ) {
    // two passes are needed when SDF_HOME is defined using SDF_CONFIG, for example
    OVERRIDE_SDF( "SDF_HOME" )
    OVERRIDE_SDF( "SDF_CONFIG" )
    OVERRIDE_SDF( "SDF_LOADER" )
    OVERRIDE_SDF( "SDF_TEMP" )
    OVERRIDE_SDF( "SDF_UPDATE" )
    OVERRIDE_SDF( "SDF_TCAANET" )
    i++;
  }
  result = TSDFExpander( sdf ).Expand( result );
  if ( result.length() > 0 && result[ 0 ] == '.' ) {
    result = GetTCAADir() + result;
  }
  result = TIniIterator::ExpandValue( data, result ).c_str();
  AnsiString folder = ExtractFilePath( c_str( result ) );
  if ( !ForceCreateDir( folder.c_str() ) ) {
    error = ( TLanguageString( "Невозможно создать папку ", "Can't create folder " ) + "\"" + folder + "\" (" + c_str( data->GetName() ) + ":" + c_str( name ) + ")" ).c_str();
    Application->MessageBox(
      error.c_str(),
      TLanguageString( "Ошибка", "Error" ),
      MB_OK | MB_ICONSTOP
    );
    result = "";
  }
  return result;
}

std::string TIniIterator::GetIniPath( const TConfigNode * meta ) {
  std::string result = meta->GetIniPath();
  if ( result.empty() ) {
    result = meta->GetParent()->GetIniPath();
  }
  return result;
}

AnsiString TIniIterator::GetIniFileName( const TConfigNode * meta, const TConfigData * data, AnsiString & error ) {
  std::string result = GetIniPath( meta );
  if ( m_old_unexpanded_ini_path != result || m_old_data != data ) {
    m_old_unexpanded_ini_path = result;
    m_old_data = data;
    std::string app = meta->GetAppForIni();
    if ( app.empty() ) {
      app = meta->GetParent()->GetAppForIni();
    }
    result = ExpandFileName( result, app, data, error );
    m_old_expanded_ini_path = result;
  }
  return c_str( m_old_expanded_ini_path );
}

TIniFile * TIniIterator::GetIniFile( std::string path, AnsiString & error ) {
  TIniFile * result = NULL;
  result;
  std::string canonic_path = CanonicalizePath( path );
  ini_files::iterator p = m_ini_files.find( canonic_path );
  if ( p == m_ini_files.end() ) {
    result = new TIniFile( canonic_path.c_str() );
    m_ini_files.insert( ini_files::value_type( canonic_path, result ) );
  } else {
    result = p->second;
  }
  if ( result->IsError() ) {
    error = result->GetErrorString().c_str();
    result = NULL;
  }
  return result;
}

bool TIniIterator::SwitchIni( const TConfigNode * meta_node, const TConfigData * data, AnsiString & error ) {
  bool bResult = true;
  AnsiString new_ini_name = GetIniFileName( meta_node, data, error );
  if ( new_ini_name.IsEmpty() ) {
    // error while creating ini file dir
    bResult = false;
  } else {
    if ( &*m_current_ini == NULL || new_ini_name != m_current_ini->GetFileName() ) {
      m_current_ini = GetIniFile( new_ini_name.c_str(), error );
      if ( m_current_ini == NULL ) {
        bResult = false;
      }
    }
  }
  return bResult;
}

std::string TIniIterator::BuildSectionName( const std::string & base, const TConfigNode * meta_node ) {
  std::string section_name = meta_node->GetSectionName();
  if ( section_name.empty() ) {
    section_name = base;
  }
  section_name = section_name + meta_node->GetSectionSuffix();
  return section_name;
}

bool TIniIterator::Iterate( TConfigData * data, const IterateMode im, AnsiString & error ) {

  ConfigSetType cst = data->GetConfigType();

  bool bResult = true;
  int page_count = m_meta_tree->GetPageCount();
  int page_index = 0;
  AnsiString ini_name;
  bool bSDFChanged = data->IsPageChanged( s_sdf ); // если изменен SDF, то пишем все, т.к. могут быть новые пути
  while ( page_index < page_count && bResult ) {
    TConfigPage * meta_page = m_meta_tree->GetPage( page_index );
    std::string page_name = meta_page->GetName();
    int node_count = meta_page->GetNodeCount();
    int node_index = 0;
    bool bAutoDeletedPage = ( ( im == imWrite || im == imWriteSDF ) && data->IsShouldBeAutoDeleted( meta_page ) );

    if (
      page_name != s_main_tab &&
      meta_page->IsEnabledFor( data ) &&
      ( im == imRead || im == imWriteSDF || bSDFChanged || data->IsPageChanged( page_name ) )
    ) {
      while ( node_index < node_count && bResult ) {

        TConfigNode * meta_node = meta_page->GetNode( node_index );

        std::string node_name = meta_node->GetName();

        std::string section_name = BuildSectionName( data->GetName(), meta_node );
        std::string name = meta_node->GetValueName();
        if ( name.empty() ) {
          name = node_name;
        }

        if (
          ( ( im == imWriteSDF ) == ( GetIniPath( meta_node ) == s_sdf_ini ) ) ||
          im == imRead
        ) {
          bResult = SwitchIni( meta_node, data, error );
          if ( bResult ) {
            data->SetLastIniName( page_name, m_current_ini->GetFileName() );
            if ( im == imWrite || im == imWriteSDF ) {
              if ( meta_node->GetType() == TConfigNode::ntDeleteSection ) {
                if ( data->IsDeleted() || data->IsRenamed() || data->IsShouldBeAutoDeleted( meta_node ) ) {
                  std::string section_to_delete = BuildSectionName( data->GetOldName(), meta_node );
                  std::string old_ini_name = data->GetOldIniName( page_name );
                  TIniFile * ini_file = NULL;
                  if ( old_ini_name.empty() ) {
                    ini_file = m_current_ini;
                  } else {
                    ini_file = GetIniFile( old_ini_name, error );
                  }
                  if ( ini_file == NULL ) {
                    bResult = false;
                  } else {
                    ini_file->EraseSection( c_str( section_to_delete ) );
                  }
                }
              } else {
                if ( !bAutoDeletedPage && !data->IsDeleted() ) {
                  std::string value = data->GetValue( page_name, node_name );
                  if (
                    !meta_node->IsVisible() && !meta_node->CanCopy() &&
                    !( cst == cstAdmin && page_name == s_sdf && node_name == SDF_UPDATE ) // the only exclusion
                  ) {
                    // non-visible non-copyable node is overriden to default value
                    value = meta_node->GetDefaultValue();
                    data->SetValue( page_name, node_name, value );
                  }
                  if ( !meta_node->IsWriteEmpty() || value.empty() ) {
                    m_current_ini->DeleteKey( c_str( section_name ), c_str( name ) );
                  }
                  if ( meta_node->IsWriteEmpty() || !value.empty() ) {
                    m_current_ini->WriteString( c_str( section_name ), c_str( name ), ExpandValue( data, value ).c_str() );
                  }
                }
              }
            } else {
              DebugTrapAssert( im == imRead, "invalid mode" );
              static const char * s_undefined = "/*UNDEFINED*/";
              AnsiString value = m_current_ini->ReadString( c_str( section_name ), c_str( name ), s_undefined ).c_str();
              if ( value != s_undefined ) {
                data->SetValue( page_name, node_name, value.c_str() );
              }
            }
          }
        }

        node_index++;
      }
    }
    page_index++;
  }

  return bResult;
}

// static
bool TIniIterator::IterateFilter( TConfigPage * main_page, TConfigData * data,
  TIniFile * ini, AnsiString section_name, const std::string prefix, const TIniIterator::IterateMode im ) {

  bool bFilterExists = false;

  if ( main_page == NULL ) {
    DebugTrap( "filter page is not found" );
  } else {

    int main_node_index = 0;
    int main_node_count = main_page->GetNodeCount();
    while ( main_node_index < main_node_count ) {
      TConfigNode * main_node = main_page->GetNode( main_node_index );
      std::string name = main_node->GetName();
      if ( name != s_main_tab_name && name != s_main_tab_additional ) {
        AnsiString value_name;
        if ( prefix.empty() ) {
          value_name = c_str( name );
        } else {
          value_name = AnsiString( c_str( prefix ) ) + "%" + c_str( name ) + "%";
        }
        std::string value;
        if ( im == imWrite ) {
          value = data->GetValue( main_page->GetName(), name );
          if ( !value.empty() ) {
            bFilterExists = true;
            ini->WriteString( section_name.c_str(), value_name.c_str(), c_str( value ) );
          }
        } else {
          DebugTrapAssert( im == imRead, "invalid mode" );
          value = ini->ReadString( section_name.c_str(), value_name.c_str(), "" ).c_str();
          data->SetValue( main_page->GetName(), name, value );
        }
        if ( !value.empty() ) {
          bFilterExists = true;
        }
      }
      main_node_index++;
    }
    if ( im == imWrite ) {
      std::string value = data->GetValue( main_page->GetName(), s_main_tab_additional );
      string_list sl;
      sl = Explode( value, "\r\n" );
      TrimStringList( sl );
      string_list::iterator i = sl.begin();
      while ( i != sl.end() ) {
        int eq_pos = i->find( "=" );
        if ( eq_pos != -1 ) {
          bFilterExists = true;
          ini->WriteString( section_name.c_str(), c_str( s_if + i->substr( 0, eq_pos ) ), c_str( i->substr( eq_pos + 1, i->length() - eq_pos - 1 ) ) );
        }
        i++;
      }
    } else {
      DebugTrapAssert( im == imRead, "invalid mode" );
      std::auto_ptr < TStringList > sl( new TStringList() );
      ini->ReadSectionValues( section_name, &*sl );
      int i = 0;
      int prefix_len = strlen( s_if );
      AnsiString text;
      while ( i < sl->Count ) {
        AnsiString key = sl->Names[ i ];
        if ( key.SubString( 1, prefix_len ) == s_if ) {
          AnsiString name = key.SubString( prefix_len + 1, key.Length() );
          if ( !data->HaveValue( main_page->GetName(), name.SubString( 2, name.Length() - 2 ).c_str() ) && name != s_1 ) {
            // strip % %
            // non-standard condition
            text += ( name + "=" + sl->Values[ key ] + "\r\n" );
          }
        }
        i++;
      }
      data->SetValue( main_page->GetName(), s_main_tab_additional, text.c_str() );
    }
    
  }

  return bFilterExists;
}

static void CopyParameter( TConfigData * data, const TConfigPage * source, const TConfigPage * target, const char * name ) {
  std::string value = data->GetValue( source->GetName(), name );
  data->SetValue( target->GetName(), name, value );
}

static void CopyFilter( TConfigData * data, const TConfigPage * source, const TConfigPage * target ) {
  CopyParameter( data, source, target, "COMPUTERNAME" );
  CopyParameter( data, source, target, "USERNAME" );
  CopyParameter( data, source, target, "USERDOMAIN" );
}

// ******* writer **************************************************************

TIniWriter::TIniWriter( const TConfigTree * meta_tree, const TReaderWriterList * data, TReaderWriterList * deleted_data )
  : m_meta_tree( meta_tree )
  , m_data( data )
  , m_deleted_data( deleted_data )
  , m_iterator( new TIniIterator( meta_tree ) )
{
}

TIniWriter::~TIniWriter() {
  delete m_iterator;
}

void UpdateOldIni( const TConfigTree * meta_tree, TConfigData * data ) {
  int page_index = meta_tree->GetPageCount();
  while ( page_index > 0 ) {
    page_index--;
    std::string name = meta_tree->GetPage( page_index )->GetName();
    data->SetOldIniName( name, data->GetLastIniName( name ) );
    data->SetLastIniName( name, "" );
  }
}

void TIniWriter::CommitData( const TConfigData * data_ ) {
  if ( data_->IsChanged() ) {
    TConfigData * data = const_cast < TConfigData * > ( data_ );
    data->SetOldName( data->GetName() );
    UpdateOldIni( m_meta_tree, data );
    data->SetChanged( false );
  }
}

bool TIniWriter::WriteData( const TConfigData * data, AnsiString & error ) {
  bool bResult = m_iterator->Iterate( const_cast < TConfigData * > ( data ), TIniIterator::imWriteSDF, error );
  if ( bResult && data->IsChanged() ) {
    bResult = m_iterator->Iterate( const_cast < TConfigData * > ( data ), TIniIterator::imWrite, error );
  }
  return bResult;
}

bool TIniWriter::WriteSDF( AnsiString & error ) {
  AnsiString sdf_name = GetSdfIniFileName();
  TIniFile * ini = m_iterator->GetIniFile( sdf_name.c_str(), error );
  bool bResult = ( ini != NULL );
  if ( bResult ) {
  //  TIniFile * ini_tcaanet = m_iterator->GetIniFile( GetTCAANetIniFileName().c_str() );
    TReaderWriterList::const_iterator i = m_data->begin();
    std::string admin_update = m_meta_tree->GetAdminUpdate();
    std::string admin_update_uc = UpperCase( c_str( admin_update ) ).c_str();
    while ( i != m_data->end()) {
      TConfigData * data = *i;
      AnsiString meta;
      if ( !data->GetOldName().empty() ) {
        meta = c_str( ini->ReadString( c_str( data->GetOldName() ), s_edited_by_meta, "" ) );
        ini->EraseSection( c_str( data->GetOldName() ) );
      }
      if ( !data->IsDeleted() ) {
        if ( data->GetOldName() != data->GetName() || data->IsRenamed() ) {
          data->SetAllPagesChanged();
        }
        ConfigSetType cst = data->GetConfigType();
        if ( cst == cstAdmin ) {
          std::string old_value = data->GetValue( s_sdf, SDF_UPDATE );
          if ( old_value != admin_update ) {
            data->SetValue( s_sdf, SDF_UPDATE, admin_update );
          }
        }
        if ( ( cst == cstAdmin || cst == cstAll ) && data->IsPageChanged( s_sdf ) ) {
          std::string value = data->GetValue( s_sdf, SDF_PATH );
          TSDFVariables::list list;
          TSDFVariables::Explode( list, UpperCase( c_str( value ) ).c_str(), s_path_divider );
          TSDFVariables::list::iterator i = list.begin();
          bool bFound = false;
          while ( i != list.end() ) {
            if ( *i == admin_update_uc ) {
              bFound = true;
              break;
            }
            i++;
          }
          if ( !bFound ) {
            if ( !value.empty() ) {
              value += s_path_divider;
            }
            value += admin_update;
            data->SetValue( s_sdf, SDF_PATH, value );
          }
        }
        AnsiString section_name = c_str( data->GetName() );
        TConfigPage * main_page = m_meta_tree->GetPage( s_main_tab );
        TConfigPage * tcaanet_page = m_meta_tree->GetPage( s_tcaanet );
        bool bCanBeUsedImplicitly = false;
        bCanBeUsedImplicitly;
        if ( data->GetConfigType() == cstTCAANet ) {
          //if ( data->GetValue( s_tcaanet, s_tag ).empty() ) {
            CopyFilter( data, main_page, tcaanet_page );
          //}
          bCanBeUsedImplicitly = false;
  //        TIniIterator::IterateFilter( tcaanet_page, data, &*ini_tcaanet, section_name, "", TIniIterator::imWrite );
        } else {
          bCanBeUsedImplicitly = TIniIterator::IterateFilter( main_page, data, &*ini, section_name, s_if, TIniIterator::imWrite );
        }
        if ( data->GetConfigType() == cstSDF ) {
          bCanBeUsedImplicitly = true;
        }
        if ( !bCanBeUsedImplicitly ) {
          ini->WriteString( section_name.c_str(), ( AnsiString( s_if ) + s_1 ).c_str(), "0" );
        }
        ini->WriteString( section_name.c_str(), s_autoedit, "1" );
        if ( cst == cstAdmin ) {
          ini->WriteInteger( section_name.c_str(), s_admin, 1 );
          cst = cstAll;
        }
        ini->WriteInteger( section_name.c_str(), s_config_type, cst );
        if ( data->IsChanged() ) {
          meta = c_str( m_meta_tree->GetName() );
        }
        if ( data->GetConfigType() == cstTCAANet ) {
          std::string comment = data->GetValue( s_comment_page, s_sdf_comment );
          ini->WriteString( section_name.c_str(), s_sdf_comment, comment.c_str() );
        }
        if ( !meta.IsEmpty() ) {
          ini->WriteString( section_name.c_str(), s_edited_by_meta, meta.c_str() );
        }
      }
      i++;
    }
  }

  return bResult;
}

bool TIniWriter::WriteMisc( AnsiString & error ) {
  bool bResult = true;
  TReaderWriterList::const_iterator i = m_data->begin();
  while ( i != m_data->end() && bResult ) {
    bResult = WriteData( *i, error );
    i++;
  }
  return bResult;
}

void TIniWriter::CommitMisc( void ) {
  TReaderWriterList::const_iterator i = m_data->begin();
  while ( i != m_data->end()  ) {
    CommitData( *i );
    i++;
  }
}

bool TIniWriter::ProcessDeleted( AnsiString & error ) {
  bool bResult = true;

  TIniFile * sdf = m_iterator->GetIniFile( GetSdfIniFileName().c_str(), error );
  if ( sdf == NULL ) {
    bResult = false;
  } else {
    TReaderWriterList::iterator i = m_deleted_data->begin();
    while ( i != m_deleted_data->end() && bResult ) {
      sdf->EraseSection( c_str( ( *i )->GetOldName() ) );
      bResult = m_iterator->Iterate( *i, TIniIterator::imWrite, error );
      i++;
    }
  }
  return bResult;
}

void TIniWriter::CommitDeleted( void ) {
  TReaderWriterList::iterator i = m_deleted_data->begin();
  while ( i != m_deleted_data->end() ) {
    delete *i;
    i++;
  }
  m_deleted_data->clear();
}

bool TIniWriter::InvalidateReferences( AnsiString & /*error*/ ) {
  // Если путь к ini-файлу зависит от имени комплекта и/или фильтров, то при
  // их изменении соответствующую страницу необходимо перезаписать.
  int page_index = m_meta_tree->GetPageCount();
  while ( page_index > 0 ) {
    page_index--;
    TConfigPage * page = m_meta_tree->GetPage( page_index );
    std::string s = page->GetIniPath();
    if ( s.find( "/*" ) != std::string::npos ) {
      TReaderWriterList::const_iterator i = m_data->begin();
      while ( i != m_data->end()  ) {
        if ( ( *i )->IsPageChanged( s_main_tab ) ) {
          ( *i )->SetRenamed();
          if ( page->IsEnabledFor( *i ) ) {
            ( *i )->SetPageChanged( page->GetName() );
          }
        }
        i++;
      }
    }
  }
  return true;
}

bool TIniWriter::Write( void ) {
  bool bResult = true;
  bResult;
  AnsiString error_descr;
  if ( InvalidateReferences( error_descr ) ) {
    bResult = ProcessDeleted( error_descr );
    if ( bResult ) {
      bResult = WriteSDF( error_descr );
    }
    if ( bResult ) {
      bResult = WriteMisc( error_descr );
    }
    if ( bResult ) {
      bResult = m_iterator->Commit( error_descr );
    }
    if ( bResult ) {
      CommitDeleted();
      CommitMisc();
    } else {
      Application->MessageBox(
        ( TLanguageString( "Ошибка при записи изменений: ", "Can't save changes: " ) + error_descr ).c_str(),
        TLanguageString( "Ошибка", "Error" ),
        MB_OK | MB_ICONSTOP
      );
    }
  }
  return bResult;
}

// ***** reader ****************************************************************

TIniReader::TIniReader( const TConfigTree * meta_tree, TReaderWriterList * data )
  : m_meta_tree( meta_tree )
  , m_data( data )
  , m_iterator( new TIniIterator( meta_tree ) )
{
}

TIniReader::~TIniReader() {
  delete m_iterator;
}

bool TIniReader::Read( void ) {
  AnsiString error_descr;
  bool bResult = ReadSDF( error_descr );
  if ( bResult ) {
    bResult = ReadMisc( error_descr );
  }
  if ( !bResult ) {
    Application->MessageBox(
      ( TLanguageString( "Ошибка при чтении конфигурации: ", "Can't read config files: " ) + error_descr ).c_str(),
      TLanguageString( "Ошибка", "Error" ),
      MB_OK | MB_ICONSTOP
    );
  }
  UpdateFilter();
  return bResult;
}

bool TIniReader::ReadMisc( AnsiString & error ) {
  bool bResult = true;
  TReaderWriterList::iterator i = m_data->begin();
  while ( i != m_data->end() && bResult ) {
    bResult = ReadData( *i, error );
    i++;
  }
  return bResult;
}

bool TIniReader::ReadSDF( AnsiString & error ) {
  TIniFile * ini = m_iterator->GetIniFile( GetSdfIniFileName().c_str(), error );
  bool bResult = ( ini != NULL );
  TIniFile * ini_tcaanet = NULL;
  if ( bResult ) {
    ini_tcaanet = m_iterator->GetIniFile( GetTCAANetIniFileName().c_str(), error );
    bResult = ( ini_tcaanet != NULL );
  }
  if ( bResult ) {
    std::auto_ptr < TStringList > sl( new TStringList() );
    ini->ReadSections( &*sl );
    int i = 0;
    while ( i < sl->Count ) {
      AnsiString data_name = sl->Strings[ i ];
      int autoedit = ini->ReadInteger( data_name.c_str(), s_autoedit, -1 );
      if ( autoedit > 0 || autoedit == -1 ) {
        TConfigData * data = new TConfigData();
        data->FillDefaultValues( m_meta_tree );
        data->SetName( data_name.c_str() );
        data->SetNaturalOrder( i );
        ConfigSetType cst = cstAll;
        cst;
        if ( autoedit == -1 ) {
          // manually created sdf
          cst = cstSDF;
        } else {
          cst = ConfigSetType( ini->ReadInteger( data_name.c_str(), s_config_type, cstAll ) );
          if ( cst == cstAll && ini->ReadInteger( data_name.c_str(), s_admin, 0 ) == 1 ) {
            cst = cstAdmin;
          }
        }
        data->SetConfigType( cst );
        m_data->push_back( data );
        TConfigPage * main_page = m_meta_tree->GetPage( s_main_tab );
        TConfigPage * tcaanet_page = m_meta_tree->GetPage( s_tcaanet );
        if ( data->GetConfigType() == cstTCAANet ) {
          TIniIterator::IterateFilter( tcaanet_page, data, &*ini_tcaanet, data_name, "", TIniIterator::imRead );
          std::string comment = ini->ReadString( data_name.c_str(), s_sdf_comment, "" );
          data->SetValue( s_comment_page, s_sdf_comment, comment );
        } else {
          TIniIterator::IterateFilter( main_page, data, &*ini, data_name, s_if, TIniIterator::imRead );
        }
      }
      i++;
    }
  }
  return bResult;
}

bool TIniReader::ReadData( TConfigData * data, AnsiString & error ) {
  bool bResult = m_iterator->Iterate( data, TIniIterator::imRead, error );
  if ( bResult ) {
    data->SetOldName( data->GetName() );
    UpdateOldIni( m_meta_tree, data );
    data->SetChanged( false );
  }
  return bResult;
}

void TIniReader::UpdateFilter( void ) {
  TConfigPage * main_page = m_meta_tree->GetPage( s_main_tab );
  TConfigPage * tcaanet_page = m_meta_tree->GetPage( s_tcaanet );
  TReaderWriterList::iterator i = m_data->begin();
  while ( i != m_data->end() ) {
    if ( ( *i )->GetConfigType() == cstTCAANet /*&& ( *i )->GetValue( s_tcaanet, s_tag ).empty()*/ ) {
      CopyFilter( *i, tcaanet_page, main_page );
    }
    i++;
  }
}

