/*
    mime.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "blat.h"

#if BLAT_LITE
#else
extern char eightBitMimeSupported;
extern char eightBitMimeRequested;
extern char binaryMimeSupported;
//extern char binaryMimeRequested;
#endif

// MIME Quoted-Printable Content-Transfer-Encoding

int CheckIfNeedQuotedPrintable(unsigned char *pszStr, int inHeader)
{
    /* get the length of a line created that way */
    int i;

    if ( inHeader ) {  // 8bit MIME does not apply to header information.
        for ( i = 0; pszStr[i]; i++ ) {
            if ( (pszStr[i] == '\t') ||
                 (pszStr[i] == '\r') ||
                 (pszStr[i] == '\n') )
                continue;

            if ( (pszStr[i] <  32  ) ||
                 (pszStr[i] >= 127 ) ) {
                return (TRUE);
            }
        }
    } else {
        for ( i = 0; pszStr[i]; i++ ) {
            if ( pszStr[i] == 13 /* '\r' */ ) {
            } else
            if ( pszStr[i] == 10 /* '\n' */ ) {
            } else
            if ( pszStr[i] == 32 ) {
                if ( !pszStr[i+1]          ||
                     (pszStr[i+1] == '\r') ||
                     (pszStr[i+1] == '\n') ) {
#if BLAT_LITE
#else
                    if ( binaryMimeSupported < 2 )
#endif
                        return (TRUE);
                }
            } else
#if BLAT_LITE
#else
            if ( binaryMimeSupported /* && binaryMimeRequested */ ) {
                if ( !pszStr[i] || ((binaryMimeSupported < 2) && (pszStr[i] == '=')) ) {
                    return (TRUE);
                }
            } else
#endif
            if ( (pszStr[i] == '=' ) ||  /* 61 */
                 (pszStr[i] == '?' ) ||  /* 63 */
                 (pszStr[i] == '!' ) ||  // Recommended by RFC1341 and 2045
                 (pszStr[i] == '"' ) ||
                 (pszStr[i] == '#' ) ||
                 (pszStr[i] == '$' ) ||
                 (pszStr[i] == '@' ) ||
                 (pszStr[i] == '[' ) ||
                 (pszStr[i] == '\\') ||
                 (pszStr[i] == ']' ) ||
                 (pszStr[i] == '^' ) ||
                 (pszStr[i] == '`' ) ||
                 (pszStr[i] == '{' ) ||
                 (pszStr[i] == '|' ) ||
                 (pszStr[i] == '}' ) ||
                 (pszStr[i] == '~' ) ||
                 (pszStr[i] == 127 ) ||
                 (pszStr[i] <  32  ) ) {
                return (TRUE);
            }

            if ( (pszStr[i] > 127)
#if BLAT_LITE
#else
                  && (!eightBitMimeSupported || !eightBitMimeRequested)
#endif
               ) {
                return (TRUE);
            }
        }
    }

    return(FALSE);
}

int GetLengthQuotedPrintable(unsigned char *pszStr, int inHeader)
{
    /* get the length of a line created that way */
    int i;
    int iLen;

    iLen = 0;
    if ( inHeader ) {  // 8bit MIME does apply to header information.
        for ( i = 0; pszStr[i]; i++ ) {
/*
 * From RFC 2047
 *
 *  In this case the set of characters that may be used in a "Q"-encoded
 *  'encoded-word' is restricted to: <upper and lower case ASCII
 *  letters, decimal digits, "!", "*", "+", "-", "/", "=", and "_"
 *  (underscore, ASCII 95.)>.
 */
            if ( pszStr[i] == '_' ) {
                iLen += 3;
            } else
            if ( ((pszStr[i] >= 'a') && (pszStr[i] <= 'z')) ||
                 ((pszStr[i] >= 'A') && (pszStr[i] <= 'Z')) ||
                 ((pszStr[i] >= '0') && (pszStr[i] <= '9')) ||
                 (pszStr[i] == 32  ) ||
                 (pszStr[i] == '!' ) ||
                 (pszStr[i] == '*' ) ||
                 (pszStr[i] == '+' ) ||
                 (pszStr[i] == '-' ) ||
                 (pszStr[i] == '/' ) ) {
                iLen++;
            } else {
                iLen += 3;
            }
        }
    } else {
        for ( i = 0; pszStr[i]; i++ ) {
            if ( pszStr[i] == 13 /* '\r' */ ) {
                // iLen += 0;
            } else
            if ( pszStr[i] == 10 /* '\n' */ ) {
                iLen += 2;
            } else
            if ( pszStr[i] == 32 ) {
                if ( !pszStr[i+1]          ||
                     (pszStr[i+1] == '\r') ||
                     (pszStr[i+1] == '\n') ) {
#if BLAT_LITE
#else
                    if ( binaryMimeSupported == 2 )
                        iLen++;
                    else
#endif
                        iLen += 3;
                } else
                    iLen++;
            } else
#if BLAT_LITE
#else
            if ( binaryMimeSupported /* && binaryMimeRequested */ ) {
                if ( !pszStr[i] || ((binaryMimeSupported < 2) && (pszStr[i] == '=')) ) {
                    iLen += 3;
                } else
                    iLen++;
            } else
#endif
            if ( (pszStr[i] == '=' ) ||  /* 61 */
                 (pszStr[i] == '?' ) ||  /* 63 */
                 (pszStr[i] == '!' ) ||  // Recommended by RFC1341 and 2045
                 (pszStr[i] == '"' ) ||
                 (pszStr[i] == '#' ) ||
                 (pszStr[i] == '$' ) ||
                 (pszStr[i] == '@' ) ||
                 (pszStr[i] == '[' ) ||
                 (pszStr[i] == '\\') ||
                 (pszStr[i] == ']' ) ||
                 (pszStr[i] == '^' ) ||
                 (pszStr[i] == '`' ) ||
                 (pszStr[i] == '{' ) ||
                 (pszStr[i] == '|' ) ||
                 (pszStr[i] == '}' ) ||
                 (pszStr[i] == '~' ) ||
                 (pszStr[i] == 127 ) ||
                 (pszStr[i] <  32  ) ) {
                iLen += 3;
            } else
            if ( (pszStr[i] > 127)
#if BLAT_LITE
#else
                  && (!eightBitMimeSupported || !eightBitMimeRequested)
#endif
               ) {
                iLen += 3;
            } else {
                iLen++;
            }
        }
    }

    return(iLen);
}

void ConvertToQuotedPrintable(Buf & source, Buf & out, int inHeader)
{
    DWORD  length;
    int    CurrPos;
    char * pszStr;
    char   tmpstr[4];

    pszStr = source.Get();
    length = source.Length();

    CurrPos = 0;

    if ( inHeader ) {  // 8bit MIME does apply to header information.
        for ( ; length; length-- ) {
            unsigned char ThisChar = *pszStr++;

/*
 * From RFC 2047
 *
 *  In this case the set of characters that may be used in a "Q"-encoded
 *  'encoded-word' is restricted to: <upper and lower case ASCII
 *  letters, decimal digits, "!", "*", "+", "-", "/", "=", and "_"
 *  (underscore, ASCII 95.)>.
 */
            if ( ThisChar == 32 ) {     // Convert spaces to underscores?
                out.Add( '_' );
                CurrPos++;
            } else
            if ( ThisChar == '_' ) {    // Have an underscore?
                sprintf( tmpstr, "=%02X", ThisChar );
                out.Add( tmpstr );
                CurrPos += 3;
            } else
            if ( ((ThisChar >= 'a') && (ThisChar <= 'z')) ||
                 ((ThisChar >= 'A') && (ThisChar <= 'Z')) ||
                 ((ThisChar >= '0') && (ThisChar <= '9')) ||
                 (ThisChar == '!' ) ||
                 (ThisChar == '*' ) ||
                 (ThisChar == '+' ) ||
                 (ThisChar == '-' ) ||
                 (ThisChar == '/' ) ) {
                out.Add( ThisChar );
                CurrPos++;
            } else {
                sprintf( tmpstr, "=%02X", ThisChar );
                out.Add( tmpstr );
                CurrPos += 3;
            }
        }
    } else {
        for ( ; length; length-- ) {
            unsigned char ThisChar = *pszStr++;

            if ( ThisChar == 13 /* '\r' */ )
                continue;

            if ( ThisChar == 10 /* '\n' */ ) {
                out.Add( "\r\n" );
                CurrPos = 0;
                continue;
            }
            if ( ThisChar == 32 ) {
                if ( (length <= 1)     ||
                     (*pszStr == '\r') ||
                     (*pszStr == '\n') ) {
#if BLAT_LITE
#else
                    if ( binaryMimeSupported == 2 ) {
                        out.Add( ThisChar );
                        CurrPos++;
                    } else
#endif
                    {
                        sprintf( tmpstr, "=%02X", ThisChar );
                        out.Add( tmpstr );
                        CurrPos += 3;
                    }
                } else {
                    out.Add( ThisChar );
                    CurrPos++;
                }
            } else
#if BLAT_LITE
#else
            if ( binaryMimeSupported /* && binaryMimeRequested */ ) {
                if ( !ThisChar || ((binaryMimeSupported < 2) && (ThisChar == '=')) ) {
                    sprintf( tmpstr, "=%02X", ThisChar );
                    out.Add( tmpstr );
                    CurrPos += 3;
                } else
                    out.Add( ThisChar );
                    CurrPos++;
            } else
#endif
            if ( (ThisChar == '=' ) ||  /* 61 */
                 (ThisChar == '?' ) ||  /* 63 */
                 (ThisChar == '!' ) ||  // Recommended by RFC1341 and 2045
                 (ThisChar == '"' ) ||
                 (ThisChar == '#' ) ||
                 (ThisChar == '$' ) ||
                 (ThisChar == '@' ) ||
                 (ThisChar == '[' ) ||
                 (ThisChar == '\\') ||
                 (ThisChar == ']' ) ||
                 (ThisChar == '^' ) ||
                 (ThisChar == '`' ) ||
                 (ThisChar == '{' ) ||
                 (ThisChar == '|' ) ||
                 (ThisChar == '}' ) ||
                 (ThisChar == '~' ) ||
                 (ThisChar == 127 ) ||
                 (ThisChar <  32  ) ) {
                sprintf( tmpstr, "=%02X", ThisChar );
                out.Add( tmpstr );
                CurrPos += 3;
            } else
            if ( (ThisChar > 127)
#if BLAT_LITE
#else
                 && (!eightBitMimeSupported || !eightBitMimeRequested)
#endif
               ) {
                sprintf( tmpstr, "=%02X", ThisChar );
                out.Add( tmpstr );
                CurrPos += 3;
            } else {
                out.Add( ThisChar );
                CurrPos++;
            }

            if ( CurrPos > 71 ) {
                out.Add( "=\r\n" ); /* Add soft line break */
                CurrPos = 0;
            }
        }

        if ( CurrPos > 71 ) {
            out.Add( "=\r\n" ); /* Add soft line break */
            CurrPos = 0;
        }
    }
}
