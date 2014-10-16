/*
    parsing.cpp

    This file is for parsing comma delimited strings into individual strings.
    This provides a central source for a common function used for email addresses,
    and attachment names.
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blat.h"


extern void printMsg( LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison
extern _TCHAR commentChar;


void parseCommaDelimitString ( LPTSTR source, Buf & parsed_strings, int pathNames )
{
    LPTSTR tmpstr;
    LPTSTR srcptr;
    LPTSTR endptr;
    size_t len, startLen;

    parsed_strings.Clear();

    len = _tcslen( source );
    tmpstr = (LPTSTR)malloc( (len + 2)*sizeof(_TCHAR) );
    if ( !tmpstr )
        return;

    _tcscpy( tmpstr, source );
    tmpstr[len + 1] = __T('\0');
    srcptr = tmpstr;
    endptr = tmpstr + len;

    for ( ; (srcptr < endptr) && *srcptr; srcptr += len + 1 ) {
        // if there's only one token left, then len will = startLen,
        // and we'll iterate once only
        int foundQuote;

        while ( *srcptr && (_tcschr (__T(" ,\n\t\r"), *srcptr)) ) // eat leading white space
            srcptr++;

        if ( *srcptr == commentChar ) {
            startLen = _tcslen (srcptr);
            len = 0;
            for ( ; srcptr[len] && (len < startLen); len++ ) {
                if ( srcptr[len] == __T('\n') )
                    break;
            }
            continue;
        }

        if ( !*srcptr )
            break;

        startLen = _tcslen (srcptr);
        foundQuote = FALSE;
        len = 0;
        for ( ; srcptr[len] && (len < startLen); len++ ) {
            if ( srcptr[len] == __T('\n') )
                break;

            if ( srcptr[len] == __T('\r') )
                break;

            if ( srcptr[len] == __T('\t') )
                break;

            if ( !pathNames && (srcptr[len] == __T('\\')) ) {
                len++;  // Skip the following byte.
                continue;
            }

            if ( srcptr[len] == __T('"') ) {
                foundQuote = (foundQuote == FALSE);
                continue;
            }

            if ( srcptr[len] == __T(',') )
                if ( !foundQuote )
                    break;
        }

        if ( len ) {
            while ( srcptr[len-1] == __T(' ') )  // eat trailing white space
                len--;
        }

        srcptr[len] = __T('\0');             // replace delim with NULL char
        if ( pathNames &&
             (srcptr[0]     == __T('"')) &&
             (srcptr[len-1] == __T('"')) ) {
            srcptr[len-1] = __T('\0');
            len -= 2;
            srcptr++;
        }
        parsed_strings.Add( srcptr, (int)len + 1 );
    }

    parsed_strings.Add( __T('\0') ); // The end of strings is identified by a null.
    free (tmpstr);
}
