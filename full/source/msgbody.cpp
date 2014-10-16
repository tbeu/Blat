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

extern void   incrementBoundary( char boundary[] );
extern void   decrementBoundary( char boundary[] );
extern LPCSTR GetNameWithoutPath(LPCSTR lpFn);
extern int    CheckIfNeedQuotedPrintable(unsigned char *pszStr, int inHeader);
extern void   ConvertToQuotedPrintable(Buf & source, Buf & out, int inHeader);
extern void   base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);
#if BLAT_LITE
#else
extern void   douuencode(Buf & source, Buf & out, const char * filename, int part, int lastpart);
extern void   convertUnicode( Buf &sourceText, int * utf, char * charset, int utfRequested );
#endif
extern void   printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison
extern void   fixup(char * string, Buf * tempstring2, int headerLen, int linewrap );

#if SMART_CONTENT_TYPE
extern char * getShortFileName (char * fileName);
extern void   getContentType(char *sDestBuffer, char *foundType, char *defaultType, char *sFileName);
#endif
#if SUPPORT_SALUTATIONS
extern void   parse_email_addresses( const char * email_addresses, Buf & parsed_addys );
#endif

#if SUPPORT_POSTSCRIPTS
extern Buf  postscript;
#endif
#if SUPPORT_TAGLINES
extern Buf  tagline;
#endif
#if SUPPORT_SIGNATURES
extern Buf  signature;
#endif
#if BLAT_LITE
#else
extern char eightBitMimeSupported;
extern char eightBitMimeRequested;

extern int  utf;
extern char base64;
extern char uuencode;
extern char yEnc;
#endif
#if INCLUDE_NNTP
extern Buf  groups;
#endif
#if SUPPORT_SALUTATIONS
extern Buf  salutation;
#endif

extern Buf  Recipients;
extern char subject[];
extern char bodyFilename[];
extern char ConsoleDone;
extern char formattedContent;
extern char mime;
extern char loginname[];    // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
extern char textmode[];
extern int  attach;
extern char haveEmbedded;
extern char haveAttachments;
extern Buf  alternateText;
extern char bodyconvert;
extern Buf  TempConsole;
extern char charset[];      // Added 25 Apr 2001 Tim Charron (default ISO-8859-1)

extern const char * stdinFileName;

char boundaryPosted;
char needBoundary;


int add_message_body ( Buf &messageBuffer, int msgBodySize, Buf &multipartHdrs, int buildSMTP,
                       char * attachment_boundary, DWORD startOffset, int part,
                       int attachNbr )
{
    int     yEnc_This;
    Buf     tmpstr;
    Buf     fileBuffer;
    WinFile atf;

    tmpstr.Alloc( 0x2000 );
    if ( !startOffset && !attachNbr ) {
        if ( part /* || alternateText.Length() */ ) {
            if ( memcmp( (messageBuffer.GetTail()-3), "\n\r\n", 3 ) != 0 )
                messageBuffer.Add( "\r\n" );

            messageBuffer.Add( multipartHdrs.Get() );
            if ( multipartHdrs.Length() &&
                 !strstr( messageBuffer.Get(), "This is a multi-part message in MIME format." ) )
                needBoundary = FALSE;
        }

        if ( formattedContent )
            if ( memcmp( (messageBuffer.GetTail()-3), "\n\r\n", 3 ) != 0 )
                messageBuffer.Add( "\r\n" );

        if ( msgBodySize ) {

#if BLAT_LITE
#else
            int utfRequested;
            int localUTF;
            if ( (eightBitMimeSupported && eightBitMimeRequested) || (_stricmp(charset, "UTF-8") == 0) )
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
                if ( CheckIfNeedQuotedPrintable((unsigned char *)TempConsole.Get(), FALSE) ) {
                    mime = TRUE;
                }
            }

            if ( needBoundary ) {
#if BLAT_LITE
#else
                if ( alternateText.Length() ) {
                    char * pp;
                    char * pp1;
                    int needQP = FALSE;
                    int flowed = FALSE;

                    char altTextCharset[8];
                    altTextCharset[0] = 0;
                    localUTF = utf;
                    convertUnicode( alternateText, &localUTF, altTextCharset, utfRequested );

                    if ( haveEmbedded || haveAttachments ) {
                        if ( haveEmbedded && haveAttachments ) {
                            messageBuffer.Add( "--" BOUNDARY_MARKER );
                            messageBuffer.Add( attachment_boundary );
                            messageBuffer.Add( "Content-Type:" );
                            messageBuffer.Add( " multipart/related;\r\n" );
                            messageBuffer.Add( " type=\"multipart/alternative\";\r\n" );
                            messageBuffer.Add( " boundary=\"" BOUNDARY_MARKER );
                            incrementBoundary( attachment_boundary );
                            messageBuffer.Add( attachment_boundary, 21 );
                            messageBuffer.Add( "\"\r\n\r\n" );
                        }
                        messageBuffer.Add( "--" BOUNDARY_MARKER );
                        messageBuffer.Add( attachment_boundary );
                        messageBuffer.Add( "Content-Type:" );
                        messageBuffer.Add( " multipart/alternative;\r\n" );
                        messageBuffer.Add( " boundary=\"" BOUNDARY_MARKER );
                        incrementBoundary( attachment_boundary );
                        messageBuffer.Add( attachment_boundary, 21 );
                        messageBuffer.Add( "\"\r\n\r\n" );
                    }

                    messageBuffer.Add( "--" BOUNDARY_MARKER );
                    messageBuffer.Add( attachment_boundary );
                    if ( CheckIfNeedQuotedPrintable((unsigned char *)alternateText.Get(), TRUE) ) {
                        needQP = TRUE;
                        messageBuffer.Add( "Content-Transfer-Encoding: quoted-printable\r\n" );
                    }
                    else
                        messageBuffer.Add( "Content-Transfer-Encoding: 7BIT\r\n" );

                    if (bodyconvert) {
                        fileBuffer.Clear();
                        pp = alternateText.Get();
                        for ( ; ; ) {
                            if ( !*pp )
                                break;

                            pp1 = strchr( pp, '|' );
                            if ( !pp1 )
                                break;

                            fileBuffer.Add( pp, (int)(pp1 - pp) );
                            fileBuffer.Add( "\r\n" );
                            pp = pp1 + 1;
                        }
                        if ( *pp )
                            fileBuffer.Add( pp );

                        alternateText.Clear();
                        alternateText.Add( fileBuffer.Get() );
                        fileBuffer.Clear();
                    }

                    if ( formattedContent ) {
                        if ( memcmp( (alternateText.GetTail()-2), "\r\n", 2 ) != 0 )
                            alternateText.Add( "\r\n\r\n" );
                        else
                            if ( memcmp( (alternateText.GetTail()-3), "\n\r\n", 3 ) != 0 )
                                alternateText.Add( "\r\n" );
                    }

                    fileBuffer.Clear();
                    pp = alternateText.Get();
                    for ( ; ; ) {
                        pp1 = strchr( pp, '\n' );
                        if ( !pp1 )
                            break;

                        if ( (pp1 - pp) > 79 ) {
                            int lineBreakFound = FALSE;

                            flowed = TRUE;
                            pp1 = pp + 72;  // RFC 2646 says to limit line lengths to 72,
                                            // with a recommended length of no more than 66.
                            for ( ; pp1 > pp; ) {
                                if ( *(pp1-1) == ' ' ) {
                                    fileBuffer.Add( pp, (int)(pp1 - pp) );
                                    fileBuffer.Add( "\r\n" );
                                    pp = pp1;
                                    lineBreakFound = TRUE;
                                    break;
                                }
                                pp1--;
                            }
                            if ( lineBreakFound )
                                continue;

                            pp1 = strchr( pp, '\n' );
                        }
                        fileBuffer.Add( pp, (int)(pp1 + 1 - pp) );
                        pp = pp1 + 1;
                    }
                    fileBuffer.Add( pp );
                    alternateText.Clear();
                    alternateText.Add( fileBuffer.Get() );
                    fileBuffer.Clear();

                    messageBuffer.Add( "Content-Type: text/plain;" );
                    if ( flowed )
                        messageBuffer.Add( " format=flowed;" );

                    if ( altTextCharset[0] )
                        sprintf( tmpstr.Get(), " charset=%s\r\n\r\n", altTextCharset);
                    else
                        sprintf( tmpstr.Get(), " charset=%s\r\n\r\n", charset);
                    messageBuffer.Add( tmpstr.Get() );
                    boundaryPosted = TRUE;
                    if ( needQP )
                        ConvertToQuotedPrintable( alternateText, messageBuffer, FALSE );
                    else
                        messageBuffer.Add( alternateText.Get() );
                }
#endif
                if ( mime ) {
                    // Indicate MIME version and type
                    if ( attach || alternateText.Length() ) {
#if SMART_CONTENT_TYPE
                        char foundType  [0x200];

                        strcpy( foundType, "text/" );
                        strcat( foundType, textmode );
#endif
                        if ( !alternateText.Length() ) {
                            if ( haveEmbedded ) {
                                if ( haveAttachments ) {
                                    messageBuffer.Add( "--" BOUNDARY_MARKER );
                                    messageBuffer.Add( attachment_boundary );
                                    messageBuffer.Add( "Content-Type:" );
                                    messageBuffer.Add( " multipart/related;\r\n" );
                                    messageBuffer.Add( " boundary=\"" BOUNDARY_MARKER );
                                    incrementBoundary( attachment_boundary );
                                    messageBuffer.Add( attachment_boundary, 21 );
                                    messageBuffer.Add( "\"\r\n\r\n" );
                                }
                            }
                        }

                        messageBuffer.Add( "--" BOUNDARY_MARKER );
                        messageBuffer.Add( attachment_boundary );
                        messageBuffer.Add( "Content-Transfer-Encoding: quoted-printable\r\n" );
#if SMART_CONTENT_TYPE
                        if ( !ConsoleDone && !strcmp( textmode, "plain") )
                            getContentType( tmpstr.Get(), foundType, foundType, getShortFileName(bodyFilename) );

                        sprintf( tmpstr.Get(), "Content-Type: %s; charset=%s\r\n\r\n", foundType, charset );
#else
                        sprintf( tmpstr.Get(), "Content-Type: text/%s; charset=%s\r\n\r\n", textmode, charset);
#endif
                        messageBuffer.Add( tmpstr.Get() );
                        boundaryPosted = TRUE;
                    }
                } else {
#if BLAT_LITE
#else
                    if ( base64 ) {
                        // Indicate MIME version and type
                        messageBuffer.Add( "--" BOUNDARY_MARKER );
                        messageBuffer.Add( attachment_boundary );

                        strcpy( tmpstr.Get(),
                                (strcmp(bodyFilename, "-") == 0) ? stdinFileName : GetNameWithoutPath(bodyFilename) );
                        fixup( tmpstr.Get(), NULL, 11, TRUE );
                        tmpstr.SetLength();

                        messageBuffer.Add( "Content-Type: application/octet-stream;\r\n name=\"" );
                        messageBuffer.Add( tmpstr.Get() );
                        messageBuffer.Add( "\"\r\nContent-Disposition: ATTACHMENT;\r\n filename=\"" );
                        messageBuffer.Add( tmpstr.Get() );
                        messageBuffer.Add( "\"\r\nContent-Transfer-Encoding: BASE64\r\n\r\n" );
                        boundaryPosted = TRUE;
                    } else
#endif
                    {
                        if ( attach ) {
                            messageBuffer.Add( "--" BOUNDARY_MARKER );
                            messageBuffer.Add( attachment_boundary );
#if BLAT_LITE
                            messageBuffer.Add( "Content-description: Mail message body\r\n" );

                            messageBuffer.Add( "Content-Transfer-Encoding: 7BIT\r\n" );
                            sprintf(tmpstr.Get(), "Content-Type: text/%s; charset=%s\r\n\r\n", textmode, charset);
                            messageBuffer.Add( tmpstr.Get() );
                            boundaryPosted = TRUE;
#else
                            if ( !uuencode && !yEnc_This && buildSMTP ) {
  #if SMART_CONTENT_TYPE
                                char foundType  [0x200];

                                strcpy( foundType, "text/" );
                                strcat( foundType, textmode );
  #endif
                                messageBuffer.Add( "Content-description: " );
                                messageBuffer.Add( "Mail" );
                                messageBuffer.Add( " message body\r\n" );

                                if ( eightBitMimeSupported && (eightBitMimeRequested || yEnc_This) ) {
                                    messageBuffer.Add( "Content-Transfer-Encoding: 8BIT\r\n" );
                                } else {
                                    messageBuffer.Add( "Content-Transfer-Encoding: 7BIT\r\n" );
                                }
  #if SMART_CONTENT_TYPE
                                if ( !ConsoleDone && !strcmp( textmode, "plain") )
                                    getContentType( tmpstr.Get(), foundType, foundType, getShortFileName(bodyFilename) );

                                sprintf( tmpstr.Get(), "Content-Type: %s; charset=%s\r\n\r\n", foundType, charset );
  #else
                                sprintf( tmpstr.Get(), "Content-Type: text/%s; charset=%s\r\n\r\n", textmode, charset);
  #endif
                                messageBuffer.Add( tmpstr.Get() );
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
                if ( strcmp(textmode, "html") == 0 ) {
                    fileBuffer.Add( "<p>" );
                    fileBuffer.Add( salutation.Get() );
                    fileBuffer.Add( "</p>" );
                } else
                    fileBuffer.Add( salutation.Get() );

                fileBuffer.Add( "\r\n\r\n" );
            }
#endif
            // Add the whole console message/file
            fileBuffer.Add( TempConsole.Get(), msgBodySize );

#if SUPPORT_SIGNATURES
            localUTF = utf;
            convertUnicode( signature, &localUTF, NULL, utfRequested );
            if ( signature.Length() ) {
                if ( strcmp(textmode, "html") == 0 ) {
                    fileBuffer.Add( "<p>\r\n-- <br>\r\n" );
                    fileBuffer.Add( signature.Get() );
                    fileBuffer.Add( "</p>" );

                } else {
                    if ( formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), "\r\n", 2 ) != 0 )
                            fileBuffer.Add( "\r\n\r\n" );
                        else
                            if ( memcmp( (fileBuffer.GetTail()-3), "\n\r\n", 3 ) != 0 )
                                fileBuffer.Add( "\r\n" );
                    }
                    fileBuffer.Add( "-- \r\n" );
                    fileBuffer.Add( signature.Get() );
                }
            }
#endif
#if SUPPORT_TAGLINES
            if ( tagline.Length() ) {
                int    x;
                char * p;

                p = tagline.Get();
                for ( x = tagline.Length(); x; ) {
                    x--;
                    if ( (p[x] == '\\') && (p[x+1] == 'n') ) {
                        p[x  ] = '\r';
                        p[x+1] = '\n';
                    }
                }
                if ( strcmp(textmode, "html") == 0 ) {
                    fileBuffer.Add( "<p>" );
                    fileBuffer.Add( tagline.Get() );
                    fileBuffer.Add( "</p>" );

                } else {
                    if ( formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), "\r\n", 2 ) != 0 )
                            fileBuffer.Add( "\r\n\r\n" );
                        else
                            if ( memcmp( (fileBuffer.GetTail()-3), "\n\r\n", 3 ) != 0 )
                                fileBuffer.Add( "\r\n" );
                    }
                    fileBuffer.Add( tagline.Get() );
                }
            }
#endif
#if SUPPORT_POSTSCRIPTS
            if ( postscript.Length() ) {
                if ( strcmp(textmode, "html") == 0 ) {
                    fileBuffer.Add( "<p>" );
                    fileBuffer.Add( postscript.Get() );
                    fileBuffer.Add( "</p>" );

                } else {
                    if ( formattedContent ) {
                        if ( memcmp( (fileBuffer.GetTail()-2), "\r\n", 2 ) != 0 )
                            fileBuffer.Add( "\r\n\r\n" );
                        else
                            if ( memcmp( (fileBuffer.GetTail()-3), "\n\r\n", 3 ) != 0 )
                                fileBuffer.Add( "\r\n" );
                    }
                    fileBuffer.Add( postscript.Get() );
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
                    if ( !attach && uuencode )
                        douuencode( fileBuffer, messageBuffer,
                                    (strcmp(bodyFilename, "-") == 0) ? stdinFileName : GetNameWithoutPath(bodyFilename),
                                    1, 1 );
                    else
#endif
                        messageBuffer.Add( fileBuffer );

            if ( formattedContent )
                if ( memcmp( (messageBuffer.GetTail()-3), "\n\r\n", 3 ) != 0 )
                    messageBuffer.Add( "\r\n" );
#if BLAT_LITE
#else
            if ( alternateText.Length() ) {
                if ( haveEmbedded || haveAttachments ) {
                    if ( *(messageBuffer.GetTail()-3) != '\n' )
                        messageBuffer.Add( "\r\n" );

                    messageBuffer.Add( "--" BOUNDARY_MARKER );
                    messageBuffer.Add( attachment_boundary, 21 );
                    messageBuffer.Add( "--\r\n" );
                    decrementBoundary( attachment_boundary );
                }
            }
#endif
        } /* end of msgBodySize */

        // make some noise about what we are doing
        sprintf(tmpstr.Get(), "Sending %s to ", bodyFilename);
        tmpstr.SetLength();
        if ( buildSMTP ) {
            if ( Recipients.Length() ) {
#if SUPPORT_SALUTATIONS
                int    x;
                Buf    parsed_addys;
                char * pa;

                parse_email_addresses( Recipients.Get(), parsed_addys );
                pa = parsed_addys.Get();
                if ( pa ) {
                    for ( x = 0; pa[x]; ) {
                        if ( x )
                            tmpstr.Add( ", " );

                        tmpstr.Add( &pa[x] );
                        x += (int)(strlen( &pa[x] ) + 1);
                    }

                    parsed_addys.Free();
                }
#else
                tmpstr.Add( Recipients.Get() );
#endif
            }
            else
                tmpstr.Add( "<unspecified>" );
        }
#if INCLUDE_NNTP
        else
            if ( groups.Length() )
                tmpstr.Add( groups.Get() );
            else
                tmpstr.Add( "<unspecified>" );
#endif

        if ( tmpstr.Length() > 1019 )
            printMsg("%.*s...\n", 1019, tmpstr.Get() );
        else
            printMsg("%s\n", tmpstr.Get() );

        if ( lstrlen(subject) ) printMsg("Subject: %s\n",subject);
        if ( lstrlen(loginname) ) printMsg("Login name is %s\n",loginname);
    }

    return(0);
}


void add_msg_boundary ( Buf &messageBuffer, int buildSMTP, char * attachment_boundary )
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
            if ( *(messageBuffer.GetTail()-3) != '\n' )
                messageBuffer.Add( "\r\n" );

            messageBuffer.Add( "--" BOUNDARY_MARKER );
            messageBuffer.Add( attachment_boundary, 21 );
            messageBuffer.Add( "--\r\n" );
        }
    }
}
