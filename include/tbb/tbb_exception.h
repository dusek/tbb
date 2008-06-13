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

#ifndef __TBB_exception_H
#define __TBB_exception_H

#if __TBB_EXCEPTIONS
#include "tbb_stddef.h"
#include "tbb/tbb_allocator.h"
#include <exception>
#include <typeinfo>
#include <new>

namespace tbb {

//! Interface to be implemented by all exceptions TBB recognizes and propagates across the threads.
/** If an unhandled exception of the type derived from tbb::tbb_exception is intercepted
    by the TBB scheduler in one of the worker threads, it is delivered to and re-thrown in
    the root thread. The root thread is the thread that has started the outermost algorithm 
    or root task sharing the same task_group_context with the guilty algorithm/task (the one
    that threw the exception first).
    
    Note: when documentation mentions workers with respect to exception handling, 
    masters are implied as well, because they are completely equivalent in this context.
    Consequently a root thread can be master or worker thread. 

    NOTE: In case of nested algorithms or complex task hierarchies when the nested 
    levels share (explicitly or by means of implicit inheritance) the task group 
    context of the outermost level, the exception may be (re-)thrown multiple times 
    (ultimately - in each worker on each nesting level) before reaching the root 
    thread at the outermost level. IMPORTANT: if you intercept an exception derived 
    from this class on a nested level, you must re-throw it in the catch block by means
    of the "throw;" operator. 
    
    TBB provides two implementations of this interface: tbb::captured_exception and 
    template class tbb::movable_exception. See their declarations for more info. **/
class tbb_exception : public std::exception {
public:
    //! Creates and returns pointer to the deep copy of this exception object. 
    /** Move semantics is allowed. **/
    virtual tbb_exception* move () throw() = 0;
    
    //! Destroys objects created by the move() method.
    /** Frees memory and calls destructor for this exception object. 
        Can and must be used only on objects created by the move method. **/
    virtual void destroy () throw() = 0;

    //! Throws this exception object.
    /** Make sure that if you have several levels of derivation from this interface
        you implement or override this method on the most derived level. The implementation 
        is as simple as "throw *this;". Failure to do this will result in exception 
        of a base class type being thrown. **/
    virtual void throw_self () = 0;

    //! Returns RTTI name of the originally intercepted exception
    virtual const char* name() const throw() = 0;

    //! Returns the result of originally intercepted exception's what() method.
    virtual const char* what() const throw() = 0;
};

//! This class is used by TBB to propagate information about unhandled exceptions into the root thread.
/** Exception of this type is thrown by TBB in the root thread (thread that started a parallel 
    algorithm ) if an unhandled exception was intercepted during the algorithm execution in one 
    of the workers.
    \sa tbb::tbb_exception **/
class captured_exception : public tbb_exception
{
public:
    captured_exception ( const captured_exception& src )
        : my_dynamic(false)
    {
        set(src.my_exception_name, src.my_exception_info);
    }

    captured_exception ( const char* name, const char* info )
        : my_dynamic(false)
    {
        set(name, info);
    }

    ~captured_exception () throw() {
        clear();
    }

    captured_exception& operator= ( const captured_exception& src ) {
        if ( this != &src ) {
            clear();
            set(src.my_exception_name, src.my_exception_info);
        }
        return *this;
    }

    /*override*/ 
    captured_exception* move () throw();

    /*override*/ 
    void destroy () throw();

    /*override*/ 
    void throw_self () { throw *this; }

    /*override*/ 
    const char* name() const throw();

    /*override*/ 
    const char* what() const throw();

private:
    //! Used only by method clone().  
    captured_exception() {}

    //! Functionally equivalent to {captured_exception e(name,info); return c.clone();}
    static captured_exception* allocate ( const char* name, const char* info );

    void set ( const char* name, const char* info ) throw();
    void clear () throw();

    bool my_dynamic;
    const char* my_exception_name;
    const char* my_exception_info;
};

//! Template that can be used to implement exception that transfers arbitrary ExceptionData to the root thread
/** Code using TBB can instantiate this template with an arbitrary ExceptionData type 
    and throw this exception object. Such exceptions are intercepted by the TBB scheduler
    and delivered to the root thread (). 
    \sa tbb::tbb_exception **/
template<typename ExceptionData>
class movable_exception : public tbb_exception
{
    typedef movable_exception<ExceptionData> self_type;
public:
    const ExceptionData  my_exception_data;

    movable_exception ( const ExceptionData& data ) 
        : my_exception_data(data)
        , my_cloned(false)
        , my_exception_name(typeid(self_type).name())
    {}

    movable_exception ( const movable_exception& src ) throw () 
        : my_exception_data(src.my_exception_data)
        , my_cloned(false)
        , my_exception_name(src.my_exception_name)
    {}

    ~movable_exception () throw() {}

    /*override*/ const char* name() const throw() { return my_exception_name; }

    /*override*/ const char* what() const throw() { return "tbb::movable_exception"; }

    /*override*/ 
    movable_exception* move () throw() {
        void* e = internal::allocate_via_handler_v3(sizeof(movable_exception));
        if ( e ) {
            new (e) movable_exception(*this);
            ((movable_exception*)e)->my_cloned = true;
        }
        return (movable_exception*)e;
    }
    /*override*/ 
    void destroy () throw() {
        if ( my_cloned ) {
            this->~movable_exception();
            internal::deallocate_via_handler_v3(this);
        }
    }
    /*override*/ 
    void throw_self () {
        throw *this;
    }

protected:
    bool my_cloned;
    /** We rely on the fact that RTTI names are static string constants. **/
    const char* my_exception_name;
};

} // namespace tbb

#endif /* __TBB_EXCEPTIONS */

#endif /* __TBB_exception_H */
