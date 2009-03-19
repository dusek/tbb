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

/** 
    Evolution.h: Header file for evolution classes; evolution classes do 
    looped evolution of patterns in a defined 2 dimensional space 
**/

#pragma once
#include "Board.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//! Threading Building Blocks includes
#include "tbb\task_scheduler_init.h"
#include "tbb\blocked_range.h"
#include "tbb\parallel_for.h"

using namespace System::Threading;

void UpdateState(Matrix * m_matrix, char * dest ,int begin, int end);

/**
    class Evolution - base class for SequentialEvolution and ParallelEvolution
**/
public ref class Evolution abstract
{
public:
    Evolution( Matrix *m,                //! beginning matrix including initial pattern
               Board^ board              //! the board to update
             ) : m_matrix(m), m_board(board), 
                 m_size(m_matrix->height * m_matrix->width), m_done(false)
    {
        //! allocate memory for second matrix data block
        m_dest = new char[m_size];
        is_paused = false;
    }

    virtual ~Evolution()
    {
        delete[] m_dest;
    }

    //! Run() - begins looped evolution
    virtual void Run() = 0;

    //! Quit() - tell the thread to terminate
    virtual void Quit() { m_done = true; }
    
    //! Step() - performs a single evolutionary generation computation on the game matrix
    virtual void Step() = 0;

    //! SetPause() - change condition of variable is_paused
    virtual void SetPause(bool condition)
    {
        if ( condition == true )
            is_paused = true;
        else
            is_paused = false;
    }
    
protected:
    /** 
        UpdateMatrix() - moves the previous destination data to the source 
        data block and zeros out destination.
    **/
    void UpdateMatrix();

protected:
    Matrix*         m_matrix;       //! Pointer to initial matrix
    char*           m_dest;         //! Pointer to calculation destination data    
    Board^          m_board;        //! The game board to update
    int             m_size;         //! size of the matrix data block
    volatile bool   m_done;         //! a flag used to terminate the thread
    Int32           m_nIteration;   //! current calculation cycle index
    volatile bool   is_paused;      //! is needed to perform next iteration
    
    //! Calculation time of the sequential version (since the start), seconds.
    /**
        This member is updated by the sequential version and read by parallel,
        so no synchronization is necessary.
    **/
    static volatile double m_serial_time = 0;

    static System::Threading::AutoResetEvent    ^m_evt_start_serial = gcnew AutoResetEvent(false),
                                                ^m_evt_start_parallel = gcnew AutoResetEvent(false);
};

/**
    class SequentialEvolution - derived from Evolution - calculate life generations serially
**/
public ref class SequentialEvolution: public Evolution
{
public:
    SequentialEvolution(Matrix *m, Board^ board)
                       : Evolution(m, board)
    {}
        
    virtual void Run() override;    
    virtual void Step() override;
};

/**
    class ParallelEvolution - derived from Evolution - calculate life generations
    in parallel using Intel's TBB package
**/
public ref class ParallelEvolution: public Evolution
{
public:

    ParallelEvolution(Matrix *m, Board^ board)
                     : Evolution(m, board),
                       m_parallel_time(0)
    {
        // instantiate a task_scheduler_init object and save a pointer to it
        m_pInit = NULL;
    }
    
    ~ParallelEvolution()
    {
        //! delete task_scheduler_init object
        if (m_pInit != NULL)
            delete m_pInit;
    }

    virtual void Run() override;
    virtual void Step() override;

private:
    tbb::task_scheduler_init* m_pInit;

    double m_parallel_time;
};
