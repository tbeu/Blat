/*
    makeargv.hpp
*/
#ifndef __MAKEARGV_HPP__
#define __MAKEARGV_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

/*
 * Parse the arguments read in from an options file.  Allow the semicolon (';') to
 * be a comment character.  This can be changed by  the user whenn using a new option
 * called -comment followed by the desired character.  For example, -comment #.
 *
 */

extern size_t make_argv( _TCHAR commentChar,
                         LPTSTR arglist,                /* argument list                     */
                         LPTSTR*static_argv,            /* pointer to argv to use            */
                         size_t max_static_entries,     /* maximum number of entries allowed */
                         size_t starting_entry,         /* entry in argv to begin placing    */
                         int    from_dll );             /* blat called as .dll               */

#endif  // #ifndef __MAKEARGV_HPP__
