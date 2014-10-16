/*
    makeargv.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdlib.h>
#include <ctype.h>

char commentChar = ';';

/*
 * Parse the arguments read in from an options file.  Allow the semicolon (';') to
 * be a comment character.  This can be changed by  the user whenn using a new option
 * called -comment followed by the desired character.  For example, -comment #.
 *
 */

size_t make_argv( char * arglist,                /* argument list                     */
                  char **static_argv,            /* pointer to argv to use            */
                  size_t max_static_entries,     /* maximum number of entries allowed */
                  size_t starting_entry,         /* entry in argv to begin placing    */
                  int    from_dll )              /* blat called as .dll               */
{
    char *  startArgs;
    char ** argv;
    char *  nextarg;
    size_t  argc;
    size_t  x, y, z;

    startArgs = arglist;
    argv = static_argv;
    for ( argc = starting_entry; argc < max_static_entries; argc++) {
        int foundQuote;

        for ( ; ; startArgs++ )
            if ( *startArgs != ' ' )
                break;

        if ( !*startArgs || (*startArgs == '\r') || (*startArgs == '\n'))
            break;

        if ( *startArgs == commentChar )
            break;

        foundQuote = FALSE;

        /* Parse this entry to determine string length */
        for ( x = 0; startArgs[ x ]; x++ ) {
            if ( startArgs[ x ] == '\\' ) {
                if ( startArgs[ x+1 ] == '"' ) {
                    x++;
                    continue;
                }

                /*
                 * If we're parsing the command line for blat.dll, then we should
                 * not make any special accomodations for the backslash.
                 */
                if ( !from_dll ) {
                    if ( !startArgs[ x+1 ] ) {
                        x++;
                        break;
                    }

                    if ( foundQuote )
                        x++;

                    continue;
                }
            }

            if ( startArgs[ x ] == '"' ) {
                foundQuote = (foundQuote != TRUE );
                continue;
            }

            if ( startArgs[ x ] == ' ' ) {
                if ( foundQuote )
                    continue;

                break;
            }

            if ( startArgs[ x ] == commentChar ) {
                if ( foundQuote )
                    continue;

                // Terminate the string at this comment character.
                for ( y = x; startArgs[y]; startArgs[y++] = 0 )
                    ;

                break;
            }

            if ( startArgs[ x ] == '\r' ) {
                if ( foundQuote )
                    continue;

                break;
            }

            if ( startArgs[ x ] == '\n' ) {
                if ( foundQuote )
                    continue;

                break;
            }
        }

        /* Found end of this argument. */
        nextarg = (char *)malloc( x + 1 );
        if ( !nextarg )
            break;

        foundQuote = FALSE;

        for ( z = y = 0; z < x; z++ ) {
            nextarg[ y ] = startArgs[ z ];

            if ( startArgs[ z ] == '\\' ) {
                if ( startArgs[ z+1 ] == '"' ) {
                    nextarg[ y++ ] = startArgs[ ++z ];
                    continue;
                }

                if ( !from_dll ) {
                    if ( !startArgs[ z+1 ] ) {
                        y++;
                        break;
                    }

                    if ( foundQuote ) {
                        switch ( startArgs[ ++z ] ) {
                        default:
                            nextarg[ y ] = startArgs[ z ];
                            break;

                        case 'a':
                            nextarg[ y ] = '\a';
                            break;

                        case 'b':
                            nextarg[ y ] = '\b';
                            break;

                        case 'f':
                            nextarg[ y ] = '\f';
                            break;

                        case 'n':
                            nextarg[ y ] = '\n';
                            break;

                        case 'r':
                            nextarg[ y ] = '\r';
                            break;

                        case 't':
                            nextarg[ y ] = '\t';
                            break;

                        case 'v':
                            nextarg[ y ] = '\v';
                            break;

                        case 'x':       /* hex conversion */
                            {
                                int hexValue = 0;
                                int c;

                                for ( z++; ; ) {
                                    if ( hexValue > (255/16) )
                                        break;

                                    c = tolower(startArgs[ z ]);
                                    if ( (c < '0') || (c > 'f') )
                                        break;

                                    if ( (c > '9') && (c < 'a') )
                                        break;

                                    if ( c <= '9' )
                                        c -= '9';
                                    else
                                        c -= ('a' - 10);

                                    hexValue = hexValue * 16 + c;
                                    z++;
                                }

                                z--;
                                nextarg[ y ] = (char)hexValue;
                                break;
                            }

                        /* octal conversion */
                        case '0':
                        case '1':
                        case '2':
                        case '3':
                        case '4':
                        case '5':
                        case '6':
                        case '7':
                            {
                                int octalValue = 0;

                                for ( ; ; ) {
                                    if ( octalValue > (255/8) )
                                        break;

                                    if ( (startArgs[ z ] < '0') || (startArgs[ z ] > '7') )
                                        break;

                                    octalValue = octalValue * 8 + (startArgs[ z++ ] - '0');
                                }

                                z--;
                                nextarg[ y ] = (char)octalValue;
                                break;
                            }
                        }

                        y++;
                        continue;
                    }
                }
            }

            if ( startArgs[ z ] == '"' ) {
                foundQuote = (foundQuote != TRUE );
                continue;
            }

            if ( startArgs[ z ] == ' ' ) {
                if ( foundQuote ) {
                    y++;
                    continue;
                }

                break;
            }

            if ( startArgs[ z ] == commentChar ) {
                if ( foundQuote ) {
                    y++;
                    continue;
                }

                break;
            }

            if ( startArgs[ z ] == '\r' ) {
                if ( foundQuote ) {
                    y++;
                    continue;
                }

                break;
            }

            if ( startArgs[ z ] == '\n' ) {
                if ( foundQuote ) {
                    y++;
                    continue;
                }

                break;
            }

            y++;
        }

        nextarg[ y ] = '\0';
        argv[ argc ] = nextarg;
        startArgs += x;
    }

    return argc;
}
