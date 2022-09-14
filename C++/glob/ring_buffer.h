#pragma once

#include "linked_list.h"

#include "thread_pool.h"

namespace LogReader {

	/**
	* The ring buffer item combines an allocated memory block wrapper and the worker thread mechanics to process this block.
	*/
	class CRingBufferBlock: public CLinkedListItem, public CThread {

		friend class CRingBuffer;

	protected:

		char * m_allocated_data = NULL; // the owned data buffer
		const char * m_data = NULL; // the data buffer
		size_t m_data_size = 0; // current buffer data size
		size_t m_allocated_size; // initial buffer allocation size
		CRingBuffer & m_ring_buffer; // parent structure
		bool m_bLocked = false; // sanity check guard, true when the block is expected to be immutable
		int m_id = 0; // id for debugging
		
		// The order of the block in the ring buffer processing queue.
		// The block with the lesser m_order value is guaranteed to be fully processed
		// before the block with the greater m_order value processing is started.
		unsigned long long int m_order = 0;

		// worker thread body callback
		virtual void OnRun( void );

	public:

		/**
		* size - size of buffer to allocate
		*/
		CRingBufferBlock( const size_t size, CRingBuffer & ring_buffer );
		virtual ~CRingBufferBlock();

		const char * GetData( void ) const {
			return m_data;
		}

		void OverrideDataPointer( const char * data ) {
			m_data = data;
		}

		char * GetMutableData( void );

		size_t GetDataSize( void ) const {
			return m_data_size;
		}

		void SetDataSize( const size_t size );

		size_t GetAllocatedSize( void ) const {
			return m_allocated_size;
		}

	};

	/**
	* The ring buffer contains a set of workers with data buffers.
	* Workers are cyclically queued (LIFO) to do their buffer processing.
	* It is guaranteed that there are no concurrently running workers.
	*/
	class CRingBuffer: protected CLinkedList {

		friend class CRingBufferBlock;

	protected:

		/**
		* The "current" block returned or waiting to be returned to the data users.
		* What "current" is - depends on the usage scenario (write data to the buffer to be processed or read generated data from the buffer).
		*/
		CRingBufferBlock * m_current_processed_block = NULL;

		bool m_bInitialRun = true; // true on the first run of block processors
		bool m_FullCycle = false; // true if block processors done at least one full cycle, and therefore all processors are fully initialized
		bool m_bShouldFlush = false; // true when the final buffer should be pushed to the processing queue on Uninit (write mode)
		unsigned int m_block_count; // count of blocks
		size_t m_block_size; // size of block data buffer
		unsigned long long int m_order = 0; // block m_order counter
		bool m_bCurrentBlockIsRunning = false; // if true then the m_current_processed_block is already pushed to the processing queue (write mode)

		/**
		* Called in the worker thread context.
		* Blocks until all blocks with the lower m_order are processed.
		* Then calls OnProcessBlock for the block specified.
		*/
		void ProcessBlock( CRingBufferBlock & block );

		CRingBufferBlock * GetNextBlock( CRingBufferBlock * block ) const;
		CRingBufferBlock * GetPrevBlock( CRingBufferBlock * block ) const;

		/**
		* The block factory.
		*/
		virtual CRingBufferBlock * CreateBlock( void );

		/**
		* Block processor function.
		* It is expected to be overrided in descendants.
		* Called in the worker thread context.
		*/ 
		virtual void OnProcessBlock( CRingBufferBlock & block ) {};

		CRingBufferBlock * GetFirstBlock( void );

		/**
		* Pushes the block specified to the processing queue.
		* Called in the main thread context.
		*/
		bool RunBlock( CRingBufferBlock & block );

	public:

		CRingBuffer( const unsigned int block_count, size_t block_size );
		virtual ~CRingBuffer();

		/**
		* Creates blocks.
		* Returns false on allocation errors.
		*/
		bool Init( void );

		/**
		* Pushes the last block into the processing queue if needed.
		* Blocks until all queued blocks are processed.
		* Deallocates blocks.
		*/
		void Uninit( void );

		/**
		* "Reader" function.
		* Returns the next processed block (the processed block with the minimum m_order).
		* If a block to be returned is not processed yet, blocks until the block is processed.
		* On first run pushes all blocks into the processing queue.
		*/
		CRingBufferBlock * GetNextProcessedBlock( void );
		
		/**
		* "Writer" function.
		* Returns the fist free block with uninitialized data to be filled by the caller.
		* The block is considered mutable and expected to be prepared until the next GetNextBlockToProcess() or StartProcessingCurrentBlock() call.
		* If there are no free blocks, blocks until the free block arrives from the processing.
		* Pushes the block returned by preceding GetNextBlockToProcess() call into the processing queue.
		*/
		CRingBufferBlock * GetNextBlockToProcess( void );

		/**
		* "Writer" function helper.
		* Pushes the block returned by the latest GetNextBlockToProcess() call into the processing queue.
		* Used when the caller doesn't need a next free block right now but the previous block is ready to be processed.
		* The caller can use GetNextBlockToProcess() later to get a free block.
		*/
		bool StartProcessingCurrentBlock( void );

		/**
		* Pushes the last block into the processing queue if needed.
		* Blocks until all queued blocks are processed.
		*/
		void WaitAll( void );

	};

} // namespace LogReader
