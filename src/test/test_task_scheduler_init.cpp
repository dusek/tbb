/*
    Copyright 2005-2009 Intel Corporation.  All Rights Reserved.

    This file is part of Threading Building Blocks.

    Threading Building Blocks is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    Threading Building Blocks is distributed in the hope that it will be
    useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Threading Building Blocks; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
*/

#include "tbb/task_scheduler_init.h"
#include <cstdlib>
#include "harness_assert.h"

//! Test that task::initialize and task::terminate work when doing nothing else.
/** maxthread is treated as the "maximum" number of worker threads. */
void InitializeAndTerminate( int maxthread ) {
    for( int i=0; i<200; ++i ) {
        switch( i&3 ) {
            default: {
                tbb::task_scheduler_init init( std::rand() % maxthread + 1 );
                ASSERT(init.is_active(), NULL);
                break;
            }
            case 0: {   
                tbb::task_scheduler_init init;
                ASSERT(init.is_active(), NULL);
                break;
            }
            case 1: {
                tbb::task_scheduler_init init( tbb::task_scheduler_init::automatic );
                ASSERT(init.is_active(), NULL);
                break;
            }
            case 2: {
                tbb::task_scheduler_init init( tbb::task_scheduler_init::deferred );
                ASSERT(!init.is_active(), "init should not be active; initialization was deferred");
                init.initialize( std::rand() % maxthread + 1 );
                ASSERT(init.is_active(), NULL);
                init.terminate();
                ASSERT(!init.is_active(), "init should not be active; it was terminated");
                break;
            }
        }
    }
}

#include <cstdio>
#include <stdexcept>
#include "harness.h"

#if _WIN64
namespace std {      // 64-bit Windows compilers have not caught up with 1998 ISO C++ standard
    using ::srand;
}
#endif /* _WIN64 */

struct ThreadedInit {
    void operator()( int ) const {
        try {
            InitializeAndTerminate(MaxThread);
        } catch( std::runtime_error& error ) {
            REPORT("ERROR: %s\n", error.what() );
        }
    }
};

#if _MSC_VER
#include <windows.h>
#include <tchar.h>
#endif /* _MSC_VER */

#include "harness_concurrency_tracker.h"
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"

typedef tbb::blocked_range<int> Range;

class ConcurrencyTrackingBody {
public:
    void operator() ( const Range& ) const {
        Harness::ConcurrencyTracker ct;
        for ( volatile int i = 0; i < 1000000; ++i );
    }
};

/** The test will fail in particular if task_scheduler_init mistakenly hooks up 
    auto-initialization mechanism. **/
void AssertExplicitInitIsNotSupplanted () {
    int hardwareConcurrency = tbb::task_scheduler_init::default_num_threads();
    tbb::task_scheduler_init init(1);
    Harness::ConcurrencyTracker::Reset();
    tbb::parallel_for( Range(0, hardwareConcurrency * 2, 1), ConcurrencyTrackingBody(), tbb::simple_partitioner() );
    ASSERT( Harness::ConcurrencyTracker::PeakParallelism() == 1, 
            "Manual init provided more threads than requested. See also the comment at the beginning of main()." );
}

__TBB_TEST_EXPORT
int main(int argc, char* argv[]) {
    // Do not use tbb::task_scheduler_init directly in the scope of main's body,
    // as a static variable, or as a member of a static variable.
#if _MSC_VER && !__TBB_NO_IMPLICIT_LINKAGE
    #ifdef _DEBUG
        ASSERT(!GetModuleHandle(_T("tbb.dll")) && GetModuleHandle(_T("tbb_debug.dll")),
            "debug application links with non-debug tbb library");
    #else
        ASSERT(!GetModuleHandle(_T("tbb_debug.dll")) && GetModuleHandle(_T("tbb.dll")),
            "non-debug application links with debug tbb library");
    #endif
#endif /* _MSC_VER && !__TBB_NO_IMPLICIT_LINKAGE */
    std::srand(2);
    // Set defaults
    MaxThread = MinThread = 2;
    ParseCommandLine( argc, argv );
    try {
        InitializeAndTerminate(MaxThread);
    } catch( std::runtime_error& error ) {
        REPORT("ERROR: %s\n", error.what() );
    }
    for( int p=MinThread; p<=MaxThread; ++p ) {
        REMARK("testing with %d threads\n", p );
        NativeParallelFor( p, ThreadedInit() );
    }
    AssertExplicitInitIsNotSupplanted();
    REPORT("done\n");
    return 0;
}
