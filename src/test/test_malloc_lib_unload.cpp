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


#include <stdio.h>
#if _WIN32 || _WIN64
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <tbb/tbb_stddef.h>
#include "harness.h"
#include "harness_memory.h"

#if TBB_USE_DEBUG
#define DEBUG_SUFFIX "_debug"
#else
#define DEBUG_SUFFIX
#endif /* TBB_USE_DEBUG */

// MALLOCLIB_NAME is the name of the TBB memory allocator library.
#if _WIN32||_WIN64
#define MALLOCLIB_NAME "tbbmalloc" DEBUG_SUFFIX ".dll"
#elif __APPLE__
#define MALLOCLIB_NAME "libtbbmalloc" DEBUG_SUFFIX ".dylib"
#elif __linux__
#define MALLOCLIB_NAME "libtbbmalloc" DEBUG_SUFFIX  __TBB_STRING(.so.TBB_COMPATIBLE_INTERFACE_VERSION)
#elif __FreeBSD__ || __sun
#define MALLOCLIB_NAME "libtbbmalloc" DEBUG_SUFFIX ".so"
#else
#error Unknown OS
#endif

struct Run {
    void operator()( int /*id*/ ) const {
        void* (*malloc_ptr)(size_t);
        void (*free_ptr)(void*);

#if _WIN32 || _WIN64
        HMODULE lib = LoadLibrary(MALLOCLIB_NAME);
#else
        void *lib = dlopen(MALLOCLIB_NAME, RTLD_NOW|RTLD_GLOBAL);
#endif
        if (NULL == lib) {
            REPORT("Can't load " MALLOCLIB_NAME "\n");
            exit(1);
        }
#if _WIN32 || _WIN64
        (void *&)malloc_ptr = GetProcAddress(lib, "scalable_malloc");
        (void *&)free_ptr = GetProcAddress(lib, "scalable_free");
#else
        (void *&)malloc_ptr = dlsym(lib, "scalable_malloc");
        (void *&)free_ptr = dlsym(lib, "scalable_free");
#endif
        if (!malloc_ptr || !free_ptr)  {
            REPORT("Can't find scalable_(malloc|free) in " MALLOCLIB_NAME "\n");
            exit(1);
        }

        void *p = malloc_ptr(100);
        memset(p, 1, 100);
        free_ptr(p);

#if _WIN32 || _WIN64
        FreeLibrary(lib);
        ASSERT(GetModuleHandle(MALLOCLIB_NAME),  
               MALLOCLIB_NAME " must not be unloaded");
#else
        dlclose(lib);
        ASSERT(dlsym(RTLD_DEFAULT, "scalable_malloc"),  
               MALLOCLIB_NAME " must not be unloaded");
#endif
    }
};

int main()
{
    // warm-up run
    NativeParallelFor( 1, Run() );

    /* Check for leaks. 1st call to GetMemoryUsage() allocate some memory, 
       but it seems memory consumption stabilized after this.
     */
    GetMemoryUsage();
    size_t memory_in_use = GetMemoryUsage();
    ASSERT(memory_in_use == GetMemoryUsage(), 
           "Memory consumption should not increase after 1st GetMemoryUsage() call");

    for (int i=0; i<10; i++)
        NativeParallelFor( 1, Run() );

    ptrdiff_t memory_leak = GetMemoryUsage() - memory_in_use;
    if( memory_leak>0 ) { // possibly too strong?
        REPORT( "Error: memory leak of up to %ld bytes\n", static_cast<long>(memory_leak));
        exit(1);
    }

    REPORT("done\n");
    return 0;
}
