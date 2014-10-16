/*
    filetype.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blat.h"

#if BLAT_LITE
#else
extern _TCHAR       uuencode;
extern _TCHAR       base64;
extern _TCHAR       yEnc;
#endif
extern _TCHAR       mime;


extern void fixup(LPTSTR string, Buf * tempstring2, int headerLen, int linewrap);
extern void fixupFileName ( LPTSTR filename, Buf & outString, int headerLen, int linewrap );

static struct {
    LPTSTR extension;
    LPTSTR contentType;
} defaultContentTypes[] = { { __T(".pdf"), __T("application/pdf")          },
                            { __T(".xls"), __T("application/vnd.ms-excel") },
                            { __T(".gif"), __T("image/gif")                },
                            { __T(".jpg"), __T("image/jpeg")               },
                            { __T(".bmp"), __T("image/bmp")                },
                            { __T(".png"), __T("image/png")                },
                            {    NULL    ,    NULL                         }  // Last entry
                          };


LPTSTR getShortFileName (LPTSTR fileName)
{
    LPTSTR sFileName;
    LPTSTR shortname;

    sFileName = fileName;
    shortname = _tcsrchr(sFileName,__T('\\'));
    if ( shortname ) sFileName = shortname + 1;

    shortname = _tcsrchr(sFileName,__T('/'));
    if ( shortname ) sFileName = shortname + 1;

    shortname = _tcsrchr(sFileName,__T(':'));
    if ( shortname ) sFileName = shortname + 1;

    return sFileName;
}

void getContentType (Buf & sDestBuffer, LPTSTR foundType, LPTSTR defaultType, LPTSTR sFileName)
{
    // Toby Korn (tkorn@snl.com)
    // SetFileType examines the file name sFileName for known file extensions and sets
    // the content type line appropriately.  This is returned in sDestBuffer.
    // sDestBuffer must have it's own memory (I don't alloc any here).

#define CONTENT_TYPE_LENGTH 80
    _TCHAR        sType [CONTENT_TYPE_LENGTH];
    _TCHAR        sExt [_MAX_PATH];
    LONG          lResult;
    HKEY          key = NULL;
    unsigned long lTypeSize;
    unsigned long lStringType = REG_SZ;
    LPTSTR        tmpStr;
    Buf           tempstring;
    unsigned      ix;
    Buf           shortNameBuf;

    lResult = ~ERROR_SUCCESS;

    // Find the last '.' in the filename
    tmpStr = _tcsrchr( sFileName, __T('.'));
    if ( tmpStr ) {
        // Found the extension type.
        _tcscpy(sExt, tmpStr);
        lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, (LPCTSTR) sExt, 0, KEY_READ, &key);
        if ( lResult == ERROR_SUCCESS ) {
            lTypeSize = CONTENT_TYPE_LENGTH;
            lResult = RegQueryValueEx(key, __T("Content Type"), NULL, &lStringType, (BYTE *)sType,
                                      &lTypeSize);
            RegCloseKey (key);
            tmpStr = sType;
        }
    }
    // keep a couple of hard coded ones in case the registry is missing something.
    if ( lResult != ERROR_SUCCESS ) {
        for ( ix = 0; ; ix++ ) {
            if ( !defaultContentTypes[ix].extension ) {
                if ( defaultType && *defaultType )
                    tmpStr = defaultType;
                else
                    tmpStr = __T("application/octet-stream");

                break;
            }
            if ( _tcsicmp(sExt, defaultContentTypes[ix].extension) == 0 ) {
                tmpStr = defaultContentTypes[ix].contentType;
                break;
            }
        }
    }

    fixupFileName( sFileName, shortNameBuf, 7, TRUE );
    sDestBuffer    = __T("Content-Type: ");
    sDestBuffer.Add( tmpStr );
    sDestBuffer.Add( __T(";\r\n name=\"") );
    sDestBuffer.Add( shortNameBuf );
    sDestBuffer.Add( __T("\"\r\n") );

    if ( foundType )
        _tcscpy( foundType, tmpStr );
}
