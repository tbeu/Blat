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

extern void fixup(char * string, Buf * tempstring2, int headerLen, int linewrap);

static struct {
    char * extension;
    char * contentType;
} defaultContentTypes[] = { { ".pdf", "application/pdf"          },
                            { ".xls", "application/vnd.ms-excel" },
                            { ".gif", "image/gif"                },
                            { ".jpg", "image/jpeg"               },
                            { ".bmp", "image/bmp"                },
                            { ".png", "image/png"                },
                            { NULL,   NULL                       }  // Last entry
                          };


char * getShortFileName (char * fileName)
{
    char * sFileName;
    char * shortname;

    sFileName = fileName;
    shortname = strrchr(sFileName,'\\');
    if ( shortname ) sFileName = shortname + 1;

    shortname = strrchr(sFileName,'/');
    if ( shortname ) sFileName = shortname + 1;

    shortname = strrchr(sFileName,':');
    if ( shortname ) sFileName = shortname + 1;

    return sFileName;
}

void getContentType (char *sDestBuffer, char *foundType, char *defaultType, char *sFileName)
{
    // Toby Korn (tkorn@snl.com)
    // SetFileType examines the file name sFileName for known file extensions and sets
    // the content type line appropriately.  This is returned in sDestBuffer.
    // sDestBuffer must have it's own memory (I don't alloc any here).

    char          sType [80];
    char          sExt [_MAX_PATH];
    LONG          lResult;
    HKEY          key = NULL;
    unsigned long lTypeSize;
    unsigned long lStringType = REG_SZ;
    char        * tmpStr;
    Buf           tempstring;
    int           ix;

    lResult = ~ERROR_SUCCESS;

    // Find the last '.' in the filename
    tmpStr = strrchr( sFileName, '.');
    if ( tmpStr ) {
        // Found the extension type.
        strcpy(sExt, tmpStr);
        lResult = RegOpenKeyEx(HKEY_CLASSES_ROOT, (LPCTSTR) sExt, 0, KEY_READ, &key);
        if ( lResult == ERROR_SUCCESS ) {
            lTypeSize = (long) sizeof(sType);
            lResult = RegQueryValueEx(key, "Content Type", NULL, &lStringType, (unsigned char *) sType,
                                      &lTypeSize); RegCloseKey (key);
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
                    tmpStr = "application/octet-stream";

                break;
            }
            if ( _stricmp(sExt, defaultContentTypes[ix].extension) == 0 ) {
                tmpStr = defaultContentTypes[ix].contentType;
                break;
            }
        }
    }

    fixup(getShortFileName(sFileName), &tempstring, 7, TRUE);
    sprintf( sDestBuffer, "Content-Type: %s;\r\n name=\"%s\"\r\n", tmpStr, tempstring.Get() );
    if ( foundType )
        strcpy( foundType, tmpStr );
}
