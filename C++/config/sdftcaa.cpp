#include "..\common.h"
#pragma hdrstop

#include "sdftcaa.h"

#include "sdf.h"
#include "sdfiniter.h"
#include "debtrap.h"
#include "tcaareg.h"
//#include "SdfTcaaVar.h"
#include "ifilemgr.h"
#include "tcaacfg.h"
#include "CommandLineParam.h"
#include "GetOSVar.h"
#include <fstream>
#include <set>

class TSDFTCAAVariables : public ISDFVariables {

  protected:

    TSDFVariables & m_var;

  public:

    TSDFTCAAVariables( TSDFVariables & var );
    virtual ~TSDFTCAAVariables();

    virtual std::string Get( const char * name, bool * found ) const;
    virtual void Set( const char * name, const char * value );
    virtual int GetCount( void ) const;
    virtual void Get( const int index, std::string & name, std::string & value ) const;
    virtual void GetList( const char * name, std::vector < std::string > & result ) const;

    TSDFVariables & GetVar( void ) { return m_var; }

};

TSDFTCAAVariables::TSDFTCAAVariables( TSDFVariables & var )
  : m_var( var )
{
}

TSDFTCAAVariables::~TSDFTCAAVariables() {
}

std::string TSDFTCAAVariables::Get( const char * name, bool * found ) const {
  return m_var.Get( name, found );
}

void TSDFTCAAVariables::Set( const char * name, const char * value ) {
  m_var.Set( name, value );
}

int TSDFTCAAVariables::GetCount( void ) const {
  return m_var.GetCount();
}

void TSDFTCAAVariables::Get( const int index, std::string & name, std::string & value ) const {
  m_var.Get( index, name, value );
}

void TSDFTCAAVariables::GetList( const char * name, std::vector < std::string > & result ) const {
  m_var.GetList( result, name, ";" );
}

// SDF-константы ******************************************************************************

class TConstants {
  protected:
    typedef std::set < std::string > set_type;
    set_type m_invariant;
    set_type m_variant;
  public:
    TConstants( void );
    ~TConstants();
    bool Contains( const std::string & s );
    bool IsVariant( const std::string & s );
};
TConstants::TConstants( void ) {
  m_invariant.insert( SDF_TCAA );
  m_invariant.insert( SDF_MYDOCUMENTS );
  m_variant.insert( SDF_TAP );
  m_variant.insert( SDF_OPERNO );
  m_variant.insert( SDF_APP );
  m_variant.insert( SDF_APPVER );
  m_variant.insert( SDF_SECTION );
}
TConstants::~TConstants() {
}
bool TConstants::Contains( const std::string & s ) {
  return ( m_invariant.find( s ) != m_invariant.end() || IsVariant( s ) );
}
bool TConstants::IsVariant( const std::string & s ) {
  return ( m_variant.find( s ) != m_variant.end() );
}
static TConstants g_constants;


// статические переменные ******************************************************

TCAAConfigPtr config;
TCAAUpdateConfigPtr uconfig;

class TSDFTCAAContext {
  public:
    TSDFVariables sdf;
    TSDFTCAAVariables isdf;

    TCAACFG::TCAAConfig * m_config;
    TCAACFG::TCAAUpdateConfig * m_uconfig;

    TSDFTCAAContext( void )
    : isdf( sdf )
    , m_config( NULL )
    , m_uconfig( NULL )
    {
    };
    virtual ~TSDFTCAAContext() {};
    void Store( void ) {
      m_config = config.get();
      m_uconfig = uconfig.get();
    }
    void Activate( bool bDestroyCurrentConfig ) {
      if ( !bDestroyCurrentConfig ) {
        config.release();
      }
      config.reset( m_config );
      if ( !bDestroyCurrentConfig ) {
        uconfig.release();
      }
      uconfig.reset( m_uconfig );
    }
};

static std::auto_ptr < TSDFTCAAContext > context( new TSDFTCAAContext );

void TConfigReader( void ) {
  ReadSDF( NULL, "", "", "", "", true );
}

#pragma startup TConfigReader 99
#pragma package(smart_init) // sic!

extern ISDFVariables & __TCAA_DLL_CLASS GetSDFVariables( void ) {
  return context->isdf;
}

TSDFVariables & _GetSDFVariables( void ) {
  return context->sdf;
}

// TSDFProcessor с TCAA-ограничениями ******************************************************************************

class TTCAASDFProcessor : public TSDFProcessor {

  private:

    typedef TSDFProcessor Inherited;

  public:

    enum Pass {
      pFirst,
      pSecond
    };

  protected:

    Pass m_pass;

    virtual void OnValue( const std::string & s );
    virtual std::string GetValue( const std::string name );

  public:

    TTCAASDFProcessor( const Pass pass, std::string force_section, TSDFVariables::list * all_sections_list );
    virtual ~TTCAASDFProcessor();

};

TTCAASDFProcessor::TTCAASDFProcessor( const Pass pass, std::string force_section,
  TSDFVariables::list * all_sections_list )
  : TSDFProcessor( force_section, all_sections_list )
  , m_pass( pass )
{
}

TTCAASDFProcessor::~TTCAASDFProcessor() {
}

void TTCAASDFProcessor::OnValue( const std::string & s ) {
  if ( g_constants.Contains( m_name ) ) {
    Error( "can't override constant " + m_name + " value" );
  }
  Inherited::OnValue( s );
}

std::string TTCAASDFProcessor::GetValue( const std::string name ) {
  if ( name == SDF_PATH ) {
    Error( "can't use SDF_PATH for substitution" );
  }
  std::string s;
  if ( m_pass == pFirst && g_constants.IsVariant( name ) ) {
    //Error( "value of " + name + " is undefined" );
    s = SDF_UNDEFINED;
  } else {
    s = Inherited::GetValue( name );
  }
  return s;
}

// Загрузка SDF-файла ******************************************************************************

class TSDFFile {

  protected:

    TSDFVariables m_variables;
    bool m_bInitialized;

    std::string DoGetVariable( const std::string name, bool * bIsDefined );
    void ExpandPath( std::string & s );
    std::string GetMyDocuments( void );
    void CorrectPaths( void );

  public:

    TSDFFile( void );
    ~TSDFFile();

    bool Init( const ITCAAStartInit * si, std::string app, std::string version,
      std::string tap, std::string force_section, TSDFVariables::list * all_sections_list );
    std::string GetVariable( const std::string name, bool * bIsDefined = NULL );
    typedef std::vector < std::string > TList;
    TList GetList( const std::string name, bool * bIsDefined = NULL );

};

TSDFFile::TSDFFile( void )
  : m_bInitialized( false )
{
}

TSDFFile::~TSDFFile() {
}

bool TSDFFile::Init( const ITCAAStartInit * si, std::string app, std::string version,
  std::string tap, std::string force_section, TSDFVariables::list * all_sections_list ) {

  m_bInitialized = true;

  TSDFVariablesTCAAIniter::Init( m_variables, si, app, version, tap );

  std::string filename;
  bool bHaveSDFOverride = false;
  std::string sdf_override = GetCommandLineParam( "-tcaadir", &bHaveSDFOverride );
  if ( bHaveSDFOverride ) {
    sdf_override = CorrectLastSlash( sdf_override );
    filename = sdf_override;
  } else {
    filename = GetTCAADir();
  }
  filename += "sdf.ini";
  std::ifstream is( filename.c_str(), std::ios_base::binary | std::ios_base::in );
  TTCAASDFProcessor sdfp( ( si == NULL ? TTCAASDFProcessor::pFirst : TTCAASDFProcessor::pSecond ), force_section, all_sections_list );
  m_variables = sdfp.Process( m_variables, is );
  m_variables.Set( "SDF_FILENAME", filename.c_str() );
  if ( bHaveSDFOverride ) {
    m_variables.Set( "SDF_OVERRIDE", sdf_override.c_str() );
    m_variables.Set( SDF_TCAANET, sdf_override.c_str() );
  }
  CorrectPaths();
  if ( all_sections_list == NULL ) {
    _GetSDFVariables() = m_variables;
  }
  return !sdfp.IsError();
}

std::string TSDFFile::DoGetVariable( const std::string name, bool * bIsDefined ) {
  std::string result;
  if ( m_bInitialized ) {
    result = m_variables.Get( name, bIsDefined );
  } else {
    DebugTrap( "not initialized" );
  }
  return result;
}

void TSDFFile::ExpandPath( std::string & s ) {
  if ( s.c_str() != NULL && s[ 0 ] == '.' ) {
    s = GetTCAADir() + s;
  }
}

std::string TSDFFile::GetVariable( const std::string name, bool * bIsDefined ) {
  std::string result = DoGetVariable( name, bIsDefined );
  ExpandPath( result );
  return result;
}

void TSDFFile::CorrectPaths( void ) {
  TSDFVariables::list paths;
  m_variables.GetList( paths, SDF_PATH, ";" );
  std::string new_path;
  std::string * i = paths.begin();
  while ( i != paths.end() ) {
    if ( ( *i ).c_str() != NULL ) {
      std::string filepath( *i );
      ExpandPath( filepath );
      filepath = CorrectLastSlash( filepath );
      if ( i != paths.begin() ) {
        new_path += ";";
      }
      new_path += filepath;
    }
    i++;
  }
  m_variables.Set( SDF_PATH, new_path );
}

/*
TSDFFile::TList TSDFFile::GetList( const std::string name, bool * pDefined_ ) {
  bool bDefined = false;
  bool * pDefined = ( pDefined_ == NULL ? &bDefined : pDefined_ );
  std::string value = DoGetVariable( name, pDefined );
  TList list;
  if ( *pDefined ) {
    list = m_variables.Explode( value, ";" );
    std::string * i = list.begin();
    while ( i != list.end() ) {
      ExpandPath( *i );
      i++;
    }
  }
  return list;
}
*/

void SDFFatalExit( const char * s ) {
  DebugTrap( s );
  Application->MessageBox( s, "Error", MB_OK | MB_ICONSTOP );
  ::TerminateProcess( ::GetCurrentProcess(), -1 );
}

extern bool __TCAA_DLL_CLASS ReadSDF( const ITCAAStartInit * si, std::string app, std::string version, std::string tap, std::string force_section, const bool bFinal ) {
  if ( &*config == NULL ) {
    //config.reset( new TCAACFG::TCAAConfig() );
  }
  bool bSuccess = TSDFFile().Init( si, app, version, tap, force_section, NULL );
  if ( bSuccess ) {
    config.reset( new TCAACFG::TCAAConfig() );
    if ( bFinal ) {
      uconfig.reset( new TCAACFG::TCAAUpdateConfig() );
      _GetSDFVariables().Set( SDF_LANG, config->lang );
    }
  } else {
    SDFFatalExit( "Config file (SDF) error" );
  }
  return bSuccess;
}

extern void __TCAA_DLL_CLASS GetSDFSections( const ITCAAStartInit * si,
  std::string app, std::string version, std::string tap, TSDFVariables::list * all_sections_list ) {
  DebugTrapAssert( all_sections_list != NULL, "incorrect parameter" );
  TSDFFile().Init( si, app, version, tap, "", all_sections_list );
}

extern void * __TCAA_DLL_CLASS SDFResetContext() {
  context->Store();
  TSDFTCAAContext * old_context = context.get();
  context.release();
  context.reset( new TSDFTCAAContext() );
  context->Activate( false );
  return ( void * )old_context;
}

extern void __TCAA_DLL_CLASS SDFRestoreContext( void * context_to_restore ) {
  context.reset( reinterpret_cast < TSDFTCAAContext * > ( context_to_restore ) );
  context->Activate( true );
}
