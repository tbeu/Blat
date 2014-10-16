/*
    msgbody.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "blat.h"
#include "common_data.h"
#include "winfile.h"

#ifndef _MAX_PATH
    #define _MAX_PATH 260
#endif

extern LPTSTR getCharsetString( COMMON_DATA & CommonData );
extern void   incrementBoundary( _TCHAR boundary[] );
extern void   decrementBoundary( _TCHAR boundary[] );
extern LPTSTR getShortFileName (LPTSTR fileName);
extern void   fixupFileName ( COMMON_DATA & CommonData, LPTSTR filename, Buf & outString, int headerLen, int linewrap );
extern int    CheckIfNeedQuotedPrintable(COMMON_DATA & CommonData, LPTSTR pszStr, int inHeader);
extern void   ConvertToQuotedPrintable(COMMON_DATA & CommonData, Buf & source, Buf & out, int inHeader);
extern void   base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);
extern void   convertPackedUnicodeToUTF( Buf & sourceText, Buf & outputText, int * utf, LPTSTR charset, int utfRequested );
#if BLAT_LITE
#else
extern void   douuencode(COMMON_DATA & CommonData, Buf & source, Buf & out, LPTSTR filename, int part, int lastpart);
extern void   convertUnicode( Buf & sourceText, int * utf, LPTSTR charset, int utfRequested );
#endif
extern void   printMsg(COMMON_DATA & CommonData, LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison
extern void   fixup(COMMON_DATA & CommonData, LPTSTR string, Buf * tempstring2, int headerLen, int linewrap );

#if SMART_CONTENT_TYPE
extern void   getContentType(COMMON_DATA & CommonData, Buf & sDestBuffer, LPTSTR foundType, LPTSTR defaultType, LPTSTR sFileName);
#endif
#if SUPPORT_SALUTATIONS
extern void   parse_email_addresses( COMMON_DATA & CommonData, LPTSTR email_addresses, Buf & parsed_addys );
#endif

int add_message_body ( COMMON_DATA & CommonData,
                       Buf &messageBuffer, size_t msgBodySize, Buf &multipartHdrs, int buildSMTP,
                       LPTSTR attachment_boundary, DWORD startOffset, int part,
                       int attachNbr )
{
    int     yEnc_This;
    Buf     tmpstr;
    Buf     fileBuffer;
    WinFile atf;
    LPTSTR  pString;
    int     utfRequested;
#if BLAT_LITE
#else
    int     localUTF;
#endif


    tmpstr.Alloc( 0x2000 );
    tmpstr.Clear();
    if ( !startOffset && !attachNbr ) {
        if ( part /* || CommonData.alternateText.Length() */ ) {
            if ( memcmp( (messageBuffer.GetTail()-3), "\n\r\n", 3*sizeof(_TCHAR) ) != 0 )
                messageBuffer.Add( __T("\r\n") );

            messageBuffer.Add( multipartHdrs );
            if ( multipartHdrs.Length() &&
                 !_tcsstr( messageBuffer.Get(), __T("This is a multi-part message in MIME format.") ) )
                CommonData.needBoundary = FALSE;
        }

        if ( CommonData.formattedContent )
            if ( memcmp( (messageBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                messageBuffer.Add( __T("\r\n") );

        if ( msgBodySize ) {

#if BLAT_LITE
#else
            if ( lstrcmpi(CommonData.charset, __T("UTF-7")) == 0 )
                utfRequested = 7;
            else
            if ( (CommonData.eightBitMimeSupported && CommonData.eightBitMimeRequested) || (lstrcmpi(CommonData.charset, __T("UTF-8")) == 0) )
                utfRequested = 8;
            else
#endif
                utfRequested = 7;

#if SUPPORT_YENC
            yEnc_This = CommonData.yEnc;
            if ( buildSMTP && !CommonData.eightBitMimeSupported )
#endif
                yEnc_This = FALSE;

            if ( !buildSMTP &&
#if BLAT_LITE
#else
                 !CommonData.base64 && !CommonData.uuencode &&
#endif
                 !yEnc_This ) {
                if ( CheckIfNeedQuotedPrintable(CommonData, CommonData.TempConsole.Get(), FALSE) ) {
                    CommonData.mime = TRUE;
                }
            }

            if ( CommonData.needBoundary ) {
#if BLAT_LITE
#else
                if ( CommonData.alternateText.Length() ) {
                    LPTSTR pp;
                    LPTSTR pp1;
                    int needQP = FALSE;
                    int flowed = FALSE;

                    _TCHAR altTextCharset[8];
                    altTextCharset[0] = 0;
                    localUTF = CommonData.utf;
                    convertUnicode( CommonData.alternateText, &localUTF, altTextCharset, utfRequested );

                    if ( CommonData.haveEmbedded || CommonData.haveAttachments ) {
                        if ( CommonData.haveEmbedded && CommonData.haveAttachments ) {
                            messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                            messageBuffer.Add( attachment_boundary );
                            messageBuffer.Add( __T("Content-Type:") );
                            messageBuffer.Add( __T(" multipart/related;\r\n") );
                            messageBuffer.Add( __T(" type=\"multipart/alternative\";\r\n") );
                            messageBuffer.Add( __T(" boundary=\"") BOUNDARY_MARKER );
                            incrementBoundary( attachment_boundary );
                            messageBuffer.Add( attachment_boundary, 21 );
                            messageBuffer.Add( __T("\"\r\n\r\n") );
                        }
                        messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                        messageBuffer.Add( attachment_boundary );
                        messageBuffer.Add( __T("Content-Type:") );
                        messageBuffer.Add( __T(" multipart/alternative;\r\n") );
                        messageBuffer.Add( __T(" boundary=\"") BOUNDARY_MARKER );
                        incrementBoundary( attachment_boundary );
                        messageBuffer.Add( attachment_boundary, 21 );
                        messageBuffer.Add( __T("\"\r\n\r\n") );
                    }

                    messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                    messageBuffer.Add( attachment_boundary );
                    if ( CheckIfNeedQuotedPrintable(CommonData, CommonData.alternateText.Get(), TRUE) ) {
                        needQP = TRUE;
                        messageBuffer.Add( __T("Content-Transfer-Encoding: quoted-printable\r\n") );
                    } else
                        messageBuffer.Add( __T("Content-Transfer-Encoding: 7BIT\r\n") );

                    if (CommonData.bodyconvert) {
                        fileBuffer.Clear();
                        pp = CommonData.alternateText.Get();
                        for ( ; ; ) {
                            if ( !*pp )
                                break;

                            pp1 = _tcschr( pp, __T('|') );
                            if ( !pp1 )
                                break;

                            fileBuffer.Add( pp, (size_t)(pp1 - pp) );
                            fileBuffer.Add( __T("\r\n") );
                            pp = pp1 + 1;
                        }
                        if ( *pp )
                            fileBuffer.Add( pp );

                        CommonData.alternateText = fileBuffer;
                        fileBuffer.Clear();
                    }

                    if ( CommonData.formattedContent ) {
                        if ( memcmp( (CommonData.alternateText.GetTail()-2), __T("\r\n"), 2*sizeof(_TCHAR) ) != 0 )
                            CommonData.alternateText.Add( __T("\r\n\r\n") );
                        else
                        if ( memcmp( (CommonData.alternateText.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                            CommonData.alternateText.Add( __T("\r\n") );
                    }

                    fileBuffer.Clear();
                    pp = CommonData.alternateText.Get();
                    for ( ; ; ) {
                        pp1 = _tcschr( pp, __T('\n') );
                        if ( !pp1 )
                            break;

                        if ( (pp1 - pp) > 79 ) {
                            int lineBreakFound = FALSE;

                            flowed = TRUE;
                            pp1 = pp + 72;  // RFC 2646 says to limit line lengths to 72,
                                            // with a recommended length of no more than 66.
                            for ( ; pp1 > pp; ) {
                                if ( *(pp1-1) == __T(' ') ) {
                                    fileBuffer.Add( pp, (size_t)(pp1 - pp) );
                                    fileBuffer.Add( __T("\r\n") );
                                    pp = pp1;
                                    lineBreakFound = TRUE;
                                    break;
                                }
                                pp1--;
                            }
                            if ( lineBreakFound )
                                continue;

                            pp1 = _tcschr( pp, __T('\n') );
                        }
                        fileBuffer.Add( pp, (size_t)(pp1 + 1 - pp) );
                        pp = pp1 + 1;
                    }
                    fileBuffer.Add( pp );
                    CommonData.alternateText = fileBuffer;
                    fileBuffer.Clear();

                    messageBuffer.Add( __T("Content-Type: text/plain;\r\n") );
                    if ( flowed )
                        messageBuffer.Add( __T(" format=flowed;\r\n") );

                    messageBuffer.Add( __T(" charset=\"") );
                    if ( altTextCharset[0] )
                        messageBuffer.Add( altTextCharset );
                    else
                        messageBuffer.Add( getCharsetString(CommonData) );
                    messageBuffer.Add( __T("\";\r\n reply-type=original\r\n\r\n") );
                    CommonData.boundaryPosted = TRUE;
                    if ( needQP )
                        ConvertToQuotedPrintable( CommonData, CommonData.alternateText, messageBuffer, FALSE );
                    else
                        messageBuffer.Add( CommonData.alternateText );
                }
#endif
                if ( CommonData.mime ) {
                    // Indicate MIME version and type
                    if ( CommonData.attach || CommonData.alternateText.Length() ) {
#if SMART_CONTENT_TYPE
                        _TCHAR foundType  [0x200];

                        _tcscpy( foundType, __T("text/") );
                        _tcscat( foundType, CommonData.textmode );
#endif
                        if ( !CommonData.alternateText.Length() ) {
                            if ( CommonData.haveEmbedded ) {
                                if ( CommonData.haveAttachments ) {
                                    messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                                    messageBuffer.Add( attachment_boundary );
                                    messageBuffer.Add( __T("Content-Type:") );
                                    messageBuffer.Add( __T(" multipart/related;\r\n") );
                                    messageBuffer.Add( __T(" boundary=\"") BOUNDARY_MARKER );
                                    incrementBoundary( attachment_boundary );
                                    messageBuffer.Add( attachment_boundary, 21 );
                                    messageBuffer.Add( __T("\"\r\n\r\n") );
                                }
                            }
                        }

                        messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                        messageBuffer.Add( attachment_boundary );
                        messageBuffer.Add( __T("Content-Transfer-Encoding: quoted-printable\r\n") );
#if SMART_CONTENT_TYPE
                        if ( !CommonData.ConsoleDone && !_tcscmp( CommonData.textmode, __T("plain") ) )
                            getContentType( CommonData, tmpstr, foundType, foundType, getShortFileName(CommonData.bodyFilename) );

                        messageBuffer.Add( __T("Content-Type: ") );
                        messageBuffer.Add( foundType );
#else
                        messageBuffer.Add( __T("Content-Type: text/") );
                        messageBuffer.Add( CommonData.textmode );
#endif
                        messageBuffer.Add( __T(";\r\n charset=\"") );
                        messageBuffer.Add( getCharsetString(CommonData) );
                        messageBuffer.Add( __T("\";\r\n reply-type=original\r\n\r\n") );
                        CommonData.boundaryPosted = TRUE;
                    }
                } else {
#if BLAT_LITE
#else
                    if ( CommonData.base64 ) {
                        // Indicate MIME version and type
                        messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                        messageBuffer.Add( attachment_boundary );

                        tmpstr.Alloc( _MAX_PATH * 4 );

                        messageBuffer.Add( __T("Content-Type: application/octet-stream;\r\n") );
                        tmpstr.Clear();
                        if ( _tcscmp(CommonData.bodyFilename, __T("-")) == 0 )
                            fixup( CommonData, stdinFileName, &tmpstr, 7, TRUE );
                        else
                            fixupFileName( CommonData, CommonData.bodyFilename, tmpstr, 7, TRUE );

                        tmpstr.SetLength();
                        messageBuffer.Add( __T(" name=\"") );

                        messageBuffer.Add( tmpstr );
                        messageBuffer.Add( __T("\"\r\nContent-Disposition: ATTACHMENT;\r\n") );
                        tmpstr.Clear();
                        if ( _tcscmp(CommonData.bodyFilename, __T("-")) == 0 )
                            fixup( CommonData, stdinFileName, &tmpstr, 11, TRUE );
                        else
                            fixupFileName( CommonData, CommonData.bodyFilename, tmpstr, 11, TRUE );

                        tmpstr.SetLength();
                        messageBuffer.Add( __T(" filename=\"") );
                        messageBuffer.Add( tmpstr );
                        messageBuffer.Add( __T("\"\r\nContent-Transfer-Encoding: BASE64\r\n\r\n") );
                        CommonData.boundaryPosted = TRUE;
                    } else
#endif
                    {
                        if ( CommonData.attach ) {
                            messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                            messageBuffer.Add( attachment_boundary );
#if BLAT_LITE
                            messageBuffer.Add( __T("Content-Description: Mail message body\r\n") );
                            messageBuffer.Add( __T("Content-Transfer-Encoding: 7BIT\r\n") );
                            messageBuffer.Add( __T("Content-Type: text/") );
                            messageBuffer.Add( CommonData.textmode );
                            messageBuffer.Add( __T(";\r\n charset=\"") );
                            messageBuffer.Add( getCharsetString(CommonData) );
                            messageBuffer.Add( __T("\";\r\n reply-type=original\r\n\r\n") );
                            CommonData.boundaryPosted = TRUE;
#else
                            if ( !CommonData.uuencode && !yEnc_This && buildSMTP ) {
  #if SMART_CONTENT_TYPE
                                _TCHAR foundType  [0x200];

                                _tcscpy( foundType, __T("text/") );
                                _tcscat( foundType, CommonData.textmode );
  #endif
                                messageBuffer.Add( __T("Content-Description: Mail message body\r\n") );

                                if ( CommonData.eightBitMimeSupported && (CommonData.eightBitMimeRequested || yEnc_This) ) {
                                    messageBuffer.Add( __T("Content-Transfer-Encoding: 8BIT\r\n") );
                                } else {
                                    messageBuffer.Add( __T("Content-Transfer-Encoding: 7BIT\r\n") );
                                }
  #if SMART_CONTENT_TYPE
                                if ( !CommonData.ConsoleDone && !_tcscmp( CommonData.textmode, __T("plain") ) )
                                    getContentType( CommonData, tmpstr, foundType, foundType, getShortFileName(CommonData.bodyFilename) );

                                messageBuffer.Add( __T("Content-Type: ") );
                                messageBuffer.Add( foundType );
  #else
                                messageBuffer.Add( __T("Content-Type: text/") );
                                messageBuffer.Add( CommonData.textmode );
  #endif
                                messageBuffer.Add( __T(";\r\n charset=\"") );
                                messageBuffer.Add( getCharsetString(CommonData) );
                                messageBuffer.Add( __T("\";\r\n reply-type=original\r\n\r\n") );
                                CommonData.boundaryPosted = TRUE;
                            }
#endif
                        }
                    }
                }
            }

            // Oversized buffer for output message...
            // Quoted printable takes the most space...up to 3 bytes/byte + LFs
            // base64 uses CR every 54 bytes
            // 0x1000 is for the minimal Mime/UUEncode header

            fileBuffer.Clear();
#if SUPPORT_SALUTATIONS
            if ( CommonData.salutation.Length() ) {
                pString = CommonData.salutation.Get();
  #if defined(_UNICODE) || defined(UNICODE)
                if ( *pString == 0xFEFF )
                    pString++;
  #endif
                if ( _tcscmp(CommonData.textmode, __T("html")) == 0 ) {
                    fileBuffer.Add( __T("<p>") );
                    fileBuffer.Add( pString );
                    fileBuffer.Add( __T("</p>") );
                } else
                    fileBuffer.Add( pString );

                fileBuffer.Add( __T("\r\n\r\n") );
            }
#endif
            // Add the whole console message/file
            pString = CommonData.TempConsole.Get();
#if defined(_UNICODE) || defined(UNICODE)
            if ( *pString == 0xFEFF ) {
                pString++;
                msgBodySize--;
            }
#endif
            fileBuffer.Add( pString, msgBodySize );

#if SUPPORT_SIGNATURES
            if ( CommonData.signature.Length() ) {
                pString = CommonData.signature.Get();
  #if defined(_UNICODE) || defined(UNICODE)
                if ( *pString == 0xFEFF )
                    pString++;
  #endif
                if ( _tcscmp(CommonData.textmode, __T("html")) == 0 ) {
                    fileBuffer.Add( __T("<p>\r\n-- <br>\r\n") );
                    fileBuffer.Add( pString );
                    fileBuffer.Add( __T("</p>") );

                } else {
                    if ( CommonData.formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), __T("\r\n"), 2*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n\r\n") );
                        else
                        if ( memcmp( (fileBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n") );
                    }
                    fileBuffer.Add( __T("-- \r\n") );
                    fileBuffer.Add( pString );
                }
            }
#endif
#if SUPPORT_TAGLINES
            if ( CommonData.tagline.Length() ) {
                int x;

                pString = CommonData.tagline.Get();
  #if defined(_UNICODE) || defined(UNICODE)
                if ( *pString == 0xFEFF )
                    pString++;
  #endif
                for ( x = (int)CommonData.tagline.Length(); x; ) {
                    x--;
                    if ( pString[x] == __T('\0') )
                        break;
                    if ( (pString[x] == __T('\\')) && (pString[x+1] == __T('n')) ) {
                        pString[x  ] = __T('\r');
                        pString[x+1] = __T('\n');
                    }
                }
                if ( _tcscmp(CommonData.textmode, __T("html")) == 0 ) {
                    fileBuffer.Add( __T("<p>") );
                    fileBuffer.Add( pString );
                    fileBuffer.Add( __T("</p>") );

                } else {
                    if ( CommonData.formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), __T("\r\n"), 2*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n\r\n") );
                        else
                        if ( memcmp( (fileBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n") );
                    }
                    fileBuffer.Add( pString );
                }
            }
#endif
#if SUPPORT_POSTSCRIPTS
            if ( CommonData.postscript.Length() ) {
                pString = CommonData.postscript.Get();
  #if defined(_UNICODE) || defined(UNICODE)
                if ( *pString == 0xFEFF )
                    pString++;
  #endif
                if ( _tcscmp(CommonData.textmode, __T("html")) == 0 ) {
                    fileBuffer.Add( __T("<p>") );
                    fileBuffer.Add( pString );
                    fileBuffer.Add( __T("</p>") );

                } else {
                    if ( CommonData.formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), __T("\r\n"), 2*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n\r\n") );
                        else
                        if ( memcmp( (fileBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n") );
                    }
                    fileBuffer.Add( pString );
                }
            }
#endif
#if defined(_UNICODE) || defined(UNICODE)
            {
                _TUCHAR * pUChar;
                size_t    x;
                Buf       holdingPen;
                int       tempUTF;

                pUChar = fileBuffer.Get();
                for ( x = 0; *pUChar != __T('\0'); pUChar++ ) {
                    if ( (*pUChar > 0x00FF) || ((memcmp(CommonData.charset, __T("UTF-"),4*sizeof(_TCHAR)) == 0) && (*pUChar > 0x007F)) ) {
                        tempUTF = NATIVE_16BIT_UTF;
                        convertPackedUnicodeToUTF( fileBuffer, holdingPen, &tempUTF, NULL, utfRequested );
                        if ( tempUTF )
                            fileBuffer = holdingPen;

                        holdingPen.Free();
                        break;
                    }
                }
            }
#endif

            // MIME Quoted-Printable Content-Transfer-Encoding
            // or BASE64 encoding of main file.
            // or UUencoding of main file
            // or nothing special...
            if ( CommonData.mime )
                ConvertToQuotedPrintable( CommonData, fileBuffer, messageBuffer, FALSE );
            else
#if BLAT_LITE
#else
            if ( CommonData.base64 )
                base64_encode( fileBuffer, messageBuffer, TRUE, TRUE );
            else
            if ( !CommonData.attach && CommonData.uuencode ) {
                if ( _tcscmp(CommonData.bodyFilename, __T("-")) == 0 )
                    douuencode( CommonData, fileBuffer, messageBuffer, stdinFileName, 1, 1 );
                else
                    douuencode( CommonData, fileBuffer, messageBuffer, getShortFileName(CommonData.bodyFilename), 1, 1 );
            }
            else
#endif
                messageBuffer.Add( fileBuffer );

            if ( CommonData.formattedContent )
                if ( memcmp( (messageBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                    messageBuffer.Add( __T("\r\n") );
#if BLAT_LITE
#else
            if ( CommonData.alternateText.Length() ) {
                if ( CommonData.haveEmbedded || CommonData.haveAttachments ) {
                    if ( *(messageBuffer.GetTail()-3) != __T('\n') )
                        messageBuffer.Add( __T("\r\n") );

                    messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                    messageBuffer.Add( attachment_boundary, 21 );
                    messageBuffer.Add( __T("--\r\n") );
                    decrementBoundary( attachment_boundary );
                }
            }
#endif
        } /* end of msgBodySize */

        // make some noise about what we are doing
        tmpstr = __T("Sending ");
        tmpstr.Add( CommonData.bodyFilename );
        tmpstr.Add( __T(" to ") );

        if ( buildSMTP ) {
            if ( CommonData.Recipients.Length() ) {
#if SUPPORT_SALUTATIONS
                int    x;
                Buf    parsed_addys;
                LPTSTR pa;

                parse_email_addresses( CommonData, CommonData.Recipients.Get(), parsed_addys );
                pa = parsed_addys.Get();
                if ( pa ) {
                    for ( x = 0; pa[x]; ) {
                        if ( x )
                            tmpstr.Add( __T(", ") );

                        tmpstr.Add( &pa[x] );
                        x += (int)(_tcslen( &pa[x] ) + 1);
                    }

                }
                parsed_addys.Free();
#else
                tmpstr.Add( CommonData.Recipients );
#endif
            }
            else
                tmpstr.Add( __T("<unspecified>") );
        }
#if INCLUDE_NNTP
        else
            if ( CommonData.groups.Length() )
                tmpstr.Add( CommonData.groups );
            else
                tmpstr.Add( __T("<unspecified>") );
#endif

        if ( tmpstr.Length() > 1019 )
            printMsg( CommonData, __T("%.*s...\n"), 1019, tmpstr.Get() );
        else
            printMsg( CommonData, __T("%s\n"), tmpstr.Get() );

        if ( CommonData.subject.Length() ) printMsg(CommonData, __T("Subject: %s\n"), CommonData.subject.Get());
        if ( _tcslen(CommonData.loginname) ) printMsg(CommonData, __T("Login name is %s\n"), CommonData.loginname);
    }

    fileBuffer.Free();
    tmpstr.Free();
    return(0);
}


void add_msg_boundary ( COMMON_DATA & CommonData, Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary )
{
#if SUPPORT_YENC
    int yEnc_This;

    yEnc_This = CommonData.yEnc;
    if ( buildSMTP && !CommonData.eightBitMimeSupported )
        yEnc_This = FALSE;
#else
    buildSMTP = buildSMTP;  // remove compiler warnings
#endif

#if BLAT_LITE
    if ( CommonData.boundaryPosted ) {
#else
    if ( CommonData.boundaryPosted && !CommonData.uuencode ) {
  #if SUPPORT_YENC
        if ( !yEnc_This )
  #endif
#endif
        {
            if ( *(messageBuffer.GetTail()-3) != __T('\n') )
                messageBuffer.Add( __T("\r\n") );

            messageBuffer.Add( __T("--") BOUNDARY_MARKER );
            messageBuffer.Add( attachment_boundary, 21 );
            messageBuffer.Add( __T("--\r\n") );
        }
    }
}
