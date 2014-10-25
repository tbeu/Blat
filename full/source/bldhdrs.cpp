/*
    blathdrs.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#ifdef __WATCOMC__
#include <mapi.h>
#else
#include <mapiwin.h>
#endif

#include "blat.h"
#include "common_data.h"
#include "gensock.h"

extern LPTSTR getShortFileName (LPTSTR fileName);
extern int    CheckIfNeedQuotedPrintable(COMMON_DATA & CommonData, LPTSTR pszStr, int inHeader);
extern int    GetLengthQuotedPrintable(COMMON_DATA & CommonData, LPTSTR pszStr, int inHeader);
extern void   ConvertToQuotedPrintable(COMMON_DATA & CommonData, Buf & source, Buf & out, int inHeader);
extern void   base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);
extern void   convertPackedUnicodeToUTF( Buf & sourceText, Buf & outputText, int * utf, LPTSTR charset, int utfRequested );

#if SMART_CONTENT_TYPE
extern void   getContentType(COMMON_DATA & CommonData, Buf & sDestBuffer, LPTSTR foundType, LPTSTR defaultType, LPTSTR sFileName);
#endif
#if SUPPORT_SALUTATIONS
extern void   find_and_strip_salutation( COMMON_DATA & CommonData, Buf & email_addresses );
#endif

extern _TCHAR blatVersion[];
extern _TCHAR blatVersionSuf[];

extern LPTSTR days[];

static _TCHAR base64_marker[] = __T("?B?"); // RFC 2047 section 4.1. The "B" encoding
static _TCHAR mime_marker[]   = __T("?Q?"); // RFC 2047 section 4.2. The "Q" encoding


static _TCHAR abclist[] = __T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");

static LPTSTR months[]  = { __T("Jan"),
                            __T("Feb"),
                            __T("Mar"),
                            __T("Apr"),
                            __T("May"),
                            __T("Jun"),
                            __T("Jul"),
                            __T("Aug"),
                            __T("Sep"),
                            __T("Oct"),
                            __T("Nov"),
                            __T("Dec") };


void incrementBoundary( _TCHAR boundary[] )
{
    int x;

    for ( x = 0; x < 62; x++ )
        if ( boundary[20] == abclist[x] )
            break;

    boundary[20] = abclist[(x + 1) % 62];
}


void decrementBoundary( _TCHAR boundary[] )
{
    int x;

    for ( x = 0; x < 62; x++ )
        if ( boundary[20] == abclist[x] )
            break;

    if ( --x < 0 )
        x = 61;

    boundary[20] = abclist[x];
}


void addStringToHeaderNoQuoting(LPTSTR string, Buf & outS, int & headerLen, int linewrap )
{
    // quoting not necessary
    LPTSTR pStr1;
    LPTSTR pStr2;
    LPTSTR pStr;

    pStr = pStr1 = pStr2 = string;
    for ( ; ; ) {
        for ( ; ; ) {
            pStr = _tcschr(pStr, __T(' '));
            if ( !pStr )
                break;

            if ( linewrap && ((headerLen + pStr - pStr2 ) >= 75) ) {
                if ( pStr1 != pStr2 )
                    pStr = pStr1;
                break;
            }
            pStr1 = pStr;
            pStr++;
        }
        if ( !pStr )
            break;

        outS.Add( pStr2, (size_t)(pStr - pStr2) );
        outS.Add( __T("\r\n ") );
        headerLen = 1;
        pStr++;
        pStr1 = pStr2 = pStr;
    }
    headerLen += (int)_tcslen( pStr2 );
    outS.Add( pStr2 );
}


/*
 * outString==NULL ..modify in place
 * outString!=NULL...don't modify string.  Put new string in outString.
 *
 * creates "=?charset?q?text?=" output from 'string', as defined by RFCs 2045 and 2046,
 *      or "=?charset?b?text?=".
 */

void fixup( COMMON_DATA & CommonData, LPTSTR string, Buf * outString, int headerLen, int linewrap )
{
    int    i, qLen, bLen;
    Buf    outS;
    Buf    fixupString;
    Buf    tempstring;
    size_t charsetLen;
    int    stringOffset;
    int    tempUTF;
    int    utfRequested;
    _TCHAR localCharset[40];

#if BLAT_LITE
#else
    _TCHAR savedEightBitMimeSupported;
    _TCHAR savedBinaryMimeSupported;

    savedEightBitMimeSupported = CommonData.eightBitMimeSupported;
    savedBinaryMimeSupported   = CommonData.binaryMimeSupported;
    if ( CommonData.eightBitMimeSupported )
        utfRequested = 8;
    else
#endif
        utfRequested = 7;

    if ( memcmp(CommonData.charset, __T("utf-"), 4*sizeof(_TCHAR)) == 0 ) {
        localCharset[0] = __T('\0');
        charsetLen   = 0;
    } else {
        _tcscpy( localCharset, CommonData.charset );
        charsetLen = _tcslen( localCharset );
    }
    if ( !charsetLen )
        charsetLen = _tcslen( defaultCharset );

    charsetLen += 7;    // =? ?q? ?=

    stringOffset = 0;
    tempstring.Add( string );
    outS.Clear();
    tempUTF = 0;

    if ( CheckIfNeedQuotedPrintable( CommonData, tempstring.Get(), TRUE ) ) {
        Buf       tmpstr;
        LPTSTR    pStr;

//#if defined(_UNICODE) || defined(UNICODE)
        _TUCHAR * pStr1;
        size_t    x;

        pStr1 = (_TUCHAR *) tempstring.Get();
        for ( x = 0; *pStr1 != __T('\0'); pStr1++ ) {
            if ( (*pStr1 > 0x00FF) || ((memcmp(localCharset, __T("UTF-"), 4*sizeof(_TCHAR)) == 0) && (*pStr1 > 0x007F)) ) {
                tempUTF = NATIVE_16BIT_UTF;
                convertPackedUnicodeToUTF( tempstring, fixupString, &tempUTF, localCharset, utfRequested );
                if ( tempUTF )
                {
                    tempstring = fixupString;
                    charsetLen = 12;    // =?utf-8?q? ?=
                }
                break;
            }
        }
//#endif

#if BLAT_LITE
#else
        CommonData.eightBitMimeSupported = FALSE;
        CommonData.binaryMimeSupported   = 0;
#endif
        do {
            Buf holdingPen;


            fixupString.Clear();
            fixupString.Add( __T("=?") );
            if ( localCharset[0] )
                fixupString.Add( localCharset );
            else
                fixupString.Add( defaultCharset );

            if ( !linewrap )
                headerLen = (int)(73-(fixupString.Length() + (tempstring.Length()*3) + headerLen));

            holdingPen = tempstring;
            i = (int)tempstring.Length();
            pStr = tempstring.Get();

            for ( ; ; ) {
                int encodingLength;

                qLen = GetLengthQuotedPrintable(CommonData, pStr, TRUE );
                bLen = ((i / 3) * 4) + ((i % 3) ? 4 : 0);
#if BLAT_LITE
#else
                if ( CommonData.forcedHeaderEncoding == __T('q') ) {
                    bLen = qLen + 1;
                } else
                if ( CommonData.forcedHeaderEncoding == __T('b') ) {
                    qLen = bLen + 1;
                }
#endif
                if ( qLen <= bLen )
                    encodingLength = qLen;
                else
                    encodingLength = bLen;

                if ( (fixupString.Length() + encodingLength) <= (size_t)(73-headerLen) )
                    break;

                for ( ; ; ) {
                    i--;
                    if ( i < 1 ) {
                        i = (int)tempstring.Length();
                        pStr[--i] = __T('\0');
                        tempstring.SetLength();
                        break;
                    }
                    if ( pStr[i] == __T(' ') ) {
                        pStr[i] = __T('\0');
                        tempstring.SetLength();
                        break;
                    }
                }
            }
            _tcscpy( holdingPen.Get(), (holdingPen.Get()+i) );
            holdingPen.SetLength();

            /* If the number of bytes needed for MIME encoding is fewer
               than or equal to how many are needed for BASE64 encoding,
               then perform MIME encoding on this string.
             */
            tmpstr.Clear();
            if ( qLen <= bLen ) {
                fixupString.Add( mime_marker );
                outS.Add(fixupString);

                ConvertToQuotedPrintable(CommonData, tempstring, tmpstr, TRUE );
            } else {
                /* BASE64 encoding is computed to be shorter.
                 */
                fixupString.Add( base64_marker );
                outS.Add(fixupString);

                base64_encode(tempstring, tmpstr, FALSE, TRUE);
            }
            outS.Add( tmpstr.Get() );
            outS.Add( __T("?=") );
            tempstring = holdingPen;
            holdingPen.Free();
            if ( tempstring.Length() ) {
                outS.Add( __T("\r\n ") );
                headerLen = 1;

//#if 0 // if this is enabled, then email clients will tend to insert an unintended space in the decoded string.
//                if ( !CheckIfNeedQuotedPrintable( CommonData, tempstring.Get(), TRUE ) ) {
//                    // quoting not necessary for the remaining balance
//                    pStr = tempstring.Get();
//                    while ( *pStr == __T(' ') )
//                        pStr++;
//
//                    addStringToHeaderNoQuoting(pStr, outS, headerLen, linewrap );
//                    tempstring.SetLength(0);
//                }
//#endif
            }
        } while ( tempstring.Length() );

        tmpstr.Free();
#if BLAT_LITE
#else
        CommonData.eightBitMimeSupported = savedEightBitMimeSupported;
        CommonData.binaryMimeSupported   = savedBinaryMimeSupported;
#endif
    } else {
        // quoting not necessary
        addStringToHeaderNoQuoting(tempstring.Get(), outS, headerLen, linewrap );
    }

    if ( outString == NULL ) {
        _tcscpy(string, outS.Get()); // Copy back to source string (calling program must ensure buffer is big enough)
    } else {
        outString->Clear();
        if ( outS.Length() )
            outString->Add(outS);
        else
            outString->Add( __T("") );
    }

    tempstring.Free();
    fixupString.Free();
    outS.Free();
    return;
}


void fixupFileName ( COMMON_DATA & CommonData, LPTSTR filename, Buf & outString, int headerLen, int linewrap )
{
    LPTSTR  shortname;

    if ( !filename )
        return;

    shortname = getShortFileName(filename);

#if defined(_UNICODE) || defined(UNICODE)
    Buf shortNameBuf;

    for ( size_t x = 0; ; x++ ) {
        if ( x || (shortname[0] != 0xFEFF) ) {
            if ( ((_TUCHAR)shortname[x] > 0x00FF) || ((memcmp(CommonData.charset, __T("UTF-"), 4*sizeof(_TCHAR)) == 0) && ((_TUCHAR)shortname[x] > 0x007F)) ) {
                shortNameBuf.Clear();
                if ( shortname[0] != 0xFEFF )
                    shortNameBuf.Add( (_TCHAR)0xFEFF );

                shortNameBuf.Add( shortname );
                break;
            }
        }
        shortNameBuf.Add( shortname[x] );
        if ( shortname[x] == __T('\0') )
            break;
    }
    fixup(CommonData, shortNameBuf.Get(), &outString, headerLen, linewrap);
    shortNameBuf.Free();
#else
    fixup(CommonData, shortname, &outString, headerLen, linewrap);
#endif
}

void fixupEmailHeaders(COMMON_DATA & CommonData, LPTSTR string, Buf * outString, int headerLen, int linewrap )
{
    int    doBase64;
    int    i, qLen, bLen;
    Buf    outS;
    Buf    fixupString;
    Buf    tempstring;
    size_t charsetLen;
    int    stringOffset;
    Buf    tmpstr;
    Buf    t;
    _TCHAR  c;
    LPTSTR pStr1;
    LPTSTR pStr2;
    LPTSTR pStr3;
    LPTSTR pStr4;
    _TCHAR localCharset[40];

#if BLAT_LITE
#else
    _TCHAR  savedEightBitMimeSupported;
    _TCHAR  savedBinaryMimeSupported;

    savedEightBitMimeSupported = CommonData.eightBitMimeSupported;
    savedBinaryMimeSupported   = CommonData.binaryMimeSupported;
#endif

    if ( memcmp( CommonData.charset, __T("utf-"), 4*sizeof(_TCHAR) ) == 0 ) {
        localCharset[0] = __T('\0');
        charsetLen = 0;
    } else {
        _tcscpy( localCharset, CommonData.charset );
        charsetLen = _tcslen( localCharset );
    }
    if ( !charsetLen )
        charsetLen = _tcslen( defaultCharset );

    _tcsupr( localCharset );
    charsetLen += 7;    // =? ?q? ?=

    stringOffset = 0;
    if ( *string == 0xFEFF )
        string++;

    tempstring = string;

    outS.Clear();

    for ( ; ; ) {
        tmpstr.Clear();
        pStr3 = 0;
        pStr2 = _tcschr( tempstring.Get(), __T('<') );
        if ( !pStr2 )
            break;

        pStr3 = _tcschr( pStr2, __T('>') );
        if ( !pStr3 )
            break;

        *pStr3 = __T('\0');
        if ( _tcschr( pStr2, __T('@') ) ) {
            *pStr2 = __T('\0');
            pStr4 = pStr2;
            for ( ; ; ) {
                if ( pStr4 == tempstring.Get() )
                    break;
                if ( pStr4[-1] != __T(' ') )
                    break;
                pStr4--;
            }
            *pStr4 = __T('\0');
            i = (int)(pStr4 - tempstring.Get());

            fixupString.Clear();

            if ( i && CheckIfNeedQuotedPrintable( CommonData, tempstring.Get(), TRUE ) ) {
                int tempUTF;
                Buf holdingPen;

                tempUTF = 0;
                holdingPen.Add( tempstring.Get() );

#if defined(_UNICODE) || defined(UNICODE)
                _TUCHAR * pStr;
                size_t    x;
                Buf       outputText;
                int       utfRequested;

                pStr = holdingPen.Get();
                for ( x = 0; *pStr != __T('\0'); pStr++ ) {
                    if ( (*pStr > 0x00FF) || ((memcmp(localCharset, __T("UTF-"), 4*sizeof(_TCHAR)) == 0) && (*pStr > 0x007F)) ) {
  #if BLAT_LITE
  #else
                        if ( savedEightBitMimeSupported && CommonData.eightBitMimeRequested )
                            utfRequested = 8;
                        else
  #endif
                            utfRequested = 7;

                        tempUTF = NATIVE_16BIT_UTF;
                        convertPackedUnicodeToUTF( holdingPen, outputText, &tempUTF, localCharset, utfRequested );
                        if ( tempUTF ) {
                            holdingPen = outputText;
                            charsetLen = 12;    // =?utf-8?q? ?=
                        }
                        break;
                    }
                }
#endif
                qLen = GetLengthQuotedPrintable( CommonData, holdingPen.Get(), TRUE );
                bLen = ((i / 3) * 4) + ((i % 3) ? 4 : 0);

#if BLAT_LITE
#else
                CommonData.eightBitMimeSupported = FALSE;
                CommonData.binaryMimeSupported   = 0;

                if ( CommonData.forcedHeaderEncoding == __T('q') ) {
                    bLen = qLen + 1;
                } else
                if ( CommonData.forcedHeaderEncoding == __T('b') ) {
                    qLen = bLen + 1;
                }
#endif
                if ( qLen <= bLen )
                    doBase64 = FALSE;
                else
                    doBase64 = TRUE;

                fixupString.Add( __T("=?") );
                if ( localCharset[0] )
                    fixupString.Add(localCharset);
                else
                    fixupString.Add(defaultCharset);

                t = holdingPen;

                if ( doBase64 ) {
                    fixupString.Add( base64_marker );
                    base64_encode(t, tmpstr, FALSE, TRUE);
                } else {
                    fixupString.Add( mime_marker );
                    ConvertToQuotedPrintable(CommonData, t, tmpstr, TRUE );
                }

                outS.Add( fixupString );
                headerLen += (int)fixupString.Length();
                for ( ; linewrap && ((int)tmpstr.Length() > (73-headerLen)); ) { // minimum fixup length too long?
                    size_t x = (size_t)(73-headerLen);

                    pStr1 = tmpstr.Get();
                    if ( doBase64 )
                        x &= ~3;
                    else
                    if ( pStr1[x-2] == __T('=') )
                        x -= 2;
                    else
                    if ( pStr1[x-1] == __T('=') )
                        x -= 1;

                    outS.Add( tmpstr.Get(), x );
                    _tcscpy( tmpstr.Get(), (LPTSTR)(pStr1+x) );
                    tmpstr.SetLength();
                    outS.Add( __T("?=\r\n ") );
                    outS.Add( fixupString );
                    headerLen = 1;
                }
                outS.Add( tmpstr );
                outS.Add( __T("?=") );
                headerLen += (int)tmpstr.Length() + 2;
#if BLAT_LITE
#else
                CommonData.eightBitMimeSupported = savedEightBitMimeSupported;
                CommonData.binaryMimeSupported   = savedBinaryMimeSupported;
#endif
                holdingPen.Free();
            }
            else {
                // quoting not necessary
                if ( i )
                    addStringToHeaderNoQuoting(tempstring.Get(), outS, headerLen, linewrap );
            }

            *pStr4 = __T(' ');
            *pStr2 = __T('<');
            *pStr3 = __T('>');

            c = pStr3[1];
            pStr3[1] = __T('\0');

            if ( CheckIfNeedQuotedPrintable( CommonData, pStr4, TRUE ) ) {
                fixupString.Clear();
                i = (int)(pStr3 + 1 - pStr4);
                qLen = GetLengthQuotedPrintable( CommonData, pStr4, TRUE );
                bLen = ((i / 3) * 4) + ((i % 3) ? 4 : 0);

#if BLAT_LITE
#else
                CommonData.eightBitMimeSupported = FALSE;
                CommonData.binaryMimeSupported   = 0;

                if ( CommonData.forcedHeaderEncoding == __T('q') ) {
                    bLen = qLen + 1;
                }
                else
                if ( CommonData.forcedHeaderEncoding == __T('b') ) {
                    qLen = bLen + 1;
                }
#endif
                if ( qLen <= bLen )
                    doBase64 = FALSE;
                else
                    doBase64 = TRUE;

                fixupString.Add( __T("=?") );
                if ( localCharset[0] )
                    fixupString.Add(localCharset);
                else
                    fixupString.Add(defaultCharset);

                t = pStr4;
                if ( doBase64 ) {
                    fixupString.Add( base64_marker );
                    base64_encode(t, tmpstr, FALSE, TRUE);
                } else {
                    fixupString.Add( mime_marker );
                    ConvertToQuotedPrintable(CommonData, t, tmpstr, TRUE );
                }

                outS.Add(fixupString);
                for ( ; linewrap && ((int)(fixupString.Length() + tmpstr.Length()) > (73-headerLen)); ) { // minimum fixup length too long?
                    size_t x = 73-headerLen-fixupString.Length();

                    pStr1 = tmpstr.Get();
                    if ( doBase64 )
                        x &= ~3;
                    else
                    if ( pStr1[x-2] == __T('=') )
                        x -= 2;
                    else
                    if ( pStr1[x-1] == __T('=') )
                        x -= 1;

                    outS.Add( tmpstr.Get(), x );
                    _tcscpy( tmpstr.Get(), (LPTSTR)(pStr1+x) );
                    tmpstr.SetLength();
                    outS.Add( __T("?=\r\n ") );
                    outS.Add( fixupString );
                    headerLen = 1;
                }
                outS.Add( tmpstr );
                outS.Add( __T("?=") );
                headerLen += (int)tmpstr.Length() + 2;
#if BLAT_LITE
#else
                CommonData.eightBitMimeSupported = savedEightBitMimeSupported;
                CommonData.binaryMimeSupported   = savedBinaryMimeSupported;
#endif
            } else {
                // quoting not necessary
                addStringToHeaderNoQuoting(pStr4, outS, headerLen, linewrap );
            }

            pStr3[1] = c;
        } else {
            /* No email address format I know, so do normal processing on this short string. */
            *pStr3 = __T('>');
            c = pStr3[1];
            pStr3[1] = __T('\0');
            tempstring.SetLength();

            fixup(CommonData, tempstring.Get(), &tmpstr, headerLen, linewrap );
            outS.Add(tmpstr.Get());
            pStr3[1] = c;
        }

        *pStr3 = __T('>');
        pStr3++;
        fixupString.Clear();
        if ( *pStr3 == __T(',') ) {
            outS.Add( __T(",") );
            pStr3++;
            headerLen++;
        }
        for ( ; *pStr3 == __T(' '); ) {
            outS.Add( __T(" ") );
            pStr3++;
            headerLen++;
        }
        _tcscpy(tempstring.Get(), pStr3);
        tempstring.SetLength();
        if ( !tempstring.Length() )
            break;

        if ( linewrap && ((headerLen + tempstring.Length() ) >= 75) ) {
            outS.Add( __T("\r\n ") );
            headerLen = 1;
        }
    }

    if ( !pStr2 || !pStr3 ) {
        fixup(CommonData, tempstring.Get(), &tmpstr, headerLen, linewrap );
        outS.Add(tmpstr.Get());
    }

    if ( outString == NULL ) {
        _tcscpy(string, outS.Get()); // Copy back to source string (calling program must ensure buffer is big enough)
    } else {
        outString->Clear();
        if ( outS.Length() )
            outString->Add(outS);
        else
            outString->Add( __T("") );
    }

    t.Free();
    tmpstr.Free();
    tempstring.Free();
    fixupString.Free();
    outS.Free();
    return;
}


LPTSTR getCharsetString( COMMON_DATA & CommonData )
{
    static _TCHAR localCharset[40];


    if ( CommonData.charset[0] )
        _tcscpy( localCharset, CommonData.charset );
    else
        _tcscpy( localCharset, defaultCharset );

    _tcsupr( localCharset );
    return( localCharset );
}


void build_headers( COMMON_DATA & CommonData, BLDHDRS & bldHdrs )
{
    int                   i;
    int                   yEnc_This;
    int                   hours, minutes;
    _TCHAR                boundary2[25];
    Buf                   tmpBuf;
    _TCHAR                tmpstr[0x2000];
    Buf                   tempstring;
    SYSTEMTIME            curtime;
    TIME_ZONE_INFORMATION tzinfo;
    DWORD                 retval;
    DWORD                 cpuTime;
    Buf                   fixedFromId;
    Buf                   fixedSenderId;
    Buf                   fixedLoginName;
    Buf                   fixedSenderName;
    Buf                   fixedReplyToId;
#if BLAT_LITE
#else
    Buf                   fixedOrganization;
#endif
    Buf                   fixedReturnPathId;
    Buf                   contentType;
    FILETIME              today;
    LPTSTR                pp;
    Buf                   shortNameBuf;


#if SUPPORT_YENC
    yEnc_This = CommonData.yEnc;
#else
    yEnc_This = FALSE;
#endif
#if BLAT_LITE
#else
    if ( bldHdrs.buildSMTP && !CommonData.eightBitMimeSupported && !CommonData.binaryMimeSupported ) {
        yEnc_This = FALSE;
        if ( _memicmp(CommonData.charset, __T("UTF-8"), 6*sizeof(_TCHAR)) == 0 )
            CommonData.charset[4] = __T('7');
    }
#endif

    CommonData.needBoundary = FALSE;

    if ( CommonData.alternateText.Length() ) {
#if BLAT_LITE
        CommonData.alternateText.Clear();
#else
        if ( CommonData.uuencode )
            CommonData.base64 = TRUE;
        else
        if ( !CommonData.base64 )
            CommonData.mime = 1;

        CommonData.uuencode = FALSE;
        yEnc_This = FALSE;
  #if SUPPORT_YENC
        CommonData.yEnc = FALSE;
  #endif
#endif
    }

    if ( CommonData.charset[0] ) {
        if ( _tcscmp(CommonData.charset, defaultCharset) ) {
            CommonData.mime     = 2;        // If -charset option was used, then set mime so the charset can be identified in the headers.
            yEnc_This           = FALSE;
#if BLAT_LITE
#else
            CommonData.uuencode = FALSE;
#endif
#if SUPPORT_YENC
            CommonData.yEnc     = FALSE;
#endif
        }
    } else
        _tcscpy(CommonData.charset, defaultCharset);

    fixupEmailHeaders(CommonData, CommonData.fromid,       &fixedFromId      , 6 , TRUE); // "From: "
    fixupEmailHeaders(CommonData, CommonData.senderid,     &fixedSenderId    , 6 , TRUE); // "From: "
    fixupEmailHeaders(CommonData, CommonData.loginname,    &fixedLoginName   , 10, TRUE); // "Sender: " or "Reply-to: "
    fixupEmailHeaders(CommonData, CommonData.sendername,   &fixedSenderName  , 8 , TRUE); // "Sender: "
    fixupEmailHeaders(CommonData, CommonData.replytoid,    &fixedReplyToId   , 10, TRUE); // "Reply-to: "
#if BLAT_LITE
#else
    fixup(CommonData, CommonData.organization, &fixedOrganization, 14, TRUE); // "Organization: "
#endif

    if ( CommonData.returnpathid[0] ) {
        _stprintf( tmpstr, __T("<%s>"), CommonData.returnpathid );
        for ( ; ; ) {
            pp = _tcschr( &(tmpstr)[1], __T('<') );
            if ( !pp )
                break;

            _tcscpy(tmpstr, pp);
        }
        *(_tcschr(tmpstr, __T('>'))+1) = __T('\0');
        fixup(CommonData, tmpstr, &fixedReturnPathId, 13, TRUE);    // "Return-Path: "
    }

    // create a header for the message

#if SUPPORT_MULTIPART
    if ( bldHdrs.part < 2 ) {
#endif
        // Generate a unique message boundary identifier.

        for ( i = 0 ; i < 21 ; i++ ) {
#if SUPPORT_MULTIPART
            bldHdrs.multipartID[i] = abclist[rand()%62];
            if ( !bldHdrs.attachNbr )
#endif
                bldHdrs.attachment_boundary[i] = abclist[rand()%62];
        }

        _tcscpy( &bldHdrs.attachment_boundary[21], __T("\r\n") );

#if SUPPORT_MULTIPART
        bldHdrs.multipartID[21] = __T('@');
        if ( bldHdrs.wanted_hostname && *bldHdrs.wanted_hostname )
            _tcscpy( &bldHdrs.multipartID[22], bldHdrs.wanted_hostname );
        else
            _tcscpy( &bldHdrs.multipartID[22], CommonData.my_hostname );
#endif

        GetLocalTime( &curtime );
        retval  = GetTimeZoneInformation( &tzinfo );
        hours   = (int) tzinfo.Bias / 60;
        minutes = (int) tzinfo.Bias % 60;
        if ( retval == TIME_ZONE_ID_STANDARD ) {
            hours   += (int) tzinfo.StandardBias / 60;
            minutes += (int) tzinfo.StandardBias % 60;
        } else
        if ( retval == TIME_ZONE_ID_DAYLIGHT ) {
            hours   += (int) tzinfo.DaylightBias / 60;
            minutes += (int) tzinfo.DaylightBias % 60;
        }

        // rfc2822 acceptable format
        // Mon, 29 Jun 1994 02:15:23 UTC
        // rfc1036 & rfc822 acceptable format
        // Mon, 29 Jun 1994 02:15:23 GMT
        _stprintf (tmpstr, __T("Date: %s, %.2hu %s %.4hu %.2hu:%.2hu:%.2hu %+03d%02d\r\n"),
                   days[curtime.wDayOfWeek],
                   curtime.wDay,
                   months[curtime.wMonth - 1],
                   curtime.wYear,
                   curtime.wHour,
                   curtime.wMinute,
                   curtime.wSecond,
                   -hours,
                   -minutes);
        bldHdrs.header->Add( tmpstr );

        // RFC 822 From: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
        bldHdrs.header->Add( __T("From: ") );
        if ( CommonData.fromid[0] )
            bldHdrs.header->Add( fixedFromId );
        else
            bldHdrs.header->Add( fixedSenderId );
        bldHdrs.header->Add( __T("\r\n") );

        // now add the Received: from x.x.x.x by y.y.y.y with HTTP;
        if ( bldHdrs.lpszFirstReceivedData->Length() ) {
            _stprintf(tmpstr, __T("%s%s, %.2hu %s %.2hu %.2hu:%.2hu:%.2hu %+03d%02d\r\n"),
                      bldHdrs.lpszFirstReceivedData->Get(),
                      days[curtime.wDayOfWeek],
                      curtime.wDay,
                      months[curtime.wMonth - 1],
                      curtime.wYear,
                      curtime.wHour,
                      curtime.wMinute,
                      curtime.wSecond,
                      -hours,
                      -minutes);
            bldHdrs.header->Add( tmpstr);
        }

        if ( CommonData.impersonating ) {
            bldHdrs.header->Add( __T("Sender: ") );
            bldHdrs.header->Add( fixedLoginName );
            bldHdrs.header->Add( __T("\r\n") );
            if ( CommonData.replytoid[0] ) {
                bldHdrs.header->Add( __T("Reply-To: ") );
                bldHdrs.header->Add( fixedReplyToId );
                bldHdrs.header->Add( __T("\r\n") );
            } else
            if ( CommonData.formattedContent ) {
                bldHdrs.header->Add( tmpstr );
                bldHdrs.header->Add( __T("Reply-To: ") );
                bldHdrs.header->Add( fixedLoginName );
                bldHdrs.header->Add( __T("\r\n") );
            }
        } else {
            // RFC 822 Sender: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
            if ( CommonData.sendername[0] ) {
                bldHdrs.header->Add( __T("Sender: ") );
                bldHdrs.header->Add( fixedSenderName );
                bldHdrs.header->Add( __T("\r\n") );
            }
            // RFC 822 Reply-To: definition in Blat changed 2000-02-03 Axel Skough SCB-SE
            if ( CommonData.replytoid[0] ) {
                bldHdrs.header->Add( __T("Reply-To: ") );
                bldHdrs.header->Add( fixedReplyToId );
                bldHdrs.header->Add( __T("\r\n") );
            }
        }

        if ( bldHdrs.buildSMTP ) {
            if ( CommonData.destination.Length() ) {
#if SUPPORT_SALUTATIONS
                find_and_strip_salutation( CommonData, CommonData.destination );
#endif
                fixupEmailHeaders(CommonData, CommonData.destination.Get(), &tempstring, 4, TRUE);
                bldHdrs.header->Add( __T("To: ") );
                bldHdrs.header->Add( tempstring );
                bldHdrs.header->Add( __T("\r\n") );
                tempstring.Clear();
            }
            else if ( !CommonData.cc_list.Length() ) {
                if ( CommonData.sendUndisclosed )
                    bldHdrs.header->Add( __T("To: Undisclosed recipients:;\r\n") );
            }

            if ( CommonData.cc_list.Length() ) {
                // Add line for the Carbon Copies
                fixupEmailHeaders(CommonData, CommonData.cc_list.Get(), &tempstring, 4, TRUE);
                bldHdrs.header->Add( __T("Cc: ") );
                bldHdrs.header->Add( tempstring );
                bldHdrs.header->Add( __T("\r\n") );
                tempstring.Clear();
            }

            if ( bldHdrs.addBccHeader && CommonData.bcc_list.Length() ) {
                // Add line for the Blind Carbon Copies, for transmitting through POP3
                fixupEmailHeaders(CommonData, CommonData.bcc_list.Get(), &tempstring, 5, TRUE);
                bldHdrs.header->Add( __T("Bcc: ") );
                bldHdrs.header->Add( tempstring );
                bldHdrs.header->Add( __T("\r\n") );
                tempstring.Clear();
            }

            // To use loginname for the RFC 822 Disposition and Return-receipt fields doesn't seem to be unambiguous.
            // Either separate definitions should be used for these fileds to get full flexibility or - as a compromise -
            // the content of the Reply-To. field would rather be used when specified. 2000-02-03 Axel Skough SCB-SE

            if ( CommonData.disposition ) {
                bldHdrs.header->Add( __T("Disposition-Notification-To: ") );
                if ( CommonData.replytoid[0] )
                    bldHdrs.header->Add( fixedReplyToId );
                else
                    bldHdrs.header->Add( CommonData.loginname );
                bldHdrs.header->Add( __T("\r\n") );
            }

            if ( CommonData.returnreceipt ) {
                bldHdrs.header->Add( __T("Return-Receipt-To: ") );
                if ( CommonData.replytoid[0] )
                    bldHdrs.header->Add( fixedReplyToId );
                else
                    bldHdrs.header->Add( CommonData.loginname );
                bldHdrs.header->Add( __T("\r\n") );
            }

            // Toby Korn tkorn@snl.com 8/4/1999
            // If priority is specified on the command line, add it to the header
            // The latter two options are X.400, mainly for Lotus Notes (blah)
            if ( CommonData.priority [0] == __T('0')) {
                bldHdrs.header->Add( __T("X-MSMail-Priority: Low\r\n") \
                                     __T("X-Priority: 5\r\n") \
                                     __T("Priority: normal\r\n") \
                                     __T("Importance: normal\r\n") );
                bldHdrs.header->Add( __T("X-MimeOLE: Produced by Blat v") );
                bldHdrs.header->Add( blatVersion );
                bldHdrs.header->Add( __T("\r\n") );
            }
            else if (CommonData.priority [0] == __T('1')) {
                bldHdrs.header->Add( __T("X-MSMail-Priority: High\r\n") \
                                     __T("X-Priority: 1\r\n") \
                                     __T("Priority: urgent\r\n") \
                                     __T("Importance: high\r\n") );
                bldHdrs.header->Add( __T("X-MimeOLE: Produced by Blat v") );
                bldHdrs.header->Add( blatVersion );
                bldHdrs.header->Add( __T("\r\n") );
            }
            // If sensitivity is specified on the command line, add it to the header
            // These are X.400
            if ( CommonData.sensitivity [0] == __T('0')) {
                bldHdrs.header->Add( __T("Sensitivity: Personal\r\n") );
            }
            else if (CommonData.sensitivity [0] == __T('1')) {
                bldHdrs.header->Add( __T("Sensitivity: Private\r\n") );
            }
            else if (CommonData.sensitivity [0] == __T('2')) {
                bldHdrs.header->Add( __T("Sensitivity: Company-Confidential\r\n") );
            }
#if INCLUDE_NNTP
        } else {
            if ( CommonData.groups.Length() && CommonData.NNTPHost[0] ) {
                fixup(CommonData, CommonData.groups.Get(), &tempstring, 12, TRUE);
                bldHdrs.header->Add( __T("Newsgroups: ") );
                bldHdrs.header->Add( tempstring );
                bldHdrs.header->Add( __T("\r\n") );
                tempstring.Clear();
            }
#endif
        }

#if BLAT_LITE
#else
        if ( CommonData.organization[0] ) {
            bldHdrs.header->Add( __T("Organization: ") );
            bldHdrs.header->Add( fixedOrganization );
            bldHdrs.header->Add( __T("\r\n") );
        }
#endif

        // RFC 822 Return-Path: definition in Blat entered 2000-02-03 Axel Skough SCB-SE
        if ( CommonData.returnpathid[0] ) {
            bldHdrs.header->Add( __T("Return-Path: ") );
            bldHdrs.header->Add( fixedReturnPathId );
            bldHdrs.header->Add( __T("\r\n") );
        }

#if BLAT_LITE
        if ( CommonData.includeUserAgent ) {
            _stprintf( tmpstr, __T("User-Agent: Blat /%s%s (a ") WIN_32_STR __T(" SMTP mailer) (http://www.blat.net)\r\n"), blatVersion, blatVersionSuf );
        } else {
            _stprintf( tmpstr, __T("X-Mailer: Blat v%s%s, a ") WIN_32_STR __T(" SMTP mailer (http://www.blat.net)\r\n"), blatVersion, blatVersionSuf );
        }
        bldHdrs.header->Add( tmpstr );
#else
        if ( CommonData.xheaders[0] ) {
            bldHdrs.header->Add( CommonData.xheaders );
            bldHdrs.header->Add( __T("\r\n") );
        }

        if ( CommonData.aheaders1[0] ) {
            bldHdrs.header->Add( CommonData.aheaders1 );
            bldHdrs.header->Add( __T("\r\n") );
        }

        if ( CommonData.aheaders2[0] ) {
            bldHdrs.header->Add( CommonData.aheaders2 );
            bldHdrs.header->Add( __T("\r\n") );
        }

        if ( CommonData.noheader < 2 ) {
            if ( CommonData.includeUserAgent ) {
                _stprintf( tmpstr, __T("User-Agent: Blat /%s%s (a ") WIN_32_STR __T(" SMTP/NNTP mailer)"), blatVersion, blatVersionSuf );
                bldHdrs.header->Add( tmpstr );
                if ( CommonData.noheader == 0 )
                    bldHdrs.header->Add( __T(" (http://www.blat.net)") );
            } else {
                _stprintf( tmpstr, __T("X-Mailer: Blat v%s%s, a ") WIN_32_STR __T(" SMTP/NNTP mailer"), blatVersion, blatVersionSuf );
                bldHdrs.header->Add( tmpstr );
                if ( CommonData.noheader == 0 )
                    bldHdrs.header->Add( __T(" http://www.blat.net") );
            }
            bldHdrs.header->Add( __T("\r\n") );
        }
#endif

#if SUPPORT_MULTIPART
        bldHdrs.multipartHdrs->Add( bldHdrs.header->Get() );
        bldHdrs.varHeaders->Clear();
        bldHdrs.varHeaders->Add( bldHdrs.header->Get() );
    } else {
        bldHdrs.header->Clear();
        bldHdrs.header->Add( bldHdrs.varHeaders->Get() );
    }
#endif
    GetSystemTimeAsFileTime( &today );

#if defined(_WIN64)
    cpuTime = (DWORD)rand();
#else
    __asm {
        pushad
        rdtsc
        mov     cpuTime, eax
        popad
    }
#endif
    _stprintf(tmpstr, __T("Message-ID: <%08lx$Blat.v%s$%08lx$%lx%lx@%s>\r\n"),
              today.dwHighDateTime, blatVersion, today.dwLowDateTime, GetCurrentProcessId(), cpuTime, bldHdrs.server_name );
    bldHdrs.header->Add( tmpstr );

    tmpstr[0] = __T('\0');
    shortNameBuf.Clear();

    if ( !CommonData.subject.Length() ) {
        if ( !CommonData.ssubject ) {          //$$ASD
            CommonData.subject.Free();
            if ( _tcscmp(CommonData.bodyFilename, __T("-")) == 0 ) {
                CommonData.subject.Add( __T("Contents of console input") );
            } else {
                CommonData.subject.Add( __T("Contents of file: ") );
                shortNameBuf.Clear();
                fixupFileName( CommonData, CommonData.bodyFilename, shortNameBuf, 27, TRUE );
                CommonData.subject.Add( shortNameBuf.Get() );
            }
        }
    }
    if ( CommonData.subject.Length() && CommonData.subject.Get()[0] ) {
        Buf fixedSubject;
        Buf newSubject;

        fixedSubject.Clear();
        newSubject = CommonData.subject;
        bldHdrs.header->Add( __T("Subject: ") );

#if SUPPORT_MULTIPART
        Buf mpSubject;

        mpSubject  = CommonData.subject;
        bldHdrs.multipartHdrs->Add( __T("Subject: ") );
#endif
        if ( bldHdrs.attachName && *bldHdrs.attachName ) {
#if SUPPORT_MULTIPART
            if ( bldHdrs.nbrOfAttachments > 1 ) {
                int sizeFactor = 0;
                int x = bldHdrs.nbrOfAttachments;
                for ( ; x; ) {
                    x /= 10;
                    sizeFactor++;
                }
                _stprintf( tmpstr, __T(" %0*d of %d"), sizeFactor, bldHdrs.attachNbr+1, bldHdrs.nbrOfAttachments );
                newSubject.Add( tmpstr );
                mpSubject.Add(  tmpstr );
            }
#endif
            newSubject.Add( __T(" \"") );
            fixupFileName( CommonData, bldHdrs.attachName, shortNameBuf, 0, FALSE );
            newSubject.Add( shortNameBuf );
            newSubject.Add( __T('\"') );

#if SUPPORT_YENC
            if ( yEnc_This && CommonData.attach ) {
  #if SUPPORT_MULTIPART
                if ( bldHdrs.totalparts > 1 ) {
                    mpSubject.Add(  __T(" yEnc") );
                    newSubject.Add( __T(" yEnc") );
                } else
  #endif
                {
                    _stprintf( tmpstr, __T(" %lu yEnc bytes"), bldHdrs.attachSize );
                    newSubject.Add( tmpstr );
                }
            }
#endif
#if SUPPORT_MULTIPART
            if ( bldHdrs.totalparts > 1 ) {
                int sizeFactor = 0;
                int x = bldHdrs.totalparts;

                for ( ; x; ) {
                    x /= 10;
                    sizeFactor++;
                }
                _stprintf( tmpstr, __T(" [%0*d/%d]"), sizeFactor, bldHdrs.part, bldHdrs.totalparts );
                newSubject.Add( tmpstr );
            }
#endif
        }

#if SUPPORT_MULTIPART
  #if INCLUDE_NNTP
        if ( CommonData.groups.Length() && CommonData.NNTPHost[0] ) {
            fixup(CommonData, mpSubject.Get(), &fixedSubject, 9, FALSE);
            bldHdrs.multipartHdrs->Add( fixedSubject );
            bldHdrs.multipartHdrs->Add( __T("\r\n") );

            fixup(CommonData, newSubject.Get(), &fixedSubject, 9, FALSE);
            bldHdrs.header->Add( fixedSubject );
            bldHdrs.header->Add( __T("\r\n") );
        } else
  #endif
        {
            fixup(CommonData, mpSubject.Get(), &fixedSubject, 9, TRUE);
            bldHdrs.multipartHdrs->Add( fixedSubject );
            bldHdrs.multipartHdrs->Add( __T("\r\n") );

            fixup(CommonData, newSubject.Get(), &fixedSubject, 9, TRUE);
            bldHdrs.header->Add( fixedSubject );
            bldHdrs.header->Add( __T("\r\n") );
        }
#else
        fixup(CommonData, newSubject.Get(), &fixedSubject, 9, TRUE);
        bldHdrs.header->Add( fixedSubject );
        bldHdrs.header->Add( __T("\r\n") );
#endif
        newSubject.Free();
        fixedSubject.Free();
    }

    memcpy(boundary2, bldHdrs.attachment_boundary, 21 * sizeof(_TCHAR) );
    _tcscpy( &boundary2[21], __T("\"\r\n") );

    // This is either mime, base64, uuencoded, or neither.  With or without attachments.  Whew!
    if ( CommonData.mime ) {
        // Indicate MIME version and type

        if ( !bldHdrs.attachNbr || (bldHdrs.totalparts > 1) || CommonData.alternateText.Length() ) {
            contentType.Add( __T("MIME-Version: 1.0\r\n") );
            if ( CommonData.attach || CommonData.alternateText.Length() ) {
                contentType.Add( __T("Content-Type:") );
                if ( CommonData.haveAttachments ) {
                    contentType.Add( __T(" multipart/mixed;\r\n") );
                } else
                if ( CommonData.haveEmbedded ) {
                    contentType.Add( __T(" multipart/related;\r\n") );
                    if ( CommonData.alternateText.Length() )
                        contentType.Add( __T(" type=\"multipart/alternative\";\r\n") );
                } else
                    contentType.Add( __T(" multipart/alternative;\r\n") );

                contentType.Add( __T(" boundary=\"") BOUNDARY_MARKER );
                contentType.Add( boundary2 );
                if ( !bldHdrs.attachNbr )
                    contentType.Add( __T("\r\nThis is a multi-part message in MIME format.\r\n") );
            } else {

#if SMART_CONTENT_TYPE
                _TCHAR foundType  [0x200];

                _tcscpy( foundType, __T("text/") );
                _tcscat( foundType, CommonData.textmode );
#endif
                contentType.Add( __T("Content-Transfer-Encoding: quoted-printable\r\n") );
#if SMART_CONTENT_TYPE
                if ( !CommonData.ConsoleDone && !_tcscmp( CommonData.textmode, __T("plain")) )
                    getContentType( CommonData, tmpBuf, foundType, foundType, getShortFileName( CommonData.bodyFilename ) );

                contentType.Add( __T("Content-Type: ") );
                contentType.Add( foundType );
#else
                contentType.Add( __T("Content-Type: text/") );
                contentType.Add( CommonData.textmode );
#endif
                contentType.Add( __T(";\r\n charset=\"") );
                contentType.Add( getCharsetString(CommonData) );
                contentType.Add( __T("\"\r\n") );
            }
        }
    } else {
#if BLAT_LITE
#else
        if ( CommonData.base64 ) {
            // Indicate MIME version and type
            contentType.Add( __T("MIME-Version: 1.0\r\n") );
            contentType.Add( __T("Content-Type:") );
            if ( CommonData.haveAttachments ) {
                contentType.Add( __T(" multipart/mixed;\r\n") );
            } else
            if ( CommonData.haveEmbedded ) {
                contentType.Add( __T(" multipart/related;\r\n") );
                // the next is required per RFC 2387
                if ( CommonData.alternateText.Length() )
                    contentType.Add( __T(" type=\"multipart/alternative\";\r\n") );
            } else
                if ( CommonData.alternateText.Length() )
                    contentType.Add( __T(" multipart/alternative;\r\n") );
                else
                    contentType.Add( __T(" multipart/mixed;\r\n") );

            contentType.Add( __T(" boundary=\"") BOUNDARY_MARKER );
            contentType.Add( boundary2 );
            contentType.Add( __T("\r\nThis is a multi-part message in MIME format.\r\n") );
        } else
#endif
        {
            if ( CommonData.attach ) {
#if BLAT_LITE
                contentType.Add( __T("MIME-Version: 1.0\r\n") );
                contentType.Add( __T("Content-Type:") );
                contentType.Add( __T(" multipart/mixed;\r\n") );
                contentType.Add( __T(" boundary=\"") BOUNDARY_MARKER );
                contentType.Add( boundary2);
                contentType.Add( __T("\r\nThis is a multi-part message in MIME format.\r\n") );
#else
                if ( CommonData.uuencode || yEnc_This || !bldHdrs.buildSMTP ) {
 /*
  * Having this code enabled causes Mozilla to ignore attachments and treat them as inline text.
  *
  #if SMART_CONTENT_TYPE
                    _TCHAR foundType  [0x200];

                    _tcscpy( foundType, __T("text/") );
                    _tcscat( foundType, CommonData.textmode );
  #endif
                    contentType.Add( __T("Content-description: " );
                    if ( bldHdrs.buildSMTP )
                        contentType.Add( __T("Mail") );
                    else
                        contentType.Add( __T("News") );
                    contentType.Add( __T(" message body\r\n") );
  #if SMART_CONTENT_TYPE
                    if ( !CommonData.ConsoleDone && !_tcscmp( CommonData.textmode, __T("plain")) )
                        getContentType( CommonData, NULL, foundType, foundType, getShortFileName( CommonData.bodyFilename ) );

                    contentType.Add( __T("Content-Type: ") );
                    contentType.Add( foundType );
  #else
                    contentType.Add( __T("Content-Type: text/") );
                    contentType.Add( CommonData.textmode );
  #endif
                    contentType.Add( __T(";\r\n charset=\"") );
                    contentType.Add( getCharsetString(CommonData) );
                    contentType.Add( __T("\"\r\n") );
  */
                } else {
                    contentType.Add( __T("MIME-Version: 1.0\r\n") );
                    contentType.Add( __T("Content-Type:") );
                    contentType.Add( __T(" multipart/mixed;\r\n") );
                    contentType.Add( __T(" boundary=\"") BOUNDARY_MARKER );
                    contentType.Add( boundary2);
  #if SUPPORT_MULTIPART
                    if ( !bldHdrs.attachNbr )
  #endif
                        contentType.Add( __T("\r\nThis is a multi-part message in MIME format.\r\n") );
                }
#endif
            } else {
                if ( CommonData.formattedContent ) {
                    LPTSTR pString1;
                    LPTSTR pString2;

                    pString1 = pString2 = NULL;
                    if ( CommonData.TempConsole.Get() ) {
                        pString1 = _tcsstr( CommonData.TempConsole.Get(), __T("\r\n\r\n") );
                        if ( pString1 ) {
                            *pString1 = __T('\0');
                            pString2 = _tcsstr( CommonData.TempConsole.Get(), __T("Content-Type: ") );
                            if ( pString2 == NULL )
                                pString2 = _tcsstr( CommonData.TempConsole.Get(), __T("Content-Transfer-Encoding:") );
                            *pString1 = __T('\r');
                        }
                    }
                    if ( pString2 == NULL ) {
#if SMART_CONTENT_TYPE
                        _TCHAR foundType  [0x200];

                        _tcscpy( foundType, __T("text/") );
                        _tcscat( foundType, CommonData.textmode );
#endif
#if BLAT_LITE
#else
                        if ( (CommonData.binaryMimeSupported || CommonData.eightBitMimeSupported) && (CommonData.eightBitMimeRequested || yEnc_This) )
                            contentType.Add( __T("Content-Transfer-Encoding: 8BIT\r\n") );
                        else
                        if ( CheckIfNeedQuotedPrintable( CommonData, CommonData.TempConsole.Get(), FALSE ) )
                            contentType.Add( __T("Content-Transfer-Encoding: 8BIT\r\n") );
                        else
#endif
                            contentType.Add( __T("Content-Transfer-Encoding: 7BIT\r\n") );

#if SMART_CONTENT_TYPE
                        if ( !CommonData.ConsoleDone && !_tcscmp( CommonData.textmode, __T("plain")) )
                            getContentType( CommonData, tmpBuf, foundType, foundType, getShortFileName( CommonData.bodyFilename ) );

                        contentType.Add( __T("Content-Type: ") );
                        contentType.Add( foundType );
#else
                        contentType.Add( __T("Content-Type: text/") );
                        contentType.Add( CommonData.textmode );
#endif
                        contentType.Add( __T(";\r\n charset=\"") );
                        contentType.Add( getCharsetString(CommonData) );
                        contentType.Add( __T("\"\r\n") );
                    }
                }
            }
        }
    }

#if SUPPORT_MULTIPART
    if ( bldHdrs.totalparts > 1 ) {
        bldHdrs.header->Add(     __T("MIME-Version: 1.0\r\n") );
        bldHdrs.header->Add(     __T("Content-Type:") );
        bldHdrs.header->Add(     __T(" message/partial;\r\n") );
        _stprintf( tmpstr, __T("    id=\"%s\";\r\n") \
                           __T("    number=%d; total=%d;\r\n") \
                           __T("    boundary=\"") BOUNDARY_MARKER __T("%s"),  // Include a boundary= incase it is not included above.
                   bldHdrs.multipartID, bldHdrs.part, bldHdrs.totalparts, boundary2 );
        bldHdrs.header->Add( tmpstr );
        bldHdrs.multipartHdrs->Add( contentType );
    } else {
        bldHdrs.multipartHdrs->Clear();
        bldHdrs.header->Add( contentType );
    }
#else
    bldHdrs.header->Add( contentType );
#endif

    if ( bldHdrs.lpszOtherHeader->Length() )
        bldHdrs.header->Add( bldHdrs.lpszOtherHeader->Get() );

    if ( CommonData.formattedContent )
        bldHdrs.header->Add( __T("\r\n") );

    bldHdrs.messageBuffer->Clear();
    bldHdrs.messageBuffer->Add( bldHdrs.header->Get() );

    if ( _tcsstr(bldHdrs.messageBuffer->Get(), BOUNDARY_MARKER ) )
        CommonData.needBoundary = TRUE;

    shortNameBuf.Free();
    contentType.Free();
    fixedReturnPathId.Free();
#if BLAT_LITE
#else
    fixedOrganization.Free();
#endif
    fixedReplyToId.Free();
    fixedSenderName.Free();
    fixedLoginName.Free();
    fixedSenderId.Free();
    fixedFromId.Free();
    tempstring.Free();
    tmpBuf.Free();
    return;
}
