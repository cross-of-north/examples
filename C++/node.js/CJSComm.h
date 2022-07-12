#pragma once

#include "StdDef.h"
#include "CComm.h"
#include "CDevice.h"

#include "../common/transportJS.h"

using namespace TWD::common;
using namespace std;

class CJSCommAsyncResult;
class CJSCommFunctionCaller;
typedef shared_ptr < CJSCommAsyncResult > async_result_list_item_t;

// communication via JS callbacks
class CJSComm : public CComm
{ 
  friend class CDevice;
  friend class CJSCommFunctionCaller;

public:
  CJSComm(msg_m_t& param, CDevice* ptrDevice);
  
  virtual bool Open() override;
  virtual bool Close(void) override;
  virtual bool read(uint datasize = MAXLONG) override;
  virtual bool write(const uchar* data = NULL, uint datasize = MAXLONG, bool callCheckFormat = true) override;
  virtual bool CheckReady(int timeout=333) override; // проверить готовность асинхронных действий
  virtual bool changeFocus( bool focusIn = false ) override;

  static void OnJSMessageProcessed( const int jsc_handle, const bool bResult, const int raw_status, const int device_state, const int paper_state, const int atb_state, const string & data );

  virtual void PrepareToTerminate( void );

  const msg_m_t & GetParams( void ) const {
    return m_mParam;
  };

protected:
  virtual ~CJSComm (void);

  bool DoCommand( const shared_ptr < CJSCommFunctionCaller > & params, async_result_list_item_t & result, const int timeout_ms = 600000 );

  int m_jsc_handle;
  bool m_bTerminating;

private:

};