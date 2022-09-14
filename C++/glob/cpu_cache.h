#pragma once

#include "pch.h"

/**
* Code to get optimal size of the memory buffer (based on the L1 cache size)
* Copied from the API description:
* https://docs.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getlogicalprocessorinformation?redirectedfrom=MSDN
*/
typedef BOOL( WINAPI * LPFN_GLPI )( PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD );
unsigned int _cdecl get_CPU_L1_cache_size() {
    unsigned int cache_size = 0;
    LPFN_GLPI GetLogicalProcessorInformation;
    BOOL done = FALSE;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
    DWORD returnLength = 0;
    DWORD logicalProcessorCount = 0;
    DWORD numaNodeCount = 0;
    DWORD processorCoreCount = 0;
    DWORD processorL1CacheCount = 0;
    DWORD processorL2CacheCount = 0;
    DWORD processorL3CacheCount = 0;
    DWORD processorPackageCount = 0;
    DWORD byteOffset = 0;
    PCACHE_DESCRIPTOR Cache;
    HMODULE h = GetModuleHandle( TEXT( "kernel32" ) );
    if ( h == NULL ) {
        return 0;
    }
    GetLogicalProcessorInformation = ( LPFN_GLPI )GetProcAddress( h, "GetLogicalProcessorInformation" );
    if ( NULL == GetLogicalProcessorInformation ) {
        //_tprintf( TEXT( "\nGetLogicalProcessorInformation is not supported.\n" ) );
        return 0;
    }
    while ( !done ) {
        DWORD rc = GetLogicalProcessorInformation( buffer, &returnLength );
        if ( FALSE == rc ) {
            if ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) {
                if ( buffer ) {
                    free( buffer );
                }
                buffer = ( PSYSTEM_LOGICAL_PROCESSOR_INFORMATION )malloc( returnLength );
                if ( NULL == buffer ) {
                    //_tprintf( TEXT( "\nError: Allocation failure\n" ) );
                    return 0;
                }
            } else {
                //_tprintf( TEXT( "\nError %d\n" ), GetLastError() );
                return 0;
            }
        } else {
            done = TRUE;
        }
    }
    ptr = buffer;
    if ( ptr != NULL ) while ( byteOffset + sizeof( SYSTEM_LOGICAL_PROCESSOR_INFORMATION ) <= returnLength ) {
        switch ( ptr->Relationship ) {
            case RelationCache:
                // Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
                Cache = &ptr->Cache;
                if ( Cache->Level == 1 ) {
                    cache_size = Cache->Size;
                }
                break;
        }
        byteOffset += sizeof( SYSTEM_LOGICAL_PROCESSOR_INFORMATION );
        ptr++;
    }
    free( buffer );
    return cache_size;
}
