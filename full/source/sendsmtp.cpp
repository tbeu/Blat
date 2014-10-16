/*
    sendSMTP.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "blat.h"
#include "winfile.h"
#include "gensock.h"
#include "md5.h"
#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
#endif

#define ALLOW_PIPELINING    FALSE
#define ALLOW_CHUNKING      FALSE

#if SUPPORT_GSSAPI
  extern BOOL    authgssapi;
  extern _TCHAR  servicename[SERVICENAME_SIZE];
  extern protLev gss_protection_level;
  extern BOOL    mutualauth;
#endif

extern void convertPackedUnicodeToUTF( Buf & sourceText, Buf & outputText, int * utf, LPTSTR charset, int utfRequested );
extern void base64_encode(_TUCHAR * in, size_t length, LPTSTR out, int inclCrLf);
extern int  base64_decode(_TUCHAR * in, LPTSTR out);
extern int  open_server_socket( LPTSTR host, LPTSTR userPort, LPTSTR defaultPort, LPTSTR portName );
extern int  get_server_response( Buf * responseStr, int * enhancedStatusCode );
#if INCLUDE_IMAP
extern int  get_imap_untagged_server_response( Buf * responseStr  );
extern int  get_imap_tagged_server_response( Buf * responseStr, LPTSTR tag  );
#endif
#if SUPPORT_GSSAPI
extern int  get_server_response( LPTSTR responseStr );
#endif
extern int  put_message_line( socktag sock, LPTSTR line );
extern int  finish_server_message( void );
extern int  close_server_socket( void );
extern void server_error( LPTSTR message);
extern int  send_edit_data (LPTSTR message, Buf * responseStr );
extern int  send_edit_data (LPTSTR message, int expected_response, Buf * responseStr );
extern int  noftry();
extern void build_headers( BLDHDRS & bldHdrs /* Buf & messageBuffer, Buf & header, int buildSMTP,
                           Buf & lpszFirstReceivedData, Buf & lpszOtherHeader,
                           LPTSTR attachment_boundary, LPTSTR wanted_hostname, LPTSTR server */ );
#if SUPPORT_MULTIPART
extern int  add_one_attachment( Buf & messageBuffer, int buildSMTP, LPTSTR attachment_boundary,
                                DWORD startOffset, DWORD & length,
                                int part, int totalparts, int attachNbr, int * prevAttachType );
extern void getAttachmentInfo( int attachNbr, LPTSTR & attachName, DWORD & attachSize, int & attachType );
extern void getMaxMsgSize( int buildSMTP, DWORD & length );
#endif
extern int  add_message_body( Buf & messageBuffer, size_t msgBodySize, Buf & multipartHdrs, int buildSMTP,
                              LPTSTR attachment_boundary, DWORD startOffset, int part, int attachNbr );
extern int  add_attachments( Buf & messageBuffer, int buildSMTP, LPTSTR attachment_boundary, int nbrOfAttachments );
extern void add_msg_boundary( Buf & messageBuffer, int buildSMTP, LPTSTR attachment_boundary );
extern void parseCommaDelimitString ( LPTSTR source, Buf & parsed_addys, int pathNames );

extern void printMsg( LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison

extern socktag ServerSocket;

#if INCLUDE_POP3
extern int   get_pop3_server_response( Buf * responseStr );

extern _TCHAR  xtnd_xmit_supported;
extern _TCHAR  xtnd_xmit_wanted;
extern _TCHAR  POP3Host[SERVER_SIZE+1];
extern _TCHAR  POP3Port[SERVER_SIZE+1];
extern Buf     POP3Login;
extern Buf     POP3Password;
#endif
#if INCLUDE_IMAP
extern _TCHAR  IMAPHost[SERVER_SIZE+1];
extern _TCHAR  IMAPPort[SERVER_SIZE+1];
extern Buf     IMAPLogin;
extern Buf     IMAPPassword;
#endif
extern _TCHAR  SMTPHost[];
extern _TCHAR  SMTPPort[];

extern Buf     AUTHLogin;
extern Buf     AUTHPassword;
extern Buf     Sender;
extern _TCHAR  my_hostname[];
extern Buf     Recipients;
extern _TCHAR  loginname[]; // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
extern _TCHAR  quiet;
extern _TCHAR  mime;
extern _TCHAR  formattedContent;
extern _TCHAR  boundaryPosted;

extern _TCHAR  my_hostname_wanted[];
extern int     delayBetweenMsgs;

#if INCLUDE_SUPERDEBUG
extern _TCHAR  superDebug;
#endif

#if BLAT_LITE
#else
extern _TCHAR  base64;
extern _TCHAR  uuencode;
extern _TCHAR  yEnc;
extern _TCHAR  deliveryStatusRequested;
extern _TCHAR  deliveryStatusSupported;

extern _TCHAR  eightBitMimeSupported;
extern _TCHAR  eightBitMimeRequested;
extern _TCHAR  binaryMimeSupported;
extern _TCHAR  chunkingSupported;
#endif
#if SUPPORT_MULTIPART
extern unsigned long multipartSize;
#endif
extern _TCHAR        disableMPS;

LPTSTR               defaultSMTPPort     = __T("25");
#if INCLUDE_POP3
LPTSTR               defaultPOP3Port     = __T("110");
#endif
#if defined(SUBMISSION_PORT) && SUBMISSION_PORT
LPTSTR               SubmissionPort      = __T("587");
#endif
#if INCLUDE_IMAP
LPTSTR               defaultIMAPPort     = __T("143");
#endif

int                  maxNames            = 0;


unsigned long maxMessageSize       = 0;
TCHAR         commandPipelining    = FALSE;
TCHAR         cramMd5AuthSupported = FALSE;
TCHAR         plainAuthSupported   = FALSE;
TCHAR         loginAuthSupported   = FALSE;
TCHAR         enhancedCodeSupport  = FALSE;
TCHAR         ByPassCRAM_MD5       = FALSE;
#if SUPPORT_GSSAPI
TCHAR         gssapiAuthSupported  = FALSE;


BOOL putline ( LPTSTR line )
{
    return (put_message_line(ServerSocket,line) == 0);
}


BOOL getline ( LPTSTR line )
{
    return (get_server_response(line) != -1);
}
#endif

#if SUPPORT_SALUTATIONS
extern Buf salutation;

void find_and_strip_salutation ( Buf &email_addresses )
{
  #if 0
    // Old (working) version

    Buf    new_addresses;
    Buf    tmpstr;
    LPTSTR srcptr;
    LPTSTR pp;
    LPTSTR pp2;
    size_t len;
    int    x;

    salutation.Free();

    parseCommaDelimitString( email_addresses.Get(), tmpstr, FALSE );
    srcptr = tmpstr.Get();
    if ( !srcptr )
        return;

    for ( x = 0; *srcptr; srcptr += len + 1 ) {
        int addToSal = FALSE;

        len = _tcslen (srcptr);

        // 30 Jun 2000 Craig Morrison, parses out just email address in TO:,CC:,BCC:
        // so that all we RCPT To with is the email address.
        pp = _tcschr(srcptr, __T('<'));
        if ( pp ) {
            pp2 = _tcschr(++pp, __T('>'));
            if ( pp2 ) {
                *(++pp2) = __T('\0');
                if ( (DWORD)(pp2 - srcptr) < len ) {
                    pp2++;
                    while ( *pp2 == __T(' ') )
                        pp2++;

                    if ( *pp2 == __T('\\') )
                        pp2++;

                    if ( *pp2 == __T('"') )
                        pp2++;

                    for ( ; *pp2 && (*pp2 != __T('"')); pp2++ )
                        if ( *pp2 != __T('\\') ) {
                            salutation.Add( *pp2 );
                            addToSal = TRUE;
                        }
                }
            }
        } else {
            pp = srcptr;
            while (*pp == __T(' '))
                pp++;

            pp2 = _tcschr(pp, __T(' '));
            if ( pp2 )
                *pp2 = __T('\0');
        }
        if ( x++ )
            new_addresses.Add( __T(", ") );

/*
        if ( *srcptr == __T('"') ) {
            srcptr++;
            len--;
        }
 */
        if ( *srcptr != __T('"') ) {
            pp = _tcsrchr( srcptr, __T('<') );
            if ( pp ) {
                *pp = __T('\0');
                pp2 = _tcschr( srcptr, __T(',') );
                *pp = __T('<');
                if ( pp2 ) {
                    new_addresses.Add( __T('"') );
                    pp2 = pp;
                    for ( pp2 = pp; pp2 > srcptr; ) {
                        if ( *(pp2-1) != __T(' ') )
                            break;
                    }
                    new_addresses.Add( srcptr, (size_t)(pp2 - srcptr)/sizeof(_TCHAR) );
                    new_addresses.Add( __T("\" "), 2 );
                    new_addresses.Add( pp );
                } else
                    new_addresses.Add( srcptr );
            } else
                new_addresses.Add( srcptr );
        } else
            new_addresses.Add( srcptr );

        if ( addToSal )
            salutation.Add( __T('\0') );
    }

    if ( salutation.Length() )
        salutation.Add( __T('\0') );

    if ( x ) {
        email_addresses.Move( new_addresses );
    }

    new_addresses.Free();
    tmpstr.Free();
  #else
    // New version

    Buf    new_addresses;
    Buf    tmpstr;
    LPTSTR srcptr;
    LPTSTR pp;
    LPTSTR pp2;
    size_t len, tempLen;
    int    x;
    int    foundQuote;
    _TCHAR c;
    int    addToSal;


    salutation.Free();

    //_tprintf( __T("find_and_strip_salutation ()\n") );
    parseCommaDelimitString( email_addresses.Get(), tmpstr, FALSE );
    srcptr = tmpstr.Get();
    if ( !srcptr )
        return;

    addToSal = FALSE;
    for ( x = 0; *srcptr; srcptr += len + 1 ) {
        //_tprintf( __T("srcptr = '%s'\n"), srcptr );
        tempLen = len = _tcslen (srcptr);

        while( srcptr[tempLen-1] == __T(' ') ) {
            srcptr[--tempLen] = __T('\0');
        }

        // 30 Jun 2000 Craig Morrison, parses out just email address in TO:,CC:,BCC:
        // so that all we RCPT To with is the email address.
        pp = _tcschr(srcptr, __T('<'));
        if ( pp ) {
            pp2 = _tcschr(++pp, __T('>'));
            if ( pp2 ) {
                ++pp2;
                c = *pp2;
                *pp2 = __T('\0');
                if ( x++ )
                    new_addresses.Add( __T(", ") );

                new_addresses.Add( srcptr );
                //_tprintf( __T("new_addresses.Add( '%s' )\n"), srcptr );
                tempLen -= (pp2 - srcptr);
                *pp2 = c;
                while ( *pp2 == __T(' ') ) {
                    pp2++;
                    tempLen--;
                }

                foundQuote = FALSE;
                while ( tempLen ) {
                    if ( (*pp2 == __T('\\')) && (tempLen > 1) ) {
                        salutation.Add( pp2[1] );
                        pp2 += 2;
                        addToSal = TRUE;
                        tempLen -= 2;
                    } else
                    if ( *pp2 == __T('"') ) {
                        foundQuote ^= (TRUE ^ FALSE);
                        pp2++;
                        tempLen--;
                    } else
                    if ( *pp2 == __T('<') ) {
                        if ( !foundQuote )
                            break;

                        salutation.Add( *pp2++ );
                        addToSal = TRUE;
                        tempLen--;
                    } else
                    if ( *pp2 == __T('[') ) {
                        salutation.Add( __T('<') );
                        pp2++;
                        addToSal = TRUE;
                        tempLen--;
                    } else
                    if ( *pp2 == __T(']') ) {
                        salutation.Add( __T('>') );
                        pp2++;
                        addToSal = TRUE;
                        tempLen--;
                    } else {
                        salutation.Add( *pp2++ );
                        addToSal = TRUE;
                        tempLen--;
                    }
                }
            }
        } else {
            pp = srcptr;
            while (*pp == __T(' '))
                pp++;

            pp2 = _tcschr(pp, __T(' '));
            if ( pp2 )
                *pp2 = __T('\0');

            if ( x++ )
                new_addresses.Add( __T(", ") );

            if ( *srcptr != __T('"') ) {
                pp = _tcsrchr( srcptr, __T('<') );
                if ( pp ) {
                    *pp = __T('\0');
                    pp2 = _tcschr( srcptr, __T(',') );
                    *pp = __T('<');
                    if ( pp2 ) {
                        new_addresses.Add( __T('"') );
                        pp2 = pp;
                        for ( pp2 = pp; pp2 > srcptr; ) {
                            if ( *(pp2-1) != __T(' ') )
                                break;
                        }
                        new_addresses.Add( srcptr, (size_t)(pp2 - srcptr) );
                        new_addresses.Add( __T("\" "), 2 );
                        new_addresses.Add( pp );
                        //_tprintf( __T("1 new_addresses.Add() = '%s' )\n"), new_addresses.Get() );
                    } else {
                        new_addresses.Add( srcptr );
                        //_tprintf( __T("2 new_addresses.Add( '%s' )\n"), srcptr );
                    }
                } else {
                    new_addresses.Add( srcptr );
                    //_tprintf( __T("3 new_addresses.Add( '%s' )\n"), srcptr );
                }
            } else {
                new_addresses.Add( srcptr );
                //_tprintf( __T("4 new_addresses.Add( '%s' )\n"), srcptr );
            }
        }
        if ( addToSal ) {
            salutation.Add( __T('\0') );
            addToSal = FALSE;
        }

    }

    if ( addToSal )
        salutation.Add( __T('\0') );

    //if ( salutation.Length() )
    //     _tprintf( __T("salutation = '%s'\n"), salutation.Get() );

    if ( x )
        email_addresses.Move( new_addresses );

    new_addresses.Free();
    tmpstr.Free();
    //_tprintf( __T("find_and_strip_salutation (), done\n") );
  #endif
}
#endif


void parse_email_addresses ( LPTSTR email_addresses, Buf & parsed_addys )
{
    Buf    tmpstr;
    LPTSTR srcptr;
    LPTSTR pp;
    LPTSTR pp2;
    DWORD  len;

    parsed_addys.Free();
    len = (DWORD)_tcslen(email_addresses);
    if ( !len )
        return;

    parseCommaDelimitString( (LPTSTR )email_addresses, tmpstr, FALSE );
    srcptr = tmpstr.Get();
    if ( !srcptr )
        return;

    for ( ; *srcptr; ) {
        len = (DWORD)_tcslen( srcptr );
        pp = _tcschr(srcptr, __T('<'));
        if ( pp ) {
            pp2 = _tcschr(++pp, __T('>'));
            if ( pp2 )
                *pp2 = __T('\0');
        } else {        // if <address> not found, then skip blanks and parse for the first blank
            pp = srcptr;
            while (*pp == __T(' '))
                pp++;

            pp2 = _tcschr(pp, __T(' '));
            if ( pp2 )
                *pp2 = __T('\0');
        }
        parsed_addys.Add( pp, _tcslen(pp) + 1 );
        srcptr += len + 1;
    }

    parsed_addys.Add( __T('\0') );
    tmpstr.Free();
}

#define EMAIL_KEYWORD   __T("myself")

void searchReplaceEmailKeyword (Buf & email_addresses)
{
    Buf    tmpstr;
    LPTSTR srcptr;
    LPTSTR pp;
    DWORD  len;

    len = (DWORD) email_addresses.Length();
    if ( !len )
        return;

    parseCommaDelimitString( email_addresses.Get(), tmpstr, FALSE );
    srcptr = tmpstr.Get();
    if ( !srcptr )
        return;

    email_addresses.Clear();
    for ( ; *srcptr; ) {
        len = (DWORD)_tcslen( srcptr );
        pp = srcptr;
        if ( !_tcscmp( srcptr, EMAIL_KEYWORD ) )
            pp = loginname;

        if ( email_addresses.Length() )
            email_addresses.Add( __T(',') );

        email_addresses.Add( pp, _tcslen(pp) );
        srcptr += len + 1;
    }
    tmpstr.Free();
}


static int say_hello ( LPTSTR wanted_hostname, BOOL bAgain = FALSE)
{
    _TCHAR out_data[MAXOUTLINE];
    Buf    responseStr;
    int    enhancedStatusCode;
    int    no_of_try;
    int    tryCount;
    int    socketError = 0;
    int    serverResponseValue;
    LPTSTR pStr;

    if ( wanted_hostname )
        if ( !*wanted_hostname )
            wanted_hostname = NULL;

    no_of_try = noftry();
    tryCount = 0;
    for ( ; tryCount < no_of_try; tryCount++ ) {
        if ( tryCount ) {
#if INCLUDE_SUPERDEBUG
            if ( superDebug ) {
                _TCHAR savedQuiet = quiet;
                quiet = FALSE;
                printMsg( __T("superDebug: ::say_hello() failed to connect, retry count remaining is %d\n"),
                          no_of_try - tryCount );
                quiet = savedQuiet;
            }
#endif
            Sleep( 1000 );
        }
        if (!bAgain) {
            socketError = open_server_socket( SMTPHost, SMTPPort, defaultSMTPPort, __T("smtp") );
            if ( socketError )
                continue;

            if ( get_server_response( NULL, &enhancedStatusCode ) != 220 ) {
                close_server_socket();
                continue;
            }
        }

        // Changed to EHLO processing 27 Feb 2001 Craig Morrison
        _stprintf( out_data, __T("EHLO %s"),
                   (wanted_hostname==NULL) ? my_hostname : wanted_hostname);
        pStr = _tcsrchr( Sender.Get(), __T('@') );
        if ( pStr ) {
            _tcscat( out_data, __T(".") );
            _tcscat( out_data, pStr+1 );
            pStr = _tcschr( out_data, __T('>') );
            if ( pStr ) {
                *pStr = __T('\0');
            }
        }
        _tcscat( out_data, __T("\r\n") );
//        printMsg( __T(" ... About to send 'EHLO'\n") );
        socketError = put_message_line( ServerSocket, out_data );
        if ( socketError ) {
//            printMsg( __T(" ... After attempting to send 'EHLO', Windows returned socket error %d\n"), socketError );
            close_server_socket();
            bAgain = FALSE;
            continue;
        }

//        printMsg( __T(" ... Waiting for the server's response.\n") );
        serverResponseValue = get_server_response( &responseStr, &enhancedStatusCode );
//        printMsg( __T(" ... Blat parsed the 'EHLO' server response code = %d\n"), serverResponseValue );
        if ( serverResponseValue == 250 ) {
            LPTSTR   index;
//            unsigned count = 0;

//            printMsg( __T(" ... About to parse the server response\n") );
            index = responseStr.Get();  // The responses are already individually NULL terminated, with no CR or LF bytes.
            for ( ; ; )
            {
                if ( index[0] == __T('\0') )
                    break;

//                printMsg( __T(" ... Parsing line #%2u, '%s'\n"), count++, index );
                /* Parse the server responses for things we can utilize. */
                _tcslwr( index );

                if ( (index[3] == __T('-')) || (index[3] == __T(' ')) ) {
#if ALLOW_PIPELINING
                    if ( memcmp(&index[4], __T("pipelining"), 10*sizeof(_TCHAR)) == 0 ) {
                        commandPipelining = TRUE;
//                        printMsg( __T(" ... Server supports pipelining.\n") );
                    } else
#endif
#if BLAT_LITE
#else
                    if ( memcmp(&index[4], __T("size"), 4*sizeof(_TCHAR)) == 0 ) {
                        if ( (index[8] == __T(' ')) || (index[8] == __T('=')) ) {
                            if ( _istdigit(index[9]) ) {
                                long maxSize = _tstol(&index[9]);

                                if ( maxSize > 0 ) {
                                    maxMessageSize = (unsigned long)(maxSize / 10L) * 7L;
//                                    {
//                                        DWORD    reduced;
//                                        unsigned index;
//                                        static LPTSTR sizeStrings[] = { __T("Bytes"),
//                                                                        __T("KBytes"),
//                                                                        __T("MBytes"),
//                                                                        __T("GBytes"),
//                                                                        __T("TBytes"),
//                                                                        __T("PBytes"),
//                                                                        __T("EBytes")
//                                                                      };
//                                        reduced = (DWORD)maxSize;
//                                        for ( index = 0; ; index++ ) {
//                                            if ( (reduced & ~0x03FFul) == 0 )
//                                                break;
//                                            if ( reduced & 0x03FFul )
//                                                break;
//                                            reduced >>= 10;
//                                        }
//                                        printMsg( __T(" ... Server supports message sizes up to %lu %s\n"), reduced, sizeStrings[index] );
//                                    }
                                }
                            } else {
//                                printMsg( __T(" ... Server mentioned SIZE argument without a value.\n") );
                            }
                        } else {
//                            printMsg( __T(" ... Server mentioned SIZE argument without a value.\n") );
                        }
                    } else
                    if ( memcmp(&index[4], __T("dsn"), 3*sizeof(_TCHAR)) == 0 ) {
                        deliveryStatusSupported = TRUE;
//                        printMsg( __T(" ... Server supports delivery status notification\n") );
                    } else
                    if ( memcmp(&index[4], __T("8bitmime"), 8*sizeof(_TCHAR)) == 0 ) {
                        eightBitMimeSupported = TRUE;
//                        printMsg( __T(" ... Server supports 8 bit MIME\n") );
                    } else
                    if ( memcmp(&index[4], __T("binarymime"), 10*sizeof(_TCHAR)) == 0 ) {
                        binaryMimeSupported = 1;
//                        printMsg( __T(" ... Server supports binary messages (non-displayable bytes)\n") );
                    } else
                    if ( memcmp(&index[4], __T("enhancedstatuscodes"), 19*sizeof(_TCHAR)) == 0 ) {
                        enhancedCodeSupport = TRUE;
//                        printMsg( __T(" ... Server supports enhanced status codes\n") );
                    } else
#endif
#if ALLOW_CHUNKING
                    if ( memcmp(&index[4], __T("chunking"), 8*sizeof(_TCHAR)) == 0 ) {
                        chunkingSupported = TRUE;
//                        printMsg( __T(" ... Server supports CHUNKING\n") );
                    } else
#endif
                    if ( (memcmp(&index[4], __T("auth"), 4*sizeof(_TCHAR)) == 0) &&
                         ((index[8] == __T(' ')) || (index[8] == __T('='))) ) {
                        int  x, y;

                        for ( x = 9; ; ) {
                            for ( y = x; ; y++ ) {
                                if ( (index[y] == __T('\0')) ||
                                     (index[y] == __T(',') ) ||
                                     (index[y] == __T(' ') ) )
                                    break;
                            }

                            if ( (y - x) == 5 ) {
                                if ( memcmp(&index[x], __T("plain"), 5*sizeof(_TCHAR)) == 0 ) {
                                    plainAuthSupported = TRUE;
//                                    printMsg( __T(" ... Server supports authentication type PLAIN\n") );
                                }

                                if ( memcmp(&index[x], __T("login"), 5*sizeof(_TCHAR)) == 0 ) {
                                    loginAuthSupported = TRUE;
//                                    printMsg( __T(" ... Server supports authentication type LOGIN\n") );
                                }
                            }

#if SUPPORT_GSSAPI
                            if ( (y - x) == 6 ) {
                                if ( memcmp(&index[x], __T("gssapi"), 6*sizeof(_TCHAR)) == 0 ) {
                                    gssapiAuthSupported = TRUE;
//                                    printMsg( __T(" ... Server supports authentication type GSSAPI") );
//                                    if ( !authgssapi )
//                                        printMsg( __T(", but this will not be used.") );
//                                    printMsg( __T("\n") );
                                }
                            }
#endif
                            if ( (y - x) == 8 ) {
                                if ( memcmp(&index[x], __T("cram-md5"), 8*sizeof(_TCHAR)) == 0 ) {
//                                    printMsg( __T(" ... Server supports authentication type CRAM MD5") );
                                    if ( !ByPassCRAM_MD5 )
                                        cramMd5AuthSupported = TRUE;
//                                    else
//                                        printMsg( __T(", but this will not be used.") );
//                                    printMsg( __T("\n") );
                                }
                            }

                            for ( x = y; ; x++ ) {
                                if ( (index[x] != __T(',')) && (index[x] != __T(' ')) )
                                    break;
                            }

                            if ( index[x] == __T('\0') )
                                break;
                        }
                    }
                }

                index += _tcslen(index) + 1;
            }
        } else {
//            printMsg( __T(" ... About to send 'HELO'\n") );
            _stprintf( out_data, __T("HELO %s"),
                       (wanted_hostname==NULL) ? my_hostname : wanted_hostname);
            pStr = _tcsrchr( Sender.Get(), __T('@') );
            if ( pStr ) {
                _tcscat( out_data, __T(".") );
                _tcscat( out_data, pStr+1 );
                pStr = _tcschr( out_data, __T('>') );
                if ( pStr ) {
                    *pStr = __T('\0');
                }
            }
            _tcscat( out_data, __T("\r\n") );
            socketError = put_message_line( ServerSocket, out_data );
            if ( socketError ) {
//                printMsg( __T(" ... After attempting to send 'HELO', Windows returned socket error %d\n"), socketError );
                close_server_socket();
                bAgain = FALSE;
                continue;
            }

            serverResponseValue = get_server_response( NULL, &enhancedStatusCode );
//            printMsg( __T(" ... Blat parsed the 'HELO' server response code = %d\n"), serverResponseValue );
            if ( serverResponseValue != 250 ) {
                close_server_socket();
                bAgain = FALSE;
                continue;
            }

            loginAuthSupported = TRUE;  // no extended responses available, so assume "250-AUTH LOGIN"
#if SUPPORT_GSSAPI
            gssapiAuthSupported = TRUE; // might as well make the same assumption for gssapi
#endif
        }
        break;
    }
    responseStr.Free();

    if ( tryCount == no_of_try ) {
        if ( !socketError )
            server_error( __T("SMTP server error") );
        return(-1);
    }

    return(0);
}

#if SUPPORT_GSSAPI

static int say_hello_again (LPTSTR wanted_hostname)
{
    return say_hello(wanted_hostname, TRUE);
}
#endif


static void cram_md5( Buf &challenge, _TCHAR str[] )
{
    md5_context   ctx;
    _TUCHAR       k_ipad[65];    /* inner padding - key XORd with ipad */
    _TUCHAR       k_opad[65];    /* outer padding - key XORd with opad */
    size_t        x;
    unsigned      i;
    _TUCHAR       digest[16];
    Buf           decodedResponse;
    Buf           tmp;
    _TCHAR        out_data[MAXOUTLINE];

    decodedResponse.Alloc( challenge.Length() + 65 );
    base64_decode( (_TUCHAR *)challenge.Get(), decodedResponse.Get() );
    decodedResponse.SetLength();
    x = _tcslen(AUTHPassword.Get());
    tmp.Alloc( x + 65 );
    if ( *decodedResponse.Get() == __T('\"') ) {
        LPTSTR pStr = _tcschr( decodedResponse.Get()+1, __T('\"') );
        if ( pStr )
            *pStr = __T('\0');

        _tcscpy( decodedResponse.Get(), decodedResponse.Get()+1 );
    }

    tmp.Add( AUTHPassword.Get(), x );
    for ( ; x % 64; x++ ) {
        tmp.Add( __T('\0') );             /* pad the password to a multiple of 64 bytes */
    }

    /* if key is longer than 64 bytes reset it to key=MD5(key) */
    if ( x > 64 ) {

        md5_context tctx;

        md5_starts( &tctx );
        md5_update( &tctx, (_TUCHAR *)tmp.Get(), (uint32_t)x );
        md5_finish( &tctx, digest );

        x = 16;
        memcpy( tmp.Get(), digest, x*sizeof(_TCHAR) );
    }

    /*
     * the HMAC_MD5 transform looks like:
     *
     * MD5(K XOR opad, MD5(K XOR ipad, text))
     *
     * where K is an n byte key
     * ipad is the byte 0x36 repeated 64 times
     * opad is the byte 0x5c repeated 64 times
     * and text is the data being protected
     */

    /* start out by storing key in pads */
    for ( i = 0; i < 64; i++ ) {
        k_ipad[i] = 0x36;
        k_opad[i] = 0x5C;
    }
    for ( i = 0; i < x; i++ ) {
        k_ipad[i] ^= (_TUCHAR)*(tmp.Get()+i);
        k_opad[i] ^= (_TUCHAR)*(tmp.Get()+i);
    }
    /*
     * perform inner MD5
     */
    md5_starts( &ctx );                 /* initialize context for first pass */
    md5_update( &ctx, k_ipad, 64 );     /* start with inner pad */
    md5_update( &ctx, (_TUCHAR *)decodedResponse.Get(), (uint32_t)decodedResponse.Length() );
                                        /* then text of datagram */
    md5_finish( &ctx, digest );         /* finish up 1st pass */
    /*
     * perform outer MD5
     */
    md5_starts( &ctx );                 /* init context for 2nd pass */
    md5_update( &ctx, k_opad, 64 );     /* start with outer pad */
    md5_update( &ctx, digest, 16 );     /* then results of 1st hash */
    md5_finish( &ctx, digest );         /* finish up 2nd pass */

    _tcscpy( out_data, AUTHLogin.Get() );
    x = _tcslen( out_data );
    out_data[x++] = __T(' ');
    for ( i = 0; i < 16; i++ ) {
        _stprintf( &out_data[x], __T("%02") _TCHAR_PRINTF_FORMAT __T("x"), digest[i] );
        x +=2;
    }

    base64_encode( (_TUCHAR *)out_data, x, str, FALSE);
    _tcscat(str, __T("\r\n") );

    tmp.Free();
    decodedResponse.Free();
}

static int authenticate_smtp_user(LPTSTR loginAuth, LPTSTR pwdAuth)
{
    int    enhancedStatusCode;
    _TCHAR out_data[MAXOUTLINE];
    _TCHAR str[1024];
    LPTSTR outstart;
    LPTSTR out;
    int    retval;

#if INCLUDE_POP3
    if ( xtnd_xmit_supported )
       return 0;
#endif

    /* NOW: auth */
#if SUPPORT_GSSAPI // Added 2003-11-07, Joseph Calzaretta
    if ( authgssapi ) {
//        printMsg( __T(" ... AUTH GSSAPI requested.\n") );
        if (!gssapiAuthSupported) {
            server_error( __T("The SMTP server will not accept AUTH GSSAPI value.") );
            return(-2);
        }
        try{
            if (!pGss)
            {
                static GssSession TheSession;
                pGss = &TheSession;
            }

            _TCHAR servname[1024];

            if (*servicename)
                _tcscpy(servname,servicename);
            else
                _stprintf(servname, __T("smtp@%s"), SMTPHost);

            pGss->Authenticate(getline,putline,AUTHLogin.Get(),servname,mutualauth,gss_protection_level);

            if (pGss->GetProtectionLevel()!=GSSAUTH_P_NONE)
            {
                if (say_hello_again(my_hostname_wanted)!=0)
                    return (-5);
            }
        }
        catch (GssSession::GssException& e)
        {
            _TCHAR szMsg[1024];
            _stprintf(szMsg, __T("Cannot do AUTH GSSAPI: %s"), e.message());
            server_error(szMsg);
            pGss=NULL;
            return (-5);
        }
        return(0);
    }
#endif
    if ( !*loginAuth )
        return(0);

    if ( cramMd5AuthSupported && !ByPassCRAM_MD5 ) {
        Buf response;
        Buf response1;

        if ( put_message_line( ServerSocket, __T("AUTH CRAM-MD5\r\n") ) )
            return(-1);

        if ( get_server_response( &response, &enhancedStatusCode ) == 334 ) {
            response1.Add( response.Get() + 4 );
            cram_md5( response1, str );
            response1.Free();
            response.Free();

            if ( put_message_line( ServerSocket, str ) ) {
                return(-1);
            }
            if ( get_server_response( NULL, &enhancedStatusCode ) == 235 ) {
                return(0);                  // cram-md5 authentication successful
            }
            printMsg( __T("The SMTP server did not accept Auth CRAM-MD5 value.\n") \
                      __T("Are your login userid and password correct?\n") );
        } else
            printMsg( __T("The SMTP server claimed CRAM-MD5, but did not accept Auth CRAM-MD5 request.\n") );

        response.Free();
    }
    if ( plainAuthSupported ) {

// The correct form of the AUTH PLAIN value is 'authid\0userid\0passwd'
// where '\0' is the null byte.

        _tcscpy(str, __T("AUTH PLAIN ") );
        outstart = &str[11];
        out = &str[12];     // leave room for a leading NULL in place of the AuthID value.

        _tcscpy(out, loginAuth);
        while ( *out ) out++;

        out++;
        _tcscpy(out, pwdAuth);
        while ( *out ) out++;

        base64_encode((_TUCHAR *)&str[11], (size_t)(out - outstart), outstart, FALSE);
        _tcscat(str, __T("\r\n"));

        if ( put_message_line( ServerSocket, str ) )
            return(-1);

        if ( get_server_response( NULL, &enhancedStatusCode ) == 235 )
            return(0);                  // plain authentication successful

        printMsg( __T("The SMTP server did not accept Auth PLAIN value.\n") \
                  __T("Are your login userid and password correct?\n") );
    }

/* At this point, authentication was requested, but not it did not match the
   server.  As a last resort, try AUTH LOGIN. */

#if ALLOW_PIPELINING
    if ( commandPipelining ) {
        int retval;
        Buf response;

        _tcscpy( out_data, __T("AUTH LOGIN\r\n") );
        out = &out_data[12];

        base64_encode((_TUCHAR *)loginAuth, _tcslen(loginAuth), out, FALSE);
        _tcscat( out, __T("\r\n") );
        while ( *out ) out++;

        base64_encode((_TUCHAR *)pwdAuth, _tcslen(pwdAuth), out, FALSE);
        _tcscat( out, __T("\r\n") );

        if ( put_message_line( ServerSocket, out_data ) )
            return(-1);

        retval = get_server_response( &response, &enhancedStatusCode );
        if ( (retval != 334) && (retval != 235) ) {
            printMsg( __T("The SMTP server does not require AUTH LOGIN.\n") \
                      __T("Are you sure server supports AUTH?\n") );
            response.Free();
            return(0);
        }

        if ( (retval != 235) && (response.Get()[3] == __T(' ')) ) {
            retval = get_server_response( &response, &enhancedStatusCode );
            if ( (retval != 334) && (retval != 235) ) {
                server_error( __T("The SMTP server did not accept LOGIN name.") );
                response.Free();
                return(-2);
            }
        }

        if ( (retval != 235) && (response.Get()[3] == __T(' ')) ) {
            retval = get_server_response( &response, &enhancedStatusCode );
            if ( retval != 235 ) {
                server_error( __T("The SMTP server did not accept Auth LOGIN PASSWD value.\n") \
                              __T("Is your password correct?") );
                response.Free();
                return(-2);
            }
        }
        response.Free();
        return(0);
    }
#endif

    if ( put_message_line( ServerSocket, __T("AUTH LOGIN\r\n") ) )
        return(-1);

    if ( get_server_response( NULL, &enhancedStatusCode ) != 334 ) {
        printMsg( __T("The SMTP server does not require AUTH LOGIN.\n") \
                  __T("Are you sure server supports AUTH?\n") );
        return(0);
    }

    base64_encode((_TUCHAR *)loginAuth, _tcslen(loginAuth), out_data, FALSE);
    _tcscat(out_data, __T("\r\n"));
    if ( put_message_line( ServerSocket, out_data ) )
        return(-1);

    retval = get_server_response( NULL, &enhancedStatusCode );
    if ( retval == 235 )
        return(0);

    if ( retval == 334 ) {
        base64_encode((_TUCHAR *)pwdAuth, _tcslen(pwdAuth), out_data, FALSE);
        _tcscat(out_data, __T("\r\n"));
        if ( put_message_line( ServerSocket, out_data ) )
            return(-1);

        if ( get_server_response( NULL, &enhancedStatusCode ) == 235 )
            return(0);

        printMsg( __T("The SMTP server did not accept Auth LOGIN PASSWD value.\n") );
    } else
        printMsg( __T("The SMTP server did not accept LOGIN name.\n") );

    finish_server_message();
    return(-2);
}


// 'destination' is the address the message is to be sent to

static int prepare_smtp_message( LPTSTR dest, DWORD msgLength )
{
    int    enhancedStatusCode;
    int    yEnc_This;
    _TCHAR out_data[MAXOUTLINE];
    LPTSTR ptr;
    LPTSTR pp;
    int    ret_temp;
    int    recipientsSent;
    int    errorsFound;
    Buf    response;
    Buf    tmpBuf;

#if INCLUDE_POP3
    if ( xtnd_xmit_supported )
        return 0;
#endif

    parse_email_addresses (loginname, tmpBuf);
    ptr = tmpBuf.Get();
    if ( !ptr ) {
        server_error( __T("No email address was found for the sender name.\n") \
                      __T("Have you set your mail address correctly?") );
        return(-2);
    }

#if SUPPORT_YENC
    yEnc_This = yEnc;
    if ( !eightBitMimeSupported && !binaryMimeSupported )
#endif
        yEnc_This = FALSE;

    if ( *ptr == __T('<') )
        _stprintf (out_data, __T("MAIL FROM:%s"), ptr);
    else
        _stprintf (out_data, __T("MAIL FROM:<%s>"), ptr);

#if BLAT_LITE
#else
    if ( binaryMimeSupported && yEnc_This )
        _tcscat (out_data, __T(" BODY=BINARYMIME"));
    else
    if ( eightBitMimeSupported && (eightBitMimeRequested || yEnc_This) )
        _tcscat (out_data, __T(" BODY=8BITMIME"));
#endif

    tmpBuf.Free();      // release the parsed login email address

    _tcscat (out_data, __T("\r\n") );
    if ( put_message_line( ServerSocket, out_data ) )
        return(-1);

    if ( get_server_response( NULL, &enhancedStatusCode ) != 250 ) {
        server_error( __T("The SMTP server does not like the sender name.\n") \
                      __T("Have you set your mail address correctly?") );
        return(-2);
    }

    // do a series of RCPT lines for each name in address line
    parse_email_addresses (dest, tmpBuf);
    pp = tmpBuf.Get();
    if ( !pp ) {
        server_error( __T("No email address was found for recipients.\n") \
                      __T("Have you set the 'To:' field correctly?") );
        return(-2);
    }

    errorsFound = recipientsSent = 0;
#if ALLOW_PIPELINING
    if ( commandPipelining ) {
        Buf outRcpts;
        int recipientsRcvd;

        for ( ptr = pp; *ptr; ptr += _tcslen(ptr) + 1 ) {
            recipientsSent++;
            outRcpts.Add( __T("RCPT TO:<") );
            outRcpts.Add( ptr );
            outRcpts.Add( __T('>') );

            if ( deliveryStatusRequested && deliveryStatusSupported ) {
                outRcpts.Add( __T(" NOTIFY=") );
                if ( deliveryStatusRequested & DSN_NEVER )
                    outRcpts.Add( __T("NEVER") );
                else {
                    if ( deliveryStatusRequested & DSN_SUCCESS )
                        outRcpts.Add( __T("SUCCESS") );

                    if ( deliveryStatusRequested & DSN_FAILURE ) {
                        if ( deliveryStatusRequested & DSN_SUCCESS )
                            outRcpts.Add( __T(',') );

                        outRcpts.Add( __T("FAILURE") );
                    }

                    if ( deliveryStatusRequested & DSN_DELAYED ) {
                        if ( deliveryStatusRequested & (DSN_SUCCESS | DSN_FAILURE) )
                            outRcpts.Add( __T(',') );

                        outRcpts.Add( __T("DELAY") );
                    }
                }
            }

            outRcpts.Add( __T("\r\n") );
        }

        put_message_line( ServerSocket, outRcpts.Get() );
        recipientsRcvd = 0;
        for ( ptr = pp; recipientsRcvd < recipientsSent; ) {
            LPTSTR index;

            if ( get_server_response( &response, &enhancedStatusCode ) < 0 ) {
                errorsFound = recipientsSent;
                break;
            }

            index = response.Get(); // The responses are already individually NULL terminated, with no CR or LF bytes.
            for ( ; *index; ) {
                recipientsRcvd++;
                ret_temp = _tstoi( index );
                out_data[3] = __T('\0');
                _tcscpy( out_data, index );

                if ( ( ret_temp != 250 ) && ( ret_temp != 251 ) ) {
                    printMsg( __T("The SMTP server does not like the name %s.\n") \
                              __T("Have you set the 'To:' field correctly, or do you need authorization (-u/-pw) ?\n"), ptr);
                    printMsg( __T("The SMTP server response was -> %s\n"), out_data );
                    errorsFound++;
                }

                if ( out_data[3] == __T(' ') )
                    break;

                index += _tcslen(index) + 1;
                ptr   += _tcslen(ptr) + 1;
            }
        }
        response.Free();
        outRcpts.Free();
    } else
#endif
    {
        for ( ptr = pp; *ptr; ptr += _tcslen(ptr) + 1 ) {
            _stprintf(out_data, __T("RCPT TO:<%s>"), ptr);

#if BLAT_LITE
#else
            if ( deliveryStatusRequested && deliveryStatusSupported ) {
                _tcscat(out_data, __T(" NOTIFY="));
                if ( deliveryStatusRequested & DSN_NEVER )
                    _tcscat(out_data, __T("NEVER"));
                else {
                    if ( deliveryStatusRequested & DSN_SUCCESS )
                        _tcscat(out_data, __T("SUCCESS"));

                    if ( deliveryStatusRequested & DSN_FAILURE ) {
                        if ( deliveryStatusRequested & DSN_SUCCESS )
                            _tcscat(out_data, __T(","));

                        _tcscat(out_data, __T("FAILURE"));
                    }

                    if ( deliveryStatusRequested & DSN_DELAYED ) {
                        if ( deliveryStatusRequested & (DSN_SUCCESS | DSN_FAILURE) )
                            _tcscat(out_data, __T(","));

                        _tcscat(out_data, __T("DELAY"));
                    }
                }
            }
#endif
            _tcscat (out_data, __T("\r\n"));
            put_message_line( ServerSocket, out_data );
            recipientsSent++;

            ret_temp = get_server_response( &response, &enhancedStatusCode );
            if ( ( ret_temp != 250 ) && ( ret_temp != 251 ) ) {
                printMsg( __T("The SMTP server does not like the name %s.\n") \
                          __T("Have you set the 'To:' field correctly, or do you need authorization (-u/-pw) ?\n"), ptr);
                printMsg( __T("The SMTP server response was -> %s\n"), response.Get() );
                errorsFound++;
            }
            response.Free();
        }
    }

    tmpBuf.Free();      // release the parsed email addresses
    if ( errorsFound == recipientsSent ) {
        finish_server_message();
        return(-1);
    }

    msgLength = msgLength; // eliminate compiler warning.
#if BLAT_LITE
#else
    if ( !binaryMimeSupported )
        chunkingSupported = FALSE;

  #if ALLOW_CHUNKING
    if ( chunkingSupported ) {
        _TCHAR serverMessage[80];

        _stprintf( serverMessage, __T("BDAT %lu LAST\r\n"), msgLength );
        if ( put_message_line( ServerSocket, serverMessage ) )
            return(-1);
    } else
  #else
  #endif
#endif
    {
        if ( put_message_line( ServerSocket, __T("DATA\r\n") ) )
            return(-1);

        if ( get_server_response( NULL, &enhancedStatusCode ) != 354 ) {
            server_error (__T("SMTP server error accepting message data"));
            return(-1);
        }
    }

    return(0);
}

/*
 * This is a snapshop of a session where enhanced status codes were available.
 *
 * <<<getline<<< 250-ENHANCEDSTATUSCODES
 * <<<getline<<< 250 HELP
 * >>>putline>>> MAIL From: </munged/>
 * <<<getline<<< 250 2.1.0 </munged/>... Sender ok
 * >>>putline>>> RCPT To: </munged/>
 * <<<getline<<< 250 2.1.5 </munged/>... Recipient ok
 * >>>putline>>> DATA
 * <<<getline<<< 354 Enter mail, end with "." on a line by itself
 * <<<getline<<< 250 2.0.0 xxxxxx Message accepted for delivery
 * >>>putline>>> QUIT
 * <<<getline<<< 221 2.0.0 /munged.domain/ closing connection
 */

static int prefetch_smtp_info(LPTSTR wanted_hostname)
{
#if INCLUDE_POP3

    if ( POP3Login.Get()[0] || POP3Password.Get()[0] )
        if ( !POP3Host[0] )
            _tcscpy( POP3Host, SMTPHost );

    if ( POP3Host[0] ) {
        _TCHAR saved_quiet;

        saved_quiet = quiet;
        quiet       = TRUE;

        if ( !open_server_socket( POP3Host, POP3Port, defaultPOP3Port, __T("pop3") ) ) {
            Buf  responseStr;

            for ( ; ; ) {
                if ( get_pop3_server_response( &responseStr ) )
                    break;

                if ( POP3Login.Get()[0] || AUTHLogin.Get()[0] ) {
                    responseStr = __T("USER ");
                    if ( POP3Login.Get()[0] )
                        responseStr.Add( POP3Login );
                    else
                        responseStr.Add( AUTHLogin );

                    responseStr.Add( __T("\r\n") );
                    if ( put_message_line( ServerSocket, responseStr.Get() ) )
                        break;

                    if ( get_pop3_server_response( &responseStr ) )
                        break;
                }
                if ( POP3Password.Get()[0] || AUTHPassword.Get()[0] ) {
                    responseStr = __T("PASS ");
                    if ( POP3Password.Get()[0] )
                        responseStr.Add( POP3Password );
                    else
                        responseStr.Add( AUTHPassword );

                    responseStr.Add( __T("\r\n") );
                    if ( put_message_line( ServerSocket, responseStr.Get() ) )
                        break;

                    if ( get_pop3_server_response( &responseStr ) )
                        break;
                }
                if ( put_message_line( ServerSocket, __T("STAT\r\n") ) )
                    break;

                if ( get_pop3_server_response( &responseStr ) )
                    break;

                if ( xtnd_xmit_wanted ) {
                    if ( put_message_line( ServerSocket, __T("XTND XMIT\r\n") ) )
                        break;

                    if ( get_pop3_server_response( &responseStr ) == 0 ) {
                        xtnd_xmit_supported = TRUE;
                        return 0;
                    }
                }

                break;
            }
            if ( !put_message_line( ServerSocket, __T("QUIT\r\n") ) )
                get_pop3_server_response( &responseStr );

            responseStr.Free();
            close_server_socket();

            if ( delayBetweenMsgs ) {
                printMsg( __T("*** Delay %d seconds...\n"), delayBetweenMsgs );
                Sleep( delayBetweenMsgs * 1000ul ); // Delay user requested seconds.
            }
        }
        quiet = saved_quiet;
    }
#endif
#if INCLUDE_IMAP
    if ( IMAPHost[0] ) {
//        Buf  savedResponse;
        _TCHAR saved_quiet;
        _TCHAR imapLogin;
        _TCHAR imapPlain;
        _TCHAR imapCRAM;
        int  retVal;

        saved_quiet = quiet;
        quiet       = TRUE;
        imapLogin   = 0;
        imapPlain   = 0;
        imapCRAM    = 0;
        retVal      = 0;

        if ( !open_server_socket( IMAPHost, IMAPPort, defaultIMAPPort, __T("imap") ) ) {
            Buf    responseStr;
            LPTSTR pStr;
            LPTSTR pStr1;
            LPTSTR pStrEnd;

            for ( ; ; ) {
                retVal = get_imap_untagged_server_response( &responseStr );
                if ( retVal < 0 ) {
                    responseStr.Free();
                    close_server_socket();
                    quiet = saved_quiet;
                    return( retVal );
                }

                _tcslwr( responseStr.Get() );
                if ( !_tcsstr( responseStr.Get(), __T(" ok") ) )
                    break;

                pStr = _tcsstr( responseStr.Get(), __T(" capability") );
                if ( !pStr ) {
                    if ( put_message_line( ServerSocket, __T("a001 CAPABILITY\r\n") ) )
                        break;

                    retVal = get_imap_tagged_server_response( &responseStr, __T("a001") );
                    if ( retVal < 0 ) {
                        responseStr.Free();
                        close_server_socket();
                        quiet = saved_quiet;
                        return( retVal );
                    }
                    _tcslwr( responseStr.Get() );
                    pStr = _tcsstr( responseStr.Get(), __T(" capability") );
                    if ( !pStr )
                        break;
                }
//                savedResponse = responseStr;
                pStr1 = _tcsstr( pStr, __T(" auth=") );
                if ( pStr1 ) {
                    pStr = pStr1 + 6;
                    pStrEnd = _tcschr( pStr, __T(' ') );
                    if ( pStrEnd )
                        *pStrEnd = __T('\0');

                    pStr = _tcstok( pStr, __T(",") );
                    for ( ; ; ) {
                        if ( !pStr )
                            break;

                        if ( _tcscmp( pStr, __T("login") ) == 0 )
                            imapLogin = 1;
                        else
                        if ( _tcscmp( pStr, __T("plain") ) == 0 )
                            imapPlain = 1;
                        else
                        if ( _tcscmp( pStr, __T("cram-md5") ) == 0 )
                            imapCRAM = 1;

                        pStr = _tcstok( NULL, __T(",") );
                    }
                } else {
                    pStr1 = _tcsstr( pStr, __T("sasl") );
                    if ( pStr1 ) {
                        if ( _tcsstr( pStr1, __T("login") ) == 0 )
                            imapLogin = 1;
                        else
                        if ( _tcsstr( pStr1, __T("plain") ) == 0 )
                            imapPlain = 1;
                        else
                        if ( _tcsstr( pStr1, __T("cram-md5") ) == 0 )
                            imapCRAM = 1;
                    } else
                         break;
                }

//                if ( _tcsstr( savedResponse.Get(), __T("logindisabled") ) )
//                    imapLogin = 0;

                if ( IMAPLogin.Get()[0] || AUTHLogin.Get()[0] || IMAPPassword.Get()[0] || AUTHPassword.Get()[0] ) {
                    if ( imapCRAM ) {
                        Buf  challenge;
                        _TCHAR str[1024];

                        responseStr = __T("a010 AUTHENTICATE \"CRAM-MD5\"");
                        retVal = get_imap_untagged_server_response( &responseStr );
                        if ( retVal < 0 ) {
                            responseStr.Free();
                            close_server_socket();
                            quiet = saved_quiet;
                            return( retVal );
                        }
                        challenge.Add( responseStr.Get()+2 );
                        cram_md5( challenge, str );

                        if ( put_message_line( ServerSocket, str ) )
                            break;

                        retVal = get_imap_tagged_server_response( &responseStr, __T("a010") );
                        if ( retVal < 0 ) {
                            responseStr.Free();
                            close_server_socket();
                            quiet = saved_quiet;
                            return( retVal );
                        }
                        _tcslwr( responseStr.Get() );
                        if ( !_tcsstr( responseStr.Get(), __T("a010 ok") ) )
                            break;
                    } else
                    if ( imapPlain ) {
                        responseStr = __T("a020 AUTHENTICATE PLAIN\r\n\0\"");
                        if ( IMAPLogin.Get()[0] )
                            responseStr.Add( IMAPLogin );
                        else
                            responseStr.Add( AUTHLogin );

                        responseStr.Add( __T("\"\0") );
                        if ( IMAPPassword.Get()[0] )
                            responseStr.Add( IMAPPassword );
                        else
                            responseStr.Add( AUTHPassword );

                        responseStr.Add( __T("\"\r\n") );
                        if ( put_message_line( ServerSocket, responseStr.Get() ) )
                            break;

                        retVal = get_imap_tagged_server_response( &responseStr, __T("a020") );
                        if ( retVal < 0 ) {
                            responseStr.Free();
                            close_server_socket();
                            quiet = saved_quiet;
                            return( retVal );
                        }
                        _tcslwr( responseStr.Get() );
                        if ( !_tcsstr( responseStr.Get(), __T("a020 ok") ) )
                            break;
                    } else
                    if ( imapLogin ) {
                        responseStr = __T("a030 LOGIN \"");
                        if ( IMAPLogin.Get()[0] )
                            responseStr.Add( IMAPLogin );
                        else
                            responseStr.Add( AUTHLogin );

                        responseStr.Add( __T("\" \"") );
                        if ( IMAPPassword.Get()[0] )
                            responseStr.Add( IMAPPassword );
                        else
                            responseStr.Add( AUTHPassword );

                        responseStr.Add( __T("\"\r\n") );
                        if ( put_message_line( ServerSocket, responseStr.Get() ) )
                            break;

                        retVal = get_imap_tagged_server_response( &responseStr, __T("a030") );
                        if ( retVal < 0 ) {
                            responseStr.Free();
                            close_server_socket();
                            quiet = saved_quiet;
                            return( retVal );
                        }
                        _tcslwr( responseStr.Get() );
                        if ( !_tcsstr( responseStr.Get(), __T("a030 ok") ) )
                            break;
                    }
                }
                break;
            }
            // if ( !put_message_line( ServerSocket, __T("a003 STATUS \"INBOX\" (MESSAGES UNSEEN)\r\n") ) )
            if ( !put_message_line( ServerSocket, __T("a097 SELECT \"INBOX\"\r\n") ) ) {
                retVal = get_imap_tagged_server_response( NULL, __T("a097") );
                if ( retVal < 0 ) {
                    responseStr.Free();
                    close_server_socket();
                    quiet = saved_quiet;
                    return( retVal );
                }
            }

            if ( (IMAPLogin.Get()[0] || AUTHLogin.Get()[0] || IMAPPassword.Get()[0] || AUTHPassword.Get()[0]) && imapLogin ) {
                if ( !put_message_line( ServerSocket, __T("a098 LOGOUT\r\n") ) ) {
                    retVal = get_imap_tagged_server_response( NULL, __T("a098") );
                }
            } else
                if ( !put_message_line( ServerSocket, __T("a099 BYE\r\n") ) ) {
                    retVal = get_imap_tagged_server_response( NULL, __T("a099")  );
                }

            responseStr.Free();
            close_server_socket();
        }
        quiet = saved_quiet;
        if ( retVal < 0 )
            return( retVal );
    }
#endif

    return say_hello( wanted_hostname );
}


int send_email( size_t msgBodySize,
                Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                LPTSTR attachment_boundary, LPTSTR multipartID,
                int nbrOfAttachments, DWORD totalsize )
{
    Buf     messageBuffer;
    Buf     multipartHdrs;
    Buf     varHeaders;
    Buf     header;
    Buf     tmpBuf;
    int     n_of_try;
    int     retcode=0;
    int     k;
    int     yEnc_This;
    LPTSTR  pp;
    int     namesFound, namesProcessed;
    int     serverOpen;
    BLDHDRS bldHdrs;
    int     userAuthenticated;

    if ( !SMTPHost[0] || !Recipients.Length() )
        return(0);

    bldHdrs.messageBuffer         = &messageBuffer;
    bldHdrs.header                = &header;
    bldHdrs.varHeaders            = &varHeaders;
    bldHdrs.multipartHdrs         = &multipartHdrs;
    bldHdrs.buildSMTP             = TRUE;
    bldHdrs.lpszFirstReceivedData = &lpszFirstReceivedData;
    bldHdrs.lpszOtherHeader       = &lpszOtherHeader;
    bldHdrs.attachment_boundary   = attachment_boundary;
    bldHdrs.multipartID           = multipartID;
    bldHdrs.wanted_hostname       = my_hostname_wanted;
    bldHdrs.server_name           = SMTPHost;
    bldHdrs.nbrOfAttachments      = nbrOfAttachments;
    bldHdrs.addBccHeader          = FALSE;

    maxMessageSize      = 0;

    commandPipelining   =
    plainAuthSupported  =
    loginAuthSupported  = FALSE;

    userAuthenticated   = FALSE;
#if BLAT_LITE
#else
    binaryMimeSupported = 0;
#endif

#if INCLUDE_POP3
    xtnd_xmit_supported = FALSE;
#endif

#if defined(_UNICODE) || defined(UNICODE)
    Buf tmpBuf2;
    int utf;

    tmpBuf.Clear();
    tmpBuf2 = AUTHLogin;
    utf = NATIVE_16BIT_UTF;
    convertPackedUnicodeToUTF( tmpBuf2, tmpBuf, &utf, NULL, 8 );
    if ( utf )
        AUTHLogin = tmpBuf;

    tmpBuf.Clear();
    tmpBuf2 = AUTHPassword;
    utf = NATIVE_16BIT_UTF;
    convertPackedUnicodeToUTF( tmpBuf2, tmpBuf, &utf, NULL, 8 );
    if ( utf )
        AUTHPassword = tmpBuf;

 #if INCLUDE_POP3
    tmpBuf.Clear();
    tmpBuf2 = POP3Login;
    utf = NATIVE_16BIT_UTF;
    convertPackedUnicodeToUTF( tmpBuf2, tmpBuf, &utf, NULL, 8 );
    if ( utf )
        POP3Login = tmpBuf;

    tmpBuf.Clear();
    tmpBuf2 = POP3Password;
    utf = NATIVE_16BIT_UTF;
    convertPackedUnicodeToUTF( tmpBuf2, tmpBuf, &utf, NULL, 8 );
    if ( utf )
        POP3Password = tmpBuf;
 #endif

 #if INCLUDE_IMAP
    tmpBuf.Clear();
    tmpBuf2 = IMAPLogin;
    utf = NATIVE_16BIT_UTF;
    convertPackedUnicodeToUTF( tmpBuf2, tmpBuf, &utf, NULL, 8 );
    if ( utf )
        IMAPLogin = tmpBuf;

    tmpBuf.Clear();
    tmpBuf2 = IMAPPassword;
    utf = NATIVE_16BIT_UTF;
    convertPackedUnicodeToUTF( tmpBuf2, tmpBuf, &utf, NULL, 8 );
    if ( utf )
        IMAPPassword = tmpBuf;
 #endif

    tmpBuf2.Free();
#endif

    retcode = prefetch_smtp_info( my_hostname_wanted );
//    printMsg( __T(" ... prefetch_smtp_info() returned %d\n"), retcode );
    if ( retcode )
        return retcode;

#if INCLUDE_POP3
    if ( xtnd_xmit_supported ) {
        bldHdrs.addBccHeader = TRUE;
  #if SUPPORT_MULTIPART
        maxMessageSize = 0;
        multipartSize  = 0;
        disableMPS     = TRUE;
  #endif
    }
#endif

    serverOpen = 1;

#if SUPPORT_YENC
    yEnc_This = yEnc;
    if ( !eightBitMimeSupported && !binaryMimeSupported )
#endif
        yEnc_This = FALSE;

#if SUPPORT_MULTIPART
    DWORD msgSize = (DWORD)(-1);

    if ( maxMessageSize ) {
        if ( multipartSize && !disableMPS ) {
            if ( maxMessageSize < multipartSize )
                msgSize = (DWORD)maxMessageSize;
            else
                msgSize = (DWORD)multipartSize;
        } else
            msgSize = (DWORD)maxMessageSize;
    } else
        if ( multipartSize && !disableMPS  )
            msgSize = (DWORD)multipartSize;

    getMaxMsgSize( TRUE, msgSize );

    if ( disableMPS && (totalsize > msgSize) ) {
        printMsg( __T("Message is too big to send.  Please try a smaller message.\n") );
        finish_server_message();
        close_server_socket();
        serverOpen = 0;
        return 14;
    }

    size_t totalParts = (size_t)(totalsize / msgSize);
    if ( totalsize % msgSize )
        totalParts++;

    if ( totalParts > 1 ) {
        // send multiple messages, one attachment per message.
        int      attachNbr;
        DWORD    attachSize;
        int      attachType, prevAttachType;
        LPTSTR   attachName;
        int      thisPart, partsCount;
        DWORD    startOffset;

        if ( totalParts > 1 )
            printMsg( __T("Sending %lu parts for this message.\n"), totalParts );

        prevAttachType = -1;
        partsCount = 0;
        for ( attachNbr = 0; attachNbr < nbrOfAttachments; attachNbr++ ) {
            getAttachmentInfo( attachNbr, attachName, attachSize, attachType );
            partsCount = (int)(attachSize / msgSize);
            if ( attachSize % msgSize )
                partsCount++;

            if ( partsCount > 1 )
                break;
        }

        // If any of the attachments have to be broken into smaller pieces,
        // and if the user did not specify an encoding scheme, then choose
        // UUEncode so that popular email clients will be able to combine
        // the messages properly.  It was found that at least some clients
        // changed the encoding to UUE when piecing messages back together,
        // and when combining the whole lot, the client would not display
        // mixed encoding types properly.

        if ( partsCount > 1 ) {
            formattedContent = TRUE;
            if ( !yEnc_This ) {
                mime     = FALSE;
                base64   = FALSE;
                uuencode = TRUE;
            }
        }

        for ( attachNbr = 0; attachNbr < nbrOfAttachments; attachNbr++ ) {
            if ( retcode )
                break;

            if ( attachNbr && delayBetweenMsgs ) {
                if ( serverOpen ) {
                    close_server_socket();
                    serverOpen = 0;
                    userAuthenticated = FALSE;
                }
                printMsg( __T("*** Delay %d seconds...\n"), delayBetweenMsgs );
                Sleep( delayBetweenMsgs * 1000ul ); // Delay user requested seconds.
            }

            n_of_try = noftry();
            for ( k = 1; k <= n_of_try || n_of_try == -1; k++ ) {
                retcode = 0;
                if ( !serverOpen )
                    retcode = say_hello( my_hostname_wanted );

                if ( retcode == 0 ) {
                    serverOpen = 1;
                    if ( !userAuthenticated ) {
//                        printMsg( __T(" ... Attempting to authenticate to the server.\n") );
                        retcode = authenticate_smtp_user( AUTHLogin.Get(), AUTHPassword.Get() );
                        if ( retcode == 0 )
                            userAuthenticated = TRUE;
                    }
                }

                if ( (retcode == 0) || (retcode == -2) )
                    break;
            }

            getAttachmentInfo( attachNbr, attachName, attachSize, attachType );
            partsCount = (int)(attachSize / msgSize);
            if ( attachSize % msgSize )
                partsCount++;

            header.Clear();
            startOffset = 0;
            boundaryPosted = FALSE;
            for ( thisPart = 0; thisPart < partsCount; ) {
                DWORD length;

                if ( retcode )
                    break;

                bldHdrs.part       = ++thisPart;
                bldHdrs.totalparts = partsCount;
                bldHdrs.attachNbr  = attachNbr;
                bldHdrs.attachName = attachName;
                bldHdrs.attachSize = attachSize;

                build_headers( bldHdrs );
                retcode= add_message_body( messageBuffer, msgBodySize, multipartHdrs, TRUE,
                                           attachment_boundary, startOffset, thisPart, attachNbr );
                if ( retcode )
                    return retcode;

                msgBodySize = 0;    // Do not include the body in subsequent messages.

                length = msgSize;
                if ( length > (attachSize - startOffset) )
                    length = attachSize - startOffset;

                retcode = add_one_attachment( messageBuffer, TRUE, attachment_boundary,
                                              startOffset, length, thisPart, partsCount,
                                              attachNbr, &prevAttachType );
                if ( thisPart == partsCount )
                    add_msg_boundary( messageBuffer, TRUE, attachment_boundary );

                multipartHdrs.Clear();
                if ( retcode )
                    return retcode;

                namesFound = 0;
                parse_email_addresses (Recipients.Get(), tmpBuf);
                pp = tmpBuf.Get();
                if ( !pp || (*pp == __T('\0')) ) {
                    server_error( __T("No email address was found for recipients.\n") \
                                  __T("Have you set the 'To:' field correctly?") );
                    return(-2);
                }
                for ( ; *pp != __T('\0'); namesFound++ )
                    pp += _tcslen( pp ) + 1;

                // send the message to the SMTP server!
                for ( namesProcessed = 0; namesProcessed < namesFound; ) {
                    Buf holdingBuf;

                    if ( namesProcessed && delayBetweenMsgs ) {
                        printMsg( __T("*** Delay %d seconds...\n"), delayBetweenMsgs );
                        Sleep( delayBetweenMsgs * 1000ul ); // Delay user requested seconds.
                    }

                    if ( maxNames <= 0 )
                        holdingBuf.Add( Recipients );
                    else {
                        int x;

                        pp = tmpBuf.Get();
                        for ( x = 0; x < namesProcessed; x++ )
                            pp += _tcslen( pp ) + 1;

                        for ( x = 0; (x < maxNames) && *pp; x++ ) {
                            if ( x )
                                holdingBuf.Add( __T(',') );

                            holdingBuf.Add( pp );
                            pp += _tcslen( pp ) + 1;
                        }
                    }

                    // send the message to the SMTP server!
                    n_of_try = noftry();
                    for ( k=1; k<=n_of_try || n_of_try == -1; k++ ) {
                        if ( n_of_try > 1 )
                            printMsg( __T("Try number %d of %d.\n"), k, n_of_try);

                        if ( k > 1 ) Sleep(15000);

                        if ( 0 == retcode )
                            retcode = prepare_smtp_message( holdingBuf.Get(), (DWORD)messageBuffer.Length() );

                        if ( 0 == retcode ) {
                            if ( partsCount > 1 )
                                printMsg( __T("Sending part %d of %d\n"), thisPart, partsCount );

                            retcode = send_edit_data( messageBuffer.Get(), 250, NULL );
                            if ( 0 == retcode ) {
                                n_of_try = 1;
                                k = 2;
                                break;
                            }
                        } else if ( -2 == retcode ) {
                            // This wasn't a connection error.  The server actively denied our connection.
                            // Stop trying to send mail.
                            n_of_try = 1;
                            k = 2;
                            break;
                        }
                    }

                    if ( k > n_of_try ) {
                        if ( maxNames <= 0 )
                            namesProcessed = namesFound;
                        else
                            namesProcessed += maxNames;
                    }
                    holdingBuf.Free();
                }
                startOffset += length;
                tmpBuf.Free();
            }
        }
        finish_server_message();
        close_server_socket();
        serverOpen = 0;
    } else
#else
    multipartID = multipartID;

    if ( maxMessageSize ) {
        if ( disableMPS && (totalsize > maxMessageSize) ) {
            printMsg( __T("Message is too big to send.  Please try a smaller message.\n") );
            finish_server_message();
            close_server_socket();
            serverOpen = 0;
            return 14;
        }
    }
#endif
    {
        // send a single message.
        boundaryPosted = FALSE;

        bldHdrs.part             = 0;
        bldHdrs.totalparts       = 0;
        bldHdrs.attachNbr        = 0;
        bldHdrs.nbrOfAttachments = 0;
        bldHdrs.attachSize       = 0;
        bldHdrs.attachName       = NULL;

//        printMsg( __T(" ... calling build_headers()\n") );
        build_headers( bldHdrs );
//        printMsg( __T(" ... calling add_message_body(), body size = %lu\n"), msgBodySize );
        retcode = add_message_body( messageBuffer, msgBodySize, multipartHdrs, TRUE,
                                    attachment_boundary, 0, 0, 0 );
//        printMsg( __T(" ... add_message_body() returned %d\n"), retcode );
        if ( retcode )
            return retcode;

        if ( nbrOfAttachments ) {
//            printMsg( __T(" ... adding %d attachments\n"), nbrOfAttachments );
            retcode = add_attachments( messageBuffer, TRUE, attachment_boundary, nbrOfAttachments );
            add_msg_boundary( messageBuffer, TRUE, attachment_boundary );

//            printMsg( __T(" ... add_attachments() returned %d\n"), retcode );
            if ( retcode )
                return retcode;
        }

        namesFound = 0;
//        printMsg( __T(" ... parsing email addresses(), %s\n"), Recipients.Get() );
        parse_email_addresses (Recipients.Get(), tmpBuf);
        pp = tmpBuf.Get();
        if ( pp ) {
            for ( ; *pp != __T('\0'); namesFound++ )
                pp += _tcslen( pp ) + 1;
        }

//        printMsg( __T(" ... number of names found = %d\n"), namesFound );
//        printMsg( __T(" ... delay between messages = %d seconds\n"), delayBetweenMsgs );
        // send the message to the SMTP server!
        for ( namesProcessed = 0; namesProcessed < namesFound; ) {
            Buf holdingBuf;

            if ( namesProcessed && delayBetweenMsgs ) {
                if ( serverOpen ) {
                    close_server_socket();
                    serverOpen = 0;
                    userAuthenticated = FALSE;
                }
                printMsg( __T("*** Delay %d seconds...\n"), delayBetweenMsgs );
                Sleep( delayBetweenMsgs * 1000ul ); // Delay user requested seconds.
            }

            holdingBuf.Clear();
            if ( maxNames <= 0 )
                holdingBuf.Add( Recipients );
            else {
                int x;

                pp = tmpBuf.Get();
                for ( x = 0; x < namesProcessed; x++ )
                    pp += _tcslen( pp ) + 1;

                for ( x = 0; (x < maxNames) && *pp; x++ ) {
                    if ( x )
                        holdingBuf.Add( __T(',') );

                    holdingBuf.Add( pp );
                    pp += _tcslen( pp ) + 1;
                }
            }

            n_of_try = noftry();
//            printMsg( __T(" ... number of try attempts is %d\n"), n_of_try );
//            printMsg( __T(" ... server connection is%s open\n"), serverOpen ? __T("") : __T(" not") );
            for ( k=1; (k<=n_of_try) || (n_of_try == -1); k++ ) {
                int anticipatedNameCount;

                if ( maxNames <= 0 )
                    anticipatedNameCount = namesFound;
                else
                    anticipatedNameCount = namesProcessed + maxNames;

                if ( n_of_try > 1 )
                    printMsg( __T("Try number %d of %d.\n"), k, n_of_try);

                if ( k > 1 ) Sleep(15000);

                retcode = 0;
                if ( !serverOpen )
                    retcode = say_hello( my_hostname_wanted );

                if ( 0 == retcode ) {
                    serverOpen = 1;
                    if ( !userAuthenticated ) {
//                        printMsg( __T(" ... Attempting to authenticate to server.\n") );
                        retcode = authenticate_smtp_user( AUTHLogin.Get(), AUTHPassword.Get() );
                        if ( 0 == retcode )
                            userAuthenticated = TRUE;
                    }
                }

                if ( 0 == retcode )
                    retcode = prepare_smtp_message( holdingBuf.Get(), (DWORD)messageBuffer.Length() );

                if ( 0 == retcode ) {
#if INCLUDE_POP3
                    if ( xtnd_xmit_supported )
                        retcode = send_edit_data( messageBuffer.Get(), NULL );
                    else
#endif
                        retcode = send_edit_data( messageBuffer.Get(), 250, NULL );
                    if ( 0 == retcode ) {
                        if ( anticipatedNameCount >= namesFound )
                            finish_server_message();

                        n_of_try = 1;
                        k = 2;
                    }
                } else if ( -2 == retcode ) {
                    // This wasn't a connection error.  The server actively denied our connection.
                    // Stop trying to send mail.
                    n_of_try = 1;
                    k = 2;
                }
                if ( anticipatedNameCount >= namesFound ) {
                    close_server_socket();
                    serverOpen = 0;
                }
            }

            if ( k > n_of_try ) {
                if ( maxNames <= 0 )
                    namesProcessed = namesFound;
                else
                    namesProcessed += maxNames;
            }
            holdingBuf.Free();
        }
    }

#if SUPPORT_SALUTATIONS
    salutation.Free();
#endif

    tmpBuf.Free();
    header.Free();
    varHeaders.Free();
    multipartHdrs.Free();
    messageBuffer.Free();

    return(retcode);
}
