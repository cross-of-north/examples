#pragma once

namespace LogReader {

    class CThreadPool;

    /**
    * Windows Thread Pool API wrapper.
    * A worker thread.
    */
    class CThread {

        friend class CThreadPool;

    protected:

        TP_WORK * m_worker = NULL;
        volatile unsigned long long m_queued = 0; // debug
        volatile unsigned long long m_running = 0; // if non-zero then the thread worker is running
        CThreadPool * m_pool = NULL; // parent object
        volatile unsigned long long m_join_count = 0; // the count of threads waiting to join
        HANDLE m_start_event = INVALID_HANDLE_VALUE; // event to signal on start
        CRITICAL_SECTION m_cs; // lock

        // worker function
        virtual void OnRun( void ) = 0;

    public:

        CThread( void );
        virtual ~CThread();

        bool IsRunning( void ) const {
            return m_running != 0;
        }

        void Join( void );
        void ResetWorker( void ); // cleanup
        bool Run( const bool bWaitForStart ); // inititate run

        void Lock( void );
        void Unlock( void );

    };

    /**
    * Windows Thread Pool API wrapper.
    * A thread pool.
    */
    class CThreadPool {

    protected:

        PTP_POOL m_pool = NULL;
        TP_CALLBACK_ENVIRON m_callback_environment = {};
        PTP_CLEANUP_GROUP m_cleanup_group = NULL;

        static VOID CALLBACK ThreadFunction( PTP_CALLBACK_INSTANCE Instance, PVOID Context, PTP_WORK Work );

    public:

        CThreadPool( void );
        ~CThreadPool();

        // run the thread specified
        bool Run( CThread & thread, const bool bWaitForStart );

    };

} // namespace LogReader