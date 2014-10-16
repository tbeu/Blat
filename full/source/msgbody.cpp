/*
    msgbody.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blat.h"
#include "winfile.h"

#ifndef _MAX_PATH
    #define _MAX_PATH 260
#endif

extern void   incrementBoundary( _TCHAR boundary[] );
extern void   decrementBoundary( _TCHAR boundary[] );
extern LPTSTR getShortFileName (LPTSTR fileName);
extern void   fixupFileName ( LPTSTR filename, Buf & outString, int headerLen, int linewrap );
extern int    CheckIfNeedQuotedPrintable(LPTSTR pszStr, int inHeader);
extern void   ConvertToQuotedPrintable(Buf & source, Buf & out, int inHeader);
extern void   base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);
#if BLAT_LITE
#else
extern void   douuencode(Buf & source, Buf & out, LPTSTR filename, int part, int lastpart);
extern void   convertUnicode( Buf & sourceText, int * utf, LPTSTR charset, int utfRequested );
#endif
extern void   printMsg(LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison
extern void   fixup(LPTSTR string, Buf * tempstring2, int headerLen, int linewrap );

#if SMART_CONTENT_TYPE
extern void   getContentType(Buf & sDestBuffer, LPTSTR foundType, LPTSTR defaultType, LPTSTR sFileName);
#endif
#if SUPPORT_SALUTATIONS
extern void   parse_email_addresses( LPTSTR email_addresses, Buf & parsed_addys );
#endif

#if SUPPORT_POSTSCRIPTS
extern Buf    postscript;
#endif
#if SUPPORT_TAGLINES
extern Buf    tagline;
#endif
#if SUPPORT_SIGNATURES
extern Buf    signature;
#endif
#if BLAT_LITE
#else
extern _TCHAR eightBitMimeSupported;
extern _TCHAR eightBitMimeRequested;

extern int    utf;
extern _TCHAR base64;
extern _TCHAR uuencode;
extern _TCHAR yEnc;
#endif
#if INCLUDE_NNTP
extern Buf    groups;
#endif
#if SUPPORT_SALUTATIONS
extern Buf    salutation;
#endif

extern Buf    Recipients;
extern _TCHAR subject[];
extern _TCHAR bodyFilename[];
extern _TCHAR ConsoleDone;
extern _TCHAR formattedContent;
extern _TCHAR mime;
extern _TCHAR loginname[];    // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
extern _TCHAR textmode[];
extern int    attach;
extern _TCHAR haveEmbedded;
extern _TCHAR haveAttachments;
extern Buf    alternateText;
extern _TCHAR bodyconvert;
extern Buf    TempConsole;
extern _TCHAR charset[];      // Added 25 Apr 2001 Tim Charron (default ISO-8859-1)

extern LPTSTR stdinFileName;

_TCHAR boundaryPosted;
_TCHAR needBoundary;


int add_message_body ( Buf &messageBuffer, int msgBodySize, Buf &multipartHdrs, int buildSMTP,
                       LPTSTR attachment_boundary, DWORD startOffset, int part,
                       int attachNbr )
{
    int     yEnc_This;
    Buf     tmpstr;
    Buf     fileBuffer;
    WinFile atf;

    tmpstr.Alloc( 0x2000 );
    tmpstr.Clear();
    if ( !startOffset && !attachNbr ) {
        if ( part /* || alternateText.Length() */ ) {
            if ( memcmp( (messageBuffer.GetTail()-3), "\n\r\n", 3*sizeof(_TCHAR) ) != 0 )
                messageBuffer.Add( __T("\r\n") );

            messageBuffer.Add( multipartHdrs );
            if ( multipartHdrs.Length() &&
                 !_tcsstr( messageBuffer.Get(), __T("This is a multi-part message in MIME format.") ) )
                needBoundary = FALSE;
        }

        if ( formattedContent )
            if ( memcmp( (messageBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                messageBuffer.Add( __T("\r\n") );

        if ( msgBodySize ) {

#if BLAT_LITE
#else
            int utfRequested;
            int localUTF;
            if ( (eightBitMimeSupported && eightBitMimeRequested) || (lstrcmpi(charset, __T("UTF-8")) == 0) )
                utfRequested = 8;
            else
                utfRequested = 7;
#endif
#if SUPPORT_YENC
            yEnc_This = yEnc;
            if ( buildSMTP && !eightBitMimeSupported )
#endif
                yEnc_This = FALSE;

            if ( !buildSMTP &&
#if BLAT_LITE
#else
                 !base64 && !uuencode &&
#endif
                 !yEnc_This ) {
                if ( CheckIfNeedQuotedPrintable(TempConsole.Get(), FALSE) ) {
                    mime = TRUE;
                }
            }

            if ( needBoundary ) {
#if BLAT_LITE
#else
                if ( alternateText.Length() ) {
                    LPTSTR pp;
                    LPTSTR pp1;
                    int needQP = FALSE;
                    int flowed = FALSE;

                    _TCHAR altTextCharset[8];
                    altTextCharset[0] = 0;
                    localUTF = utf;
                    convertUnicode( alternateText, &localUTF, altTextCharset, utfRequested );

                    if ( haveEmbedded || haveAttachments ) {
                        if ( haveEmbedded && haveAttachments ) {
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
                    if ( CheckIfNeedQuotedPrintable(alternateText.Get(), TRUE) ) {
                        needQP = TRUE;
                        messageBuffer.Add( __T("Content-Transfer-Encoding: quoted-printable\r\n") );
                    } else
                        messageBuffer.Add( __T("Content-Transfer-Encoding: 7BIT\r\n") );

                    if (bodyconvert) {
                        fileBuffer.Clear();
                        pp = alternateText.Get();
                        for ( ; ; ) {
                            if ( !*pp )
                                break;

                            pp1 = _tcschr( pp, __T('|') );
                            if ( !pp1 )
                                break;

                            fileBuffer.Add( pp, (int)(pp1 - pp) );
                            fileBuffer.Add( __T("\r\n") );
                            pp = pp1 + 1;
                        }
                        if ( *pp )
                            fileBuffer.Add( pp );

                        alternateText = fileBuffer;
                        fileBuffer.Clear();
                    }

                    if ( formattedContent ) {
                        if ( memcmp( (alternateText.GetTail()-2), __T("\r\n"), 2*sizeof(_TCHAR) ) != 0 )
                            alternateText.Add( __T("\r\n\r\n") );
                        else
                        if ( memcmp( (alternateText.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                            alternateText.Add( __T("\r\n") );
                    }

                    fileBuffer.Clear();
                    pp = alternateText.Get();
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
                                    fileBuffer.Add( pp, (int)(pp1 - pp) );
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
                        fileBuffer.Add( pp, (int)(pp1 + 1 - pp) );
                        pp = pp1 + 1;
                    }
                    fileBuffer.Add( pp );
                    alternateText = fileBuffer;
                    fileBuffer.Clear();

                    messageBuffer.Add( __T("Content-Type: text/plain;") );
                    if ( flowed )
                        messageBuffer.Add( __T(" format=flowed;") );

                    messageBuffer.Add( __T(" charset=") );
                    if ( altTextCharset[0] )
                        messageBuffer.Add( altTextCharset );
                    else
                        messageBuffer.Add( charset );
                    messageBuffer.Add( __T("\r\n\r\n") );
                    boundaryPosted = TRUE;
                    if ( needQP )
                        ConvertToQuotedPrintable( alternateText, messageBuffer, FALSE );
                    else
                        messageBuffer.Add( alternateText );
                }
#endif
                if ( mime ) {
                    // Indicate MIME version and type
                    if ( attach || alternateText.Length() ) {
#if SMART_CONTENT_TYPE
                        _TCHAR foundType  [0x200];

                        _tcscpy( foundType, __T("text/") );
                        _tcscat( foundType, textmode );
#endif
                        if ( !alternateText.Length() ) {
                            if ( haveEmbedded ) {
                                if ( haveAttachments ) {
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
                        if ( !ConsoleDone && !_tcscmp( textmode, __T("plain") ) )
                            getContentType( tmpstr, foundType, foundType, getShortFileName(bodyFilename) );

                        messageBuffer.Add( __T("Content-Type: ") );
                        messageBuffer.Add( foundType );
#else
                        messageBuffer.Add( __T("Content-Type: text/") );
                        messageBuffer.Add( textmode );
#endif
                        messageBuffer.Add( __T("; charset=") );
                        messageBuffer.Add( charset );
                        messageBuffer.Add( __T("\r\n\r\n") );
                        boundaryPosted = TRUE;
                    }
                } else {
#if BLAT_LITE
#else
                    if ( base64 ) {
                        // Indicate MIME version and type
                        messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                        messageBuffer.Add( attachment_boundary );

                        tmpstr.Alloc( _MAX_PATH * 4 );
                        tmpstr.Clear();
                        if ( _tcscmp(bodyFilename, __T("-")) == 0 )
                            fixup( stdinFileName, &tmpstr, 11, TRUE );
                        else {
                            fixupFileName( bodyFilename, tmpstr, 11, TRUE );
                        }

                        tmpstr.SetLength();

                        messageBuffer.Add( __T("Content-Type: application/octet-stream;\r\n name=\"") );
                        messageBuffer.Add( tmpstr );
                        messageBuffer.Add( __T("\"\r\nContent-Disposition: ATTACHMENT;\r\n filename=\"") );
                        messageBuffer.Add( tmpstr );
                        messageBuffer.Add( __T("\"\r\nContent-Transfer-Encoding: BASE64\r\n\r\n") );
                        boundaryPosted = TRUE;
                    } else
#endif
                    {
                        if ( attach ) {
                            messageBuffer.Add( __T("--") BOUNDARY_MARKER );
                            messageBuffer.Add( attachment_boundary );
#if BLAT_LITE
                            messageBuffer.Add( __T("Content-description: Mail message body\r\n") );
                            messageBuffer.Add( __T("Content-Transfer-Encoding: 7BIT\r\n") );
                            messageBuffer.Add( __T("Content-Type: text/") );
                            messageBuffer.Add( textmode );
                            messageBuffer.Add( __T("; charset=") );
                            messageBuffer.Add( charset );
                            messageBuffer.Add( __T("\r\n\r\n") );
                            boundaryPosted = TRUE;
#else
                            if ( !uuencode && !yEnc_This && buildSMTP ) {
  #if SMART_CONTENT_TYPE
                                _TCHAR foundType  [0x200];

                                _tcscpy( foundType, __T("text/") );
                                _tcscat( foundType, textmode );
  #endif
                                messageBuffer.Add( __T("Content-description: ") );
                                messageBuffer.Add( __T("Mail") );
                                messageBuffer.Add( __T(" message body\r\n") );

                                if ( eightBitMimeSupported && (eightBitMimeRequested || yEnc_This) ) {
                                    messageBuffer.Add( __T("Content-Transfer-Encoding: 8BIT\r\n") );
                                } else {
                                    messageBuffer.Add( __T("Content-Transfer-Encoding: 7BIT\r\n") );
                                }
  #if SMART_CONTENT_TYPE
                                if ( !ConsoleDone && !_tcscmp( textmode, __T("plain") ) )
                                    getContentType( tmpstr, foundType, foundType, getShortFileName(bodyFilename) );

                                messageBuffer.Add( __T("Content-Type: ") );
                                messageBuffer.Add( foundType );
  #else
                                messageBuffer.Add( __T("Content-Type: text/") );
                                messageBuffer.Add( textmode );
  #endif
                                messageBuffer.Add( __T("; charset=") );
                                messageBuffer.Add( charset );
                                messageBuffer.Add( __T("\r\n\r\n") );
                                boundaryPosted = TRUE;
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
            if ( salutation.Length() ) {
                if ( _tcscmp(textmode, __T("html")) == 0 ) {
                    fileBuffer.Add( __T("<p>") );
                    fileBuffer.Add( salutation );
                    fileBuffer.Add( __T("</p>") );
                } else
                    fileBuffer.Add( salutation );

                fileBuffer.Add( __T("\r\n\r\n") );
            }
#endif
            // Add the whole console message/file
            fileBuffer.Add( TempConsole.Get(), msgBodySize );

#if SUPPORT_SIGNATURES
            localUTF = utf;
            convertUnicode( signature, &localUTF, NULL, utfRequested );
            if ( signature.Length() ) {
                if ( _tcscmp(textmode, __T("html")) == 0 ) {
                    fileBuffer.Add( __T("<p>\r\n-- <br>\r\n") );
                    fileBuffer.Add( signature );
                    fileBuffer.Add( __T("</p>") );

                } else {
                    if ( formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), __T("\r\n"), 2*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n\r\n") );
                        else
                        if ( memcmp( (fileBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n") );
                    }
                    fileBuffer.Add( __T("-- \r\n") );
                    fileBuffer.Add( signature );
                }
            }
#endif
#if SUPPORT_TAGLINES
            if ( tagline.Length() ) {
                int    x;
                LPTSTR p;

                p = tagline.Get();
                for ( x = (int)tagline.Length(); x; ) {
                    x--;
                    if ( (p[x] == __T('\\')) && (p[x+1] == __T('n')) ) {
                        p[x  ] = __T('\r');
                        p[x+1] = __T('\n');
                    }
                }
                if ( _tcscmp(textmode, __T("html")) == 0 ) {
                    fileBuffer.Add( __T("<p>") );
                    fileBuffer.Add( tagline );
                    fileBuffer.Add( __T("</p>") );

                } else {
                    if ( formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), __T("\r\n"), 2*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n\r\n") );
                        else
                        if ( memcmp( (fileBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n") );
                    }
                    fileBuffer.Add( tagline );
                }
            }
#endif
#if SUPPORT_POSTSCRIPTS
            if ( postscript.Length() ) {
                if ( _tcscmp(textmode, __T("html")) == 0 ) {
                    fileBuffer.Add( __T("<p>") );
                    fileBuffer.Add( postscript );
                    fileBuffer.Add( __T("</p>") );

                } else {
                    if ( formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), __T("\r\n"), 2*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n\r\n") );
                        else
                        if ( memcmp( (fileBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                            fileBuffer.Add( __T("\r\n") );
                    }
                    fileBuffer.Add( postscript );
                }
            }
#endif

            // MIME Quoted-Printable Content-Transfer-Encoding
            // or BASE64 encoding of main file.
            // or UUencoding of main file
            // or nothing special...
            if ( mime )
                ConvertToQuotedPrintable( fileBuffer, messageBuffer, FALSE );
            else
#if BLAT_LITE
#else
            if ( base64 )
                base64_encode( fileBuffer, messageBuffer, TRUE, TRUE );
            else
            if ( !attach && uuencode ) {
                if ( _tcscmp(bodyFilename, __T("-")) == 0 )
                    douuencode( fileBuffer, messageBuffer, stdinFileName, 1, 1 );
                else
                    douuencode( fileBuffer, messageBuffer, getShortFileName(bodyFilename), 1, 1 );
            }
            else
#endif
                messageBuffer.Add( fileBuffer );

            if ( formattedContent )
                if ( memcmp( (messageBuffer.GetTail()-3), __T("\n\r\n"), 3*sizeof(_TCHAR) ) != 0 )
                    messageBuffer.Add( __T("\r\n") );
#if BLAT_LITE
#else
            if ( alternateText.Length() ) {
                if ( haveEmbedded || haveAttachments ) {
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
        tmpstr.Add( bodyFilename );
        tmpstr.Add( __T(" to ") );

        if ( buildSMTP ) {
            if ( Recipients.Length() ) {
#if SUPPORT_SALUTATIONS
                int    x;
                Buf    parsed_addys;
                LPTSTR pa;

                parse_email_addresses( Recipients.Get(), parsed_addys );
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
                tmpstr.Add( Recipients );
#endif
            }
            else
                tmpstr.Add( __T("<unspecified>") );
        }
#if INCLUDE_NNTP
        else
            if ( groups.Length() )
                tmpstr.Add( groups );
            else
                tmpstr.Add( __T("<unspecified>") );
#endif

        if ( tmpstr.Length() > 1019 )
            printMsg( __T("%.*s...\n"), 1019, tmpstr.Get() );
        else
            printMsg( __T("%s\n"), tmpstr.Get() );

        if ( _tcslen(subject) ) printMsg( __T("Subject: %s\n"), subject);
        if ( _tcslen(loginname) ) printMsg( __T("Login name is %s\n"), loginname);
    }

    fileBuffer.Free();
    tmpstr.Free();
    return(0);
}


void add_msg_boundary ( Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary )
{
#if SUPPORT_YENC
    int yEnc_This;

    yEnc_This = yEnc;
    if ( buildSMTP && !eightBitMimeSupported )
        yEnc_This = FALSE;
#else
    buildSMTP = buildSMTP;  // remove compiler warnings
#endif

#if BLAT_LITE
    if ( boundaryPosted ) {
#else
    if ( boundaryPosted && !uuencode ) {
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
