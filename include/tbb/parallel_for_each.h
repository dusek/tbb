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

#ifndef __TBB_parallel_for_each_H
#define __TBB_parallel_for_each_H

#include "parallel_do.h"

namespace tbb {

//! @cond INTERNAL
namespace internal {
    // The class calls user function in operator()
    template <typename Function, typename Iterator>
    class parallel_for_each_body : internal::no_assign {
        Function &my_func;
    public:
        parallel_for_each_body(Function &_func) : my_func(_func) {}
        parallel_for_each_body(const parallel_for_each_body<Function, Iterator> &_caller) : my_func(_caller.my_func) {}

        void operator() ( typename Iterator::value_type value ) const {
            my_func(value);
        }
    };
} // namespace internal
//! @endcond

/** \name parallel_for_each
    **/
//@{
//! Calls function _Func for all items from [_First, Last) interval using user-supplied context
/** @ingroup algorithms */
template<typename Input_iterator, typename Function>
Function parallel_for_each(Input_iterator _First, Input_iterator _Last, Function _Func, task_group_context &context) {
    internal::parallel_for_each_body<Function, Input_iterator> body(_Func);

    tbb::parallel_do (_First, _Last, body, context);
    return _Func;
}

//! uses default context
template<typename Input_iterator, typename Function>
Function parallel_for_each(Input_iterator _First, Input_iterator _Last, Function _Func) {
    tbb::task_group_context context;

    tbb::parallel_for_each (_First, _Last, _Func, context);
    return _Func;
}

//@}

} // namespace

#endif /* __TBB_parallel_for_each_H */
