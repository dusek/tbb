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

#include "tbb/blocked_range.h"
#include "tbb/parallel_for.h"
#include "tbb/parallel_reduce.h"


#define NRUNS               7
#define ONE_TEST_DURATION   0.01

#include "perf_util.h"



#define NUM_CHILD_TASKS     256
#define NUM_ROOT_TASKS      16
#define NUM_ROOTS_IN_GROUP  8

#define N       100000
#define GRAIN   1000


typedef ANCHOR_TYPE count_type;
typedef tbb::blocked_range<count_type> range_type;


const count_type N_reduce = (count_type)(N/log((double)N/GRAIN));


inline void PrintTitle() {  
    printf ("%-32s Rate      Repeats   Clean time  Gross time  Variation, %% Deviation, %%\n", "Test name");
}


class simple_leaf_task : public tbb::task
{
    task* execute () {
        util::anchor += 1;
        return NULL;
    }
};

class simple_root_task : public tbb::task
{
    task* execute () {
        set_ref_count(NUM_CHILD_TASKS + 1);
        for ( size_t i = 0; i < NUM_CHILD_TASKS; ++i ) {
            simple_leaf_task &t = *new( allocate_child() ) simple_leaf_task;
            spawn(t);
        }
        return NULL;
    }
};

void Test1 () {
    tbb::empty_task &r = *new( tbb::task::allocate_root() ) tbb::empty_task;
    r.set_ref_count(NUM_CHILD_TASKS + 1);
    for ( size_t i = 0; i < NUM_CHILD_TASKS; ++i ) {
        simple_leaf_task &t = *new( r.allocate_child() ) simple_leaf_task;
        r.spawn(t);
    }
    r.wait_for_all();
    r.destroy(r);
}

void Test2 ()
{
    simple_root_task &r = *new( tbb::task::allocate_root() ) simple_root_task;
    tbb::task::spawn_root_and_wait(r);
}


class root_launcher_task : public tbb::task
{
    task* execute () {
        simple_root_task &r = *new( allocate_root() ) simple_root_task;
        spawn_root_and_wait(r);
        return NULL;
    }
};

class hierarchy_root_task : public tbb::task
{
    task* execute () {
        tbb::task_list  tl;
        for ( size_t i = 0; i < NUM_ROOT_TASKS; ++i ) {
            root_launcher_task &r = *new( allocate_root() ) root_launcher_task;
            tl.push_back(r);
        }
        spawn_root_and_wait(tl);
        return NULL;
    }
};

void Test3 ()
{
    hierarchy_root_task &r = *new( tbb::task::allocate_root() ) hierarchy_root_task;
    tbb::task::spawn_root_and_wait(r);
}


class simple_pfor_body {
public:
    void operator()( const range_type& r ) const {
        count_type end = r.end();
        for( count_type i = r.begin(); i < end; ++i )
            util::anchor += i;
    }
};

void Test11 () {
    volatile count_type zero = 0;
    tbb::parallel_for( range_type(0, zero, 1), simple_pfor_body() );
}

void Test12 () {
    tbb::parallel_for( range_type(0, N_reduce, 1), simple_pfor_body() );
}

void Test13 () {
    tbb::parallel_for( range_type(0, N, GRAIN), simple_pfor_body() );
}


class simple_preduce_body {
public:
    count_type my_sum;
    simple_preduce_body () : my_sum(0) {}
    simple_preduce_body ( simple_preduce_body&, tbb::split ) : my_sum(0) {}
    void join( simple_preduce_body& rhs ) { my_sum += rhs.my_sum;}
    void operator()( const range_type& r ) {
        count_type end = r.end();
        for( count_type i = r.begin(); i < end; ++i )
            util::anchor += i;
        my_sum = util::anchor;
    }
};

void Test21 () {
    volatile count_type zero = 0;
    simple_preduce_body body;
    tbb::parallel_reduce( range_type(0, zero, 1), body );
}

void Test22 () {
    simple_preduce_body body;
    tbb::parallel_reduce( range_type(0, N_reduce, 1), body );
}

void Test23 () {
    simple_preduce_body body;
    tbb::parallel_reduce( range_type(0, N, GRAIN), body );
}


void Test () {
    PrintTitle();
    RunTest ("Simple task tree (empty root)", Test1);
    RunTest ("Simple task tree (with root)", Test2);
    RunTest ("Complex task tree", Test3);

    RunTest ("Empty pfor", Test11);
    RunTest ("Fine grained pfor", Test12);
    RunTest ("Coarse grained pfor", Test13);

    RunTest ("Empty preduce", Test21);
    RunTest ("Fine grained preduce", Test22);
    RunTest ("Coarse grained preduce", Test23);
}


int main( int argc, char* argv[] ) {
    test_main(argc, argv);
    return 0;
}
