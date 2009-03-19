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

#include "tbb/enumerable_thread_specific.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"
#include "tbb/blocked_range.h"
#include "tbb/tick_count.h"
#include "tbb/tbb_allocator.h"
#include "tbb/tbb_thread.h"

#include <cstring>
#include <vector>
#include <deque>
#include <list>
#include <map>
#include <utility>

#include "harness_assert.h"
#include "harness.h"

static tbb::atomic<int> construction_counter;
static tbb::atomic<int> destruction_counter;

const int REPETITIONS = 10;
const int N = 100000;
const int VALID_NUMBER_OF_KEYS = 100;
const double EXPECTED_SUM = (REPETITIONS + 1) * N;

//
// A minimal class
// Define: default and copy constructor, and allow implicit operator&
// Hide: operator=
//

class minimal: NoAssign {
private:
    int my_value;
public:
    minimal() : my_value(0) { ++construction_counter; }
    minimal( const minimal &m ) : my_value(m.my_value) { ++construction_counter; }
    ~minimal() { ++destruction_counter; }
    void set_value( const int i ) { my_value = i; }
    int value( ) const { return my_value; }
};

//
// A helper class that simplifies writing the tests since minimal does not 
// define = or + operators.
//

template< typename T >
struct test_helper {
   static inline void init(T &e) { e = static_cast<T>(0); }  
   static inline void sum(T &e, const int addend ) { e += static_cast<T>(addend); }
   static inline void sum(T &e, const double addend ) { e += static_cast<T>(addend); }
   static inline void set(T &e, const int value ) { e = static_cast<T>(value); }
   static inline double get(T &e ) { return static_cast<double>(e); }
};

template< >
struct test_helper<minimal> {
   static inline void init(minimal &sum) { sum.set_value( 0 ); }  
   static inline void sum(minimal &sum, const int addend ) { sum.set_value( sum.value() + addend); }
   static inline void sum(minimal &sum, const double addend ) { sum.set_value( sum.value() + static_cast<int>(addend)); }
   static inline void sum(minimal &sum, const minimal &addend ) { sum.set_value( sum.value() + addend.value()); }
   static inline void set(minimal &v, const int value ) { v.set_value( static_cast<int>(value) ); }
   static inline double get(minimal &sum ) { return static_cast<double>(sum.value()); }
};


template< typename T >
void run_serial_scalar_tests(const char *test_name) {
    tbb::tick_count t0;
    T sum;
    test_helper<T>::init(sum);

    if (Verbose) printf("Testing serial %s... ", test_name);  
    for (int t = -1; t < REPETITIONS; ++t) {
        if (Verbose && t == 0) t0 = tbb::tick_count::now(); 
        for (int i = 0; i < N; ++i) {
            test_helper<T>::sum(sum,1); 
        }
    }
 
    double result_value = test_helper<T>::get(sum);
    ASSERT( EXPECTED_SUM == result_value, NULL);
    if (Verbose)
        printf("done\nserial %s, 0, %g, %g\n", test_name, result_value, ( tbb::tick_count::now() - t0).seconds());
}


template <typename T>
class parallel_scalar_body: NoAssign {
    
    tbb::enumerable_thread_specific<T> &sums;
 
public:

    parallel_scalar_body ( tbb::enumerable_thread_specific<T> &_sums ) : sums(_sums) { }

    void operator()( const tbb::blocked_range<int> &r ) const {
        for (int i = r.begin(); i != r.end(); ++i) 
            test_helper<T>::sum( sums.local(), 1 );
    }
   
};

template< typename T >
void run_parallel_scalar_tests(const char *test_name) {

    tbb::task_scheduler_init init(tbb::task_scheduler_init::deferred);
    T exemplar;
    test_helper<T>::init(exemplar);

    for (int p = MinThread; p <= MaxThread; ++p) { 


        if (p == 0) continue;

        if (Verbose) printf("Testing parallel %s on %d thread(s)... ", test_name, p);
        init.initialize(p);

        tbb::tick_count t0;

        T iterator_sum;
        test_helper<T>::init(iterator_sum);

        T const_iterator_sum; 
        test_helper<T>::init(const_iterator_sum);

        T range_sum;
        test_helper<T>::init(range_sum);

        T const_range_sum;
        test_helper<T>::init(const_range_sum);

        for (int t = -1; t < REPETITIONS; ++t) {
            if (Verbose && t == 0) t0 = tbb::tick_count::now(); 

            tbb::enumerable_thread_specific<T> sums(exemplar);

            ASSERT( sums.empty(), NULL);
            tbb::parallel_for( tbb::blocked_range<int>( 0, N, 10000 ), parallel_scalar_body<T>( sums ) );
            ASSERT( !sums.empty(), NULL);

            // use iterator
            typename tbb::enumerable_thread_specific<T>::size_type size = 0;
            for ( typename tbb::enumerable_thread_specific<T>::iterator i = sums.begin(); i != sums.end(); ++i ) {
                 ++size;
                 test_helper<T>::sum(iterator_sum, *i);
            }
            ASSERT( sums.size() == size, NULL);

            // use const_iterator
            for ( typename tbb::enumerable_thread_specific<T>::const_iterator i = sums.begin(); i != sums.end(); ++i ) {
                 test_helper<T>::sum(const_iterator_sum, *i);
            }
           
            // use range_type
            typename tbb::enumerable_thread_specific<T>::range_type r = sums.range();  
            for ( typename tbb::enumerable_thread_specific<T>::range_type::const_iterator i = r.begin(); i != r.end(); ++i ) {
                 test_helper<T>::sum(range_sum, *i);
            }
           
            // use const_range_type
            typename tbb::enumerable_thread_specific<T>::const_range_type cr = sums.range();  
            for ( typename tbb::enumerable_thread_specific<T>::const_range_type::iterator i = cr.begin(); i != cr.end(); ++i ) {
                 test_helper<T>::sum(const_range_sum, *i);
            }

        }

        ASSERT( EXPECTED_SUM == test_helper<T>::get(iterator_sum), NULL);
        ASSERT( EXPECTED_SUM == test_helper<T>::get(const_iterator_sum), NULL);
        ASSERT( EXPECTED_SUM == test_helper<T>::get(range_sum), NULL);
        ASSERT( EXPECTED_SUM == test_helper<T>::get(const_range_sum), NULL);

        if (Verbose)
            printf("done\nparallel %s, %d, %g, %g\n", test_name, p, test_helper<T>::get(iterator_sum), 
                                                      ( tbb::tick_count::now() - t0).seconds());
        init.terminate();
    }
}


template <typename T>
class parallel_vector_for_body: NoAssign {
    
    tbb::enumerable_thread_specific< std::vector<T, tbb::tbb_allocator<T> > > &locals;
 
public:

    parallel_vector_for_body ( tbb::enumerable_thread_specific< std::vector<T, tbb::tbb_allocator<T> > > &_locals ) : locals(_locals) { }

    void operator()( const tbb::blocked_range<int> &r ) const {
        T one;
        test_helper<T>::set(one, 1);

        for (int i = r.begin(); i < r.end(); ++i) {
            locals.local().push_back( one );
        }
    }
   
};

template <typename R, typename T>
struct parallel_vector_reduce_body {

    T sum;    
    size_t count;    

    parallel_vector_reduce_body ( ) : count(0) { test_helper<T>::init(sum); }
    parallel_vector_reduce_body ( parallel_vector_reduce_body<R, T> &, tbb::split ) : count(0) {  test_helper<T>::init(sum); }

    void operator()( const R &r ) {
        for (typename R::iterator ri = r.begin(); ri != r.end(); ++ri) {
            const std::vector< T, tbb::tbb_allocator<T>  > &v = *ri; 
            ++count;
            for (typename std::vector<T, tbb::tbb_allocator<T> >::const_iterator vi = v.begin(); vi != v.end(); ++vi) {
                test_helper<T>::sum(sum, *vi);
            }
        }
    }

    void join( const parallel_vector_reduce_body &b ) {
        test_helper<T>::sum(sum,b.sum);
        count += b.count;
    }
   
};

template< typename T >
void run_parallel_vector_tests(const char *test_name) {
    tbb::tick_count t0;
    tbb::task_scheduler_init init(tbb::task_scheduler_init::deferred);
    typedef std::vector<T, tbb::tbb_allocator<T> > container_type;

    for (int p = MinThread; p <= MaxThread; ++p) { 

        if (p == 0) continue;
        if (Verbose) printf("Testing parallel %s on %d thread(s)... ", test_name, p);
        init.initialize(p);

        T sum;
        test_helper<T>::init(sum);

        for (int t = -1; t < REPETITIONS; ++t) {
            if (Verbose && t == 0) t0 = tbb::tick_count::now(); 
            typedef typename tbb::enumerable_thread_specific< container_type > ets_type;
            ets_type vs;

            ASSERT( vs.empty(), NULL);
            tbb::parallel_for ( tbb::blocked_range<int> (0, N, 10000), parallel_vector_for_body<T>( vs ) );
            ASSERT( !vs.empty(), NULL);

            parallel_vector_reduce_body< typename tbb::enumerable_thread_specific< std::vector< T, tbb::tbb_allocator<T>  > >::const_range_type, T > pvrb;
            tbb::parallel_reduce ( vs.range(1), pvrb );

            test_helper<T>::sum(sum, pvrb.sum);

            ASSERT( vs.size() == pvrb.count, NULL);

            tbb::flattened2d<ets_type> fvs = flatten2d(vs);
            size_t ccount = fvs.size();
            size_t elem_cnt = 0;
            for(typename tbb::flattened2d<ets_type>::const_iterator i = fvs.begin(); i != fvs.end(); ++i) {
                ++elem_cnt;
            };
            ASSERT(ccount == elem_cnt, NULL);

            elem_cnt = 0;
            for(typename tbb::flattened2d<ets_type>::iterator i = fvs.begin(); i != fvs.end(); ++i) {
                ++elem_cnt;
            };
            ASSERT(ccount == elem_cnt, NULL);
        }

        double result_value = test_helper<T>::get(sum);
        ASSERT( EXPECTED_SUM == result_value, NULL);
        if (Verbose)
            printf("done\nparallel %s, %d, %g, %g\n", test_name, p, result_value, ( tbb::tick_count::now() - t0).seconds());
        init.terminate();
    }
}

template< typename T >
void run_serial_vector_tests(const char *test_name) {
    tbb::tick_count t0;
    T sum;
    test_helper<T>::init(sum);
    T one;
    test_helper<T>::set(one, 1);

    if (Verbose) printf("Testing serial %s... ", test_name);
    for (int t = -1; t < REPETITIONS; ++t) {
        if (Verbose && t == 0) t0 = tbb::tick_count::now(); 
        std::vector<T, tbb::tbb_allocator<T> > v; 
        for (int i = 0; i < N; ++i) {
            v.push_back( one );
        }
        for (typename std::vector<T, tbb::tbb_allocator<T> >::const_iterator i = v.begin(); i != v.end(); ++i) 
            test_helper<T>::sum(sum, *i); 
    }

    double result_value = test_helper<T>::get(sum);
    ASSERT( EXPECTED_SUM == result_value, NULL);
    if (Verbose)
        printf("done\nserial %s, 0, %g, %g\n", test_name, result_value, ( tbb::tick_count::now() - t0).seconds());
}

void 
run_serial_tests() {
    run_serial_scalar_tests<int>("int");
    run_serial_scalar_tests<double>("double");
    run_serial_scalar_tests<minimal>("minimal");
    run_serial_vector_tests<int>("std::vector<int, tbb::tbb_allocator<int> >");
    run_serial_vector_tests<double>("std::vector<double, tbb::tbb_allocator<double> >");
}

void 
run_parallel_tests() {
    run_parallel_scalar_tests<int>("int");
    run_parallel_scalar_tests<double>("double");
    run_parallel_scalar_tests<minimal>("minimal");
    run_parallel_vector_tests<int>("std::vector<int, tbb::tbb_allocator<int> >");
    run_parallel_vector_tests<double>("std::vector<double, tbb::tbb_allocator<double> >");
}

typedef tbb::enumerable_thread_specific<minimal> * minimal_ptr;

class set_body {
    minimal_ptr *a;

public:
    set_body( minimal_ptr *_a ) : a(_a) { }

    void operator() ( ) const {
        for (int i = 0; i < VALID_NUMBER_OF_KEYS; ++i) {
            a[i]->local().set_value(i + 1);
        }
    }
 
};

void do_tbb_threads( int max_threads, minimal_ptr *a ) {
    std::vector< tbb::tbb_thread * > threads;

    for (int p = 0; p < max_threads; ++p) { 
        threads.push_back( new tbb::tbb_thread ( set_body( a ) ) ); 
    }

    for (int p = 0; p < max_threads; ++p) {
        threads[p]->join();
    }
}

void
flog_key_creation_and_deletion() {

    const int FLOG_REPETITIONS = 100;
    minimal_ptr a[VALID_NUMBER_OF_KEYS];
    tbb::task_scheduler_init init(tbb::task_scheduler_init::deferred);

    for (int p = MinThread; p <= MaxThread; ++p) { 

        if (p == 0) continue;

        if (Verbose) printf("Testing repeated deletes on %d threads... ", p);

        for (int j = 0; j < FLOG_REPETITIONS; ++j) {
            construction_counter = 0;
            destruction_counter = 0;

            // causes VALID_NUMER_OF_KEYS exemplar instances to be constructed 
            for (int i = 0; i < VALID_NUMBER_OF_KEYS; ++i) {
                a[i] = new tbb::enumerable_thread_specific<minimal>;
            }

            // causes p * VALID_NUMBER_OF_KEYS minimals to be created
            do_tbb_threads(p, a); 

            for (int i = 0; i < VALID_NUMBER_OF_KEYS; ++i) {
                for ( tbb::enumerable_thread_specific< minimal >::iterator tli = a[i]->begin();
                      tli != a[i]->end(); ++tli ) {
                    ASSERT( (*tli).value() == i+1, NULL );
                }
                delete a[i];
                a[i] = NULL;
            }
        }

        ASSERT( int(construction_counter) == (p+1)*VALID_NUMBER_OF_KEYS, NULL );
        ASSERT( int(destruction_counter) == (p+1)*VALID_NUMBER_OF_KEYS, NULL );

        if (Verbose) printf("done\nTesting repeated clears on %d threads... ", p);

        construction_counter = 0;
        destruction_counter = 0;

        // causes VALID_NUMER_OF_KEYS exemplar instances to be constructed 
        for (int i = 0; i < VALID_NUMBER_OF_KEYS; ++i) {
            a[i] = new tbb::enumerable_thread_specific<minimal>;
        }
 
        for (int j = 0; j < FLOG_REPETITIONS; ++j) {

            // causes p * VALID_NUMBER_OF_KEYS minimals to be created
            do_tbb_threads(p, a);

            for (int i = 0; i < VALID_NUMBER_OF_KEYS; ++i) {
                for ( tbb::enumerable_thread_specific< minimal >::iterator tli = a[i]->begin();
                      tli != a[i]->end(); ++tli ) {
                    ASSERT( (*tli).value() == i+1, NULL );
                }
                a[i]->clear();
                ASSERT( static_cast<int>(a[i]->end() - a[i]->begin()) == 0, NULL );
            }

        }

        for (int i = 0; i < VALID_NUMBER_OF_KEYS; ++i) {
            delete a[i];
            a[i] = NULL;
        }

        ASSERT( int(construction_counter) == (FLOG_REPETITIONS*p+1)*VALID_NUMBER_OF_KEYS, NULL );
        ASSERT( int(destruction_counter) == (FLOG_REPETITIONS*p+1)*VALID_NUMBER_OF_KEYS, NULL );

        if (Verbose) printf("done\n");
    }

}

template <typename inner_container>
void 
flog_segmented_interator() {

    bool found_error = false;
    typedef typename inner_container::value_type T;
    typedef std::vector< inner_container > nested_vec;
    inner_container my_inner_container;
    my_inner_container.clear();
    nested_vec my_vec;

    // simple nested vector (neither level empty)
    const int maxval = 10;
    for(int i=0; i < maxval; i++) {
        my_vec.push_back(my_inner_container);
        for(int j = 0; j < maxval; j++) {
            my_vec.at(i).push_back((T)(maxval * i + j));
        }
    }

    tbb::internal::segmented_iterator<nested_vec, T> my_si(my_vec);

    T ii;
    for(my_si=my_vec.begin(), ii=0; my_si != my_vec.end(); ++my_si, ++ii) {
        if((*my_si) != ii) {
            found_error = true;
            if(Verbose) printf( "*my_si=%d\n", int(*my_si));
        }
    }

    // outer level empty
    my_vec.clear();
    for(my_si=my_vec.begin(); my_si != my_vec.end(); ++my_si) {
        found_error = true;
    }

    // inner levels empty
    my_vec.clear();
    for(int i =0; i < maxval; ++i) {
        my_vec.push_back(my_inner_container);
    }
    for(my_si = my_vec.begin(); my_si != my_vec.end(); ++my_si) {
        found_error = true;
    }

    // every other inner container is empty
    my_vec.clear();
    for(int i=0; i < maxval; ++i) {
        my_vec.push_back(my_inner_container);
        if(i%2) {
            for(int j = 0; j < maxval; ++j) {
                my_vec.at(i).push_back((T)(maxval * (i/2) + j));
            }
        }
    }
    for(my_si = my_vec.begin(), ii=0; my_si != my_vec.end(); ++my_si, ++ii) {
        if((*my_si) != ii) {
            found_error = true;
            if(Verbose) printf("*my_si=%d, ii=%d\n", (int)(*my_si), (int)ii);
        }
    }

    tbb::internal::segmented_iterator<nested_vec, const T> my_csi(my_vec);
    for(my_csi=my_vec.begin(), ii=0; my_csi != my_vec.end(); ++my_csi, ++ii) {
        if((*my_csi) != ii) {
            found_error = true;
            if(Verbose) printf( "*my_csi=%d\n", int(*my_csi));
        }
    }

    // outer level empty
    my_vec.clear();
    for(my_csi=my_vec.begin(); my_csi != my_vec.end(); ++my_csi) {
        found_error = true;
    }

    // inner levels empty
    my_vec.clear();
    for(int i =0; i < maxval; ++i) {
        my_vec.push_back(my_inner_container);
    }
    for(my_csi = my_vec.begin(); my_csi != my_vec.end(); ++my_csi) {
        found_error = true;
    }

    // every other inner container is empty
    my_vec.clear();
    for(int i=0; i < maxval; ++i) {
        my_vec.push_back(my_inner_container);
        if(i%2) {
            for(int j = 0; j < maxval; ++j) {
                my_vec.at(i).push_back((T)(maxval * (i/2) + j));
            }
        }
    }
    for(my_csi = my_vec.begin(), ii=0; my_csi != my_vec.end(); ++my_csi, ++ii) {
        if((*my_csi) != ii) {
            found_error = true;
            if(Verbose) printf("*my_csi=%d, ii=%d\n", (int)(*my_csi), (int)ii);
        }
    }


    if(found_error) printf("segmented_iterator failed\n");
}

template <typename Key, typename Val>
void
flog_segmented_iterator_map() {
   typedef typename std::map<Key, Val> my_map;
   typedef std::vector< my_map > nested_vec;
   my_map my_inner_container;
   my_inner_container.clear();
   nested_vec my_vec;
   my_vec.clear();
   bool found_error = false;

   // simple nested vector (neither level empty)
   const int maxval = 4;
   for(int i=0; i < maxval; i++) {
       my_vec.push_back(my_inner_container);
       for(int j = 0; j < maxval; j++) {
           my_vec.at(i).insert(std::make_pair<Key,Val>(maxval * i + j, 2*(maxval*i + j)));
       }
   }

   tbb::internal::segmented_iterator<nested_vec, std::pair<const Key, Val> > my_si(my_vec);
   Key ii;
   for(my_si=my_vec.begin(), ii=0; my_si != my_vec.end(); ++my_si, ++ii) {
       if(((*my_si).first != ii) || ((*my_si).second != 2*ii)) {
           found_error = true;
           if(Verbose) printf( "ii=%d, (*my_si).first=%d, second=%d\n",ii, int((*my_si).first), int((*my_si).second));
       }
   }

   tbb::internal::segmented_iterator<nested_vec, const std::pair<const Key, Val> > my_csi(my_vec);
   for(my_csi=my_vec.begin(), ii=0; my_csi != my_vec.end(); ++my_csi, ++ii) {
       if(((*my_csi).first != ii) || ((*my_csi).second != 2*ii)) {
           found_error = true;
           if(Verbose) printf( "ii=%d, (*my_csi).first=%d, second=%d\n",ii, int((*my_csi).first), int((*my_csi).second));
       }
   }
}

void
run_segmented_iterator_tests() {
   // only the following containers can be used with the segmented iterator.
    if(Verbose) printf("Running Segmented Iterator Tests\n");
   flog_segmented_interator<std::vector< int > >();
   flog_segmented_interator<std::vector< double > >();
   flog_segmented_interator<std::deque< int > >();
   flog_segmented_interator<std::deque< double > >();
   flog_segmented_interator<std::list< int > >();
   flog_segmented_interator<std::list< double > >();

   flog_segmented_iterator_map<int, int>();
   flog_segmented_iterator_map<int, double>(); 
}

int main(int argc, char *argv[]) {
   ParseCommandLine(argc, argv);
   run_segmented_iterator_tests();

   flog_key_creation_and_deletion();

   if (MinThread == 0) 
      run_serial_tests();

   if (MaxThread > 0)
      run_parallel_tests();

   printf("done\n");
   return 0;
}

