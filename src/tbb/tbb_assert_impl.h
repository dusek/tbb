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

// IMPORTANT: To use assertion handling in TBB, exactly one of the TBB source files
// should #include tbb_assert_impl.h thus instantiating assertion handling routines.
// The intent of putting it to a separate file is to allow some tests to use it
// as well in order to avoid dependency on the library.

// include headers for required function declarations
#include <cstdio>
#include <cstdlib>
#if _WIN32||_WIN64
#include <crtdbg.h>
#endif

namespace tbb {
    //! Type for an assertion handler
    typedef void(*assertion_handler_type)( const char* filename, int line, const char* expression, const char * comment );

    static assertion_handler_type assertion_handler;

    assertion_handler_type set_assertion_handler( assertion_handler_type new_handler ) {
        assertion_handler_type old_handler = assertion_handler;
        assertion_handler = new_handler;
        return old_handler;
    }

    void assertion_failure( const char* filename, int line, const char* expression, const char* comment ) {
        if( assertion_handler_type a = assertion_handler ) {
            (*a)(filename,line,expression,comment);
        } else {
            static bool already_failed;
            if( !already_failed ) {
                already_failed = true;
                fprintf( stderr, "Assertion %s failed on line %d of file %s\n",
                         expression, line, filename );
                if( comment )
                    fprintf( stderr, "Detailed description: %s\n", comment );
#if (_WIN32||_WIN64) && defined(_DEBUG)
                if(1 == _CrtDbgReport(_CRT_ASSERT, filename, line, "tbb_debug.dll", "%s\r\n%s", expression, comment?comment:""))
                        _CrtDbgBreak();
#else
                abort();
#endif
            }
        }
    }
} /* namespace tbb */
