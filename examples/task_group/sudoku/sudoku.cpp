/*
    Copyright 2005-2010 Intel Corporation.  All Rights Reserved.

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

#include <cstdio>
#include <cstdlib>
#include "tbb/atomic.h"
#include "tbb/tick_count.h"
#include "tbb/task_scheduler_init.h"
#include "tbb/task_group.h"

const unsigned BOARD_SIZE=81;
const unsigned BOARD_DIM=9;

using namespace tbb;

atomic<unsigned> nSols;
unsigned NThreads, NSolutions;
bool Verbose=false;
unsigned short init_values[BOARD_SIZE];
task_group *g;

typedef struct {
    unsigned short solved_element;
    unsigned potential_set;
} board_element;

void read_board(char *filename) {
    FILE *fp;
    fp = fopen(filename, "r");
    for (int i=0; i<BOARD_SIZE; ++i) {
        fscanf(fp, "%d", &(init_values[i]));
    }
    fclose(fp);
}

void print_board(board_element *b) {
    for (int row=0; row<BOARD_DIM; ++row) {
        for (int col=0; col<BOARD_DIM; ++col) {
            printf(" %d", b[row*BOARD_DIM+col].solved_element);
            if (col==2 || col==5) printf(" |");
        }
        printf("\n");
        if (row==2 || row==5) printf(" ---------------------\n");
    }
}

void print_potential_board(board_element *b) {
    for (int row=0; row<BOARD_DIM; ++row) {
        for (int col=0; col<BOARD_DIM; ++col) {
            if (b[row*BOARD_DIM+col].solved_element) 
                printf("  %4d ", b[row*BOARD_DIM+col].solved_element);
            else
                printf(" [%4d]", b[row*BOARD_DIM+col].potential_set);
            if (col==2 || col==5) printf(" |");
        }
        printf("\n");
        if (row==2 || row==5)
            printf(" ------------------------------------------------------------------\n");
    }
}

void init_board(board_element *b) {
    for (int i=0; i<BOARD_SIZE; ++i)
        b[i].solved_element = b[i].potential_set = 0;
}

void init_board(board_element *b, unsigned short arr[81]) {
    for (int i=0; i<BOARD_SIZE; ++i) {
        b[i].solved_element = arr[i]; 
        b[i].potential_set = 0;
    }
}

void init_potentials(board_element *b) {
    for (int i=0; i<BOARD_SIZE; ++i)
        b[i].potential_set = 0;
}

void copy_board(board_element *src, board_element *dst) {
    for (int i=0; i<BOARD_SIZE; ++i)
        dst[i].solved_element = src[i].solved_element;
}

bool fixed_board(board_element *b) {
    for (int i=BOARD_SIZE-1; i>=0; --i)
        if (b[i].solved_element==0) return false;
    return true;
}

bool in_row(board_element *b, unsigned row, unsigned col, unsigned short p) {
    for (int c=0; c<BOARD_DIM; ++c)
        if (c!=col && b[row*BOARD_DIM+c].solved_element==p)  return true;
    return false;
}

bool in_col(board_element *b, unsigned row, unsigned col, unsigned short p) {
    for (int r=0; r<BOARD_DIM; ++r)
        if (r!=row && b[r*BOARD_DIM+col].solved_element==p)  return true;
    return false;
}

bool in_block(board_element *b, unsigned row, unsigned col, unsigned short p) {
    unsigned b_row = row/3 * 3, b_col = col/3 * 3;
    for (int i=b_row; i<b_row+3; ++i)
        for (int j=b_col; j<b_col+3; ++j)
            if (!(i==row && j==col) && b[i*BOARD_DIM+j].solved_element==p) return true;
    return false;
}

void calculate_potentials(board_element *b) {
    for (int i=0; i<BOARD_SIZE; ++i) {
        b[i].potential_set = 0;
        if (!b[i].solved_element) { // element is not yet fixed
            unsigned row = i/BOARD_DIM, col = i%BOARD_DIM;
            for (int potential=1; potential<=BOARD_DIM; ++potential) {
                if (!in_row(b, row, col, potential) && !in_col(b, row, col, potential)
                    && !in_block(b, row, col, potential))
                    b[i].potential_set |= 1<<(potential-1);
            }
        }
    }
}

bool valid_board(board_element *b) {
    bool success=true;
    for (int i=0; i<BOARD_SIZE; ++i) {
        if (success && b[i].solved_element) { // element is fixed
            unsigned row = i/BOARD_DIM, col = i%BOARD_DIM;
            if (in_row(b, row, col, b[i].solved_element) || in_col(b, row, col, b[i].solved_element) || in_block(b, row, col, b[i].solved_element))
                success = false;
        }
    }
    return success;
}

bool examine_potentials(board_element *b, bool *progress) {
    bool singletons = false;
    for (int i=0; i<BOARD_SIZE; ++i) {
        if (b[i].solved_element==0 && b[i].potential_set==0) // empty set
	    return false;
        switch (b[i].potential_set) {
        case 1:   { b[i].solved_element = 1; singletons=true; break; }
        case 2:   { b[i].solved_element = 2; singletons=true; break; }
        case 4:   { b[i].solved_element = 3; singletons=true; break; }
        case 8:   { b[i].solved_element = 4; singletons=true; break; }
        case 16:  { b[i].solved_element = 5; singletons=true; break; }
        case 32:  { b[i].solved_element = 6; singletons=true; break; }
        case 64:  { b[i].solved_element = 7; singletons=true; break; }
        case 128: { b[i].solved_element = 8; singletons=true; break; }
        case 256: { b[i].solved_element = 9; singletons=true; break; }
        }
    }
    *progress = singletons;
    return valid_board(b);
}

void partial_solve(board_element *b, unsigned first_potential_set) {
    if (fixed_board(b)) {
        if (NSolutions == 1)
            g->cancel();
        if (++nSols==1 && Verbose) {
            print_board(b);
        }
        free(b);
        return;
    }
    calculate_potentials(b);
    bool progress=true;
    bool success = examine_potentials(b, &progress);
    if (success && progress) {
        partial_solve(b, first_potential_set);
    } else if (success && !progress) {
        board_element *new_board;
        while (b[first_potential_set].solved_element!=0) ++first_potential_set;
        for (unsigned short potential=1; potential<=BOARD_DIM; ++potential) {
            if (1<<(potential-1) & b[first_potential_set].potential_set) {
                new_board = (board_element *)malloc(BOARD_SIZE*sizeof(board_element));
                copy_board(b, new_board);
                new_board[first_potential_set].solved_element = potential;
                g->run( [=]{ partial_solve(new_board, first_potential_set); } );
            }
        }
	free(b);
    }
    else {
	free(b);
    }
}

void ParseCommandLine(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, 
                "Usage: sudoku <inputfilename> <nthreads> <nSolutions> [-p]\n"
                "  nSolutions=1 stops after finding first solution\n"
                "    and any other value finds all solutions; \n"
                "  -p prints the first solution.\n");
        exit(1);
    }
    else {
        sscanf(argv[2], "%d", &NThreads);
        sscanf(argv[3], "%d", &NSolutions);
    }
    if (argc==5) Verbose = true;
}

int main(int argc, char *argv[]) {
    board_element *start_board;
    start_board = (board_element *)malloc(BOARD_SIZE*sizeof(board_element));
    NThreads = 1;
    nSols = 0;
    ParseCommandLine(argc, argv);
    read_board(argv[1]);
    init_board(start_board, init_values);
    task_scheduler_init init(NThreads);
    g = new task_group;
    tick_count t0 = tick_count::now();
    partial_solve(start_board, 0);
    g->wait();
    tick_count t1 = tick_count::now();

    if (NSolutions == 1) {
        printf("Sudoku: Time to find first solution on %d threads: %6.6f seconds.\n", NThreads, (t1 - t0).seconds());
    }
    else {
        printf("Sudoku: Time to find all %d solutions on %d threads: %6.6f seconds.\n", (int)nSols, NThreads, (t1 - t0).seconds());
  }
};
