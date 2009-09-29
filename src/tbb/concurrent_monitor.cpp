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

#include "concurrent_monitor.h"

namespace tbb {
namespace internal {

void concurrent_monitor::prepare_wait( thread_context& thr, void* ctx ) {
    // this is good place to pump previous spurious wakeup
    if( thr.spurious ) {
        thr.spurious = false;
        thr.sema.P();
    }
    thr.context = ctx;
    thr.in_waitset = true;
    {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        thr.epoch = epoch;
        waitset_ec.add( (waitset_t::node_t*)&thr );
    }
    __TBB_rel_acq_fence();
}

void concurrent_monitor::cancel_wait( thread_context& thr ) {
    // spurious wakeup will be pumped in the following prepare_wait()
    thr.spurious = true;
    // try to remove node from waitset
    bool th_in_waitset = thr.in_waitset;
    if( th_in_waitset ) {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        if (thr.in_waitset) {
            // successfully removed from waitset,
            // so there will be no spurious wakeup
            thr.in_waitset = false;
            thr.spurious = false;
            waitset_ec.remove( (waitset_t::node_t&)thr );
        }
    }
}

void concurrent_monitor::notify_one_relaxed() {
    if( waitset_ec.size()==0 )
        return;
    waitset_node_t* n;
    const waitset_node_t* end = waitset_ec.end();
    {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        epoch = epoch + 1;
        n = waitset_ec.front();
        if( n!=end ) {
            waitset_ec.remove( *n );
            to_thread_context(n)->in_waitset = false;
        }
    }
    if( n!=end ) 
        to_thread_context(n)->sema.V();
}

void concurrent_monitor::notify_all_relaxed() {
    if( waitset_ec.size()==0 )
        return;
    dllist_t temp;
    const waitset_node_t* end;
    {
        tbb::spin_mutex::scoped_lock l( mutex_ec );
        epoch = epoch + 1;
        waitset_ec.flush_to( temp );
        end = temp.end();
        for( waitset_node_t* n=temp.front(); n!=end; n=n->next )
            to_thread_context(n)->in_waitset = false;
    }
    waitset_node_t* nxt;
    for( waitset_node_t* n=temp.front(); n!=end; n=nxt ) {
        nxt = n->next;
        to_thread_context(n)->sema.V();
    }
#if TBB_USE_DEBUG
    temp.clear();
#endif
}

} // namespace internal
} // namespace tbb
