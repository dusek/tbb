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

#ifndef __TBB_enumerable_thread_specific_H
#define __TBB_enumerable_thread_specific_H

#include "concurrent_vector.h"
#include "cache_aligned_allocator.h"

#if _WIN32||_WIN64
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace tbb {

    //! enum for selecting between single key and key-per-instance versions
    enum ets_key_usage_type { ets_single_key, ets_key_per_instance };

    //! @cond
    namespace internal {
        
        //! Random access iterator for traversing the thread local copies.
        template< typename Container, typename Value >
        class enumerable_thread_specific_iterator 
#if defined(_WIN64) && defined(_MSC_VER) 
            // Ensure that Microsoft's internal template function _Val_type works correctly.
            : public std::iterator<std::random_access_iterator_tag,Value>
#endif /* defined(_WIN64) && defined(_MSC_VER) */
        {
            //! current position in the concurrent_vector 
        
            Container *my_container;
            typename Container::size_type my_index;
            mutable Value *my_value;
        
            template<typename C, typename T>
            friend enumerable_thread_specific_iterator<C,T> operator+( ptrdiff_t offset, 
                                                                       const enumerable_thread_specific_iterator<C,T>& v );
        
            template<typename C, typename T, typename U>
            friend bool operator==( const enumerable_thread_specific_iterator<C,T>& i, 
                                    const enumerable_thread_specific_iterator<C,U>& j );
        
            template<typename C, typename T, typename U>
            friend bool operator<( const enumerable_thread_specific_iterator<C,T>& i, 
                                   const enumerable_thread_specific_iterator<C,U>& j );
        
            template<typename C, typename T, typename U>
            friend ptrdiff_t operator-( const enumerable_thread_specific_iterator<C,T>& i, const enumerable_thread_specific_iterator<C,U>& j );
            
            template<typename C, typename U> 
            friend class enumerable_thread_specific_iterator;
        
            public:
        
            enumerable_thread_specific_iterator( const Container &container, typename Container::size_type index ) : 
                my_container(&const_cast<Container &>(container)), my_index(index), my_value(NULL) {}
        
            //! Default constructor
            enumerable_thread_specific_iterator() : my_container(NULL), my_index(0), my_value(NULL) {}
        
            template<typename U>
            enumerable_thread_specific_iterator( const enumerable_thread_specific_iterator<Container, U>& other ) :
                    my_container( other.my_container ), my_index( other.my_index), my_value( const_cast<Value *>(other.my_value) ) {}
        
            enumerable_thread_specific_iterator operator+( ptrdiff_t offset ) const {
                return enumerable_thread_specific_iterator(*my_container, my_index + offset);
            }
        
            enumerable_thread_specific_iterator &operator+=( ptrdiff_t offset ) {
                my_index += offset;
                my_value = NULL;
                return *this;
            }
        
            enumerable_thread_specific_iterator operator-( ptrdiff_t offset ) const {
                return enumerable_thread_specific_iterator( *my_container, my_index-offset );
            }
        
            enumerable_thread_specific_iterator &operator-=( ptrdiff_t offset ) {
                my_index -= offset;
                my_value = NULL;
                return *this;
            }
        
            Value& operator*() const {
                Value* value = my_value;
                if( !value ) {
                    value = my_value = &(*my_container)[my_index].value;
                }
                __TBB_ASSERT( value==&(*my_container)[my_index].value, "corrupt cache" );
                return *value;
            }
        
            Value& operator[]( ptrdiff_t k ) const {
               return (*my_container)[my_index + k].value;
            }
        
            Value* operator->() const {return &operator*();}
        
            enumerable_thread_specific_iterator& operator++() {
                ++my_index;
                my_value = NULL;
                return *this;
            }
        
            enumerable_thread_specific_iterator& operator--() {
                --my_index;
                my_value = NULL;
                return *this;
            }
        
            //! Post increment
            enumerable_thread_specific_iterator operator++(int) {
                enumerable_thread_specific_iterator result = *this;
                ++my_index;
                my_value = NULL;
                return result;
            }
        
            //! Post decrement
            enumerable_thread_specific_iterator operator--(int) {
                enumerable_thread_specific_iterator result = *this;
                --my_index;
                my_value = NULL;
                return result;
            }
        
            // STL support
            typedef ptrdiff_t difference_type;
            typedef Value value_type;
            typedef Value* pointer;
            typedef Value& reference;
            typedef std::random_access_iterator_tag iterator_category;
        };
        
        template<typename Container, typename T>
        enumerable_thread_specific_iterator<Container,T> operator+( ptrdiff_t offset, 
                                                                    const enumerable_thread_specific_iterator<Container,T>& v ) {
            return enumerable_thread_specific_iterator<Container,T>( v.my_container, v.my_index + offset );
        }
        
        template<typename Container, typename T, typename U>
        bool operator==( const enumerable_thread_specific_iterator<Container,T>& i, 
                         const enumerable_thread_specific_iterator<Container,U>& j ) {
            return i.my_index==j.my_index && i.my_container == j.my_container;
        }
        
        template<typename Container, typename T, typename U>
        bool operator!=( const enumerable_thread_specific_iterator<Container,T>& i, 
                         const enumerable_thread_specific_iterator<Container,U>& j ) {
            return !(i==j);
        }
        
        template<typename Container, typename T, typename U>
        bool operator<( const enumerable_thread_specific_iterator<Container,T>& i, 
                        const enumerable_thread_specific_iterator<Container,U>& j ) {
            return i.my_index<j.my_index;
        }
        
        template<typename Container, typename T, typename U>
        bool operator>( const enumerable_thread_specific_iterator<Container,T>& i, 
                        const enumerable_thread_specific_iterator<Container,U>& j ) {
            return j<i;
        }
        
        template<typename Container, typename T, typename U>
        bool operator>=( const enumerable_thread_specific_iterator<Container,T>& i, 
                         const enumerable_thread_specific_iterator<Container,U>& j ) {
            return !(i<j);
        }
        
        template<typename Container, typename T, typename U>
        bool operator<=( const enumerable_thread_specific_iterator<Container,T>& i, 
                         const enumerable_thread_specific_iterator<Container,U>& j ) {
            return !(j<i);
        }
        
        template<typename Container, typename T, typename U>
        ptrdiff_t operator-( const enumerable_thread_specific_iterator<Container,T>& i, 
                             const enumerable_thread_specific_iterator<Container,U>& j ) {
            return i.my_index-j.my_index;
        }

    template<typename SegmentedContainer, typename Value >
        class segmented_iterator
#if defined(_WIN64) && defined(_MSC_VER)
        : public std::iterator<std::input_iterator_tag, Value>
#endif
        {
            template<typename C, typename T, typename U>
            friend bool operator==(const segmented_iterator<C,T>& i, const segmented_iterator<C,U>& j);

            template<typename C, typename T, typename U>
            friend bool operator!=(const segmented_iterator<C,T>& i, const segmented_iterator<C,U>& j);
            
            template<typename C, typename U> 
            friend class segmented_iterator;

            public:

                segmented_iterator() {my_segcont = NULL;}

                segmented_iterator( const SegmentedContainer& _segmented_container ) : 
                    my_segcont(const_cast<SegmentedContainer*>(&_segmented_container)),
                    outer_iter(my_segcont->end()) { }

                ~segmented_iterator() {}

                typedef typename SegmentedContainer::iterator outer_iterator;
                typedef typename SegmentedContainer::value_type InnerContainer;
                typedef typename InnerContainer::iterator inner_iterator;

                // STL support
                typedef ptrdiff_t difference_type;
                typedef Value value_type;
                typedef typename SegmentedContainer::size_type size_type;
                typedef Value* pointer;
                typedef Value& reference;
                typedef std::input_iterator_tag iterator_category;

                // Copy Constructor
                template<typename U>
                segmented_iterator(const segmented_iterator<SegmentedContainer, U>& other) :
                    my_segcont(other.my_segcont),
                    outer_iter(other.outer_iter),
                    // can we assign a default-constructed iterator to inner if we're at the end?
                    inner_iter(other.inner_iter)
                {}

                // assignment
                template<typename U>
                segmented_iterator& operator=( const segmented_iterator<SegmentedContainer, U>& other) {
                    if(this != &other) {
                        my_segcont = other.my_segcont;
                        outer_iter = other.outer_iter;
                        if(outer_iter != my_segcont->end()) inner_iter = other.inner_iter;
                    }
                    return *this;
                }

                // allow assignment of outer iterator to segmented iterator.  Once it is
                // assigned, move forward until a non-empty inner container is found or
                // the end of the outer container is reached.
                segmented_iterator& operator=(const outer_iterator& new_outer_iter) {
                    __TBB_ASSERT(my_segcont != NULL, NULL);
                    // check that this iterator points to something inside the segmented container
                    for(outer_iter = new_outer_iter ;outer_iter!=my_segcont->end(); ++outer_iter) {
                        if( !outer_iter->empty() ) {
                            inner_iter = outer_iter->begin();
                            break;
                        }
                    }
                    return *this;
                }

                // pre-increment
                segmented_iterator& operator++() {
                    advance_me();
                    return *this;
                }

                // post-increment
                segmented_iterator operator++(int) {
                    segmented_iterator tmp = *this;
                    operator++();
                    return tmp;
                }

                bool operator==(const outer_iterator& other_outer) const {
                    __TBB_ASSERT(my_segcont != NULL, NULL);
                    return (outer_iter == other_outer &&
                            (outer_iter == my_segcont->end() || inner_iter == outer_iter->begin()));
                }

                bool operator!=(const outer_iterator& other_outer) const {
                    return !operator==(other_outer);

                }

                // (i)* RHS
                reference operator*() const {
                    __TBB_ASSERT(my_segcont != NULL, NULL);
                    __TBB_ASSERT(outer_iter != my_segcont->end(), "Dereferencing a pointer at end of container");
                    __TBB_ASSERT(inner_iter != outer_iter->end(), NULL); // should never happen
                    return *inner_iter;
                }

                // i->
                pointer operator->() const { return &operator*();}

            private:
                SegmentedContainer*             my_segcont;
                outer_iterator outer_iter;
                inner_iterator inner_iter;

                void advance_me() {
                    __TBB_ASSERT(my_segcont != NULL, NULL);
                    __TBB_ASSERT(outer_iter != my_segcont->end(), NULL); // not true if there are no inner containers
                    __TBB_ASSERT(inner_iter != outer_iter->end(), NULL); // not true if the inner containers are all empty.
                    ++inner_iter;
                    while(inner_iter == outer_iter->end() && ++outer_iter != my_segcont->end()) {
                        inner_iter = outer_iter->begin();
                    }
                }
        };    // segmented_iterator

        template<typename SegmentedContainer, typename T, typename U>
        bool operator==( const segmented_iterator<SegmentedContainer,T>& i, 
                         const segmented_iterator<SegmentedContainer,U>& j ) {
            if(i.my_segcont != j.my_segcont) return false;
            if(i.my_segcont == NULL) return true;
            if(i.outer_iter != j.outer_iter) return false;
            if(i.outer_iter == i.my_segcont->end()) return true;
            return i.inner_iter == j.inner_iter;
        }

        // !=
        template<typename SegmentedContainer, typename T, typename U>
        bool operator!=( const segmented_iterator<SegmentedContainer,T>& i, 
                         const segmented_iterator<SegmentedContainer,U>& j ) {
            return !(i==j);
        }

        //! The internal key manage used when using a single key across instances
        struct tls_single_key_manager_v4 {
            typedef tbb::concurrent_vector<void *>::size_type tls_key_t;
            static void __TBB_EXPORTED_FUNC create_key( tls_key_t &k);
            static void __TBB_EXPORTED_FUNC destroy_key( tls_key_t &k );
            static void __TBB_EXPORTED_FUNC set_tls( tls_key_t &k, void * value );
            static void * __TBB_EXPORTED_FUNC get_tls( tls_key_t &k );
        };

        // empty template for following specializations
        template<ets_key_usage_type et>
        struct tls_manager {};
        
        //! Struct that uses the internal key manager
        template <>
        struct tls_manager<ets_single_key> {
            typedef tls_single_key_manager_v4::tls_key_t tls_key_t;
            static inline void create_key( tls_key_t &k) { internal::tls_single_key_manager_v4::create_key(k); }
            static inline void destroy_key( tls_key_t &k ) { internal::tls_single_key_manager_v4::destroy_key(k); }
            static inline void set_tls( tls_key_t &k, void * value ) { 
                internal::tls_single_key_manager_v4::set_tls(k, value); 
            }
            static inline void * get_tls( tls_key_t &k ) { return internal::tls_single_key_manager_v4::get_tls(k); }
        };

        
        //! Struct to use native TLS support directly
        template <>
        struct tls_manager <ets_key_per_instance> {
#if _WIN32||_WIN64
            typedef DWORD tls_key_t;
            static inline void create_key( tls_key_t &k) { k = TlsAlloc(); }
            static inline void destroy_key( tls_key_t &k) { TlsFree(k); }
            static inline void set_tls( tls_key_t &k, void * value) { TlsSetValue(k, (LPVOID)value); }
            static inline void * get_tls( tls_key_t &k ) { return (void *)TlsGetValue(k); }
#else
            typedef pthread_key_t tls_key_t;
            static inline void create_key( tls_key_t &k) { pthread_key_create(&k, NULL); }
            static inline void destroy_key( tls_key_t &k) { pthread_key_delete(k); }
            static inline void set_tls( tls_key_t &k, void * value) { pthread_setspecific(k, value); }
            static inline void * get_tls( tls_key_t &k ) { return pthread_getspecific(k); }
#endif
        };

    } // namespace internal
    //! @endcond

    //! The thread local class template
    template <typename T, 
              typename Allocator=cache_aligned_allocator<T>, 
              ets_key_usage_type ETS_key_type=ets_single_key > 
    class enumerable_thread_specific { 
    
        typedef internal::tls_manager< ETS_key_type > my_tls_manager;

        //! The padded elements; padded to avoid false sharing
        struct padded_element {
            T value;
            char padding[ ( (sizeof(T) - 1) / internal::NFS_MaxLineSize + 1 ) * internal::NFS_MaxLineSize - sizeof(T) ];
            padded_element(const T &v) : value(v) {}
            padded_element() {}
        };
    
        //! A generic range, used to create range objects from the iterators
        template<typename I>
        class generic_range_type: public blocked_range<I> {
        public:
            typedef T value_type;
            typedef T& reference;
            typedef const T& const_reference;
            typedef I iterator;
            typedef ptrdiff_t difference_type;
            generic_range_type( I begin_, I end_, size_t grainsize = 1) : blocked_range<I>(begin_,end_,grainsize) {} 
            template<typename U>
            generic_range_type( const generic_range_type<U>& r) : blocked_range<I>(r.begin(),r.end(),r.grainsize()) {} 
            generic_range_type( generic_range_type& r, split ) : blocked_range<I>(r,split()) {}
        };
    
        typedef typename Allocator::template rebind< padded_element >::other padded_allocator_type;
        typedef tbb::concurrent_vector< padded_element, padded_allocator_type > internal_collection_type;
    
        typename my_tls_manager::tls_key_t my_key;
        padded_element my_exemplar;
        internal_collection_type my_locals;
    
    public:
    
        //! Basic types
        typedef Allocator allocator_type;
        typedef T value_type;
        typedef T& reference;
        typedef const T& const_reference;
        typedef T* pointer;
        typedef typename internal_collection_type::size_type size_type;
        typedef typename internal_collection_type::difference_type difference_type;
    
        // Iterator types
        typedef typename internal::enumerable_thread_specific_iterator< internal_collection_type, value_type > iterator;
        typedef typename internal::enumerable_thread_specific_iterator< internal_collection_type, const value_type > const_iterator;

        // Parallel range types
        typedef generic_range_type< iterator > range_type;
        typedef generic_range_type< const_iterator > const_range_type;
    
        //! Default constructor, which leads to default construction of local copies
        enumerable_thread_specific() { 
            my_tls_manager::create_key(my_key); 
        }
    
        //! Constuction with exemplar, which leads to copy construction of local copies
        enumerable_thread_specific(const T &_exemplar) : my_exemplar(_exemplar) {
            my_tls_manager::create_key(my_key); 
        }
    
        //! Destructor
        ~enumerable_thread_specific() { 
            my_tls_manager::destroy_key(my_key); 
        }
      
    
        //! Returns reference to calling thread's local copy, creating one if necessary
        reference local()  {
            if ( pointer local_ptr = static_cast< pointer >(my_tls_manager::get_tls(my_key)) ) {
               return *local_ptr;
            } else {
                typename internal_collection_type::size_type local_index = my_locals.push_back( my_exemplar );
                reference local_ref = my_locals[local_index].value;
                my_tls_manager::set_tls( my_key, static_cast<void *>(&local_ref) );
                return local_ref;
            }
        }

        //! Get the number of local copies
        size_type size() const { return my_locals.size(); }
    
        //! true if there have been no local copies created
        bool empty() const { return my_locals.empty(); }
    
        //! begin iterator
        iterator begin() { return iterator( my_locals, 0 ); }
        //! end iterator
        iterator end() { return iterator(my_locals, my_locals.size() ); }
    
        //! begin const iterator
        const_iterator begin() const { return const_iterator(my_locals, 0); }
    
        //! end const iterator
        const_iterator end() const { return const_iterator(my_locals, my_locals.size()); }

        //! Get range for parallel algorithms
        range_type range( size_t grainsize=1 ) { return range_type( begin(), end(), grainsize ); } 
        
        //! Get const range for parallel algorithms
        const_range_type range( size_t grainsize=1 ) const { return const_range_type( begin(), end(), grainsize ); }
    
        //! Destroys local copies
        void clear() {
            my_locals.clear();
            my_tls_manager::destroy_key(my_key);
            my_tls_manager::create_key(my_key); 
        }
    
    };

    template< typename Container >
    class flattened2d {

        // This intermediate typedef is to address issues with VC7.1 compilers
        typedef typename Container::value_type conval_type;

    public:

        //! Basic types
        typedef typename conval_type::size_type size_type;
        typedef typename conval_type::difference_type difference_type;
        typedef typename conval_type::allocator_type allocator_type;
        typedef typename conval_type::value_type value_type;
        typedef typename conval_type::reference reference;
        typedef typename conval_type::const_reference const_reference;
        typedef typename conval_type::pointer pointer;
        typedef typename conval_type::const_pointer const_pointer;

        typedef typename internal::segmented_iterator<Container, value_type> iterator;
        typedef typename internal::segmented_iterator<Container, const value_type> const_iterator;

        flattened2d( const Container &c, typename Container::const_iterator b, typename Container::const_iterator e ) : 
            my_container(const_cast<Container*>(&c)), my_begin(b), my_end(e) { }

        flattened2d( const Container &c ) : 
            my_container(const_cast<Container*>(&c)), my_begin(c.begin()), my_end(c.end()) { }

        iterator begin() { return iterator(*my_container) = my_begin; }
        iterator end() { return iterator(*my_container) = my_end; }
        const_iterator begin() const { return const_iterator(*my_container) = my_begin; }
        const_iterator end() const { return const_iterator(*my_container) = my_end; }

        size_type size() const {
            size_type tot_size = 0;
            for(typename Container::const_iterator i = my_begin; i != my_end; ++i) {
                tot_size += i->size();
            }
            return tot_size;
        }

    private:

        Container *my_container;
        typename Container::const_iterator my_begin;
        typename Container::const_iterator my_end;

    };

    template <typename Container>
    flattened2d<Container> flatten2d(const Container &c, const typename Container::const_iterator b, const typename Container::const_iterator e) {
        return flattened2d<Container>(c, b, e);
    }

    template <typename Container>
    flattened2d<Container> flatten2d(const Container &c) {
        return flattened2d<Container>(c);
    }

} // namespace tbb

#endif
