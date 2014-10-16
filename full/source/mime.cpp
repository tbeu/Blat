/*
    mime.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>

#include "blat.h"

#if BLAT_LITE
#else
extern _TCHAR eightBitMimeSupported;
extern _TCHAR eightBitMimeRequested;
extern _TCHAR binaryMimeSupported;
//extern _TCHAR binaryMimeRequested;
#endif

// MIME Quoted-Printable Content-Transfer-Encoding

int CheckIfNeedQuotedPrintable(LPTSTR pszStr, int inHeader)
{
    /* get the length of a line created that way */
    int i;

    if ( inHeader ) {  // 8bit MIME does not apply to header information.
        for ( i = 0; pszStr[i]; i++ ) {
            if ( (pszStr[i] == __T('\t')) ||
                 (pszStr[i] == __T('\r')) ||
                 (pszStr[i] == __T('\n')) )
                continue;

            if ( ((_TUCHAR)pszStr[i] <  32  ) ||
                 ((_TUCHAR)pszStr[i] >= 127 ) ) {
                return (TRUE);
            }
        }
    } else {
        for ( i = 0; pszStr[i]; i++ ) {

#if defined(_UNICODE) || defined(UNICODE)
            if ( (_TUCHAR)pszStr[i] > 0x00FF )
                return (TRUE);
#endif
            if ( pszStr[i] == __T('\r') ) {
            } else
            if ( pszStr[i] == __T('\n') ) {
            } else
            if ( pszStr[i] == 32 ) {
                if ( !pszStr[i+1]          ||
                     (pszStr[i+1] == __T('\r')) ||
                     (pszStr[i+1] == __T('\n')) ) {
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
                if ( !pszStr[i] || ((binaryMimeSupported < 2) && (pszStr[i] == __T('='))) ) {
                    return (TRUE);
                }
            } else
#endif
            if ( (pszStr[i] == __T('=') ) ||  /* 61 */
                 (pszStr[i] == __T('?') ) ||  /* 63 */
                 (pszStr[i] == __T('!') ) ||  // Recommended by RFC1341 and 2045
                 (pszStr[i] == __T('"') ) ||
                 (pszStr[i] == __T('#') ) ||
                 (pszStr[i] == __T('$') ) ||
                 (pszStr[i] == __T('@') ) ||
                 (pszStr[i] == __T('[') ) ||
                 (pszStr[i] == __T('\\')) ||
                 (pszStr[i] == __T(']') ) ||
                 (pszStr[i] == __T('^') ) ||
                 (pszStr[i] == __T('`') ) ||
                 (pszStr[i] == __T('{') ) ||
                 (pszStr[i] == __T('|') ) ||
                 (pszStr[i] == __T('}') ) ||
                 (pszStr[i] == __T('~') ) ||
                 (pszStr[i] == __T('\x7F') ) ||
                 ((_TUCHAR)pszStr[i] <  __T(' ') ) ) {
                return (TRUE);
            }

            if ( ((_TUCHAR)pszStr[i] > __T('\x7F'))
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

int GetLengthQuotedPrintable(LPTSTR pszStr, int inHeader)
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
            if ( pszStr[i] == __T('_') ) {
                iLen += 3;
            } else
            if ( ((pszStr[i] >= __T('a')) && (pszStr[i] <= __T('z'))) ||
                 ((pszStr[i] >= __T('A')) && (pszStr[i] <= __T('Z'))) ||
                 ((pszStr[i] >= __T('0')) && (pszStr[i] <= __T('9'))) ||
                 (pszStr[i] == __T(' ') ) ||
                 (pszStr[i] == __T('!') ) ||
                 (pszStr[i] == __T('*') ) ||
                 (pszStr[i] == __T('+') ) ||
                 (pszStr[i] == __T('-') ) ||
                 (pszStr[i] == __T('/') ) ) {
                iLen++;
            } else {
                iLen += 3;
            }
        }
    } else {
        for ( i = 0; pszStr[i]; i++ ) {
            if ( pszStr[i] == __T('\r') ) {
                // iLen += 0;
            } else
            if ( pszStr[i] == __T('\n') ) {
                iLen += 2;
            } else
            if ( pszStr[i] == __T(' ') ) {
                if ( !pszStr[i+1]          ||
                     (pszStr[i+1] == __T('\r')) ||
                     (pszStr[i+1] == __T('\n')) ) {
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
                if ( !pszStr[i] || ((binaryMimeSupported < 2) && (pszStr[i] == __T('='))) ) {
                    iLen += 3;
                } else
                    iLen++;
            } else
#endif
            if ( (pszStr[i] == __T('=') ) ||  /* 61 */
                 (pszStr[i] == __T('?') ) ||  /* 63 */
                 (pszStr[i] == __T('!') ) ||  // Recommended by RFC1341 and 2045
                 (pszStr[i] == __T('"') ) ||
                 (pszStr[i] == __T('#') ) ||
                 (pszStr[i] == __T('$') ) ||
                 (pszStr[i] == __T('@') ) ||
                 (pszStr[i] == __T('[') ) ||
                 (pszStr[i] == __T('\\')) ||
                 (pszStr[i] == __T(']') ) ||
                 (pszStr[i] == __T('^') ) ||
                 (pszStr[i] == __T('`') ) ||
                 (pszStr[i] == __T('{') ) ||
                 (pszStr[i] == __T('|') ) ||
                 (pszStr[i] == __T('}') ) ||
                 (pszStr[i] == __T('~') ) ||
                 (pszStr[i] == __T('\x7F') ) ||
                 (pszStr[i] <  __T(' ') ) ) {
                iLen += 3;
            } else
            if ( (pszStr[i] > __T('\x7F') )
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
    size_t length;
    int    CurrPos;
    LPTSTR pszStr;
    _TCHAR tmpstr[4];

    pszStr = source.Get();
    length = source.Length();

    CurrPos = 0;

    if ( inHeader ) {  // 8bit MIME does apply to header information.
        for ( ; length; length-- ) {
            _TCHAR ThisChar = *pszStr++;

/*
 * From RFC 2047
 *
 *  In this case the set of characters that may be used in a "Q"-encoded
 *  'encoded-word' is restricted to: <upper and lower case ASCII
 *  letters, decimal digits, "!", "*", "+", "-", "/", "=", and "_"
 *  (underscore, ASCII 95.)>.
 */
            if ( ThisChar == __T(' ') ) {     // Convert spaces to underscores?
                out.Add( __T('_') );
                CurrPos++;
            } else
            if ( ThisChar == __T('_') ) {     // Have an underscore?
                _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar );
                out.Add( tmpstr );
                CurrPos += 3;
            } else
            if ( ((ThisChar >= __T('a')) && (ThisChar <= __T('z'))) ||
                 ((ThisChar >= __T('A')) && (ThisChar <= __T('Z'))) ||
                 ((ThisChar >= __T('0')) && (ThisChar <= __T('9'))) ||
                 (ThisChar == __T('!') ) ||
                 (ThisChar == __T('*') ) ||
                 (ThisChar == __T('+') ) ||
                 (ThisChar == __T('-') ) ||
                 (ThisChar == __T('/') ) ) {
                out.Add( ThisChar );
                CurrPos++;
            } else {
                _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar );
                out.Add( tmpstr );
                CurrPos += 3;
            }
        }
    } else {
        for ( ; length; length-- ) {
            _TCHAR ThisChar = *pszStr++;

            if ( ThisChar == __T('\r') )
                continue;

            if ( ThisChar == __T('\n') ) {
                out.Add( __T("\r\n") );
                CurrPos = 0;
                continue;
            }
            if ( ThisChar == __T(' ') ) {
                if ( (length <= 1)     ||
                     (*pszStr == __T('\r')) ||
                     (*pszStr == __T('\n')) ) {
#if BLAT_LITE
#else
                    if ( binaryMimeSupported == 2 ) {
                        out.Add( ThisChar );
                        CurrPos++;
                    } else
#endif
                    {
                        _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar );
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
                if ( !ThisChar || ((binaryMimeSupported < 2) && (ThisChar == __T('='))) ) {
                    _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar );
                    out.Add( tmpstr );
                    CurrPos += 3;
                } else
                    out.Add( ThisChar );
                    CurrPos++;
            } else
#endif
            if ( (ThisChar == __T('=') ) ||  /* 61 */
                 (ThisChar == __T('?') ) ||  /* 63 */
                 (ThisChar == __T('!') ) ||  // Recommended by RFC1341 and 2045
                 (ThisChar == __T('"') ) ||
                 (ThisChar == __T('#') ) ||
                 (ThisChar == __T('$') ) ||
                 (ThisChar == __T('@') ) ||
                 (ThisChar == __T('[') ) ||
                 (ThisChar == __T('\\')) ||
                 (ThisChar == __T(']') ) ||
                 (ThisChar == __T('^') ) ||
                 (ThisChar == __T('`') ) ||
                 (ThisChar == __T('{') ) ||
                 (ThisChar == __T('|') ) ||
                 (ThisChar == __T('}') ) ||
                 (ThisChar == __T('~') ) ||
                 (ThisChar == __T('\x7F') ) ||
                 (ThisChar <  __T(' ') ) ) {
                _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar );
                out.Add( tmpstr );
                CurrPos += 3;
            } else
            if ( ((ThisChar < 0) || (ThisChar > 127))
#if BLAT_LITE
#else
                 && (!eightBitMimeSupported || !eightBitMimeRequested)
#endif
               ) {
                _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar );
                out.Add( tmpstr );
                CurrPos += 3;
            } else {
                out.Add( ThisChar );
                CurrPos++;
            }

            if ( CurrPos > 71 ) {
                out.Add( __T("=\r\n") ); /* Add soft line break */
                CurrPos = 0;
            }
        }

        if ( CurrPos > 71 ) {
            out.Add( __T("=\r\n") ); /* Add soft line break */
            CurrPos = 0;
        }
    }
}
