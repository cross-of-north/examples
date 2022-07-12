#ifndef TCAA_SDFTCAA_H
#define TCAA_SDFTCAA_H

#include "tcaahead.h"
#include "sdfvar.h"

// invariant constants
#define SDF_TCAA "SDF_TCAA"
#define SDF_MYDOCUMENTS "SDF_MYDOCUMENTS"
#define SDF_LANG "SDF_LANG"

// variant constants
#define SDF_TAP "SDF_TAP"
#define SDF_OPERNO "SDF_OPERNO"
#define SDF_APP "SDF_APP"
#define SDF_APPVER "SDF_APPVER"

// variables
#define SDF_PATH "SDF_PATH"
#define SDF_CONFIG "SDF_CONFIG"
#define SDF_HOME "SDF_HOME"
#define SDF_TEMP "SDF_TEMP"
#define SDF_UPDATE "SDF_UPDATE"
#define SDF_LOADER "SDF_LOADER"
#define SDF_CONFIG_TYPE "SDF_CONFIG_TYPE"
#define SDF_TCAANET "SDF_TCAANET"
#define SDF_LOCAL "SDF_LOCAL"

#define SDF_UNDEFINED "::UNDEFINED::"

extern bool __TCAA_DLL_CLASS ReadSDF(
  const ITCAAStartInit * si, std::string app, std::string version, std::string tap,
  std::string force_section, const bool bFinal
);

extern void __TCAA_DLL_CLASS GetSDFSections(
  const ITCAAStartInit * si, std::string app, std::string version, std::string tap,
  TSDFVariables::list * all_sections_list
);

extern void * __TCAA_DLL_CLASS SDFResetContext();
extern void __TCAA_DLL_CLASS SDFRestoreContext( void * context_to_restore );

class TSDFTemporaryContext {
  protected:
    void * m_old_context;
  public:
    TSDFTemporaryContext( void )
    : m_old_context( SDFResetContext() )
    {
    }
    virtual ~TSDFTemporaryContext() {
      SDFRestoreContext( m_old_context );
    }
};


void SDFFatalExit( const char * s );
TSDFVariables & _GetSDFVariables( void );

#endif // TCAA_SDFTCAA_H

