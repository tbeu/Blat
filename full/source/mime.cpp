/*
    mime.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>

#include "blat.h"
#include "common_data.h"
#include "blatext.hpp"
#include "macros.h"
#include "mime.hpp"

// MIME Quoted-Printable Content-Transfer-Encoding

int CheckIfNeedQuotedPrintable(COMMON_DATA & CommonData, LPTSTR pszStr, int inHeader)
{
    FUNCTION_ENTRY();
#if BLAT_LITE
    CommonData = CommonData;
#endif
    /* get the length of a line created that way */
    int i;
    int returnValue;

    returnValue = FALSE;

    if ( inHeader ) {  // 8bit MIME does not apply to header information.
        for ( i = 0; pszStr[i]; i++ ) {
            if ( (pszStr[i] == __T('\t')) ||
                 (pszStr[i] == __T('\r')) ||
                 (pszStr[i] == __T('\n')) )
                continue;

            if ( ((_TUCHAR)pszStr[i] <  32  ) ||
                 ((_TUCHAR)pszStr[i] >= 127 ) ) {
                returnValue = TRUE;
                break;
            }
        }
    } else {
        for ( i = 0; pszStr[i]; i++ ) {

#if defined(_UNICODE) || defined(UNICODE)
            if ( (_TUCHAR)pszStr[i] > 0x00FF ) {
                returnValue = TRUE;
                break;
            }
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
                    if ( CommonData.binaryMimeSupported < 2 )
#endif
                    {
                        returnValue = TRUE;
                        break;
                    }
                }
            } else
#if BLAT_LITE
#else
            if ( CommonData.binaryMimeSupported /* && CommonData.binaryMimeRequested */ ) {
                if ( !pszStr[i] || ((CommonData.binaryMimeSupported < 2) && (pszStr[i] == __T('='))) ) {
                    returnValue = TRUE;
                    break;
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
                returnValue = TRUE;
                break;
            }

            if ( ((_TUCHAR)pszStr[i] > __T('\x7F'))
#if BLAT_LITE
#else
                  && (!CommonData.eightBitMimeSupported || !CommonData.eightBitMimeRequested)
#endif
               ) {
                returnValue = TRUE;
                break;
            }
        }
    }

    FUNCTION_EXIT();
    return returnValue;
}

int GetLengthQuotedPrintable(COMMON_DATA & CommonData, LPTSTR pszStr, int inHeader)
{
    FUNCTION_ENTRY();
    /* get the length of a line created that way */
    int i;
    int iLen;

#if BLAT_LITE
    CommonData = CommonData;
#endif
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
                    if ( CommonData.binaryMimeSupported == 2 )
                        iLen++;
                    else
#endif
                        iLen += 3;
                } else
                    iLen++;
            } else
#if BLAT_LITE
#else
            if ( CommonData.binaryMimeSupported /* && CommonData.binaryMimeRequested */ ) {
                if ( !pszStr[i] || ((CommonData.binaryMimeSupported < 2) && (pszStr[i] == __T('='))) ) {
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
                  && (!CommonData.eightBitMimeSupported || !CommonData.eightBitMimeRequested)
#endif
               ) {
                iLen += 3;
            } else {
                iLen++;
            }
        }
    }

    FUNCTION_EXIT();
    return(iLen);
}

void ConvertToQuotedPrintable(COMMON_DATA & CommonData, Buf & source, Buf & out, int inHeader)
{
    FUNCTION_ENTRY();
    size_t length;
    int    CurrPos;
    LPTSTR pszStr;
    _TCHAR tmpstr[8];

#if BLAT_LITE
    CommonData = CommonData;
#endif
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
                _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar & 0xFF );
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
                _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar & 0xFF );
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
                    if ( CommonData.binaryMimeSupported == 2 ) {
                        out.Add( ThisChar );
                        CurrPos++;
                    } else
#endif
                    {
                        _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar & 0xFF );
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
            if ( CommonData.binaryMimeSupported /* && CommonData.binaryMimeRequested */ ) {
                if ( !ThisChar || ((CommonData.binaryMimeSupported < 2) && (ThisChar == __T('='))) ) {
                    _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar & 0xFF );
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
                _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar & 0xFF );
                out.Add( tmpstr );
                CurrPos += 3;
            } else
            if ( (ThisChar == __T('\t')) || ((ThisChar >= 32) && (ThisChar < 127)) ) {
                out.Add( ThisChar );
                CurrPos++;
            } else
#if BLAT_LITE
#else
            if ( CommonData.eightBitMimeSupported && CommonData.eightBitMimeRequested ) {
                out.Add( ThisChar );
                CurrPos++;
            } else
#endif
//#if defined(_UNICODE) || defined(UNICODE)
//            if (ThisChar > 0x00FF) {
//                _stprintf( tmpstr, __T("&#u;"), ThisChar );
//                out.Add( tmpstr );
//                CurrPos += _tcslen( tmpstr );
//            } else
//#endif
            {
                _stprintf( tmpstr, __T("=%02") _TCHAR_PRINTF_FORMAT __T("X"), ThisChar & 0xFF );
                out.Add( tmpstr );
                CurrPos += 3;
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
    FUNCTION_EXIT();
}
