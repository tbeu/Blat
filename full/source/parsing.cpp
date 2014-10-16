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
#include <string.h>

#include "blat.h"
#include "common_data.h"


extern void printMsg(COMMON_DATA & CommonData, LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison


void parseCommaDelimitString ( COMMON_DATA & CommonData, LPTSTR source, Buf & parsed_strings, int pathNames )
{
    LPTSTR tmpstr;
    LPTSTR srcptr;
    LPTSTR endptr;
    size_t len, startLen;

    parsed_strings.Clear();

    len = _tcslen( source );
    if ( len ) {
        int x;

        srcptr = source;
        for ( ; ; ) {
            if ( (srcptr = _tcschr( srcptr, __T('%') )) != NULL ) {
                _TCHAR holdingstr[3];

                holdingstr[0] = srcptr[1];
                holdingstr[1] = srcptr[2];
                holdingstr[2] = __T('\0');
                _stscanf( holdingstr, __T("%02x"), &x );
                *srcptr = (_TCHAR)x;
                _tcscpy( srcptr+1, srcptr+3 );
                srcptr++;
            }
            else
                break;
        }

        srcptr = source;
        for ( ; ; ) {
            if ( (srcptr = _tcschr( srcptr, __T('&') )) != NULL ) {
                size_t len1;
                static struct {
                    _TCHAR   replacementChar;
                    LPTSTR   str;
                } htmlStrings[] = { { __T('\x27'), __T("&apostrophe;") },
                                    { __T('&')   , __T("&amp;")        },
                                    { __T('*')   , __T("&asterisk;")   },
                                    { __T(':')   , __T("&colon;")      },
                                    { __T(',')   , __T("&comma;")      },
                                    { __T('@')   , __T("&commat;")     },
                                    { __T('$')   , __T("&dollar;")     },
                                    { __T('=')   , __T("&equals;")     },
                                    { __T('!')   , __T("&excl;")       },
                                    { __T('-')   , __T("&hyphen;")     },
                                    { __T('(')   , __T("&lpar;")       },
                                    { __T('[')   , __T("&lsqb;")       },
                                    { __T('%')   , __T("&percent;")    },
                                    { __T('.')   , __T("&period;")     },
                                    { __T('+')   , __T("&plus;")       },
                                    { __T('?')   , __T("&quest;")      },
                                    { __T('"')   , __T("&quot;")       },
                                    { __T(')')   , __T("&rpar;")       },
                                    { __T(']')   , __T("&rsqb;")       },
                                    { __T(';')   , __T("&semi;")       },
                                    { __T('|')   , __T("&verbar;")     },
                                    { __T('#')   , __T("&pound;")      },
                                    { __T('~')   , __T("&tilde;")      },
                                    { __T('<')   , __T("&lt;")         },
                                    { __T('>')   , __T("&gt;")         }
                                  };
                #define HTML_STRINGS_COUNT (sizeof(htmlStrings) / sizeof(htmlStrings[0]))

                if ( srcptr[1] == __T('#') ) {
                    int val;

                    val = 0;
                    for ( x = 2; ; x++ ) {
                        if ( _istdigit( srcptr[x] ) ) {
                            val = (val * 10) + (srcptr[x] - __T('0'));
                        }
                        else
                            break;
                    }
                    if ( srcptr[x] == __T(';') ) {
                        if ( val < (1 << (sizeof(_TCHAR)*8)) ) {
                            srcptr[0] = (_TCHAR)val;
                            _tcscpy( srcptr+1, srcptr+1+x );
                        }
                    }
                }
                for ( x = 0; x < HTML_STRINGS_COUNT; x++ ) {
                    len1 = _tcslen( htmlStrings[x].str );
                    if ( _memicmp( srcptr, htmlStrings[x].str, len1*sizeof(_TCHAR) ) == 0 ) {
                        srcptr[0] = htmlStrings[x].replacementChar;
                        _tcscpy( srcptr+1, srcptr+len1 );
                    }
                }
                srcptr++;
            }
            else
                break;
        }
        len = _tcslen( source );
    }
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

        if ( *srcptr == CommonData.commentChar ) {
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
        parsed_strings.Add( srcptr, len + 1 );
    }

    parsed_strings.Add( __T('\0') ); // The end of strings is identified by a null.
    free (tmpstr);
}
