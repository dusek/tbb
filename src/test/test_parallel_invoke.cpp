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

#include "tbb/parallel_invoke.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/atomic.h"
#include "tbb/tbb_exception.h"
#include "harness.h"
#include "harness_trace.h"
#include "harness_sleep.h"

static const size_t MAX_NUMBER_OF_PINVOKE_ARGS = 10;
tbb::atomic<size_t> function_counter;

// Some macros to make the test easier to read

// 10 functions test0 ... test9 are defined
// pointer to each function is also defined

#define TEST_FUNCTION(value) void test##value () \
{   \
    ASSERT(!(function_counter & (1 << value)), "Test function has already been called"); \
    function_counter += 1 << value; \
}   \
void (*test_pointer##value)(void) = &test##value;

TEST_FUNCTION(0)
TEST_FUNCTION(1)
TEST_FUNCTION(2)
TEST_FUNCTION(3)
TEST_FUNCTION(4)
TEST_FUNCTION(5)
TEST_FUNCTION(6)
TEST_FUNCTION(7)
TEST_FUNCTION(8)
TEST_FUNCTION(9)

// The same with functors
#define TEST_FUNCTOR(value) class test_functor##value  \
{   \
public: \
    void operator() () const {  \
        function_counter += 1 << value;   \
    }   \
} functor##value;

TEST_FUNCTOR(0)
TEST_FUNCTOR(1)
TEST_FUNCTOR(2)
TEST_FUNCTOR(3)
TEST_FUNCTOR(4)
TEST_FUNCTOR(5)
TEST_FUNCTOR(6)
TEST_FUNCTOR(7)
TEST_FUNCTOR(8)
TEST_FUNCTOR(9)

#define INIT_TEST function_counter = 0;

#define VALIDATE_INVOKE_RUN(number_of_args, test_type) \
    ASSERT( (size_t)function_counter == (size_t)(1 << number_of_args) - 1, "parallel_invoke called with " #number_of_args " arguments didn't process all " #test_type);

// Calls parallel_invoke for different number of arguments
// It can be called with and without user context
template <typename F0, typename F1, typename F2, typename F3, typename F4, typename F5,
    typename F6, typename F7, typename F8, typename F9>
void call_parallel_invoke( size_t n, F0& f0, F1& f1, F2& f2, F3& f3, F4 &f4, F5 &f5,
                          F6& f6, F7 &f7, F8 &f8, F9 &f9, tbb::task_group_context* context) {
    switch(n) {
    default:
        ASSERT(false, "number of arguments must be between 2 and 10");
    case 2:
        if (context)
            tbb::parallel_invoke (f0, f1, *context);
        else
            tbb::parallel_invoke (f0, f1);
        break;
    case 3:
        if (context)
            tbb::parallel_invoke (f0, f1, f2, *context);
        else
            tbb::parallel_invoke (f0, f1, f2);
        break;
    case 4:
        if(context)
            tbb::parallel_invoke (f0, f1, f2, f3, *context);
        else
            tbb::parallel_invoke (f0, f1, f2, f3);
        break;
    case 5:
        if(context)
            tbb::parallel_invoke (f0, f1, f2, f3, f4, *context);
        else
            tbb::parallel_invoke (f0, f1, f2, f3, f4);
        break;
    case 6:
        if(context)
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, *context);
        else
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5);
        break;
    case 7:
        if(context)
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, f6, *context);
        else
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, f6);
        break;
    case 8:
        if(context)
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, f6, f7, *context);
        else
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, f6, f7);
        break;
    case 9:
        if(context)
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, f6, f7, f8, *context);
        else
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, f6, f7, f8);
        break;
    case 10:
        if(context)
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, *context);
        else
            tbb::parallel_invoke (f0, f1, f2, f3, f4, f5, f6, f7, f8, f9);
        break;
    }
}

void test_parallel_invoke()
{
    REMARK (__FUNCTION__);
    // Testing parallel_invoke with functions
    for (int n = 2; n <=10; n++)
    {
        INIT_TEST;
        call_parallel_invoke(n, test0, test1, test2, test3, test4, test5, test6, test7, test8, test9, NULL);
        VALIDATE_INVOKE_RUN(n, "functions");
    }
    
    // Testing with pointers to functions
    for (int n = 2; n <=10; n++)
    {
        INIT_TEST;
        call_parallel_invoke(n, test_pointer0, test_pointer1, test_pointer2, test_pointer3, test_pointer4,
            test_pointer5, test_pointer6, test_pointer7, test_pointer8, test_pointer9, NULL);
        VALIDATE_INVOKE_RUN(n, "pointers to function");
    }

    // Testing parallel_invoke with functors
    for (int n = 2; n <=10; n++)
    {
        INIT_TEST;
        call_parallel_invoke(n, functor0, functor1, functor2, functor3, functor4,
            functor5, functor6, functor7, functor8, functor9, NULL);
        VALIDATE_INVOKE_RUN(n, "functors");
    }
}

// Exception handling support test
// Some variables and macros to test exceptions
tbb::atomic<size_t> g_cur_executed;
volatile bool g_exception_occured = false;
volatile bool g_unknown_exception = false;
int g_max_concurrency = 0;
#define EXCEPTION_DESCR "Test exception"

class test_exception : public std::exception
{
public:
    test_exception ( ) {}

    const char* what() const throw() {
        REMARK ("About to throw test_exception... :");
        return EXCEPTION_DESCR;
    }
};

volatile size_t exception_mask; // each bit represents whether the function should throw exception or not

#define TRY()   \
    try {
#define CATCH() \
    } catch ( tbb::captured_exception& e ) {    \
        REMARK("tbb::captured_exception is caught");   \
        ASSERT (strcmp(e.what(), EXCEPTION_DESCR) == 0, "Unexpected original exception info");   \
        g_exception_occured = true;   \
    }   \
    catch ( ... ) { \
        g_exception_occured = true;   \
        g_unknown_exception = true;   \
    }

#define ASSERT_EXCEPTION()    \
    ASSERT (g_exception_occured, "no exception occurred");  \
    ASSERT (!g_unknown_exception, "unknown exception was caught");

#define CATCH_AND_ASSERT()    \
    CATCH() \
    ASSERT_EXCEPTION()

void reset_globals () {
    INIT_TEST;
    g_cur_executed = 0;
    g_exception_occured = false;
    g_unknown_exception = false;
}

void throw_test_exception () {
    throw test_exception();
}

// throws exception if corresponding exception_mask bit is set
#define TEST_FUNCTION_WITH_THROW(value) void test_with_throw##value () \
{   \
    if (exception_mask & (1 << value)){   \
        throw_test_exception();    \
    }else{  \
    }   \
}

TEST_FUNCTION_WITH_THROW(0)
TEST_FUNCTION_WITH_THROW(1)
TEST_FUNCTION_WITH_THROW(2)
TEST_FUNCTION_WITH_THROW(3)
TEST_FUNCTION_WITH_THROW(4)
TEST_FUNCTION_WITH_THROW(5)
TEST_FUNCTION_WITH_THROW(6)
TEST_FUNCTION_WITH_THROW(7)
TEST_FUNCTION_WITH_THROW(8)
TEST_FUNCTION_WITH_THROW(9)

void test_exception_handling()
{
    REMARK (__FUNCTION__);
    for( size_t n = 2; n <= 10; ++n ) {
        for( exception_mask = 1; exception_mask < (size_t) (1 << n); ++exception_mask ) {
            reset_globals();
            TRY();
                REMARK("Calling parallel_invoke, number of functions = %d, exeption_mask = %d\n", n, exception_mask);
                call_parallel_invoke(n, test_with_throw0, test_with_throw1, test_with_throw2, test_with_throw3,
                    test_with_throw4, test_with_throw5, test_with_throw6, test_with_throw7, test_with_throw8, test_with_throw9, NULL);
            CATCH_AND_ASSERT();
        }
    }
}

// Cancellaton support test

class my_cancellator_task : public tbb::task
{
    tbb::task_group_context &my_ctx_to_cancel;
    size_t my_cancel_threshold;

    tbb::task* execute () {
        s_cancellator_ready = true;
        while ( g_cur_executed < my_cancel_threshold )
            __TBB_Yield();
        my_ctx_to_cancel.cancel_group_execution();
        return NULL;
    }
public:
    my_cancellator_task ( tbb::task_group_context& ctx, size_t threshold )
        : my_ctx_to_cancel(ctx), my_cancel_threshold(threshold)
    {}    
    static volatile bool s_cancellator_ready;
};

volatile bool my_cancellator_task::s_cancellator_ready = false;

void function_to_cancel() {
    ++g_cur_executed;
    do {
        Harness::Sleep(10);
        __TBB_Yield();
    } while( !my_cancellator_task::s_cancellator_ready );
}

// The function is used to test cancellation
void simple_test_nothrow (){
    g_cur_executed++;
}

class my_worker_pinvoke_task : public tbb::task
{
    tbb::task_group_context &my_ctx;
    size_t my_number_of_functions;
    size_t my_number_of_function_to_cancel;
    void(*func_array[10])(void);

    tbb::task* execute () {
        func_array[my_number_of_function_to_cancel] = &function_to_cancel;
        call_parallel_invoke(my_number_of_functions, func_array[0], func_array[1], func_array[2], func_array[3],
            func_array[4], func_array[5], func_array[6], func_array[7], func_array[8], func_array[9], &my_ctx);
        return NULL;
    }
public:
    my_worker_pinvoke_task ( tbb::task_group_context& ctx, size_t number_of_functions, size_t number_of_function_to_cancel ) 
        : my_ctx(ctx), my_number_of_functions (number_of_functions), my_number_of_function_to_cancel(number_of_function_to_cancel)
    {
        for (int i = 0; i <=9; ++i)
            func_array[i] = &simple_test_nothrow;
    }
};

void test_cancellation(size_t number_of_functions, size_t number_of_function_to_cancel)
{
    REMARK (__FUNCTION__);
    reset_globals();
    tbb::task_group_context  ctx;
    my_cancellator_task::s_cancellator_ready = false;
    tbb::empty_task &r = *new( tbb::task::allocate_root(ctx) ) tbb::empty_task;
    r.set_ref_count(3);
    r.spawn( *new( r.allocate_child() ) my_cancellator_task(ctx, 1) );
    __TBB_Yield();
    r.spawn( *new( r.allocate_child() ) my_worker_pinvoke_task(ctx, number_of_functions, number_of_function_to_cancel) );
    TRY();
        r.wait_for_all();
    CATCH();
    r.destroy(r);
    ASSERT (!g_exception_occured, "Cancelling tasks should not cause any exceptions");
}

//------------------------------------------------------------------------
// Entry point
//------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // Set default minimum number of threads
    MinThread = 2;
    ParseCommandLine( argc, argv );
    MinThread = min(MinThread, MaxThread);
    ASSERT (MinThread>=1, "Minimal number of threads must be 1 or more");
    int step = max(MaxThread - MinThread, 1);
    for ( int g_num_threads = MinThread; g_num_threads <= MaxThread; g_num_threads += step ) {
        tbb::task_scheduler_init init( g_num_threads );
        g_max_concurrency = min(g_num_threads, tbb::task_scheduler_init::default_num_threads());
        test_parallel_invoke();
        if (g_num_threads > 1) {
#if __GLIBC__==2&&__GLIBC_MINOR__==3
            printf("Warning: Exception handling tests are skipped due to a known issue.\n");
#else
            test_exception_handling();
#endif
            for(int n = 2; n <=10; ++n)
                for (int m = 0; m <= n - 1; ++m)
                    test_cancellation(n, m);
        }
    }
    printf("done\n");
    return 0;
}

