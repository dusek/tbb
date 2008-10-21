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

#ifndef __TBB_enumerable_thread_specific_H
#define __TBB_enumerable_thread_specific_H

#include "tbb/concurrent_vector.h"
#include "tbb/cache_aligned_allocator.h"

#if _WIN32||_WIN64
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace tbb {

//! The thread local class template
template <typename T, typename Allocator=cache_aligned_allocator<T> > 
class enumerable_thread_specific { 

#if _WIN32||_WIN64
    typedef DWORD tls_key_t;
    static inline void create_key( tls_key_t &k) { k = TlsAlloc(); }
    static inline void destroy_key( tls_key_t &k) { TlsFree(k); }
    static inline void set_tls( tls_key_t &k, T * value) { TlsSetValue(k, (LPVOID)value); }
    static inline T * get_tls( tls_key_t &k ) { return (T *)TlsGetValue(k); }
#else
    typedef pthread_key_t tls_key_t;
    static inline void create_key( tls_key_t &k) { pthread_key_create(&k, NULL); }
    static inline void destroy_key( tls_key_t &k) { pthread_key_delete(k); }
    static inline void set_tls( tls_key_t &k, T * value) { pthread_setspecific(k, (void *)value); }
    static inline T * get_tls( tls_key_t &k ) { return (T *) pthread_getspecific(k); }
#endif

    tls_key_t my_key;
    T my_exemplar;

    typedef tbb::concurrent_vector< T, Allocator > internal_collection_type;
    internal_collection_type my_locals;

public:

    //! Basic types
    typedef Allocator allocator_type;
    typedef T value_type;
    typedef T& reference;
    typedef T* pointer;
    typedef typename internal_collection_type::size_type size_type;
    typedef typename internal_collection_type::difference_type difference_type;

    // Parallel range types
    typedef typename internal_collection_type::range_type range_type;
    typedef typename internal_collection_type::const_range_type const_range_type;

    // Iterator types
    typedef typename internal_collection_type::iterator iterator;
    typedef typename internal_collection_type::const_iterator const_iterator;

    //! Default constructor, which leads to default construction of local copies
    enumerable_thread_specific() { 
        create_key(my_key); 
    }

    //! Constuction with exemplar, which leads to copy construction of local copies
    enumerable_thread_specific(const T &_exemplar) : my_exemplar(_exemplar) {
        create_key(my_key); 
    }

    //! Destructor
   ~enumerable_thread_specific() { 
        destroy_key(my_key); 
   }

    //! Returns reference to calling thread's local copy, creating one if necessary
    reference local()  {
        if ( pointer local_ptr = get_tls(my_key) ) {
           return *local_ptr;
        } else {
            typename internal_collection_type::size_type local_index = my_locals.push_back(my_exemplar);
            reference local_ref = my_locals[local_index];
            set_tls( my_key, &local_ref );
            return local_ref;
        }
    }

    //! Get the number of local copies
    size_type size() const { return my_locals.size(); }

    //! true if there have been no local copies created
    bool empty() const { return my_locals.empty(); }

    //! Get range for parallel algorithms
    range_type range( size_t grainsize=1 ) { return my_locals.range(grainsize); }
    
    //! Get const range for parallel algorithms
    const_range_type range( size_t grainsize=1 ) const { return my_locals.range(grainsize); }

    //! begin iterator
    iterator begin() { return my_locals.begin(); }
    //! end iterator
    iterator end() { return my_locals.end(); }
    //! begin const iterator
    const_iterator begin() const { return my_locals.begin(); }
    //! end const iterator
    const_iterator end() const { return my_locals.end(); }

    //! Destroys local copies
    void clear() {
        my_locals.clear();
        destroy_key(my_key);
        create_key(my_key); 
    }

};

}

#endif
