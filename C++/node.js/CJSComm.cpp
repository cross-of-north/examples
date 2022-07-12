#include "pch.h"
#include "CJSComm.h"

#include "CDeviceImpl.h"
#include "../common/service.h"

//---------------------------------------------------------------------------

#define SYNC_CALL -1

class CJSCommAsyncResult : private boost::noncopyable {
public:
	CJSCommAsyncResult( const string & _command_name )
		: semaphore( 0 )
		, bResult( false )
		, raw_status( -1 )
		, device_state( -1 )
		, paper_state( -1 )
		, atb_state( -1 )
		, command_name( _command_name )
	{
	}
	binary_semaphore semaphore;
	bool bResult;
	string data;
	int raw_status;
	int device_state;
	int paper_state;
	int atb_state;
	string command_name;
};

typedef map < int, async_result_list_item_t > async_result_list_t;

static mutex m;
static async_result_list_t async_result_list;

static int jsc_handle_counter = 0;

class CJSCommFunctionCaller : public JSCallWithParameters {
protected:
	friend class CJSComm;
	const CJSComm & m_comm;
	const string m_command_name;
	Local < Object > m_params;
public:
	explicit CJSCommFunctionCaller( const CJSComm & comm, const string & command_name )
		: JSCallWithParameters( comm.pDevice->getParams().callback )
		, m_comm( comm )
		, m_command_name( command_name )
	{
	}

	virtual bool PrepareParameters( Isolate * isolate, Local<Context> & context ) {

		js_new_object( objMessage );
		js_new_object( objStatus );
		m_params = objStatus;

		js_set_object_prop_string( objMessage, "devname", m_comm.pDevice->GetName() );
		js_set_object_prop_string( objMessage, "type", "jscomm_message" );
		js_set_object_prop_integer( objStatus, "handle", m_comm.m_jsc_handle );
		js_set_object_prop_string( objStatus, "command", m_command_name );

		js_set_object_prop( objMessage, "data", objStatus );

		SetParameter( 0, objMessage );
		return true;
	}
};

class CJSCommCreateCaller : public CJSCommFunctionCaller {
public:
	explicit CJSCommCreateCaller( const CJSComm & comm, const string & command_name )
		: CJSCommFunctionCaller( comm, command_name )
	{
	}

	virtual bool PrepareParameters( Isolate * isolate, Local<Context> & context ) {
		CJSCommFunctionCaller::PrepareParameters( isolate, context );
		const msg_m_t & src( m_comm.GetParams() );
		js_new_object( dst );
		for ( auto it = src.begin(); it != src.end(); it++ ) {
			js_set_object_prop_string( dst, it->first, it->second );
		}
		js_set_object_prop( m_params, "params", dst );
		return true;
	}
};

class CJSCommCheckReadyCaller : public CJSCommFunctionCaller {
protected:
	int m_timeout;
public:
	explicit CJSCommCheckReadyCaller( const CJSComm & comm, const string & command_name, const int timeout )
		: CJSCommFunctionCaller( comm, command_name )
		, m_timeout( timeout )
	{
	}

	virtual bool PrepareParameters( Isolate * isolate, Local<Context> & context ) {
		CJSCommFunctionCaller::PrepareParameters( isolate, context );
		js_set_object_prop_integer( m_params, "timeout", m_timeout );
		time_t t = 0;
		t = time( &t );
		string st = num_to_hexString( t );
		js_set_object_prop_string( m_params, "time_t", st );
		return true;
	}
};

class CJSCommWriteCaller : public CJSCommFunctionCaller {
protected:
	const uchar * m_data;
	uint m_datasize;
	bool m_callCheckFormat;
public:
	explicit CJSCommWriteCaller( const CJSComm & comm, const string & command_name, const uchar * data, uint datasize, bool callCheckFormat )
		: CJSCommFunctionCaller( comm, command_name )
		, m_data( data )
		, m_datasize( datasize )
	  , m_callCheckFormat( callCheckFormat )
	{
	}

	virtual bool PrepareParameters( Isolate * isolate, Local<Context> & context ) {
		CJSCommFunctionCaller::PrepareParameters( isolate, context );
		js_set_object_prop_array_buffer( m_params, "data", m_data, m_datasize );
		js_set_object_prop_boolean( m_params, "check_format", m_callCheckFormat );
		return true;
	}
};

class CJSCommReadCaller : public CJSCommFunctionCaller {
protected:
	uint m_datasize;
public:
	explicit CJSCommReadCaller( const CJSComm & comm, const string & command_name, const uint datasize )
		: CJSCommFunctionCaller( comm, command_name )
		, m_datasize( datasize ) {
	}

	virtual bool PrepareParameters( Isolate * isolate, Local<Context> & context ) {
		CJSCommFunctionCaller::PrepareParameters( isolate, context );
		js_set_object_prop_integer( m_params, "data_size", m_datasize );
		return true;
	}
};

class CJSCommChangeFocusCaller : public CJSCommFunctionCaller {
protected:
	bool m_bFocusIn;
public:
	explicit CJSCommChangeFocusCaller( const CJSComm & comm, const string & command_name, const bool bFocusIn )
		: CJSCommFunctionCaller( comm, command_name )
		, m_bFocusIn( bFocusIn ) {
	}

	virtual bool PrepareParameters( Isolate * isolate, Local<Context> & context ) {
		CJSCommFunctionCaller::PrepareParameters( isolate, context );
		js_set_object_prop_boolean( m_params, "focus_in", m_bFocusIn );
		return true;
	}
};

CJSComm::CJSComm(msg_m_t &param, CDevice* ptrDevice)
	: CComm(param, ptrDevice)
	, m_jsc_handle( ++jsc_handle_counter )
	, m_bTerminating( false )
{
	async_result_list_item_t async_result;
	DoCommand( make_shared < CJSCommCreateCaller >( *this, "create" ), async_result, SYNC_CALL );
	_log(m_szDevName, "CJSComm()", LogLevel_Trash);
}

CJSComm::~CJSComm(void) {
	_log(m_szDevName, "~CJSComm()", LogLevel_Trash);
	Close();
	async_result_list_item_t async_result;
	DoCommand( make_shared < CJSCommFunctionCaller >( *this, "destroy" ), async_result, SYNC_CALL );
}

void CJSComm::OnJSMessageProcessed( const int jsc_handle, const bool bResult, const int raw_status, const int device_state, const int paper_state, const int atb_state, const string & data ) {
	scoped_lock lock( m );
	auto async_result = async_result_list.find( jsc_handle );
	if ( async_result == async_result_list.end() ) {
		_log( "jscomm", "orphan command returned", LogLevel_Warning );
	} else {
		async_result->second->bResult = bResult;
		async_result->second->data = data;
		async_result->second->raw_status = raw_status;
		async_result->second->device_state = device_state;
		async_result->second->paper_state = paper_state;
		async_result->second->atb_state = atb_state;
		async_result->second->semaphore.release();
	}
}

void CJSComm::PrepareToTerminate( void ) {
	scoped_lock lock( m );
	m_bTerminating = true;
	// break all pending commands
	for ( auto existing_async_result = async_result_list.begin(); existing_async_result != async_result_list.end(); existing_async_result++ ) {
		if ( m_jsc_handle == existing_async_result->first ) {
			existing_async_result->second->semaphore.release();
		}
	}
}

bool CJSComm::Open() {
	async_result_list_item_t async_result;
	return DoCommand( make_shared < CJSCommFunctionCaller >( *this, "open" ), async_result );
}

bool CJSComm::Close() {
	PrepareToTerminate();
	async_result_list_item_t async_result;
	return DoCommand( make_shared < CJSCommFunctionCaller >( *this, "close" ), async_result );
}

bool CJSComm::CheckReady( int timeout ) {
	async_result_list_item_t async_result;
	bool bResult = DoCommand( make_shared < CJSCommCheckReadyCaller >( *this, "check_ready", timeout ), async_result, timeout );
	if ( bResult ) {
		read();
	}
	return bResult;
}

bool CJSComm::write( const uchar * data_, uint datasize, bool callCheckFormat ) {
	async_result_list_item_t async_result;
	uchar * data = const_cast < uchar * > ( data_ );
	shared_ptr < uchar > p_data;
	if ( callCheckFormat ) {
		data = new uchar[ datasize + 2 ];
		p_data.reset( data );
		CDirectComm::checkFormat( data, data_, datasize );
	}
	bool bResult = DoCommand( make_shared < CJSCommWriteCaller >( *this, "write", data, datasize, callCheckFormat ), async_result );
	if ( bResult ) {
		pDevice->newDataWritten( OK );
	} else {
		//pDevice->newDataRead( EDataState::ErrorWrite );
		pDevice->noData();
	}
	return bResult;
}

bool CJSComm::read( uint datasize ) {
	async_result_list_item_t async_result;
	bool bResult = DoCommand( make_shared < CJSCommReadCaller >( *this, "read", datasize ), async_result );
	if ( bResult ) {
		pDevice->newDataRead( OK, ( const uchar * )async_result->data.c_str(), async_result->data.length() );
	} else {
  	pDevice->noData();
	}
	return bResult;
}

bool CJSComm::changeFocus( bool focusIn ) {
	async_result_list_item_t async_result;
	return DoCommand( make_shared < CJSCommChangeFocusCaller >( *this, "change_focus", focusIn ), async_result );
}

bool CJSComm::DoCommand( const shared_ptr < CJSCommFunctionCaller > & params, async_result_list_item_t & result, const int timeout_ms ) {
	
	const string command( params->m_command_name );
	bool bResult = false;
	
	if ( !m_bTerminating || params->m_command_name == "close" ) {

		if ( pDevice == NULL ) {

			_log( m_szDevName, "JSComm::" + command + ": NULL device pointer", LogLevel_Warning );

		} else {

			async_result_list_item_t current_async_result;

			{
				scoped_lock lock( m );
				auto existing_async_result = async_result_list.find( m_jsc_handle );
				if ( existing_async_result == async_result_list.end() ) {
					current_async_result = make_shared < CJSCommAsyncResult >( command );
					async_result_list[ m_jsc_handle ] = current_async_result;
				} else {
					_log( m_szDevName, "JSComm::" + command + ": recursive command chain, '" + existing_async_result->second->command_name + "' is active", LogLevel_Warning );
				}
			}

			if ( current_async_result ) {
				time_t t = 0;
				t = time( &t );
				string st = num_to_hexString( t );
				_log( m_szDevName, "JSComm::" + command + " start", LogLevel_Trash );
				//_log( m_szDevName, "JSComm::" + command + " #" + st + " start", LogLevel_Warning );
				toJS( params );
				if ( timeout_ms != SYNC_CALL ) {
					try {
						string s_timeout = getStringKeyMapValue( m_mParam, "jscomm_timeout", string( "60" ) );
						auto ini_timeout = stoi( s_timeout );
						if ( current_async_result->semaphore.try_acquire_for( std::chrono::seconds( ini_timeout ) ) ) {
							bResult = current_async_result->bResult;
							//_log( m_szDevName, "JSComm::" + command + " #" + st + ": semaphore done", LogLevel_Warning );
						} else {
							_log( m_szDevName, "JSComm::" + command + " #" + st + ": semaphore timeout " + s_timeout + " sec", LogLevel_Warning );
						}
					} catch ( ... ) {
						_log( m_szDevName, "JSComm::" + command + " #" + st + ": caught semaphore exception", LogLevel_Warning );
					}
					//_log( m_szDevName, "JSComm::" + command + " #" + st + " end, " + ( bResult ? "OK" : "error" ), LogLevel_Trash );
				}

				{
					scoped_lock lock( m );
					async_result_list.erase( m_jsc_handle );
				}

				result = current_async_result;

			}
		}

		if ( result->raw_status != -1 ) {
			pDevice->newStatus( result->raw_status );
		}
		if ( result->device_state != -1 || result->paper_state != -1 ) {
			pDevice->checkStateMachine(
				( result->paper_state == -1 ) ? pDevice->m_ePaperState : ( CDevice::ePaperState )result->paper_state,
				( result->device_state == -1 ) ? pDevice->m_eDeviceState : ( CDevice::eDeviceState )result->device_state
			);
		}
		if ( result->atb_state != -1 ) {
			static_cast < CDeviceImpl * > ( pDevice )->newData( ( CDevice::EATBState )result->atb_state );
		}
	}

	return bResult;
}
