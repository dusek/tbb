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

/**
    This test ensures that tbb.h brings in all the public TBB interface definitions.

    The test is compile-time only. Nothing is actually executed except prinitng 
    the final "done" message.
**/

#include "tbb/tbb.h"

static volatile size_t g_sink;

#define TestTypeDefinitionPresence( Type) g_sink = sizeof(tbb::Type);
#define TestTypeDefinitionPresence2(TypeStart, TypeEnd) g_sink = sizeof(tbb::TypeStart,TypeEnd);
#define TestFuncDefinitionPresence(Fn, Args, ReturnType) { ReturnType (*pfn)Args = &tbb::Fn; (void)pfn; }

struct Body {
    void operator() () const {}
};
struct Body1 {
    void operator() ( int ) const {}
};
struct Body2 {
    Body2 () {}
    Body2 ( const Body2&, tbb::split ) {}
    void operator() ( const tbb::blocked_range<int>& ) const {}
    void join( const Body2& ) {}
};
struct Body3 {
    Body3 () {}
    Body3 ( const Body3&, tbb::split ) {}
    void operator() ( const tbb::blocked_range2d<int>&, tbb::pre_scan_tag ) const {}
    void operator() ( const tbb::blocked_range2d<int>&, tbb::final_scan_tag ) const {}
    void reverse_join( Body3& ) {}
    void assign( const Body3& ) {}
};

#if __TBB_TEST_SECONDARY
/* This mode is used to produce secondary object file that should be linked with
   main object file in order to detect "multiple definition" linker error.
*/
int secondary(int /*argc*/, char* /*argv*/[])
#else
#define HARNESS_NO_PARSE_COMMAND_LINE 1
#include "harness.h"
__TBB_TEST_EXPORT
int main(int /*argc*/, char* /*argv*/[])
#endif
{
    TestTypeDefinitionPresence2(aligned_space<int, 1> );
    TestTypeDefinitionPresence( atomic<int> );
    TestTypeDefinitionPresence( cache_aligned_allocator<int> );
    TestTypeDefinitionPresence( tbb_hash_compare<int> );
    TestTypeDefinitionPresence2(concurrent_hash_map<int, int> );
    TestTypeDefinitionPresence( concurrent_bounded_queue<int> );
    TestTypeDefinitionPresence( deprecated::concurrent_queue<int> );
    TestTypeDefinitionPresence( strict_ppl::concurrent_queue<int> );
    TestTypeDefinitionPresence( concurrent_vector<int> );
    TestTypeDefinitionPresence( enumerable_thread_specific<int> );
    TestTypeDefinitionPresence( mutex );
    TestTypeDefinitionPresence( null_mutex );
    TestTypeDefinitionPresence( null_rw_mutex );
    TestTypeDefinitionPresence( queuing_mutex );
    TestTypeDefinitionPresence( queuing_rw_mutex );
    TestTypeDefinitionPresence( recursive_mutex );
    TestTypeDefinitionPresence( spin_mutex );
    TestTypeDefinitionPresence( spin_rw_mutex );
    TestTypeDefinitionPresence( tbb_exception );
    TestTypeDefinitionPresence( captured_exception );
    TestTypeDefinitionPresence( movable_exception<int> );
#if !TBB_USE_CAPTURED_EXCEPTION
    TestTypeDefinitionPresence( tbb_exception_ptr );
#endif /* !TBB_USE_CAPTURED_EXCEPTION */
    TestTypeDefinitionPresence( blocked_range3d<int> );
    TestFuncDefinitionPresence( parallel_invoke, (Body&, Body&), void );
    TestFuncDefinitionPresence( parallel_do, (int*, int*, const Body1&), void );
    TestFuncDefinitionPresence( parallel_for_each, (int*, int*, Body1), Body1 );
    TestFuncDefinitionPresence( parallel_for, (const tbb::blocked_range<int>&, const Body2&, const tbb::simple_partitioner&), void );
    TestFuncDefinitionPresence( parallel_reduce, (const tbb::blocked_range<int>&, Body2&, tbb::affinity_partitioner&), void );
    TestFuncDefinitionPresence( parallel_scan, (const tbb::blocked_range2d<int>&, Body3&, const tbb::auto_partitioner&), void );
    TestFuncDefinitionPresence( parallel_sort, (int*, int*), void );
    TestTypeDefinitionPresence( pipeline );
    TestTypeDefinitionPresence( task );
    TestTypeDefinitionPresence( empty_task );
    TestTypeDefinitionPresence( task_list );
    TestTypeDefinitionPresence( task_group_context );
    TestTypeDefinitionPresence( task_group );
    TestTypeDefinitionPresence( task_handle<Body> );
    TestTypeDefinitionPresence( task_scheduler_init );
    TestTypeDefinitionPresence( task_scheduler_observer );
    TestTypeDefinitionPresence( tbb_thread );
    TestTypeDefinitionPresence( tbb_allocator<int> );
    TestTypeDefinitionPresence( zero_allocator<int> );
    TestTypeDefinitionPresence( tick_count );
#if !__TBB_TEST_SECONDARY
    REPORT("done\n");
#endif
    return 0;
}
