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

#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"
#include "tbb/blocked_range.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/tick_count.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cctype>

// This file is a microbenchmark for TBB.  
//
// Command line: 
//     test_unit.exe [P] filter1 filter2 ...
// where
//     p is of the form m or m:n, which indicates the number of threads to use.  m:n indicates a closed range [m:n]
//     filteri is a string indicating which tests to select.  If the filter is xyz, only tests with xyz in their name 
//     will be run.  Multiple filters are logically ANDed.
//
// Results are printed in tab-separate form suitable for importing into a spreadsheet.
// Each line has the form
//     name     P       N       rate0...rate4
// where 
//     name describes the test
//     P is the number of threads
//     N is the value of N used.  It is set so that the test runs in 1 millisecond.
//     rate0...rate4 are the rates that were measured.  Rates are in printed in per-millisecond.  

int filterc;

//! Pointer into argv where filter arguments begin. 
char** filterv;

//! Miniumum number of threads.
int min_thread=1;

//! Maximum number of thread
int max_thread=1;

//! Type of a count.  
typedef size_t count_type;

typedef void (*benchmark_type)(count_type);

//! Desired time in which each test must run.
/** It is a millisecond, because TBB targets desktop parallelism, and 
    interactive programs have to do calculations in fractions of a video frame. */
const double DESIRED_TEST_TIME = 0.001;

//! Number of times to repeat a test.
const int REPETITIONS = 10;

//! Each run must not vary more than a relative tolerand of (+/-)TOLERANCE
const double TOLERANCE = 0.1;

//! Number of times to run each test.
const int N_TRIAL = 5;

//! Measure time it takes to run test with n iterations.
double time_one( benchmark_type test, count_type n ) {
    tbb::tick_count t0 = tbb::tick_count::now();
    for( int i=0; i<REPETITIONS; ++i )
        test(n);
    tbb::tick_count t1 = tbb::tick_count::now();
    return (t1-t0).seconds()/REPETITIONS;
}

//! Print a rate.
/** This routine prints a typical rate in only 7 characters, instead of the 9 
    that would be print if a %g format were used.  The savings are because
    print_one_rate prints the exponent without a "+", and without a "0" 
    if the exponent is a single digit. */
void print_one_rate( double rate ) {
    int exponent = int(floor(log10(rate)));
    printf("%.3fe%d", rate/pow(10.0,exponent), exponent );
}

//! Time a benchmark
/** Fills in elements of rate[] with per-second rates. 
    Returns nominal number of times the benchmark was run in DESIRED_TIME.
    Returns 0 if it cannot get a repeatable time. */
count_type run_one( const char* name, benchmark_type benchmark, double rate[N_TRIAL] ) {
    // Estimate appropriate value for n
    double time;
    count_type n;
    int n_retry = 0;
retry:
    for(n=1;;n*=2) {
        time = time_one(benchmark,n);
        // Quit early if overflow might happen
        if( time>=DESIRED_TEST_TIME ) break;
        if( n*2<n ) {
            printf("error: overflow for %s\n",name);
            exit(1);
        }
    }
    n = count_type(n*DESIRED_TEST_TIME/time+0.5);
    time = time_one(benchmark,n);
    n = count_type(n*DESIRED_TEST_TIME/time+0.5);
    double expected_rate = n/time;
    for( int i=0; i<N_TRIAL; ++i ) {
        // Test is run n+wobble times, to reveal any sensitivity to small variations in n.
        int wobble = (i-N_TRIAL)/2;
        time = time_one(benchmark,n+wobble);
        rate[i] = (n+wobble)/time;
        if( rate[i]<expected_rate*(1-TOLERANCE) || rate[i]>expected_rate*(1+TOLERANCE) ) {
            if( ++n_retry==3 ) {
                return 0;
            }
            goto retry; 
        }
    }
    return n;
}

void run( const char* name, benchmark_type benchmark ) {
    for( int k=0; k<filterc; ++k )
        if( std::strstr( name, filterv[k] )==NULL )
            return;
    double rate[N_TRIAL];
    for( int nthread=min_thread; nthread<=max_thread; ++nthread ) {
        tbb::task_scheduler_init init(nthread);
        if( count_type n = run_one( name, benchmark, rate ) ) {
            printf("%-64s\t%4u\t%10lu",name,nthread,(unsigned long)n);
            for( int i=0; i<N_TRIAL; ++i ) {
                printf("\t");
                // Rates in rate[] are per-second.  We print them in per-milliscond form
                print_one_rate(rate[i]*DESIRED_TEST_TIME);
            }
            printf("\n");
        } else {
            printf("%-64s\t%4u\tcannot get repeatable time\n",name,nthread);
        }
    }
}  

//! Print table header
void print_header() {
    printf("%-64s\t%4s\t%10s","TEST","P","N");
    for( int i=0; i<N_TRIAL; ++i )
        printf("\t  rate%d",i);
    printf("\n");
}

void time_tick_count( count_type n ) {
    for( count_type i=0; i<n; ++i ) {
        volatile tbb::tick_count t = tbb::tick_count::now();
    }
}

struct sum_task_continuation: public tbb::task {
    typedef unsigned long value_t;
    value_t& sum;
    value_t x, y;
    sum_task_continuation( value_t& sum_ ) : sum(sum_) {}
    task* execute() {
        sum = x+y;
        return NULL;
    }
};

//! Task for unbalanced recursion.  
/** The recursion splits with ratio 1 to N-1. */
template<unsigned long N>
struct sum_task: public tbb::task {
    typedef unsigned long value_t;
    value_t& sum;
    value_t lower, upper;
    sum_task( value_t& sum_, value_t lower_, value_t upper_ ) : sum(sum_), lower(lower_), upper(upper_) {}
    /*override*/ task* execute() {
        if( upper-lower==1 ) {
            sum = upper-lower;
            return NULL;
        } else {
            value_t d = lower+(upper-lower+N-1)/N;
            sum_task_continuation& c = *new( allocate_continuation() ) sum_task_continuation(sum);
            c.set_ref_count(2);
            sum_task& b = *new( c.allocate_child() ) sum_task(c.y,d,upper);
            c.spawn( b );
            return new( c.allocate_child() ) sum_task(c.x,lower,d);
        }
    }
};

template<unsigned long N>
void time_sum_task( count_type n ) {
    unsigned long sum;
    sum_task<N>& s = *new(tbb::task::allocate_root()) sum_task<N>(sum,0,n);
    tbb::task::spawn_root_and_wait(s);
}

struct trivial_body {
    void operator()( const tbb::blocked_range<count_type>& r ) const {
        volatile long x;
        int end = r.end();
        for( int i=r.begin(); i<end; ++i )
            x = 0;
    }
};

template<typename Partitioner>
void time_empty_parallel_for( count_type n ) {
    Partitioner partitioner;
    volatile count_type zero = 0;
    for( count_type i=0; i<n; ++i ) {
        tbb::parallel_for( tbb::blocked_range<count_type>( 0, zero, 1 ), trivial_body(), partitioner );
    }
}

template<typename Partitioner, const size_t chunk_size>
void time_n_parallel_for( count_type n ) {
    Partitioner partitioner;
    tbb::parallel_for( tbb::blocked_range<count_type>( 0, n, chunk_size ), trivial_body(), partitioner );
}

struct trivial_reduction_body {
    int sum;
    trivial_reduction_body() {sum=0;}
    trivial_reduction_body( trivial_reduction_body& other, tbb::split ) {sum=0;}
    void join( trivial_reduction_body& other ) {sum+=other.sum;}
    void operator()( const tbb::blocked_range<count_type>& r ) {
        volatile long x;
        int end = r.end();
        for( int i=r.begin(); i<end; ++i )
            x = 0;
        sum = 0;
    }
};

template<typename Partitioner>
void time_empty_parallel_reduce( count_type n ) {
    Partitioner partitioner;
    trivial_reduction_body body;
    volatile count_type zero = 0;
    for( count_type i=0; i<n; ++i ) {
        tbb::parallel_reduce( tbb::blocked_range<count_type>( 0, zero, 1 ), body, partitioner );
    }
}

template<typename Partitioner, const size_t chunk_size>
void time_n_parallel_reduce( count_type n ) {
    Partitioner partitioner;
    trivial_reduction_body body;
    tbb::parallel_reduce( tbb::blocked_range<count_type>( 0, n, chunk_size ), body, partitioner );
}

//! Parse the command line
void parse_command_line( int argc, char* argv[] ) {
    int i = 1;
    if( i<argc && std::isdigit(argv[i][0])) {
        char* endptr;
        min_thread = strtol( argv[i], &endptr, 0 );
        if( *endptr==':' )
            max_thread = strtol( endptr+1, &endptr, 0 );
        else if( *endptr=='\0' )
            max_thread = min_thread;
        if( *endptr!='\0' ) {
            fprintf(stderr,"garbled nthread range\n");
            exit(1);
        }
        if( min_thread<0 ) {
            fprintf(stderr,"nthread must be nonnegative\n");
            exit(1);
        }
        if( max_thread<min_thread ) {
            fprintf(stderr,"nthread range is backwards\n");
            exit(1);
        }
        ++i;
    }
    filterv = argv+i;
    filterc = argc-i;
}

int main( int argc, char* argv[] ) {
    parse_command_line( argc, argv );
    print_header();
    run( "tick_count::now()", time_tick_count );

    // tasks on unbalanced tree
    run( "sum_task<4>", time_sum_task<4> );

    // parallel_for
    run( "parallel_for(blocked_range(0,zero,1),b,simple_partitioner())", time_empty_parallel_for<tbb::simple_partitioner> );
    run( "parallel_for(blocked_range(0,N,1),b,simple_partitioner())", time_n_parallel_for<tbb::simple_partitioner,1> );
    run( "parallel_for(blocked_range(0,N,1),b,auto_partitioner())", time_n_parallel_for<tbb::auto_partitioner,1> );
#if __TBB_AFFINITY
    run( "parallel_for(blocked_range(0,N,1),b,affinity_partitioner())", time_n_parallel_for<tbb::affinity_partitioner,1> );
#endif /* __TBB_AFFINITY */
    run( "parallel_for(blocked_range(0,N,10000),b,simple_partitioner())", time_n_parallel_for<tbb::simple_partitioner,10000> );
    run( "parallel_for(blocked_range(0,N,10000),b,auto_partitioner())", time_n_parallel_for<tbb::auto_partitioner,10000> );
#if __TBB_AFFINITY
    run( "parallel_for(blocked_range(0,N,10000),b,affinity_partitioner())", time_n_parallel_for<tbb::affinity_partitioner,10000> );
#endif /* __TBB_AFFINITY */

    // parallel_reduce
    run( "parallel_reduce(blocked_range(0,zero,1),b,simple_partitioner())", time_empty_parallel_reduce<tbb::simple_partitioner> );
    run( "parallel_reduce(blocked_range(0,N,1),b,simple_partitioner())", time_n_parallel_reduce<tbb::simple_partitioner,1> );
    run( "parallel_reduce(blocked_range(0,N,1),b,auto_partitioner())", time_n_parallel_reduce<tbb::auto_partitioner,1> );
#if __TBB_AFFINITY
    run( "parallel_reduce(blocked_range(0,N,1),b,affinity_partitioner())", time_n_parallel_reduce<tbb::affinity_partitioner,1> );
#endif /* __TBB_AFFINITY */
    run( "parallel_reduce(blocked_range(0,N,10000),b,simple_partitioner())", time_n_parallel_reduce<tbb::simple_partitioner,10000> );
    run( "parallel_reduce(blocked_range(0,N,10000),b,auto_partitioner())", time_n_parallel_reduce<tbb::auto_partitioner,10000> );
#if __TBB_AFFINITY
    run( "parallel_reduce(blocked_range(0,N,10000),b,affinity_partitioner())", time_n_parallel_reduce<tbb::affinity_partitioner,10000> );
#endif /* __TBB_AFFINITY */

    printf("done\n");
    return 0;
}

