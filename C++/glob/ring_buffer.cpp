#include "pch.h"

#include "ring_buffer.h"

#include "misc.h"

namespace LogReader {


	CRingBufferBlock::CRingBufferBlock( const size_t size, CRingBuffer & ring_buffer )
		: m_allocated_data( new ( std::nothrow ) char[ size ] )
		, m_data( m_allocated_data )
		, m_allocated_size( size )
		, m_ring_buffer( ring_buffer ) {
	}
	CRingBufferBlock::~CRingBufferBlock() {
		delete[] m_allocated_data;
	}

	void CRingBufferBlock::OnRun( void ) {
		m_ring_buffer.ProcessBlock( *this );
	}

	void CRingBufferBlock::SetDataSize( const size_t size ) {
		if ( m_bLocked ) {
			log( "Setting size of the locked block\n" );
		}
		m_data_size = size;
	}

	char * CRingBufferBlock::GetMutableData( void ) {
		if ( m_bLocked ) {
			log( "Getting mutable data for the locked block\n" );
		}
		if ( m_allocated_data != m_data || m_allocated_size == 0 ) {
			log( "Getting mutable data for the immutable block\n" );
		}
		return m_allocated_data;
	}

	CRingBuffer::CRingBuffer( const unsigned int block_count, size_t block_size )
		: m_block_count( block_count )
		, m_block_size( block_size ) {
	}

	CRingBuffer::~CRingBuffer() {
		Uninit();
	}

	CRingBufferBlock * CRingBuffer::GetNextBlock( CRingBufferBlock * block ) const {
		CRingBufferBlock * result = static_cast < CRingBufferBlock * > ( block->GetNext() );
		if ( result == NULL ) {
			result = static_cast < CRingBufferBlock * > ( GetFirst() );
		}
		return result;
	}

	CRingBufferBlock * CRingBuffer::GetPrevBlock( CRingBufferBlock * block ) const {
		CRingBufferBlock * result = static_cast < CRingBufferBlock * > ( block->GetPrev() );
		if ( result == NULL ) {
			result = static_cast < CRingBufferBlock * > ( GetLast() );
		}
		return result;
	}

	CRingBufferBlock * CRingBuffer::CreateBlock( void ) {
		return new ( std::nothrow ) CRingBufferBlock( m_block_size, *this );
	}

	bool CRingBuffer::Init( void ) {

		Clear();

		for ( unsigned int i = 0; i < m_block_count; i++ ) {
			CRingBufferBlock * block = CreateBlock();
			if ( block == NULL || block->GetData() == NULL ) {
				log( "Allocation failure\n" );
				delete block;
				return false;
			} else {
				block->m_id = i;
				Add( block );
			}
		}

		m_current_processed_block = NULL;
		m_bInitialRun = true;
		m_bShouldFlush = false;
		m_FullCycle = false;

		return true;
	}

	void CRingBuffer::WaitAll( void ) {
		if ( m_bShouldFlush ) {
			GetNextBlockToProcess();
			m_bShouldFlush = false;
		}
		CRingBufferBlock * block = GetFirstBlock();
		while ( block != NULL ) {
			if ( block->m_order > 0 ) {
				// join only initialized blocks
				block->Join();
			}
			block = static_cast < CRingBufferBlock * > ( block->GetNext() );
		}
	}

	void CRingBuffer::Uninit() {
		WaitAll();
		m_current_processed_block = NULL;
		Clear();
	}

	CRingBufferBlock * CRingBuffer::GetFirstBlock( void ) {
		return static_cast < CRingBufferBlock * > ( GetFirst() );
	}

	void CRingBuffer::ProcessBlock( CRingBufferBlock & block ) {
		CRingBufferBlock * prev = NULL;
		if ( block.GetPrev() == NULL && m_bInitialRun ) {
			// The firstmost block processing shouldn't wait previous block processing.
			m_bInitialRun = false;
		} else {
			prev = GetPrevBlock( &block );
			prev->Lock(); // lock to prevent race conditions
			if ( prev->m_order < block.m_order ) {
				// wait for the block with the lower m_order to finish processing
				prev->Join();
			}
			prev->Unlock();
		}
		// run block processing
		OnProcessBlock( block );
	}

	bool CRingBuffer::RunBlock( CRingBufferBlock & block ) {
		block.Lock(); // lock to prevent race conditions
		// debug
		//if ( block.m_join_count > 0 ) {
		//	block.m_order++; block.m_order--;
		//}
		block.m_order = ++m_order; // set the next block number
		block.Unlock();
		if ( !block.Run( false ) ) {
			log( "Can't run thread\n" );
			return false;
		}
		return true;
	}

	CRingBufferBlock * CRingBuffer::GetNextProcessedBlock( void ) {

		CRingBufferBlock * first_block = GetFirstBlock();
		if ( first_block == NULL || first_block->GetData() == NULL ) {
			log( "Stream block is not allocated\n" );
			return NULL;
		}

		if ( m_current_processed_block == NULL ) {

			// the first run
			// push all blocks into the processing queue

			CRingBufferBlock * block = first_block;
			while ( block != NULL ) {
				if ( !RunBlock( *block ) ) {
					return NULL;
				}
				block = static_cast < CRingBufferBlock * > ( block->GetNext() );
			}

			// wait for the first block to be processed
			m_current_processed_block = first_block;
			m_current_processed_block->Join();

		} else {

			// not a first run
			// wait for the next block to be ready

			m_current_processed_block->Join();
	
			// return the previous block into the processing queue 
			CRingBufferBlock * prev_block = GetPrevBlock( m_current_processed_block );
			prev_block->m_bLocked = false;
			if ( !RunBlock( *prev_block ) ) {
				// can't run thread
				return NULL;
			}

		}

		CRingBufferBlock * result = m_current_processed_block;
		m_current_processed_block = GetNextBlock( m_current_processed_block );

		result->m_bLocked = true;

		return result;
	}

	CRingBufferBlock * CRingBuffer::GetNextBlockToProcess( void ) {

		CRingBufferBlock * first_block = GetFirstBlock();
		if ( first_block == NULL || first_block->GetData() == NULL ) {
			log( "Stream block is not allocated\n" );
			return NULL;
		}

		if ( m_current_processed_block == NULL ) {

			// the first run

			m_current_processed_block = first_block;

		} else {

			// not a first run
			// start processing of the previous block

			if ( !StartProcessingCurrentBlock() ) {
				return NULL;
			}

			if ( m_current_processed_block->GetNext() == NULL ) {
				m_FullCycle = true;
			}
			
			m_current_processed_block = GetNextBlock( m_current_processed_block );
			if ( m_FullCycle ) {
				// when blocks are gone full cycle it is expected for the next free block to be done with processing first
				m_current_processed_block->Join();
			}

		}

		m_bShouldFlush = true; // should run the returned block when needed

		m_current_processed_block->m_bLocked = false;
		m_bCurrentBlockIsRunning = false;

		return m_current_processed_block;
	}

	bool CRingBuffer::StartProcessingCurrentBlock( void ) {
		if ( !m_bCurrentBlockIsRunning ) {
			m_bCurrentBlockIsRunning = true;
			m_current_processed_block->m_bLocked = true;
			if ( !RunBlock( *m_current_processed_block ) ) {
				// can't run thread
				return false;
			}
		}
		return true;
	}

} // namespace LogReader