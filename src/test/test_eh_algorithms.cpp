/*
    Copyright 2005-2008 Intel Corporation.  All Rights Reserved.

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

// to avoid usage of #pragma comment
#define __TBB_NO_IMPLICIT_LINKAGE 1

#define  COUNT_TASK_NODES 1
#define __TBB_TASK_CPP_DIRECTLY_INCLUDED 1
#include "../tbb/task.cpp"

#if __TBB_EXCEPTIONS

#include "tbb/task_scheduler_init.h"
#include "tbb/atomic.h"
#include "tbb/parallel_for.h"
#include "tbb/blocked_range.h"

#include <cmath>

#include "harness.h"
#include "harness_trace.h"


//------------------------------------------------------------------------
// Utility definitions
//------------------------------------------------------------------------


#define ITER_RANGE  100000
#define ITER_GRAIN  1000
#define NESTING_RANGE  100
#define NESTING_GRAIN  10
#define NESTED_RANGE  (ITER_RANGE / NESTING_RANGE)
#define NESTED_GRAIN  (ITER_GRAIN / NESTING_GRAIN)
#define EXCEPTION_DESCR "Test exception"

namespace internal = tbb::internal;
using internal::intptr;

namespace util {

void sleep ( int ms ) {
#if _WIN32 || _WIN64
    ::Sleep(ms);
#else
    timespec  requested = { ms / 1000, (ms % 1000)*1000000 };
    timespec  remaining = {0};
    nanosleep(&requested, &remaining);
#endif // _WIN32 || _WIN64
}

inline intptr num_subranges ( intptr length, intptr grain ) {
    return (intptr)pow(2, ceil(log((double)length / grain) / log(2.)));
}

} // namespace util

int g_max_concurrency = 0;
int g_num_threads = 0;

inline void yield_if_singlecore() { if ( g_max_concurrency == 1 ) __TBB_Yield(); }


class test_exception : public std::exception
{
    const char* my_description;
public:
    test_exception ( const char* description ) : my_description(description) {}

    const char* what() const throw() { return my_description; }
};

class solitary_test_exception : public test_exception
{
public:
    solitary_test_exception ( const char* description ) : test_exception(description) {}
};


tbb::atomic<intptr> g_cur_executed, // number of times a body was requested to process data
                    g_exc_executed, // snapshot of execution statistics at the moment of the 1st exception throwing
                    g_catch_executed, // snapshot of execution statistics at the moment when the 1st exception is caught
                    g_exceptions; // number of exceptions exposed to TBB users (i.e. intercepted by the test code)

internal::GenericScheduler  *g_master = NULL;

volatile intptr g_exception_thrown = 0;
volatile bool g_throw_exception = true;
volatile bool g_no_exception = true;
volatile bool g_unknown_exception = false;
volatile bool g_task_was_cancelled = false;

bool    g_exception_in_master = false;
bool    g_solitary_exception = true;
volatile bool   g_wait_completed = false;


void reset_globals () {
    g_cur_executed = g_exc_executed = g_catch_executed = 0;
    g_exceptions = 0;
    g_exception_thrown = 0;
    g_throw_exception = true;
    g_no_exception = true;
    g_unknown_exception = false;
    g_task_was_cancelled = false;
    g_wait_completed = false;
    //g_num_tasks_when_last_exception = 0;
}

intptr num_tasks () { return g_master->get_task_node_count(true); }

void throw_test_exception ( intptr throw_threshold ) {
    if ( !g_throw_exception  ||  g_exception_in_master ^ (internal::GetThreadSpecific() == g_master) )
        return; 
    if ( !g_solitary_exception ) {
        __TBB_CompareAndSwapW(&g_exc_executed, g_cur_executed, 0);
        TRACE ("About to throw one of multiple test_exceptions (thread %08x):", internal::GetThreadSpecific());
        throw (test_exception(EXCEPTION_DESCR));
    }
    while ( g_cur_executed < throw_threshold )
        yield_if_singlecore();
    if ( __TBB_CompareAndSwapW(&g_exception_thrown, 1, 0) == 0 ) {
        g_exc_executed = g_cur_executed;
        TRACE ("About to throw solitary test_exception... :");
        throw (solitary_test_exception(EXCEPTION_DESCR));
    }
}

#define TRY()   \
    bool no_exception = true, unknown_exception = false;    \
    try {

#define CATCH()     \
    } catch ( tbb::unhandled_exception& e ) {     \
        g_catch_executed = g_cur_executed;  \
        ASSERT (strcmp(e.name(), (g_solitary_exception ? typeid(solitary_test_exception) : typeid(test_exception)).name() ) == 0, "Unexpected original exception name");    \
        ASSERT (strcmp(e.what(), EXCEPTION_DESCR) == 0, "Unexpected original exception info");   \
        TRACE ("Executed at throw moment %d; upon catch %d", (intptr)g_exc_executed, (intptr)g_catch_executed);  \
        g_no_exception = no_exception = false;   \
        ++g_exceptions; \
    }   \
    catch ( ... ) { \
        g_no_exception = false;   \
        g_unknown_exception = unknown_exception = true;   \
    }

#define ASSERT_EXCEPTION()     \
    ASSERT (!g_no_exception, "no exception occurred");    \
    ASSERT (!g_unknown_exception, "unknown exception was caught");

#define CATCH_AND_ASSERT()     \
    CATCH() \
    ASSERT_EXCEPTION()

#define ASSERT_TEST_POSTCOND()
    //ASSERT (!num_tasks(), "Not all tasks objects have been destroyed")


//------------------------------------------------------------------------
// Tests
//------------------------------------------------------------------------

typedef size_t count_type;
typedef tbb::blocked_range<count_type> range_type;


template<class Body>
intptr test_num_subranges_calculation ( intptr length, intptr grain, intptr nested_length, intptr nested_grain ) {
    reset_globals();
    g_throw_exception = false;
    intptr  nesting_body_calls = util::num_subranges(length, grain),
            nested_body_calls = util::num_subranges(nested_length, nested_grain),
            calls_in_normal_case = nesting_body_calls * (nested_body_calls + 1);
    tbb::parallel_for( range_type(0, length, grain), Body() );
    ASSERT (g_cur_executed == calls_in_normal_case, "Wrong estimation of bodies invocation count");
    return calls_in_normal_case;
}

class no_throw_pfor_body {
public:
    void operator()( const range_type& r ) const {
        volatile long x;
        count_type end = r.end();
        for( count_type i=r.begin(); i<end; ++i )
            x = 0;
    }
};

void Test0 () {
    TRACEP ("");
    reset_globals();
    tbb::simple_partitioner p;
    for( size_t i=0; i<10; ++i ) {
        tbb::parallel_for( range_type(0, 0, 1), no_throw_pfor_body() );
        tbb::parallel_for( range_type(0, 0, 1), no_throw_pfor_body(), p );
        tbb::parallel_for( range_type(0, 128, 8), no_throw_pfor_body() );
        tbb::parallel_for( range_type(0, 128, 8), no_throw_pfor_body(), p );
    }
} // void Test0 ()


class simple_pfor_body {
public:
    void operator()( const range_type& r ) const {
        volatile long x;
        count_type end = r.end();
        for( count_type i=r.begin(); i<end; ++i )
            x = 0;
        ++g_cur_executed;
        if ( g_exception_in_master  ^  (internal::GetThreadSpecific() == g_master) )
        {
            // Make absolutely sure that worker threads on multicore machines had a chance to steal something
            util::sleep(10);
        }
        throw_test_exception(1);
    }
};

void Test1 () {
    TRACEP ("");
    reset_globals();
    TRY();
        tbb::parallel_for( range_type(0, ITER_RANGE, ITER_GRAIN), simple_pfor_body() ); // , tbb::simple_partitioner()
    CATCH_AND_ASSERT();
    ASSERT (g_cur_executed <= g_catch_executed + g_num_threads, "Too many tasks survived exception");
    TRACE ("Executed at the end of test %d; number of exceptions", (intptr)g_cur_executed);
    ASSERT (g_exceptions == 1, "No try_blocks in any body expected in this test");
    if ( !g_solitary_exception )
        ASSERT (g_cur_executed <= g_catch_executed + g_num_threads, "Too many tasks survived exception");
} // void Test1 ()


class nesting_pfor_body {
public:
    void operator()( const range_type& ) const {
        ++g_cur_executed;
        if ( internal::GetThreadSpecific() == g_master )
            yield_if_singlecore();
        tbb::parallel_for( tbb::blocked_range<size_t>(0, NESTED_RANGE, NESTED_GRAIN), simple_pfor_body() );
    }
};

//! Uses parallel_for body containing a nested parallel_for with the default context not wrapped by a try-block.
/** Nested algorithms are spawned inside the new bound context by default. Since 
    exceptions thrown from the nested parallel_for are not handled by the caller 
    (nesting parallel_for body) in this test, they will cancel all the sibling nested 
    algorithms. **/
void Test2 () {
    TRACEP ("");
    reset_globals();
    TRY();
        tbb::parallel_for( range_type(0, NESTING_RANGE, NESTING_GRAIN), nesting_pfor_body() );
    CATCH_AND_ASSERT();
    ASSERT (!no_exception, "No exception thrown from the nesting parallel_for");
    //if ( g_solitary_exception )
        ASSERT (g_cur_executed <= g_catch_executed + g_num_threads, "Too many tasks survived exception");
    TRACE ("Executed at the end of test %d", (intptr)g_cur_executed);
    ASSERT (g_exceptions == 1, "No try_blocks in any body expected in this test");
    if ( !g_solitary_exception )
        ASSERT (g_cur_executed <= g_catch_executed + g_num_threads, "Too many tasks survived exception");
} // void Test2 ()


class nesting_pfor_with_isolated_context_body {
public:
    void operator()( const range_type& ) const {
        tbb::asynch_context ctx(tbb::asynch_context::isolated);
        ++g_cur_executed;
        util::sleep(1); // Give other threads a chance to steal their first tasks
        tbb::parallel_for( tbb::blocked_range<size_t>(0, NESTED_RANGE, NESTED_GRAIN), simple_pfor_body(), ctx );
    }
};

//! Uses parallel_for body invoking a nested parallel_for with an isolated context without a try-block.
/** Even though exceptions thrown from the nested parallel_for are not handled 
    by the caller in this test, they will not affect sibling nested algorithms 
    already running because of the isolated contexts. However because the first 
    exception cancels the root parallel_for only the first g_num_threads subranges
    will be processed (which launch nested parallel_fors) **/
void Test3 () {
    TRACEP ("");
    reset_globals();
    typedef nesting_pfor_with_isolated_context_body body_type;
    intptr  nested_body_calls = util::num_subranges(NESTED_RANGE, NESTED_GRAIN),
            min_num_calls = (g_num_threads - 1) * nested_body_calls;
    TRY();
        tbb::parallel_for( range_type(0, NESTING_RANGE, NESTING_GRAIN), body_type() );
    CATCH_AND_ASSERT();
    ASSERT (!no_exception, "No exception thrown from the nesting parallel_for");
    TRACE ("Executed at the end of test %d", (intptr)g_cur_executed);
    if ( g_solitary_exception ) {
        ASSERT (g_cur_executed > min_num_calls, "Too few tasks survived exception");
        ASSERT (g_cur_executed <= min_num_calls + (g_catch_executed + g_num_threads), "Too many tasks survived exception");
    }
    ASSERT (g_exceptions == 1, "No try_blocks in any body expected in this test");
    if ( !g_solitary_exception )
        ASSERT (g_cur_executed <= g_catch_executed + g_num_threads, "Too many tasks survived exception");
} // void Test3 ()


class nesting_pfor_with_eh_body {
public:
    void operator()( const range_type& ) const {
        tbb::asynch_context ctx(tbb::asynch_context::isolated);
        ++g_cur_executed;
        TRY();
            tbb::parallel_for( tbb::blocked_range<size_t>(0, NESTED_RANGE, NESTED_GRAIN), simple_pfor_body(), ctx );
        CATCH();
    }
};

//! Uses parallel_for body invoking a nested parallel_for (with default bound context) inside a try-block.
/** Since exception(s) thrown from the nested parallel_for are handled by the caller 
    in this test, they do not affect neither other tasks of the the root parallel_for 
    nor sibling nested algorithms. **/
void Test4 () {
    TRACEP ("");
    reset_globals();
    intptr  nested_body_calls = util::num_subranges(NESTED_RANGE, NESTED_GRAIN),
            nesting_body_calls = util::num_subranges(NESTING_RANGE, NESTING_GRAIN),
            calls_in_normal_case = nesting_body_calls * (nested_body_calls + 1);
    TRY();
        tbb::parallel_for( range_type(0, NESTING_RANGE, NESTING_GRAIN), nesting_pfor_with_eh_body() );
    CATCH();
    ASSERT (no_exception, "All exceptions must have been handled in the parallel_for body");
    TRACE ("Executed %d (normal case %d), exceptions %d, in master only? %d", (intptr)g_cur_executed, calls_in_normal_case, (intptr)g_exceptions, g_exception_in_master);
    intptr  min_num_calls = 0;
    if ( g_solitary_exception ) {
        min_num_calls = calls_in_normal_case - nested_body_calls;
        ASSERT (g_exceptions == 1, "No exception registered");
        ASSERT (g_cur_executed <= min_num_calls + g_num_threads, "Too many tasks survived exception");
    }
    else if ( !g_exception_in_master ) {
        // Each nesting body + at least 1 of its nested body invocations
        min_num_calls = 2 * nesting_body_calls;
        TRACE ("g_exceptions %d, nesting_body_calls %d, g_exception_in_master %d", (intptr)g_exceptions, nesting_body_calls, g_exception_in_master);
        ASSERT (g_exceptions > 1 && g_exceptions <= nesting_body_calls, "Unexpected actual number of exceptions");
        //ASSERT (g_exceptions == (g_exception_in_master ? 1 : (nesting_body_calls - 1)), "Unexpected actual number of exceptions");
        ASSERT (g_cur_executed >= min_num_calls + (nesting_body_calls - g_exceptions) * nested_body_calls, "Too few tasks survived exception");
        ASSERT (g_cur_executed <= g_catch_executed + g_num_threads, "Too many tasks survived multiple exceptions");
        // Additional nested_body_calls accounts for the minimal amount of tasks spawned 
        // by not throwing threads. In the minimal case it is either the master thread or the only worker.
        TRACE ("g_cur_executed %d", (intptr)g_cur_executed);
        ASSERT (g_cur_executed <= min_num_calls + (nesting_body_calls - g_exceptions + 1) * nested_body_calls + g_exceptions + g_num_threads, "Too many tasks survived exception");
    }
} // void Test4 ()


class my_cancellation_root_task : public tbb::task
{
    tbb::asynch_context &my_ctx_to_cancel;
    intptr              my_cancel_threshold;

    task* execute () {
        while ( g_cur_executed < my_cancel_threshold )
            yield_if_singlecore();
        my_ctx_to_cancel.cancel_task_group();
        g_catch_executed = g_cur_executed;
        return NULL;
    }
public:
    my_cancellation_root_task ( tbb::asynch_context& ctx, intptr threshold ) 
        : my_ctx_to_cancel(ctx), my_cancel_threshold(threshold)
    {}
};

class pfor_body_to_cancel {
public:
    void operator()( const range_type& r ) const {
        ++g_cur_executed;
        util::sleep(20);
//        yield_if_singlecore();
    }
};

class my_calculation_root_task : public tbb::task
{
    tbb::asynch_context &my_ctx;

    task* execute () {
        tbb::parallel_for( range_type(0, ITER_RANGE, ITER_GRAIN), pfor_body_to_cancel(), my_ctx );
        return NULL;
    }
public:
    my_calculation_root_task ( tbb::asynch_context& ctx ) : my_ctx(ctx) {}
};


//! Test for cancelling an algorithm from outside (from a task running in parallel with the algorithm).
void Test5 () {
    TRACEP ("");
    reset_globals();
    g_throw_exception = false;
    intptr  threshold = util::num_subranges(ITER_RANGE, ITER_GRAIN) / 4;
    TRACE ("Threshold %d", threshold);
    tbb::asynch_context  ctx;
    tbb::task_list  tl;
    tl.push_back( *new( tbb::task::allocate_root() ) my_calculation_root_task(ctx) );
    tl.push_back( *new( tbb::task::allocate_root() ) my_cancellation_root_task(ctx, threshold) );
    TRY();
        tbb::task::spawn_root_and_wait(tl);
    CATCH();
    ASSERT (no_exception, "Cancelling tasks should not cause any exceptions");
    TRACE ("Threshold %d, total executed %d, executed after cancellation signal %d", threshold, (intptr)g_cur_executed, (intptr)g_catch_executed);
    ASSERT (g_cur_executed <= threshold + g_num_threads, "Too many tasks were executed after cancellation");
} // void Test5 ()


void TestExceptionHandling ()
{
    TRACE ("Number of threads %d", g_num_threads);
    tbb::task_scheduler_init init (g_num_threads);
    g_master = internal::GetThreadSpecific();

#if 0 /* !(__APPLE__ || (__linux__ && __TBB_x86_64)) */
    test_num_subranges_calculation<nesting_pfor_body>(NESTING_RANGE, NESTING_GRAIN, NESTED_RANGE, NESTED_GRAIN);
    Test2();
#endif /* 0 */

    Test0();
    Test1();
    Test3();
    Test4();
    Test5();
}

#endif /* __TBB_EXCEPTIONS */


//------------------------------------------------------------------------
// Entry point
//------------------------------------------------------------------------

#include <algorithm>

int main(int argc, char* argv[]) {
#if __TBB_EXCEPTIONS
    ParseCommandLine( argc, argv );
    MinThread = std::min<int>(MinThread, MaxThread);
    ASSERT (MinThread>=2, "Minimal number of threads must be 2 or more");
    ASSERT (ITER_RANGE >= ITER_GRAIN * MaxThread, "Fix defines");
    int step = std::max<int>(MaxThread - MinThread, 1);
    for ( g_num_threads = MinThread; g_num_threads <= MaxThread; g_num_threads += step ) {
        g_max_concurrency = std::min<int>(g_num_threads, tbb::task_scheduler_init::default_num_threads());
        // Execute in all the possible modes
        for ( size_t j = 0; j < 4; ++j ) {
            g_exception_in_master = (j & 1) == 1;
            g_solitary_exception = (j & 2) == 1;
            TestExceptionHandling();
        }
    }
#endif /* __TBB_EXCEPTIONS */
    printf("done\n");
    return 0;
}

