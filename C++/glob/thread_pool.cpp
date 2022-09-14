#include "pch.h"

#include "thread_pool.h"

#include "misc.h"

namespace LogReader {

    // Thread pool singleton with the lazy initialization form the CThread::Run().
    // Application can set some options before running the first thread.
    static CThreadPool * thread_pool = NULL;

    class CThreadPoolSingletonCleanup {
    public:
        CThreadPoolSingletonCleanup( void ) {
        }
        ~CThreadPoolSingletonCleanup() {
            delete thread_pool;
        }
    };
    CThreadPoolSingletonCleanup thread_pool_cleanup;

    CThread::CThread( void ) {
        ::InitializeCriticalSection( &m_cs );
    }

    CThread::~CThread() {
        ResetWorker();
    }

    void CThread::Lock( void ) {
        ::EnterCriticalSection( &m_cs );
    }

    void CThread::Unlock( void ) {
        ::LeaveCriticalSection( &m_cs );
    }

    void CThread::Join( void ) {
        if ( m_worker == NULL ) {
            log( "Failed to join: thread is not running\n" );
            return;
        }
        ::InterlockedIncrement( &m_join_count );
        ::WaitForThreadpoolWorkCallbacks( m_worker, FALSE );
        ::InterlockedDecrement( &m_join_count );
    }

    void CThread::ResetWorker( void ) {
        if ( m_worker != NULL ) {
            Join();
            ::CloseThreadpoolWork( m_worker );
        }
        m_worker = NULL;
    }

    bool CThread::Run( const bool bWaitForStart ) {
        if ( thread_pool == NULL ) {
            thread_pool = new ( std::nothrow ) CThreadPool();
            if ( thread_pool == NULL ) {
                log( "Allocation failure\n" );
                return false;
            }
        }
        return thread_pool->Run( *this, bWaitForStart );
    }

    CThreadPool::CThreadPool( void )
    {
    }
    
    CThreadPool::~CThreadPool() {
        ::CloseThreadpool( m_pool );
    }

    VOID CALLBACK CThreadPool::ThreadFunction( PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work ) {
        CThread * thread = reinterpret_cast < CThread * > ( Context );
        ::InterlockedDecrement( &thread->m_queued );
        if ( ::SetEvent( thread->m_start_event ) == 0 ) {
            log( "Failed to set thread start event\n" );
        }
        thread->OnRun();
        ::InterlockedDecrement( &thread->m_running );
    }

    bool CThreadPool::Run( CThread & thread, const bool bWaitForStart ) {

        thread.m_pool = this;
        
        if ( m_pool == NULL ) {

            m_pool = ::CreateThreadpool( NULL );
            if ( m_pool == NULL ) {
                log( "CreateThreadpool failed\n" );
                return false;
            }
            ::InitializeThreadpoolEnvironment( &m_callback_environment );
            m_cleanup_group = ::CreateThreadpoolCleanupGroup();
            if ( m_cleanup_group == NULL ) {
                log( "CreateThreadpoolCleanupGroup failed\n" );
                return false;
            }
            ::SetThreadpoolCallbackPool( &m_callback_environment, m_pool );
            ::SetThreadpoolCallbackCleanupGroup( &m_callback_environment, m_cleanup_group, NULL );
        }

        if ( thread.m_worker == NULL ) {
            thread.m_worker = ::CreateThreadpoolWork( CThreadPool::ThreadFunction, &thread, &m_callback_environment );
            if ( thread.m_worker == NULL ) {
                log( "Failed to register thread worker\n" );
                return false;
            }
        }

        if ( thread.m_start_event == INVALID_HANDLE_VALUE ) {
            thread.m_start_event = ::CreateEvent( NULL, TRUE, FALSE, NULL );
            if ( thread.m_start_event == NULL ) {
                log( "Failed to create thread start event\n" );
                return false;
            }
        }

        if ( bWaitForStart ) {
            if ( ::ResetEvent( thread.m_start_event ) == 0 ) {
                log( "Failed to reset thread start event\n" );
                return false;
            }
        }

        // additionally wait for all other joiners
        while ( thread.m_join_count > 0 ) {
            ::Sleep( 1 );
        }

        ::InterlockedIncrement( &thread.m_running );
        ::InterlockedIncrement( &thread.m_queued );
        ::SubmitThreadpoolWork( thread.m_worker );
        if ( bWaitForStart ) {
            if ( ::WaitForSingleObject( thread.m_start_event, INFINITE ) == WAIT_FAILED ) {
                log( "Failed to wait for thread start event\n" );
                return false;
            }
        }

        return true;
    }


} // namespace LogReader