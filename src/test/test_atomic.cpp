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

// Put tbb/atomic.h first, so if it is missing a prerequisite header, we find out about it.
// The tests here do *not* test for atomicity, just serial correctness. */

#include "tbb/atomic.h"
#include "harness_assert.h"

//! Structure that holds an atomic<T> and some guard bytes around it.
template<typename T>
struct TestStruct {
    T prefix;
    tbb::atomic<T> counter;
    T suffix;
    TestStruct( T i ) : prefix(T(0x1234)), suffix(T(0x5678)) {
        counter = i;
        ASSERT( sizeof(*this)==3*sizeof(T), NULL );
    }
    ~TestStruct() {
        // Check for writes outside the counter.
        ASSERT( prefix==T(0x1234), NULL );
        ASSERT( suffix==T(0x5678), NULL );
    }
};

//! Test compare_and_swap template members of class atomic<T> for memory_semantics=M
template<typename T,tbb::memory_semantics M>
void TestCompareAndSwapAcquireRelease( T i, T j, T k ) {
    ASSERT( i!=k, "values must be distinct" ); 
    // Test compare_and_swap that should fail
    TestStruct<T> x(i);
    T old = x.counter.template compare_and_swap<M>( j, k );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==i, "old value not retained" );
    // Test compare and swap that should suceed
    old = x.counter.template compare_and_swap<M>( j, i );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==j, "value not updated?" );
}

//! i, j, k must be different values
template<typename T>
void TestCompareAndSwap( T i, T j, T k ) {
    ASSERT( i!=k, "values must be distinct" ); 
    // Test compare_and_swap that should fail
    TestStruct<T> x(i);
    T old = x.counter.compare_and_swap( j, k );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==i, "old value not retained" );
    // Test compare and swap that should suceed
    old = x.counter.compare_and_swap( j, i );
    ASSERT( old==i, NULL );
    if( x.counter==i ) {
        ASSERT( x.counter==j, "value not updated?" );
    } else {    
        ASSERT( x.counter==j, "value trashed" );
    }
    TestCompareAndSwapAcquireRelease<T,tbb::acquire>(i,j,k);
    TestCompareAndSwapAcquireRelease<T,tbb::release>(i,j,k);
}

//! memory_semantics variation on TestFetchAndStore
template<typename T, tbb::memory_semantics M>
void TestFetchAndStoreAcquireRelease( T i, T j ) {
    ASSERT( i!=j, "values must be distinct" ); 
    TestStruct<T> x(i);
    T old = x.counter.template fetch_and_store<M>( j );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==j, NULL );
}

//! i and j must be different values
template<typename T>
void TestFetchAndStore( T i, T j ) {
    ASSERT( i!=j, "values must be distinct" ); 
    TestStruct<T> x(i);
    T old = x.counter.fetch_and_store( j );
    ASSERT( old==i, NULL );
    ASSERT( x.counter==j, NULL );
    TestFetchAndStoreAcquireRelease<T,tbb::acquire>(i,j);
    TestFetchAndStoreAcquireRelease<T,tbb::release>(i,j);
}

//! Test fetch_and_add members of class atomic<T> for memory_semantics=M
template<typename T,tbb::memory_semantics M>
void TestFetchAndAddAcquireRelease( T i ) {
    TestStruct<T> x(i);
    T actual;
    T expected = i;

    // Test fetch_and_add member template
    for( int j=0; j<10; ++j ) {
        actual = x.counter.fetch_and_add(j);
        ASSERT( actual==expected, NULL );
        expected += j;
    }
    for( int j=0; j<10; ++j ) {
        actual = x.counter.fetch_and_add(-j);
        ASSERT( actual==expected, NULL );
        expected -= j;
    }

    // Test fetch_and_increment member template
    ASSERT( x.counter==i, NULL );
    actual = x.counter.template fetch_and_increment<M>();
    ASSERT( actual==i, NULL );
    ASSERT( x.counter==T(i+1), NULL );

    // Test fetch_and_decrement member template
    actual = x.counter.template fetch_and_decrement<M>();
    ASSERT( actual==T(i+1), NULL );
    ASSERT( x.counter==i, NULL );
}

//! Test fetch_and_add and related operators
template<typename T>
void TestFetchAndAdd( T i ) {
    TestStruct<T> x(i);
    T value;
    value = ++x.counter;
    ASSERT( value==T(i+1), NULL );
    value = x.counter++;
    ASSERT( value==T(i+1), NULL );
    value = x.counter--;
    ASSERT( value==T(i+2), NULL );
    value = --x.counter;
    ASSERT( value==i, NULL );
    T actual;
    T expected = i;
    for( int j=-100; j<=100; ++j ) {
        expected += j;
        actual = x.counter += j;
        ASSERT( actual==expected, NULL );
    }
    for( int j=-100; j<=100; ++j ) {
        expected -= j;
        actual = x.counter -= j;
        ASSERT( actual==expected, NULL );
    }
    // Test fetch_and_increment
    ASSERT( x.counter==i, NULL );
    actual = x.counter.fetch_and_increment();
    ASSERT( actual==i, NULL );
    ASSERT( x.counter==T(i+1), NULL );

    // Test fetch_and_decrement
    actual = x.counter.fetch_and_decrement();
    ASSERT( actual==T(i+1), NULL );
    ASSERT( x.counter==i, NULL );
    x.counter = i;
    ASSERT( x.counter==i, NULL );

    TestFetchAndAddAcquireRelease<T,tbb::acquire>(i);
    TestFetchAndAddAcquireRelease<T,tbb::release>(i);
}

void TestFetchAndAdd( void* ) {
    // There are no fetch-and-add operations on a void*.
}

void TestFetchAndAdd( bool ) {
    // There are no fetch-and-add operations on a bool.
}

template<typename T>
void TestConst( T i ) { 
    // Try const 
    const TestStruct<T> x(i);
    ASSERT( reinterpret_cast<const T&>(x.counter)==i, "write to atomic<T> broken?" );
    ASSERT( x.counter==i, "read of atomic<T> broken?" );
}

template<typename T>
void TestOperations( T i, T j, T k ) {
    TestConst(i);
    TestCompareAndSwap(i,j,k);
    TestFetchAndStore(i,k);    // Pass i,k instead of i,j, because callee requires two distinct values.
    TestFetchAndAdd(i);
}

template<typename T>
void TestLoadAndStoreFences( const char* name );

bool MemoryFenceError;

template<typename T>
struct AlignmentChecker {
    char c;
    tbb::atomic<T> i;
};

#include "harness.h"

#if _MSC_VER && !defined(__INTEL_COMPILER)
#pragma warning( push )
// unary minus operator applied to unsigned type, result still unsigned
#pragma warning( disable: 4146 )
#endif /* _MSC_VER && !defined(__INTEL_COMPILER) */

/** T is an integral type. */
template<typename T>
void TestAtomicInteger( const char* name ) {
    if( Verbose )
        printf("testing atomic<%s>\n",name);
#if __linux__ && __TBB_x86_32 && __GNUC__==3 && __GNUC_MINOR__==3
    // gcc 3.3 has known problem for 32-bit Linux, so only warn if there is a problem.
    if( sizeof(T)==8 ) {
        if( sizeof(AlignmentChecker<T>)!=2*sizeof(tbb::atomic<T>) ) {
            printf("Warning: alignment for atomic<%s> is wrong (known issue with gcc 3.3 for IA32)\n",name);
        }
    } else
#endif /* __linux__ && __GNUC__ */
    ASSERT( sizeof(AlignmentChecker<T>)==2*sizeof(tbb::atomic<T>), NULL );
    TestOperations<T>(0L,T(-T(1)),T(1));
    for( int k=0; k<int(sizeof(long))*8-1; ++k ) {
        TestOperations<T>(T(1L<<k),T(~(1L<<k)),T(1-(1L<<k)));
        TestOperations<T>(T(-1L<<k),T(~(-1L<<k)),T(1-(-1L<<k)));
    }
    TestLoadAndStoreFences<T>( name );
}
#if _MSC_VER && !defined(__INTEL_COMPILER)
#pragma warning( pop )
#endif /* _MSC_VER && !defined(__INTEL_COMPILER) */


template<typename T>
struct Foo {
    T x, y, z;
};


template<typename T>
void TestIndirection() {
    Foo<T> item;
    tbb::atomic<Foo<T>*> pointer;
    pointer = &item;
    for( int k=-10; k<=10; ++k ) {
        // Test various syntaxes for indirection to fields with non-zero offset.   
        T value1, value2;
        for( size_t j=0; j<sizeof(T); ++j ) {
            *(char*)&value1 = char(k^j);
            *(char*)&value2 = char(k^j*j);
        }
        pointer->y = value1;
        (*pointer).z = value2;
        T result1 = (*pointer).y;
        T result2 = pointer->z;
        ASSERT( memcmp(&value1,&result1,sizeof(T))==0, NULL );
        ASSERT( memcmp(&value2,&result2,sizeof(T))==0, NULL );
    }
}

template<typename T>
void TestAtomicPointer() {
    T array[1000];
    TestOperations<T*>(&array[500],&array[250],&array[750]);
    TestOperations<void*>(&array[500],&array[250],&array[750]);
    TestIndirection<T>();
    TestLoadAndStoreFences<T*>( "pointer" );
}

// Specialization for void*
template<>
void TestAtomicPointer<void*>() {
    void* array[1000];
    TestOperations<void*>(&array[500],&array[250],&array[750]);
    TestLoadAndStoreFences<void*>( "pointer" );
}

void TestAtomicBool() {
    TestOperations<bool>(true,true,false);
    TestOperations<bool>(false,false,true);
    TestLoadAndStoreFences<bool>( "bool" );
}

template<unsigned N>
class ArrayElement {
    char item[N];
};

int main( int argc, char* argv[] ) {
    ParseCommandLine( argc, argv );
#if defined(__INTEL_COMPILER)||!defined(_MSC_VER)||_MSC_VER>=1400
    TestAtomicInteger<unsigned long long>("unsigned long long");
    TestAtomicInteger<long long>("long long");
#else
    printf("Warning: atomic<64-bits> not tested because of known problem in Microsoft compiler\n");
#endif /*defined(__INTEL_COMPILER)||!defined(_MSC_VER)||_MSC_VER>=1400 */
    TestAtomicInteger<unsigned long>("unsigned long");
    TestAtomicInteger<long>("long");
    TestAtomicInteger<unsigned int>("unsigned int");
    TestAtomicInteger<int>("int");
    TestAtomicInteger<unsigned short>("unsigned short");
    TestAtomicInteger<short>("short");
    TestAtomicInteger<signed char>("signed char");
    TestAtomicInteger<unsigned char>("unsigned char");
    TestAtomicInteger<char>("char");
    TestAtomicInteger<wchar_t>("wchar_t");
    TestAtomicInteger<size_t>("size_t");
    TestAtomicInteger<ptrdiff_t>("ptrdiff_t");
    TestAtomicPointer<ArrayElement<1> >();
    TestAtomicPointer<ArrayElement<2> >();
    TestAtomicPointer<ArrayElement<3> >();
    TestAtomicPointer<ArrayElement<4> >();
    TestAtomicPointer<ArrayElement<5> >();
    TestAtomicPointer<ArrayElement<6> >();
    TestAtomicPointer<ArrayElement<7> >();
    TestAtomicPointer<ArrayElement<8> >();
    TestAtomicPointer<void*>();
    TestAtomicBool();
    ASSERT( !MemoryFenceError, NULL );
    printf("done\n");
    return 0;
}

// Portions dependent on blocked_range.h are down here, so that preceding tests do not
// accidentally depend upon it.

#include "tbb/blocked_range.h"

template<typename T>
struct FlagAndMessage {
    //! 0 if message not set yet, 1 if message is set.
    tbb::atomic<T> flag;
    /** Force flag and message to be on distinct cache lines for machines with cache line size <= 4096 bytes */
    char pad[4096/sizeof(T)];
    //! Non-zero if message is ready
    T message;    
};

// A special template function used for summation.
// Actually it is only necessary because of its specialization for void*
template<typename T>
T special_sum(intptr_t arg1, intptr_t arg2) {
    return (T)((T)arg1 + arg2);
}

// The specialization for void* is required
// because pointer arithmetic (+) is impossible with void*
template<>
void* special_sum<void*>(intptr_t arg1, intptr_t arg2) {
    return (void*)(arg1 + arg2);
}

// The specialization for bool is required to shut up gratuitous compiler warnings,
// because some compilers warn about casting int to bool.
template<>
bool special_sum<bool>(intptr_t arg1, intptr_t arg2) {
    return ((arg1!=0) + arg2)!=0;
}

volatile int One = 1;
 
template<typename T>
class HammerLoadAndStoreFence {
    FlagAndMessage<T>* fam;
    const int n;
    const int p;
    const int trial;
    const char* name;
    mutable T accum;
public:
    HammerLoadAndStoreFence( FlagAndMessage<T>* fam_, int n_, int p_, const char* name_, int trial_ ) : fam(fam_), n(n_), p(p_), trial(trial_), name(name_) {}
    void operator()( const tbb::blocked_range<int>& range ) const {
        int one = One;
        int k = range.begin();
        FlagAndMessage<T>* s = fam+k;
        FlagAndMessage<T>* s_next = fam + (k+1)%p;
        for( int i=0; i<n; ++i ) {
            // The inner for loop is a spin-wait loop, which is normally considered very bad style. 
            // But we must use it here because we are interested in examining subtle hardware effects.
            for(unsigned short cnt=1; ; ++cnt) {
                if( !cnt ) // to help 1-core systems complete the test, yield every 2^16 iterations
                    __TBB_Yield();
                // Compilers typically generate non-trivial sequence for division by a constant.
                // The expression here is dependent on the loop index i, so it cannot be hoisted.
#define COMPLICATED_ZERO (i*(one-1)/100)
                // Read flag and then the message
                T flag, message;
                if( trial&1 ) { 
                    // COMPLICATED_ZERO here tempts compiler to hoist load of message above reading of flag.
                    flag = (s+COMPLICATED_ZERO)->flag;
                    message = s->message;
                } else {
                    flag = s->flag;
                    message = s->message;
                }
                if( flag ) {
                    if( flag!=(T)-1 ) {
                        printf("ERROR: flag!=(T)-1 k=%d i=%d trial=%x type=%s (atomicity problem?)\n", k, i, trial, name );
                        MemoryFenceError = true;
                    } 
                    if( message!=(T)-1 ) {
                        printf("ERROR: message!=(T)-1 k=%d i=%d trial=%x type=%s (memory fence problem?)\n", k, i, trial, name );
                        MemoryFenceError = true;
                    }
                    s->message = 0; 
                    s->flag = 0;
                    // Set message and then the flag
                    if( trial&2 ) {
                        // COMPLICATED_ZERO here tempts compiler to sink store below setting of flag
                        s_next->message = special_sum<T>(-1, COMPLICATED_ZERO);
                        s_next->flag = (T)-1;
                    } else {
                        s_next->message = (T)-1;
                        s_next->flag = (T)-1;
                    }
                    break;
                } else {
                    // Force compiler to use message anyway, so it cannot sink read of s->message below the if.
                    accum = message;
                }
            }
        }
    }
};

//! Test that atomic<T> has acquire semantics for loads and release semantics for stores.
/** Test performs round-robin passing of message among p processors, 
    where p goes from MinThread to MaxThread. */
template<typename T>
void TestLoadAndStoreFences( const char* name ) {
    for( int p=MinThread<2 ? 2 : MinThread; p<=MaxThread; ++p ) {
        FlagAndMessage<T>* fam = new FlagAndMessage<T>[p];
        // Each of four trials excercise slightly different expresion pattern within the test.
        // See occurrences of COMPLICATED_ZERO for details. 
        for( int trial=0; trial<4; ++trial ) {
            memset( fam, 0, p*sizeof(FlagAndMessage<T>) );
            fam->message = (T)-1;
            fam->flag = (T)-1;
            NativeParallelFor( tbb::blocked_range<int>(0,p,1), HammerLoadAndStoreFence<T>( fam, 100, p, name, trial ) );
            for( int k=0; k<p; ++k ) {
                ASSERT( fam[k].message==(k==0 ? (T)-1 : 0), "incomplete round-robin?" ); 
                ASSERT( fam[k].flag==(k==0 ? (T)-1 : 0), "incomplete round-robin?" ); 
            }
        }
        delete[] fam;
    }
}
