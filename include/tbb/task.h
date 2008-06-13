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

#ifndef __TBB_task_H
#define __TBB_task_H

#include "tbb_stddef.h"

#if __TBB_EXCEPTIONS
#include <exception>
#endif /* __TBB_EXCEPTIONS */

namespace tbb {

class task;
class task_list;
#if __TBB_EXCEPTIONS
class asynch_context;
#endif /* __TBB_EXCEPTIONS */

//! @cond INTERNAL
namespace internal {

#if __TBB_EXCEPTIONS
    class exception_data;
    template<typename T> class CustomScheduler;
#endif /* __TBB_EXCEPTIONS */

    class scheduler {
    public:
        //! For internal use only
        virtual void spawn( task& first, task*& next ) = 0;

        //! For internal use only
        virtual void wait_for_all( task& parent, task* child ) = 0;

        //! For internal use only
        virtual void spawn_root_and_wait( task& first, task*& next ) = 0;

        //! Pure virtual destructor;
        //  Have to have it just to shut up overzealous compilation warnings
        virtual ~scheduler() = 0;
    };

    //! A reference count
    /** Should always be non-negative.  A signed type is used so that underflow can be detected. */
    typedef intptr reference_count;

    //! An id as used for specifying affinity.
    typedef unsigned short affinity_id;

    class allocate_root_proxy {
    public:
        static task& allocate( size_t size );
        static void free( task& );
    };

#if __TBB_EXCEPTIONS
    class allocate_root_with_context_proxy {
        asynch_context& my_context;
    public:
        allocate_root_with_context_proxy ( asynch_context& ctx ) : my_context(ctx) {}
        task& allocate( size_t size ) const;
        void free( task& ) const;
    };
#endif /* __TBB_EXCEPTIONS */

    class allocate_continuation_proxy {
    public:
        task& allocate( size_t size ) const;
        void free( task& ) const;
    };

    class allocate_child_proxy {
    public:
        task& allocate( size_t size ) const;
        void free( task& ) const;
    };

    class allocate_additional_child_of_proxy {
        task& self;
        task& parent;
    public:
        allocate_additional_child_of_proxy( task& self_, task& parent_ ) : self(self_), parent(parent_) {}
        task& allocate( size_t size ) const;
        void free( task& ) const;
    };

    //! Memory prefix to a task object.
    /** This class is internal to the library.
        Do not reference it directly, except within the library itself.
        Fields are ordered in way that preserves backwards compatibility and yields 
        good packing on typical 32-bit and 64-bit platforms.
        @ingroup task_scheduling */
    class task_prefix {
    private:
        friend class tbb::task;
        friend class tbb::task_list;
        friend class internal::scheduler;
        friend class internal::allocate_root_proxy;
        friend class internal::allocate_child_proxy;
        friend class internal::allocate_continuation_proxy;
        friend class internal::allocate_additional_child_of_proxy;

#if __TBB_EXCEPTIONS
        //! Shared context that is used to communicate asynchronous state changes
        /** Currently it is used to broadcast cancellation requests generated both 
            by users and as the result of unhandled exceptions in the task::execute()
            methods. */
        asynch_context  *context;
#endif /* __TBB_EXCEPTIONS */
        
        //! The scheduler that allocated the task, or NULL if task is big.
        /** Small tasks are pooled by the scheduler that allocated the task.
            If a scheduler needs to free a small task allocated by another scheduler,
            it returns the task to that other scheduler.  This policy avoids
            memory space blowup issues for memory allocators that allocate from 
            thread-specific pools. */
        scheduler* origin;

        //! scheduler that owns the task.
        scheduler* owner;

        //! task whose reference count includes me.
        /** In the "blocking style" of programming, this field points to the parent task.
            In the "continuation-passing style" of programming, this field points to the
            continuation of the parent. */
        tbb::task* parent;

        //! Reference count used for synchronization.
        /** In the "continuation-passing style" of programming, this field is
            the difference of the number of allocated children minus the
            number of children that have completed.
            In the "blocking style" of programming, this field is one more than the difference. */
        reference_count ref_count;

        //! Scheduling depth
        int depth;

        //! A task::state_type, stored as a byte for compactness.
        /** This state is exposed to users via method task::state(). */
        unsigned char state;

        //! Miscellaneous state that is not directly visible to users, stored as a byte for compactness.
        /** 0x0 -> version 1.0 task
            0x1 -> version 3.0 task
            0x2 -> task_proxy
            0x40 -> task has live ref_count */
        unsigned char extra_state;

        affinity_id affinity;

        //! "next" field for list of task
        tbb::task* next;

        //! task corresponding to this task_prefix.
        tbb::task& task() {return *reinterpret_cast<tbb::task*>(this+1);}
    };

} // namespace internal
//! @endcond

#if __TBB_EXCEPTIONS

class unhandled_exception : public std::exception 
{
public:
    unhandled_exception ( internal::exception_data* data ) : my_exception_data(data) {}

    ~unhandled_exception () throw();

    const char* name() const throw();

    /*override*/
    const char* what() const throw();

private:
    friend class internal::scheduler;

    internal::exception_data* my_exception_data;
};

//! Contains information about events that can happen asynchronously w.r.t. the tasks associated with it 
/** @ingroup task_scheduling 
    Currently the context services cancellation requests from user code, and unhandled 
    exceptions intercepted during tasks execution. Intercepting an exception results 
    in fact in generating internal cancellation requests. 

    The context is associated with one or more root tasks and defines the cancellation 
    group that includes all the children of the corresponding root task(s). Association 
    is established when a context object is passed as an argument to the task::allocate_root()
    method. See asynch_context::asynch_context for more details.
    
    The context can be bound to another one, and other contexts can be bound to it,
    forming a tree-like structure: parent -> this -> children. Arrows here designate
    cancellation propagation direction: if a task in a cancellation group is canceled
    all the other tasks in this group and groups bound to it (as children) get canceled too.*/
class asynch_context : internal::no_copy
{
    typedef unsigned char  mutex_type;

    //! Pointer to the context of the parent cancellation group. NULL for isolated contexts.
    asynch_context  *my_parent;

    //! Pointer to the context of the first subordinate cancellation group.
    asynch_context  *my_first_child;

    //! Pointer to the context of my_parent's previous subordinate cancellation group.
    /** If this == my_parent->my_first_child, then my_prev_sibling == my_parent.
        This allows to simplify locking in binding/unbinding routines. **/
    asynch_context  *my_prev_sibling;

    //! Pointer to the context of my_parent's next subordinate cancellation group.
    asynch_context  *my_next_sibling;

    internal::exception_data    *my_exception_data;

    volatile internal::intptr   my_cancellation_requested;

    mutex_type  my_mutex;

    void*   my_reserved1;   //?? Do we need more reserved space?

public:
    typedef internal::intptr kind_t;

    static const kind_t isolated = internal::intptr(0);
    static const kind_t bound = internal::intptr(-1);

    //! Default & binding constructor.
    /** By default a bound context is created. That is this context will be bound 
        (as child) to the context of the task calling task::allocate_root(this_context) 
        method. Cancellation requests passed to the parent context are propagated
        to all the contexts bound to it.

        If asynch_context::isolated is used as the argument, then the tasks associated
        with this context will never be affected by events in any other context. */
    asynch_context ( kind_t relation_with_parent = bound )
        : my_parent(reinterpret_cast<asynch_context*>(relation_with_parent))
        , my_first_child(NULL)
        , my_prev_sibling(NULL)
        , my_next_sibling(NULL)
        , my_exception_data(NULL)
        , my_cancellation_requested(false)
        , my_mutex(0)
    {}

    ~asynch_context ();

    //! Forcefully reinitializes context object after an algorithm it was used with finished.
    /** Because the method assumes that the all the tasks that used to be associated with 
        this context have already finished, you must be extremely careful to not invalidate 
        the context while it is still in use somewhere in the task hierarchy.
        
        It is assumed that this method is not used concurrently!

        The method does not change the kind of the context and its parent if the latter is set.
        \todo Decide if we need asynch_context::reset() at all. We may prohibit context object reuse instead. **/ 
    void reset ();

    //! Initiates cancellation of all tasks in this cancellation group and its subordinate groups.
    /** \return false if cancellation has already been requested, true otherwise. **/
    bool cancel_task_group ();

    //! Returns true if the context received cancellation request.
    bool was_cancelled () { return my_cancellation_requested != 0; }

#if TBB_DO_ASSERT
    bool assert_okay () const {
        /// \todo Implement it.
        return true;
    }
    bool assert_unbound () const {
        bool bound_to_another = my_parent || my_prev_sibling || my_next_sibling;
        __TBB_ASSERT (!bound_to_another, "context still has parent or siblings");
        __TBB_ASSERT (!my_first_child, "context still has children");
        return  !(bound_to_another || my_first_child);
    }

    bool assert_parent_is_valid ( const asynch_context& parent ) {
        /// \todo Implement it.
        // Make sure that ultimate parent of my_parent is the same as for "parent" argument
        // Assert: "Attempt to bind already bound asynch context to another hierarchy"
        return true;
    }
#endif /* TBB_DO_ASSERT */

private:
    friend class task;
    friend class internal::scheduler;
    friend class internal::allocate_root_with_context_proxy;
    template<typename T> friend class internal::CustomScheduler;

    void bind_to ( asynch_context& parent );
    void unbind ();

    //! Stores information about exception in the context
    /** If the task tree is not canceled, creates new exception_data structure 
        and stores it in the context **/
    void register_exception ( const char* name, const char* info );
    //! Stores information about exception in the context
    /** If the task tree is not canceled, stores data in the context, otherwise
        decrements data's refcount. **/
    void register_exception ( internal::exception_data* data );
}; // class asynch_context

#endif /* __TBB_EXCEPTIONS */


//! Base class for user-defined tasks.
/** @ingroup task_scheduling */
class task: internal::no_copy {
    //! Set reference count
    void internal_set_ref_count( int count );

protected:
    //! Default constructor.
    task() {prefix().extra_state=1;}

public:
    //! Destructor.
    virtual ~task() {}

    //! Should be overriden by derived classes.
    virtual task* execute() = 0;

    //! Enumeration of task states that the scheduler considers.
    enum state_type {
        //! task is running, and will be destroyed after method execute() completes.
        executing,
        //! task to be rescheduled.
        reexecute,
        //! task is in ready pool, or is going to be put there, or was just taken off.
        ready,
        //! task object is freshly allocated or recycled.
        allocated,
        //! task object is on free list, or is going to be put there, or was just taken off.
        freed,
        //! task to be recycled as continuation
        recycle
    };

    //------------------------------------------------------------------------
    // Allocating tasks
    //------------------------------------------------------------------------

    //! Returns proxy for overloaded new that allocates a root task.
    static internal::allocate_root_proxy allocate_root() {
        return internal::allocate_root_proxy();
    }

#if __TBB_EXCEPTIONS
    //! Returns proxy for overloaded new that allocates a root task associated with user supplied context.
    static internal::allocate_root_with_context_proxy allocate_root( asynch_context& ctx ) {
        return internal::allocate_root_with_context_proxy(ctx);
    }
#endif /* __TBB_EXCEPTIONS */

    //! Returns proxy for overloaded new that allocates a continuation task of *this.
    /** The continuation's parent becomes the parent of *this. */
    internal::allocate_continuation_proxy& allocate_continuation() {
        return *reinterpret_cast<internal::allocate_continuation_proxy*>(this);
    }

    //! Returns proxy for overloaded new that allocates a child task of *this.
    internal::allocate_child_proxy& allocate_child() {
        return *reinterpret_cast<internal::allocate_child_proxy*>(this);
    }

    //! Like allocate_child, except that task's parent becomes "t", not this.
    /** Typically used in conjunction with schedule_to_reexecute to implement while loops.
        Atomically increments the reference count of t.parent() */
    internal::allocate_additional_child_of_proxy allocate_additional_child_of( task& t ) {
        return internal::allocate_additional_child_of_proxy(*this,t);
    }

    //! Destroy a task.
    /** Usually, calling this method is unnecessary, because a task is
        implicitly deleted after its execute() method runs.  However,
        sometimes a task needs to be explicitly deallocated, such as
        when a root task is used as the parent in spawn_and_wait_for_all. */
    void destroy( task& victim );

    //------------------------------------------------------------------------
    // Recycling of tasks
    //------------------------------------------------------------------------

    //! Change this to be a continuation of its former self.
    /** The caller must guarantee that the task's refcount does not become zero until
        after the method execute() returns.  Typically, this is done by having
        method execute() return a pointer to a child of the task.  If the guarantee
        cannot be made, use method recycle_as_safe_continuation instead. 
       
        Because of the hazard, this method may be deprecated in the future. */
    void recycle_as_continuation() {
        __TBB_ASSERT( prefix().state==executing, "execute not running?" );
        prefix().state = allocated;
    }

    //! Recommended to use, safe variant of recycle_as_continuation
    /** For safety, it requires additional increment of ref_count. */
    void recycle_as_safe_continuation() {
        __TBB_ASSERT( prefix().state==executing, "execute not running?" );
        prefix().state = recycle;
    }

    //! Change this to be a child of new_parent.
    void recycle_as_child_of( task& new_parent ) {
        internal::task_prefix& p = prefix();
        __TBB_ASSERT( prefix().state==executing||prefix().state==allocated, "execute not running, or already recycled" );
        __TBB_ASSERT( prefix().ref_count==0, "no child tasks allowed when recycled as a child" );
        __TBB_ASSERT( p.parent==NULL, "parent must be null" );
        __TBB_ASSERT( new_parent.prefix().state<=recycle, "corrupt parent's state" );
        __TBB_ASSERT( new_parent.prefix().state!=freed, "parent already freed" );
        p.state = allocated;
        p.parent = &new_parent;
        p.depth = new_parent.prefix().depth+1;
    }

    //! Schedule this for reexecution after current execute() returns.
    /** Requires that this.execute() be running. */
    void recycle_to_reexecute() {
        __TBB_ASSERT( prefix().state==executing, "execute not running, or already recycled" );
        __TBB_ASSERT( prefix().ref_count==0, "no child tasks allowed when recycled for reexecution" );
        prefix().state = reexecute;
    }

    //! A scheduling depth.
    /** Guaranteed to be a signed integral type. */
    typedef internal::intptr depth_type;

    //! Scheduling depth
    depth_type depth() const {return prefix().depth;}

    //! Set scheduling depth to given value.
    /** The depth must be non-negative */
    void set_depth( depth_type new_depth ) {
        __TBB_ASSERT( state()!=ready, "cannot change depth of ready task" );
        __TBB_ASSERT( new_depth>=0, "depth cannot be negative" );
        __TBB_ASSERT( new_depth==int(new_depth), "integer overflow error");
        prefix().depth = int(new_depth);
    }

    //! Change scheduling depth by given amount.
    /** The resulting depth must be non-negative. */
    void add_to_depth( int delta ) {
        __TBB_ASSERT( state()!=ready, "cannot change depth of ready task" );
        __TBB_ASSERT( prefix().depth>=-delta, "depth cannot be negative" );
        prefix().depth+=delta;
    }

    //------------------------------------------------------------------------
    // Spawning and blocking
    //------------------------------------------------------------------------

    //! Set reference count
    void set_ref_count( int count ) {
#if TBB_DO_ASSERT
        internal_set_ref_count(count);
#else
        prefix().ref_count = count;
#endif /* TBB_DO_ASSERT */
    }

    //! Schedule task for execution when a worker becomes available.
    /** After all children spawned so far finish their method task::execute,
        their parent's method task::execute may start running.  Therefore, it
        is important to ensure that at least one child has not completed until
        the parent is ready to run. */
    void spawn( task& child ) {
        __TBB_ASSERT( is_owned_by_current_thread(), "'this' not owned by current thread" );
        prefix().owner->spawn( child, child.prefix().next );
    }

    //! Spawn multiple tasks and clear list.
    /** All of the tasks must be at the same depth. */
    void spawn( task_list& list );

    //! Similar to spawn followed by wait_for_all, but more efficient.
    void spawn_and_wait_for_all( task& child ) {
        __TBB_ASSERT( is_owned_by_current_thread(), "'this' not owned by current thread" );
        prefix().owner->wait_for_all( *this, &child );
    }

    //! Similar to spawn followed by wait_for_all, but more efficient.
    void spawn_and_wait_for_all( task_list& list );

    //! Spawn task allocated by allocate_root, wait for it to complete, and deallocate it.
    /** The thread that calls spawn_root_and_wait must be the same thread
        that allocated the task. */
    static void spawn_root_and_wait( task& root ) {
        __TBB_ASSERT( root.is_owned_by_current_thread(), "root not owned by current thread" );
        root.prefix().owner->spawn_root_and_wait( root, root.prefix().next );
    }

    //! Spawn root tasks on list and wait for all of them to finish.
    /** If there are more tasks than worker threads, the tasks are spawned in
        order of front to back. */
    static void spawn_root_and_wait( task_list& root_list );

    //! Wait for reference count to become one, and set reference count to zero.
    /** Works on tasks while waiting. */
    void wait_for_all() {
        __TBB_ASSERT( is_owned_by_current_thread(), "'this' not owned by current thread" );
        prefix().owner->wait_for_all( *this, NULL );
    }

    //! The task() currently being run by this thread.
    static task& self();

    //! task on whose behalf this task is working, or NULL if this is a root.
    task* parent() const {return prefix().parent;}

    //! True if task is owned by different thread than thread that owns its parent.
    bool is_stolen_task() const {
        internal::task_prefix& p = prefix();
        internal::task_prefix& q = parent()->prefix();
        return p.owner!=q.owner;
    }

    //------------------------------------------------------------------------
    // Debugging
    //------------------------------------------------------------------------

    //! Current execution state
    state_type state() const {return state_type(prefix().state);}

    //! The internal reference count.
    int ref_count() const {
#if TBB_DO_ASSERT
        internal::reference_count ref_count = prefix().ref_count;
        __TBB_ASSERT( ref_count==int(ref_count), "integer overflow error");
#endif
        return int(prefix().ref_count);
    }

    //! True if this task is owned by the calling thread; false otherwise.
    bool is_owned_by_current_thread() const;

    //------------------------------------------------------------------------
    // Affinity
    //------------------------------------------------------------------------
 
    //! An id as used for specifying affinity.
    /** Guaranteed to be integral type.  Value of 0 means no affinity. */
    typedef internal::affinity_id affinity_id;

    //! Set affinity for this task.
    void set_affinity( affinity_id id ) {prefix().affinity = id;}

    //! Current affinity of this task
    affinity_id affinity() const {return prefix().affinity;}

    //! Invoked by scheduler to notify task that it ran on unexpected thread.
    /** Invoked before method execute() runs, if task is stolen, or task has 
        affinity but will be executed on another thread. 

        The default action does nothing. */
    virtual void note_affinity( affinity_id id );

#if __TBB_EXCEPTIONS
    //! Initiates cancellation of all tasks in this cancellation group and its subordinate groups.
    /** \return false if cancellation has already been requested, true otherwise. **/
    bool cancel_task_group () { return prefix().context->cancel_task_group(); }

    //! Returns true if the context received cancellation request.
    bool was_cancelled () { return prefix().context->was_cancelled(); }
#endif /* __TBB_EXCEPTIONS */

private:
    friend class task_list;
    friend class internal::scheduler;
    friend class internal::allocate_root_proxy;
#if __TBB_EXCEPTIONS
    friend class internal::allocate_root_with_context_proxy;
#endif /* __TBB_EXCEPTIONS */
    friend class internal::allocate_continuation_proxy;
    friend class internal::allocate_child_proxy;
    friend class internal::allocate_additional_child_of_proxy;

    //! Get reference to corresponding task_prefix.
    internal::task_prefix& prefix() const {
        return reinterpret_cast<internal::task_prefix*>(const_cast<task*>(this))[-1];
    }
}; // class task

//! task that does nothing.  Useful for synchronization.
/** @ingroup task_scheduling */
class empty_task: public task {
    /*override*/ task* execute() {
        return NULL;
    }
};

//! A list of children.
/** Used for method task::spawn_children
    @ingroup task_scheduling */
class task_list: internal::no_copy {
private:
    task* first;
    task** next_ptr;
    friend class task;
public:
    //! Construct empty list
    task_list() : first(NULL), next_ptr(&first) {}

    //! Destroys the list, but does not destroy the task objects.
    ~task_list() {}

    //! True if list if empty; false otherwise.
    bool empty() const {return !first;}

    //! Push task onto back of list.
    void push_back( task& task ) {
        task.prefix().next = NULL;
        *next_ptr = &task;
        next_ptr = &task.prefix().next;
    }

    //! Pop the front task from the list.
    task& pop_front() {
        __TBB_ASSERT( !empty(), "attempt to pop item from empty task_list" );
        task* result = first;
        first = result->prefix().next;
        if( !first ) next_ptr = &first;
        return *result;
    }

    //! Clear the list
    void clear() {
        first=NULL;
        next_ptr=&first;
    }
};

inline void task::spawn( task_list& list ) {
    __TBB_ASSERT( is_owned_by_current_thread(), "'this' not owned by current thread" );
    if( task* t = list.first ) {
        prefix().owner->spawn( *t, *list.next_ptr );
        list.clear();
    }
}

inline void task::spawn_root_and_wait( task_list& root_list ) {
    if( task* t = root_list.first ) {
        __TBB_ASSERT( t->is_owned_by_current_thread(), "'this' not owned by current thread" );
        t->prefix().owner->spawn_root_and_wait( *t, *root_list.next_ptr );
        root_list.clear();
    }
}

} // namespace tbb

inline void *operator new( size_t bytes, const tbb::internal::allocate_root_proxy& p ) {
    return &p.allocate(bytes);
}

inline void operator delete( void* task, const tbb::internal::allocate_root_proxy& p ) {
    p.free( *static_cast<tbb::task*>(task) );
}

#if __TBB_EXCEPTIONS
inline void *operator new( size_t bytes, const tbb::internal::allocate_root_with_context_proxy& p ) {
    return &p.allocate(bytes);
}

inline void operator delete( void* task, const tbb::internal::allocate_root_with_context_proxy& p ) {
    p.free( *static_cast<tbb::task*>(task) );
}
#endif /* __TBB_EXCEPTIONS */

inline void *operator new( size_t bytes, const tbb::internal::allocate_continuation_proxy& p ) {
    return &p.allocate(bytes);
}

inline void operator delete( void* task, const tbb::internal::allocate_continuation_proxy& p ) {
    p.free( *static_cast<tbb::task*>(task) );
}

inline void *operator new( size_t bytes, const tbb::internal::allocate_child_proxy& p ) {
    return &p.allocate(bytes);
}

inline void operator delete( void* task, const tbb::internal::allocate_child_proxy& p ) {
    p.free( *static_cast<tbb::task*>(task) );
}

inline void *operator new( size_t bytes, const tbb::internal::allocate_additional_child_of_proxy& p ) {
    return &p.allocate(bytes);
}

inline void operator delete( void* task, const tbb::internal::allocate_additional_child_of_proxy& p ) {
    p.free( *static_cast<tbb::task*>(task) );
}

#endif /* __TBB_task_H */
