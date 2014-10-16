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


extern void printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison
extern char commentChar;


void parseCommaDelimitString ( char * source, Buf & parsed_strings, int pathNames )
{
    char * tmpstr;
    char * srcptr;
    char * endptr;
    size_t len, startLen;

    parsed_strings.Clear();
    parsed_strings.Add( "" );

    len = strlen( source );
    tmpstr = (char *)malloc( len + 2 );
    if ( !tmpstr )
        return;

    strcpy( tmpstr, source );
    tmpstr[len + 1] = 0;
    srcptr = tmpstr;
    endptr = tmpstr + len;

    for ( ; (srcptr < endptr) && *srcptr; srcptr += len + 1 ) {
        // if there's only one token left, then len will = startLen,
        // and we'll iterate once only
        int foundQuote;

        while ( *srcptr && (strchr (" ,\n\t\r", *srcptr)) ) // eat leading white space
            srcptr++;

        if ( *srcptr == commentChar ) {
            startLen = strlen (srcptr);
            len = 0;
            for ( ; srcptr[len] && (len < startLen); len++ ) {
                if ( srcptr[len] == '\n' )
                    break;
            }
            continue;
        }

        if ( !*srcptr )
            break;

        startLen = strlen (srcptr);
        foundQuote = FALSE;
        len = 0;
        for ( ; srcptr[len] && (len < startLen); len++ ) {
            if ( srcptr[len] == '\n' )
                break;

            if ( srcptr[len] == '\r' )
                break;

            if ( srcptr[len] == '\t' )
                break;

            if ( !pathNames && (srcptr[len] == '\\') ) {
                len++;  // Skip the following byte.
                continue;
            }

            if ( srcptr[len] == '"' ) {
                foundQuote = (foundQuote == FALSE);
                continue;
            }

            if ( srcptr[len] == ',' )
                if ( !foundQuote )
                    break;
        }

        if ( len ) {
            while ( srcptr[len-1] == ' ' )  // eat trailing white space
                len--;
        }

        srcptr[len] = '\0';             // replace delim with NULL char
        if ( pathNames &&
             (srcptr[0]     == '"') &&
             (srcptr[len-1] == '"') ) {
            srcptr[len-1] = 0;
            len -= 2;
            srcptr++;
        }
        parsed_strings.Add( srcptr, (int)len + 1 );
    }

    parsed_strings.Add( '\0' ); // The end of strings is identified by a null.
    free (tmpstr);
}
