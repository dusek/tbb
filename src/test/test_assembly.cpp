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

// Program for basic correctness testing of assembly-language routines.
// This program deliberately #includes ../internal/task.cpp so that
// it can get intimate access to the scheduler.

#define TEST_ASSEMBLY_ROUTINES 1
#define __TBB_TASK_CPP_DIRECTLY_INCLUDED 1
// to avoid usage of #pragma comment
#define __TBB_NO_IMPLICIT_LINKAGE 1

#include "../tbb/task.cpp"
#include <new>
#include "harness.h"

namespace tbb {

namespace internal {

class TestTask: public task {
public:
    /*override*/ task* execute() {
        return NULL;
    }
    const char* name;
    TestTask( const char* name_ ) : name(name_) {}
};

void GenericScheduler::test_assembly_routines() {
    __TBB_ASSERT( assert_okay(), NULL );
    try_enter_arena();
    ASSERT( arena_slot->task_pool == dummy_slot.task_pool, "entering arena must not lock the task pool" );
    arena->mark_pool_full();
    acquire_task_pool();
    release_task_pool();
    acquire_task_pool();    // leave_arena requires the pool to be locked
    leave_arena();
}

//! Test __TBB_CompareAndSwapW
static void TestCompareExchange() {
    ASSERT( intptr(-10)<10, "intptr not a signed integral type?" ); 
    REMARK("testing __TBB_CompareAndSwapW\n");
    for( intptr a=-10; a<10; ++a )
        for( intptr b=-10; b<10; ++b )
            for( intptr c=-10; c<10; ++c ) {
// Workaround for a bug in GCC 4.3.0; and one more is below.
#if __GNUC__==4&&__GNUC_MINOR__==3&&__GNUC_PATCHLEVEL__==0
                intptr x;
                __TBB_store_with_release( x, a );
#else
                intptr x = a;
#endif
                intptr y = __TBB_CompareAndSwapW(&x,b,c);
                ASSERT( y==a, NULL ); 
                if( a==c ) 
                    ASSERT( x==b, NULL );
                else
                    ASSERT( x==a, NULL );
            }
}

//! Test __TBB___TBB_FetchAndIncrement and __TBB___TBB_FetchAndDecrement
static void TestAtomicCounter() {
    // "canary" is a value used to detect illegal overwrites.
    const internal::reference_count canary = ~(internal::uintptr)0/3;
    REMARK("testing __TBB_FetchAndIncrement\n");
    struct {
        internal::reference_count prefix, i, suffix;
    } x;
    x.prefix = canary;
    x.i = 0;
    x.suffix = canary;
    for( int k=0; k<10; ++k ) {
        internal::reference_count j = __TBB_FetchAndIncrementWacquire((volatile void *)&x.i);
        ASSERT( x.prefix==canary, NULL );
        ASSERT( x.suffix==canary, NULL );
        ASSERT( x.i==k+1, NULL );
        ASSERT( j==k, NULL );
    }
    REMARK("testing __TBB_FetchAndDecrement\n");
    x.i = 10;
    for( int k=10; k>0; --k ) {
        internal::reference_count j = __TBB_FetchAndDecrementWrelease((volatile void *)&x.i);
        ASSERT( j==k, NULL );
        ASSERT( x.i==k-1, NULL );
        ASSERT( x.prefix==canary, NULL );
        ASSERT( x.suffix==canary, NULL );
    }
}

static void TestTinyLock() {
    REMARK("testing __TBB_LockByte\n");
    unsigned char flags[16];
    for( int i=0; i<16; ++i )
        flags[i] = i;
#if __GNUC__==4&&__GNUC_MINOR__==3&&__GNUC_PATCHLEVEL__==0
    __TBB_store_with_release( flags[8], 0 );
#else
    flags[8] = 0;
#endif
    __TBB_LockByte(flags[8]);
    for( int i=0; i<16; ++i )
        ASSERT( flags[i]==(i==8?1:i), NULL );
}

static void TestLog2() {
    REMARK("testing __TBB_Log2\n");
    for( uintptr_t i=1; i; i<<=1 ) {
        for( uintptr_t j=1; j<1<<16; ++j ) {
            if( uintptr_t k = i*j ) {
                uintptr_t actual = __TBB_Log2(k);
                const uintptr_t ONE = 1; // warning suppression again
                ASSERT( k >= ONE<<actual, NULL );          
                ASSERT( k>>1 < ONE<<actual, NULL );        
            }
        }
    }
}

static void TestPause() {
    REMARK("testing __TBB_Pause\n");
    __TBB_Pause(1);
}


} // namespace internal 
} // namespace tbb

using namespace tbb;

__TBB_TEST_EXPORT
int main( int argc, char* argv[] ) {
    try {
        ParseCommandLine( argc, argv );
        TestLog2();
        TestTinyLock();
        TestCompareExchange();
        TestAtomicCounter();
        TestPause();

        task_scheduler_init init(1);

        REMARK("testing __TBB_(scheduler assists)\n");
        GenericScheduler* scheduler = internal::Governor::local_scheduler();
        scheduler->test_assembly_routines();

    } catch(...) {
        ASSERT(0,"unexpected exception");
    }
    REPORT("done\n");
    return 0;
}
