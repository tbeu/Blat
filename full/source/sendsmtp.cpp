/*
    sendSMTP.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blat.h"
#include "winfile.h"
#include "gensock.h"
#include "md5.h"
#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
#endif

#define ALLOW_PIPELINING    FALSE

#if SUPPORT_GSSAPI
  extern BOOL    authgssapi;
  extern char    servicename[SERVICENAME_SIZE];
  extern char    SMTPHost[];
  extern char    AUTHLogin[];
  extern protLev gss_protection_level;
  extern BOOL    mutualauth;
#endif

extern void base64_encode(const unsigned char *in, int length, char *out, int inclCrLf);
extern int  base64_decode(const unsigned char *in, char *out);
extern int  open_server_socket( char *host, char *userPort, const char *defaultPort, char *portName );
extern int  get_server_response( Buf * responseStr, int * enhancedStatusCode );
#if INCLUDE_IMAP
extern int  get_imap_untagged_server_response( Buf * responseStr  );
extern int  get_imap_tagged_server_response( Buf * responseStr, char * tag  );
#endif
#if SUPPORT_GSSAPI
extern int get_server_response( char * responseStr );
#endif
extern int  put_message_line( socktag sock, const char * line );
extern int  finish_server_message( void );
extern int  close_server_socket( void );
extern void server_error (const char * message);
extern int  send_edit_data (char * message, Buf * responseStr );
extern int  send_edit_data (char * message, int expected_response, Buf * responseStr );
extern int  noftry();
extern void build_headers( BLDHDRS &bldHdrs /* Buf &messageBuffer, Buf &header, int buildSMTP,
                           Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                           char * attachment_boundary, char * wanted_hostname, char * server */ );
#if SUPPORT_MULTIPART
extern int  add_one_attachment( Buf &messageBuffer, int buildSMTP, char * attachment_boundary,
                                DWORD startOffset, DWORD &length,
                                int part, int totalparts, int attachNbr, int * prevAttachType );
extern void getAttachmentInfo( int attachNbr, char * & attachName, DWORD & attachSize, int & attachType );
extern void getMaxMsgSize( int buildSMTP, DWORD &length );
#endif
extern int  add_message_body( Buf &messageBuffer, int msgBodySize, Buf &multipartHdrs, int buildSMTP,
                              char * attachment_boundary, DWORD startOffset, int part, int attachNbr );
extern int  add_attachments( Buf &messageBuffer, int buildSMTP, char * attachment_boundary, int nbrOfAttachments );
extern void add_msg_boundary( Buf &messageBuffer, int buildSMTP, char * attachment_boundary );
extern void parseCommaDelimitString ( char * source, Buf & parsed_addys, int pathNames );

extern void printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison

extern socktag ServerSocket;

#if INCLUDE_POP3
extern int   get_pop3_server_response( Buf * responseStr );

extern char    xtnd_xmit_supported;
extern char    xtnd_xmit_wanted;
extern char    POP3Host[SERVER_SIZE+1];
extern char    POP3Port[SERVER_SIZE+1];
extern char    POP3Login[];
extern char    POP3Password[];
#endif
#if INCLUDE_IMAP
extern char    IMAPHost[SERVER_SIZE+1];
extern char    IMAPPort[SERVER_SIZE+1];
extern char    IMAPLogin[];
extern char    IMAPPassword[];
#endif
extern char    SMTPHost[];
extern char    SMTPPort[];

extern char    AUTHLogin[];
extern char    AUTHPassword[];
extern char    Sender[];
extern char    my_hostname[];
extern Buf     Recipients;
extern char    loginname[]; // RFC 821 MAIL From. <loginname>. There are some inconsistencies in usage
extern char    quiet;
extern char    mime;
extern char    formattedContent;
extern char    boundaryPosted;

extern char    my_hostname_wanted[];
extern int     delayBetweenMsgs;

#if INCLUDE_SUPERDEBUG
extern char    superDebug;
#endif

#if BLAT_LITE
#else
extern char    base64;
extern char    uuencode;
extern char    yEnc;
extern char    deliveryStatusRequested;
extern char    deliveryStatusSupported;

extern char    eightBitMimeSupported;
extern char    eightBitMimeRequested;
extern char    binaryMimeSupported;
#endif
#if SUPPORT_MULTIPART
extern unsigned long multipartSize;
#endif
extern char          disableMPS;

const  char        * defaultSMTPPort     = "25";
#if INCLUDE_POP3
const  char        * defaultPOP3Port     = "110";
#endif
#if SUBMISSION_PORT
const  char        * SubmissionPort      = "587";
#endif
#if INCLUDE_IMAP
const  char        * defaultIMAPPort     = "143";
#endif

int                  maxNames            = 0;


unsigned long maxMessageSize       = 0;
char          commandPipelining    = FALSE;
char          cramMd5AuthSupported = FALSE;
char          plainAuthSupported   = FALSE;
char          loginAuthSupported   = FALSE;
char          enhancedCodeSupport  = FALSE;
char          ByPassCRAM_MD5       = FALSE;
#if SUPPORT_GSSAPI
char          gssapiAuthSupported  = FALSE;


BOOL putline ( char *line )
{
    return (put_message_line(ServerSocket,line) == 0);
}


BOOL getline ( char *line )
{
    return (get_server_response(line) != -1);
}
#endif

#if SUPPORT_SALUTATIONS
extern Buf salutation;

void find_and_strip_salutation ( Buf &email_addresses )
{
    Buf    new_addresses;
    Buf    tmpstr;
    char * srcptr;
    char * pp;
    char * pp2;
    size_t len;
    int    x;

    salutation.Free();

    parseCommaDelimitString( email_addresses.Get(), tmpstr, FALSE );
    srcptr = tmpstr.Get();
    if ( !srcptr )
        return;

    for ( x = 0; *srcptr; srcptr += len + 1 ) {
        int addToSal = FALSE;

        len = strlen (srcptr);

        // 30 Jun 2000 Craig Morrison, parses out just email address in TO:,CC:,BCC:
        // so that all we RCPT To with is the email address.
        pp = strchr(srcptr, '<');
        if ( pp ) {
            pp2 = strchr(++pp, '>');
            if ( pp2 ) {
                *(++pp2) = 0;
                if ( (size_t)(pp2 - srcptr) < len ) {
                    pp2++;
                    while ( *pp2 == ' ' )
                        pp2++;

                    if ( *pp2 == '\\' )
                        pp2++;

                    if ( *pp2 == '"' )
                        pp2++;

                    for ( ; *pp2 && (*pp2 != '"'); pp2++ )
                        if ( *pp2 != '\\' ) {
                            salutation.Add( *pp2 );
                            addToSal = TRUE;
                        }
                }
            }
        } else {
            pp = srcptr;
            while (*pp == ' ')
                pp++;

            pp2 = strchr(pp, ' ');
            if ( pp2 )
                *pp2 = 0;
        }
        if ( x++ )
            new_addresses.Add( ", " );

/*
        if ( *srcptr == '"' ) {
            srcptr++;
            len--;
        }
 */
        if ( *srcptr != '"' ) {
            pp = strrchr( srcptr, '<' );
            if ( pp ) {
                *pp = 0;
                pp2 = strchr( srcptr, ',' );
                *pp = '<';
                if ( pp2 ) {
                    new_addresses.Add( '"' );
                    pp2 = pp;
                    for ( pp2 = pp; pp2 > srcptr; ) {
                        if ( *(pp2-1) != ' ' )
                            break;
                    }
                    new_addresses.Add( srcptr, (int)(pp2 - srcptr) );
                    new_addresses.Add( "\" ", 2 );
                    new_addresses.Add( pp );
                } else
                    new_addresses.Add( srcptr );
            } else
                new_addresses.Add( srcptr );
        } else
            new_addresses.Add( srcptr );

        if ( addToSal )
            salutation.Add( '\0' );
    }

    if ( salutation.Length() )
        salutation.Add( '\0' );

    if ( x ) {
        email_addresses.Move( new_addresses );
    }

    tmpstr.Free();
}
#endif


void parse_email_addresses (const char * email_addresses, Buf & parsed_addys )
{
    Buf    tmpstr;
    char * srcptr;
    char * pp;
    char * pp2;
    size_t len;

    parsed_addys.Free();
    len = strlen(email_addresses);
    if ( !len )
        return;

    parseCommaDelimitString( (char *)email_addresses, tmpstr, FALSE );
    srcptr = tmpstr.Get();
    if ( !srcptr )
        return;

    for ( ; *srcptr; ) {
        len = strlen( srcptr );
        pp = strchr(srcptr, '<');
        if ( pp ) {
            pp2 = strchr(++pp, '>');
            if ( pp2 )
                *pp2 = 0;
        } else {        // if <address> not found, then skip blanks and parse for the first blank
            pp = srcptr;
            while (*pp == ' ')
                pp++;

            pp2 = strchr(pp, ' ');
            if ( pp2 )
                *pp2 = 0;
        }
        parsed_addys.Add( pp, (int)strlen(pp) + 1 );
        srcptr += len + 1;
    }

    parsed_addys.Add( '\0' );
}

#define EMAIL_KEYWORD   "myself"

void searchReplaceEmailKeyword (Buf & email_addresses)
{
    Buf    tmpstr;
    char * srcptr;
    char * pp;
    size_t len;

    len = (size_t) email_addresses.Length();
    if ( !len )
        return;

    parseCommaDelimitString( email_addresses.Get(), tmpstr, FALSE );
    srcptr = tmpstr.Get();
    if ( !srcptr )
        return;

    email_addresses.Clear();
    for ( ; *srcptr; ) {
        len = strlen( srcptr );
        pp = srcptr;
        if ( !strcmp( srcptr, EMAIL_KEYWORD ) )
            pp = loginname;

        if ( email_addresses.Length() )
            email_addresses.Add( ',' );

        email_addresses.Add( pp, (int)strlen(pp) );
        srcptr += len + 1;
    }
}


static int say_hello (const char* wanted_hostname, BOOL bAgain = FALSE)
{
    char   out_data[MAXOUTLINE];
    Buf    responseStr;
    int    enhancedStatusCode;
    int    no_of_try;
    int    tryCount;
    int    socketError = 0;
    char * pStr;

    if ( wanted_hostname )
        if ( !*wanted_hostname )
            wanted_hostname = NULL;

    no_of_try = noftry();
    tryCount = 0;
    for ( ; tryCount < no_of_try; tryCount++ ) {
        if ( tryCount ) {
#if INCLUDE_SUPERDEBUG
            if ( superDebug ) {
                char savedQuiet = quiet;
                quiet = FALSE;
                printMsg("superDebug: ::say_hello() failed to connect, retry count remaining is %u\n",
                         no_of_try - tryCount );
                quiet = savedQuiet;
            }
#endif
            Sleep( 1000 );
        }
        if (!bAgain) {
            socketError = open_server_socket( SMTPHost, SMTPPort, defaultSMTPPort, "smtp" );
            if ( socketError )
                continue;

            if ( get_server_response( NULL, &enhancedStatusCode ) != 220 ) {
                close_server_socket();
                continue;
            }
        }

        // Changed to EHLO processing 27 Feb 2001 Craig Morrison
        sprintf( out_data, "EHLO %s",
                 (wanted_hostname==NULL) ? my_hostname : wanted_hostname);
        pStr = strrchr( Sender, '@' );
        if ( pStr ) {
            strcat( out_data, "." );
            strcat( out_data, pStr+1 );
            pStr = strchr( out_data, '>' );
            if ( pStr ) {
                *pStr = 0;
            }
        }
        strcat( out_data, "\r\n" );
        socketError = put_message_line( ServerSocket, out_data );
        if ( socketError ) {
            close_server_socket();
            bAgain = FALSE;
            continue;
        }

        if ( get_server_response( &responseStr, &enhancedStatusCode ) == 250 ) {
            char * index;

            index = responseStr.Get();  // The responses are already individually NULL terminated, with no CR or LF bytes.
            for ( ; *index; )
            {
                /* Parse the server responses for things we can utilize. */
                _strlwr( index );

                if ( (index[3] == '-') || (index[3] == ' ') ) {
#if ALLOW_PIPELINING
                    if ( memcmp(&index[4], "pipelining", 10) == 0 ) {
                        commandPipelining = TRUE;
                    } else
#endif
#if BLAT_LITE
#else
                    if ( memcmp(&index[4], "size ", 5) == 0 ) {
                        long maxSize = atol(&index[9]);
                        if ( maxSize > 0 )
                            maxMessageSize = (unsigned long)(maxSize / 10L) * 7L;
                    } else
                    if ( memcmp(&index[4], "dsn", 3) == 0 ) {
                        deliveryStatusSupported = TRUE;
                    } else
                    if ( memcmp(&index[4], "8bitmime", 8) == 0 ) {
                        eightBitMimeSupported = TRUE;
                    } else
                    if ( memcmp(&index[4], "binarymime", 10) == 0 ) {
                        binaryMimeSupported = 1;
                    } else
                    if ( memcmp(&index[4], "enhancedstatuscodes", 19) == 0 ) {
                        enhancedCodeSupport = TRUE;
                    } else
#endif
                    if ( (memcmp(&index[4], "auth", 4) == 0) &&
                         ((index[8] == ' ') || (index[8] == '=')) ) {
                        int  x, y;

                        for ( x = 9; ; ) {
                            for ( y = x; ; y++ ) {
                                if ( (index[y] == '\0') ||
                                     (index[y] == ',' ) ||
                                     (index[y] == ' ' ) )
                                    break;
                            }

                            if ( (y - x) == 5 ) {
                                if ( memcmp(&index[x], "plain", 5) == 0 )
                                    plainAuthSupported = TRUE;

                                if ( memcmp(&index[x], "login", 5) == 0 )
                                    loginAuthSupported = TRUE;
                            }

#if SUPPORT_GSSAPI
                            if ( (y - x) == 6 ) {
                                if ( memcmp(&index[x], "gssapi", 6) == 0 )
                                    gssapiAuthSupported = TRUE;
                            }
#endif
                            if ( (y - x) == 8 ) {
                                if ( memcmp(&index[x], "cram-md5", 8) == 0 ) {
                                    if ( !ByPassCRAM_MD5 )
                                        cramMd5AuthSupported = TRUE;
                                }
                            }

                            for ( x = y; ; x++ ) {
                                if ( (index[x] != ',') && (index[x] != ' ') )
                                    break;
                            }

                            if ( index[x] == '\0' )
                                break;
                        }
                    }
                }

                index += strlen(index) + 1;
            }
        } else {
            sprintf( out_data, "HELO %s",
                     (wanted_hostname==NULL) ? my_hostname : wanted_hostname);
            pStr = strrchr( Sender, '@' );
            if ( pStr ) {
                strcat( out_data, "." );
                strcat( out_data, pStr+1 );
                pStr = strchr( out_data, '>' );
                if ( pStr ) {
                    *pStr = 0;
                }
            }
            strcat( out_data, "\r\n" );
            if ( put_message_line( ServerSocket, out_data ) ) {
                close_server_socket();
                bAgain = FALSE;
                continue;
            }

            if ( get_server_response( NULL, &enhancedStatusCode ) != 250 ) {
                close_server_socket();
                bAgain = FALSE;
                continue;
            }

            loginAuthSupported = TRUE;  // no extended responses available,so assume "250-AUTH LOGIN"
#if SUPPORT_GSSAPI
            gssapiAuthSupported = TRUE; // might as well make the same assumption for gssapi
#endif
        }
        break;
    }

    if ( tryCount == no_of_try ) {
        if ( !socketError )
            server_error ("SMTP server error");
        return(-1);
    }

    return(0);
}

#if SUPPORT_GSSAPI

static int say_hello_again (const char* wanted_hostname)
{
    return say_hello(wanted_hostname, TRUE);
}
#endif


static void cram_md5( Buf &challenge, char str[] )
{
    md5_context   ctx;
    unsigned char k_ipad[65];    /* inner padding - key XORd with ipad */
    unsigned char k_opad[65];    /* outer padding - key XORd with opad */
    int           x;
    int           i;
    uint8         digest[16];
    Buf           decodedResponse;
    Buf           tmp;
    char          out_data[MAXOUTLINE];

    decodedResponse.Alloc( challenge.Length() + 65 );
    base64_decode( (const unsigned char *)challenge.Get(), decodedResponse.Get() );
    decodedResponse.SetLength();
    x = (int)strlen(AUTHPassword);
    tmp.Alloc( x + 65 );
    if ( *decodedResponse.Get() == '\"' ) {
        char * pStr = strchr( decodedResponse.Get()+1, '\"' );
        if ( pStr )
            *pStr = 0;

        strcpy( decodedResponse.Get(), decodedResponse.Get()+1 );
    }

    tmp.Add( AUTHPassword, x );
    for ( ; x % 64; x++ ) {
        tmp.Add( '\0' );             /* pad the password to a multiple of 64 bytes */
    }

    /* if key is longer than 64 bytes reset it to key=MD5(key) */
    if ( x > 64 ) {

        md5_context tctx;

        md5_starts( &tctx );
        md5_update( &tctx, (unsigned char *)tmp.Get(), x );
        md5_finish( &tctx, digest );

        memcpy( tmp.Get(), digest, x = 16 );
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
    memset( k_ipad, 0x36, 64 );
    memset( k_opad, 0x5C, 64 );
    for ( i = 0; i < x; i++ ) {
        k_ipad[i] ^= (unsigned char)*(tmp.Get()+i);
        k_opad[i] ^= (unsigned char)*(tmp.Get()+i);
    }
    /*
     * perform inner MD5
     */
    md5_starts( &ctx );                 /* initialize context for first pass */
    md5_update( &ctx, k_ipad, 64 );     /* start with inner pad */
    md5_update( &ctx, (unsigned char *)decodedResponse.Get(), decodedResponse.Length() );
                                        /* then text of datagram */
    md5_finish( &ctx, digest );         /* finish up 1st pass */
    /*
     * perform outer MD5
     */
    md5_starts( &ctx );                 /* init context for 2nd pass */
    md5_update( &ctx, k_opad, 64 );     /* start with outer pad */
    md5_update( &ctx, digest, 16 );     /* then results of 1st hash */
    md5_finish( &ctx, digest );         /* finish up 2nd pass */

    strcpy( out_data, AUTHLogin );
    x = (int)strlen( out_data );
    out_data[x++] = ' ';
    for ( i = 0; i < 16; i++ ) {
        sprintf( &out_data[x], "%02x", digest[i] );
        x +=2;
    }

    base64_encode( (const unsigned char *)out_data, x, str, FALSE);
    strcat(str, "\r\n");
}

static int authenticate_smtp_user(char* loginAuth, char* pwdAuth)
{
    int    enhancedStatusCode;
    char   out_data[MAXOUTLINE];
    char   str[1024];
    char * outstart;
    char * out;
    int    retval;

#if INCLUDE_POP3
    if ( xtnd_xmit_supported )
       return 0;
#endif

    /* NOW: auth */
#if SUPPORT_GSSAPI // Added 2003-11-07, Joseph Calzaretta
    if ( authgssapi ) {
        if (!gssapiAuthSupported) {
            server_error("The SMTP server will not accept AUTH GSSAPI value.");
            return(-2);
        }
        try{
            if (!pGss)
            {
                static GssSession TheSession;
                pGss = &TheSession;
            }

            char servname[1024] = "";
            if (*servicename)
                strcpy(servname,servicename);
            else
                sprintf(servname,"smtp@%s",SMTPHost);

            pGss->Authenticate(getline,putline,AUTHLogin,servname,mutualauth,gss_protection_level);

            if (pGss->GetProtectionLevel()!=GSSAUTH_P_NONE)
            {
                if (say_hello_again(my_hostname_wanted)!=0)
                    return (-5);
            }
        }
        catch (GssSession::GssException& e)
        {
            char szMsg[1024];
            sprintf(szMsg,"Cannot do AUTH GSSAPI: %s",e.message());
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

        if ( put_message_line( ServerSocket, "AUTH CRAM-MD5\r\n" ) )
            return(-1);

        if ( get_server_response( &response, &enhancedStatusCode ) == 334 ) {
            response1.Add( response.Get() + 4 );
            cram_md5( response1, str );

            if ( put_message_line( ServerSocket, str ) )
                return(-1);

            if ( get_server_response( NULL, &enhancedStatusCode ) == 235 )
                return(0);                  // cram-md5 authentication successful

            printMsg ("The SMTP server did not accept Auth CRAM-MD5 value.\n" \
                      "Are your login userid and password correct?\n");
        } else
            printMsg ("The SMTP server claimed CRAM-MD5, but did not accept Auth CRAM-MD5 request.\n");
    }
    if ( plainAuthSupported ) {

// The correct form of the AUTH PLAIN value is 'authid\0userid\0passwd'
// where '\0' is the null byte.

        strcpy(str, "AUTH PLAIN ");
        outstart = &str[11];
        out = &str[12];     // leave room for a leading NULL in place of the AuthID value.

        strcpy(out, loginAuth);
        while ( *out ) out++;

        out++;
        strcpy(out, pwdAuth);
        while ( *out ) out++;

        base64_encode((const unsigned char *)&str[11], (int)(out - outstart), outstart, FALSE);
        strcat(str, "\r\n");

        if ( put_message_line( ServerSocket, str ) )
            return(-1);

        if ( get_server_response( NULL, &enhancedStatusCode ) == 235 )
            return(0);                  // plain authentication successful

        printMsg ("The SMTP server did not accept Auth PLAIN value.\n" \
                  "Are your login userid and password correct?\n");
    }

/* At this point, authentication was requested, but not it did not match the
   server.  As a last resort, try AUTH LOGIN. */

#if ALLOW_PIPELINING
    {
        int retval;
        Buf response;

        if ( commandPipelining ) {
            strcpy( out_data, "AUTH LOGIN\r\n" );
            out = &out_data[12];

            base64_encode((const unsigned char *)loginAuth, lstrlen(loginAuth), out, FALSE);
            strcat( out, "\r\n" );
            while ( *out ) out++;

            base64_encode((const unsigned char *)pwdAuth, lstrlen(pwdAuth), out, FALSE);
            strcat( out, "\r\n" );

            if ( put_message_line( ServerSocket, out_data ) )
                return(-1);

            retval = get_server_response( &response, &enhancedStatusCode );
            if ( (retval != 334) && (retval != 235) ) {
                printMsg("The SMTP server does not require AUTH LOGIN.\n" \
                         "Are you sure server supports AUTH?\n");
                return(0);
                /*
                server_error ("The SMTP server does not require AUTH LOGIN.\n" \
                              "Are you sure server supports AUTH?");
                return(-2);
                 */
            }

            if ( (retval != 235) && (response.Get()[3] == ' ') ) {
                retval = get_server_response( &response, &enhancedStatusCode );
                if ( (retval != 334) && (retval != 235) ) {
                    server_error ("The SMTP server did not accept LOGIN name.");
                    return(-2);
                }
            }

            if ( (retval != 235) && (response.Get()[3] == ' ') ) {
                retval = get_server_response( &response, &enhancedStatusCode );
                if ( retval != 235 ) {
                    server_error ("The SMTP server did not accept Auth LOGIN PASSWD value.\n" \
                                  "Is your password correct?");
                    return(-2);
                }
            }
            return(0);
        }
    }
#endif

    if ( put_message_line( ServerSocket, "AUTH LOGIN\r\n" ) )
        return(-1);

    if ( get_server_response( NULL, &enhancedStatusCode ) != 334 ) {
        printMsg("The SMTP server does not require AUTH LOGIN.\n" \
                 "Are you sure server supports AUTH?\n");
        return(0);
        /*
        server_error ("The SMTP server does not require AUTH LOGIN.\n" \
                      "Are you sure server supports AUTH?");
        return(-2);
         */
    }

    base64_encode((const unsigned char *)loginAuth, lstrlen(loginAuth), out_data, FALSE);
    strcat(out_data,"\r\n");
    if ( put_message_line( ServerSocket, out_data ) )
        return(-1);

    retval = get_server_response( NULL, &enhancedStatusCode );
    if ( retval == 235 )
        return(0);

    if ( retval == 334 ) {
        base64_encode((const unsigned char *)pwdAuth, lstrlen(pwdAuth), out_data, FALSE);
        strcat(out_data,"\r\n");
        if ( put_message_line( ServerSocket, out_data ) )
            return(-1);

        if ( get_server_response( NULL, &enhancedStatusCode ) == 235 )
            return(0);

        printMsg ("The SMTP server did not accept Auth LOGIN PASSWD value.\n");
    }
    else
        printMsg ("The SMTP server did not accept LOGIN name.\n");

    finish_server_message();
    return(-2);
}


// 'destination' is the address the message is to be sent to

static int prepare_smtp_message( char * dest )
{
    int    enhancedStatusCode;
    int    yEnc_This;
    char   out_data[MAXOUTLINE];
    char * ptr;
    char * pp;
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
        server_error ("No email address was found for the sender name.\n" \
                      "Have you set your mail address correctly?");
        return(-2);
    }

#if SUPPORT_YENC
    yEnc_This = yEnc;
    if ( !eightBitMimeSupported && !binaryMimeSupported )
#endif
        yEnc_This = FALSE;

    if ( *ptr == '<' )
        sprintf (out_data, "MAIL FROM:%s", ptr);
    else
        sprintf (out_data, "MAIL FROM:<%s>", ptr);

#if BLAT_LITE
#else
    if ( binaryMimeSupported && yEnc_This )
        strcat (out_data, " BODY=BINARYMIME");
    else
        if ( eightBitMimeSupported && (eightBitMimeRequested || yEnc_This) )
            strcat (out_data, " BODY=8BITMIME");
#endif

    tmpBuf.Free();      // release the parsed login email address

    strcat (out_data, "\r\n" );
    if ( put_message_line( ServerSocket, out_data ) )
        return(-1);

    if ( get_server_response( NULL, &enhancedStatusCode ) != 250 ) {
        server_error ("The SMTP server does not like the sender name.\n" \
                      "Have you set your mail address correctly?");
        return(-2);
    }

    // do a series of RCPT lines for each name in address line
    parse_email_addresses (dest, tmpBuf);
    pp = tmpBuf.Get();
    if ( !pp ) {
        server_error ("No email address was found for recipients.\n" \
                      "Have you set the 'To:' field correctly?");
        return(-2);
    }

    errorsFound = recipientsSent = 0;
#if ALLOW_PIPELINING
    if ( commandPipelining ) {
        Buf outRcpts;
        int recipientsRcvd;

        for ( ptr = pp; *ptr; ptr += strlen(ptr) + 1 ) {
            recipientsSent++;
            outRcpts.Add( "RCPT TO:<" );
            outRcpts.Add( ptr );
            outRcpts.Add( '>' );

            if ( deliveryStatusRequested && deliveryStatusSupported ) {
                outRcpts.Add( " NOTIFY=" );
                if ( deliveryStatusRequested & DSN_NEVER )
                    outRcpts.Add( "NEVER" );
                else {
                    if ( deliveryStatusRequested & DSN_SUCCESS )
                        outRcpts.Add( "SUCCESS" );

                    if ( deliveryStatusRequested & DSN_FAILURE ) {
                        if ( deliveryStatusRequested & DSN_SUCCESS )
                            outRcpts.Add( ',' );

                        outRcpts.Add( "FAILURE" );
                    }

                    if ( deliveryStatusRequested & DSN_DELAYED ) {
                        if ( deliveryStatusRequested & (DSN_SUCCESS | DSN_FAILURE) )
                            outRcpts.Add( ',' );

                        outRcpts.Add( "DELAY" );
                    }
                }
            }

            outRcpts.Add( "\r\n" );
        }

        put_message_line( ServerSocket, outRcpts.Get() );
        recipientsRcvd = 0;
        for ( ptr = pp; recipientsRcvd < recipientsSent; ) {
            char * index;

            if ( get_server_response( &response, &enhancedStatusCode ) < 0 ) {
                errorsFound = recipientsSent;
                break;
            }

            index = response.Get(); // The responses are already individually NULL terminated, with no CR or LF bytes.
            for ( ; *index; ) {
                recipientsRcvd++;
                ret_temp = atoi( index );
                out_data[3] = 0;
                strcpy( out_data, index );

                if ( ( ret_temp != 250 ) && ( ret_temp != 251 ) ) {
                    printMsg( "The SMTP server does not like the name %s.\n" \
                              "Have you set the 'To:' field correctly, or do you need authorization (-u/-pw) ?\n", ptr);
                    printMsg( "The SMTP server response was -> %s\n", out_data );
                    errorsFound++;
                }

                if ( out_data[3] == ' ' )
                    break;

                index += strlen(index) + 1;
                ptr   += strlen(ptr) + 1;
            }
        }
    } else
#endif
    {
        for ( ptr = pp; *ptr; ptr += strlen(ptr) + 1 ) {
            sprintf(out_data, "RCPT TO:<%s>", ptr);

#if BLAT_LITE
#else
            if ( deliveryStatusRequested && deliveryStatusSupported ) {
                strcat(out_data, " NOTIFY=");
                if ( deliveryStatusRequested & DSN_NEVER )
                    strcat(out_data, "NEVER");
                else {
                    if ( deliveryStatusRequested & DSN_SUCCESS )
                        strcat(out_data, "SUCCESS");

                    if ( deliveryStatusRequested & DSN_FAILURE ) {
                        if ( deliveryStatusRequested & DSN_SUCCESS )
                            strcat(out_data, ",");

                        strcat(out_data, "FAILURE");
                    }

                    if ( deliveryStatusRequested & DSN_DELAYED ) {
                        if ( deliveryStatusRequested & (DSN_SUCCESS | DSN_FAILURE) )
                            strcat(out_data, ",");

                        strcat(out_data, "DELAY");
                    }
                }
            }
#endif
            strcat (out_data, "\r\n");
            put_message_line( ServerSocket, out_data );
            recipientsSent++;

            ret_temp = get_server_response( &response, &enhancedStatusCode );
            if ( ( ret_temp != 250 ) && ( ret_temp != 251 ) ) {
                printMsg( "The SMTP server does not like the name %s.\n" \
                          "Have you set the 'To:' field correctly, or do you need authorization (-u/-pw) ?\n", ptr);
                printMsg( "The SMTP server response was -> %s\n", response.Get() );
                errorsFound++;
            }
        }
    }

    tmpBuf.Free();      // release the parsed email addresses
    if ( errorsFound == recipientsSent ) {
        finish_server_message();
        return(-1);
    }

    if ( put_message_line( ServerSocket, "DATA\r\n" ) )
        return(-1);

    if ( get_server_response( NULL, &enhancedStatusCode ) != 354 ) {
        server_error ("SMTP server error accepting message data");
        return(-1);
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

static int prefetch_smtp_info(const char* wanted_hostname)
{
#if INCLUDE_POP3

    if ( POP3Login[0] || POP3Password[0] )
        if ( !POP3Host[0] )
            strcpy( POP3Host, SMTPHost );

    if ( POP3Host[0] ) {
        char saved_quiet;

        saved_quiet = quiet;
        quiet       = TRUE;

        if ( !open_server_socket( POP3Host, POP3Port, defaultPOP3Port, "pop3" ) ) {
            Buf  responseStr;

            for ( ; ; ) {
                if ( get_pop3_server_response( &responseStr ) )
                    break;

                if ( POP3Login[0] || AUTHLogin[0] ) {
                    responseStr = "USER ";
                    if ( POP3Login[0] )
                        responseStr.Add( POP3Login );
                    else
                        responseStr.Add( AUTHLogin );

                    responseStr.Add( "\r\n" );
                    if ( put_message_line( ServerSocket, responseStr.Get() ) )
                        break;

                    if ( get_pop3_server_response( &responseStr ) )
                        break;
                }
                if ( POP3Password[0] || AUTHPassword[0] ) {
                    responseStr = "PASS ";
                    if ( POP3Password[0] )
                        responseStr.Add( POP3Password );
                    else
                        responseStr.Add( AUTHPassword );

                    responseStr.Add( "\r\n" );
                    if ( put_message_line( ServerSocket, responseStr.Get() ) )
                        break;

                    if ( get_pop3_server_response( &responseStr ) )
                        break;
                }
                if ( put_message_line( ServerSocket, "STAT\r\n" ) )
                    break;

                if ( get_pop3_server_response( &responseStr ) )
                    break;

                if ( xtnd_xmit_wanted ) {
                    if ( put_message_line( ServerSocket, "XTND XMIT\r\n" ) )
                        break;

                    if ( get_pop3_server_response( &responseStr ) == 0 ) {
                        xtnd_xmit_supported = TRUE;
                        return 0;
                    }
                }

                break;
            }
            if ( !put_message_line( ServerSocket, "QUIT\r\n" ) )
                get_pop3_server_response( &responseStr );

            close_server_socket();

            if ( delayBetweenMsgs ) {
                printMsg("*** Delay %u seconds...\n", delayBetweenMsgs );
                Sleep( delayBetweenMsgs * 1000 );   // Delay user requested seconds.
            }
        }
        quiet = saved_quiet;
    }
#endif
#if INCLUDE_IMAP
    if ( IMAPHost[0] ) {
//        Buf  savedResponse;
        char saved_quiet;
        char imapLogin;
        char imapPlain;
        char imapCRAM;
        int  retVal;

        saved_quiet = quiet;
        quiet       = TRUE;
        imapLogin   = 0;
        imapPlain   = 0;
        imapCRAM    = 0;
        retVal      = 0;

        if ( !open_server_socket( IMAPHost, IMAPPort, defaultIMAPPort, "imap" ) ) {
            Buf    responseStr;
            char * pStr;
            char * pStr1;
            char * pStrEnd;

            for ( ; ; ) {
                retVal = get_imap_untagged_server_response( &responseStr );
                if ( retVal < 0 ) {
                    close_server_socket();
                    quiet = saved_quiet;
                    return( retVal );
                }

                _strlwr( responseStr.Get() );
                if ( !strstr( responseStr.Get(), " ok" ) )
                    break;

                pStr = strstr( responseStr.Get(), " capability" );
                if ( !pStr ) {
                    if ( put_message_line( ServerSocket, "a001 CAPABILITY\r\n" ) )
                        break;

                    retVal = get_imap_tagged_server_response( &responseStr, "a001" );
                    if ( retVal < 0 ) {
                        close_server_socket();
                        quiet = saved_quiet;
                        return( retVal );
                    }
                    _strlwr( responseStr.Get() );
                    pStr = strstr( responseStr.Get(), " capability" );
                    if ( !pStr )
                        break;
                }
//                savedResponse = responseStr;
                pStr1 = strstr( pStr, " auth=" );
                if ( pStr1 ) {
                    pStr = pStr1 + 6;
                    pStrEnd = strchr( pStr, ' ' );
                    if ( pStrEnd )
                        *pStrEnd = 0;

                    pStr = strtok( pStr, "," );
                    for ( ; ; ) {
                        if ( !pStr )
                            break;

                        if ( strcmp( pStr, "login" ) == 0 )
                            imapLogin = 1;
                        else
                        if ( strcmp( pStr, "plain" ) == 0 )
                            imapPlain = 1;
                        else
                        if ( strcmp( pStr, "cram-md5" ) == 0 )
                            imapCRAM = 1;

                        pStr = strtok( NULL, "," );
                    }
                } else {
                    pStr1 = strstr( pStr, "sasl" );
                    if ( pStr1 ) {
                        if ( strstr( pStr1, "login" ) == 0 )
                            imapLogin = 1;
                        else
                        if ( strstr( pStr1, "plain" ) == 0 )
                            imapPlain = 1;
                        else
                        if ( strstr( pStr1, "cram-md5" ) == 0 )
                            imapCRAM = 1;
                    } else
                         break;
                }

//                if ( strstr( savedResponse.Get(), "logindisabled" ) )
//                    imapLogin = 0;

                if ( IMAPLogin[0] || AUTHLogin[0] || IMAPPassword[0] || AUTHPassword[0] ) {
                    if ( imapCRAM ) {
                        Buf  challenge;
                        char str[1024];

                        responseStr = "a010 AUTHENTICATE \"CRAM-MD5\"";
                        retVal = get_imap_untagged_server_response( &responseStr );
                        if ( retVal < 0 ) {
                            close_server_socket();
                            quiet = saved_quiet;
                            return( retVal );
                        }
                        challenge.Add( responseStr.Get()+2 );
                        cram_md5( challenge, str );

                        if ( put_message_line( ServerSocket, str ) )
                            break;

                        retVal = get_imap_tagged_server_response( &responseStr, "a010" );
                        if ( retVal < 0 ) {
                            close_server_socket();
                            quiet = saved_quiet;
                            return( retVal );
                        }
                        _strlwr( responseStr.Get() );
                        if ( !strstr( responseStr.Get(), "a010 ok" ) )
                            break;
                    } else
                    if ( imapPlain ) {
                        responseStr = "a020 AUTHENTICATE PLAIN\r\n\0\"";
                        if ( IMAPLogin[0] )
                            responseStr.Add( IMAPLogin );
                        else
                            responseStr.Add( AUTHLogin );

                        responseStr.Add( "\"\0" );
                        if ( IMAPPassword[0] )
                            responseStr.Add( IMAPPassword );
                        else
                            responseStr.Add( AUTHPassword );

                        responseStr.Add( "\"\r\n" );
                        if ( put_message_line( ServerSocket, responseStr.Get() ) )
                            break;

                        retVal = get_imap_tagged_server_response( &responseStr, "a020" );
                        if ( retVal < 0 ) {
                            close_server_socket();
                            quiet = saved_quiet;
                            return( retVal );
                        }
                        _strlwr( responseStr.Get() );
                        if ( !strstr( responseStr.Get(), "a020 ok" ) )
                            break;
                    } else
                    if ( imapLogin ) {
                        responseStr = "a030 LOGIN \"";
                        if ( IMAPLogin[0] )
                            responseStr.Add( IMAPLogin );
                        else
                            responseStr.Add( AUTHLogin );

                        responseStr.Add( "\" \"" );
                        if ( IMAPPassword[0] )
                            responseStr.Add( IMAPPassword );
                        else
                            responseStr.Add( AUTHPassword );

                        responseStr.Add( "\"\r\n" );
                        if ( put_message_line( ServerSocket, responseStr.Get() ) )
                            break;

                        retVal = get_imap_tagged_server_response( &responseStr, "a030" );
                        if ( retVal < 0 ) {
                            close_server_socket();
                            quiet = saved_quiet;
                            return( retVal );
                        }
                        _strlwr( responseStr.Get() );
                        if ( !strstr( responseStr.Get(), "a030 ok" ) )
                            break;
                    }
                }
                break;
            }
            // if ( !put_message_line( ServerSocket, "a003 STATUS \"INBOX\" (MESSAGES UNSEEN)\r\n" ) )
            if ( !put_message_line( ServerSocket, "a097 SELECT \"INBOX\"\r\n" ) ) {
                retVal = get_imap_tagged_server_response( NULL, "a097" );
                if ( retVal < 0 ) {
                    close_server_socket();
                    quiet = saved_quiet;
                    return( retVal );
                }
            }

            if ( (IMAPLogin[0] || AUTHLogin[0] || IMAPPassword[0] || AUTHPassword[0]) && imapLogin ) {
                if ( !put_message_line( ServerSocket, "a098 LOGOUT\r\n" ) ) {
                    retVal = get_imap_tagged_server_response( NULL, "a098" );
                }
            } else
                if ( !put_message_line( ServerSocket, "a099 BYE\r\n" ) ) {
                    retVal = get_imap_tagged_server_response( NULL, "a099"  );
                }

            close_server_socket();
        }
        quiet = saved_quiet;
        if ( retVal < 0 )
            return( retVal );
    }
#endif

    return say_hello( wanted_hostname );
}


int send_email( int msgBodySize,
                Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                char * attachment_boundary, char * multipartID,
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
    char  * pp;
    int     namesFound, namesProcessed;
    int     serverOpen;
    BLDHDRS bldHdrs;

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

#if BLAT_LITE
#else
    binaryMimeSupported = 0;
#endif

#if INCLUDE_POP3
    xtnd_xmit_supported = FALSE;
#endif

    retcode = prefetch_smtp_info( my_hostname_wanted );
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
    }
    else
        if ( multipartSize && !disableMPS  )
            msgSize = (DWORD)multipartSize;

    getMaxMsgSize( TRUE, msgSize );

    if ( disableMPS && (totalsize > msgSize) ) {
        printMsg("Message is too big to send.  Please try a smaller message.\n" );
        finish_server_message();
        close_server_socket();
        serverOpen = 0;
        return 14;
    }

    int totalParts = (int)(totalsize / msgSize);
    if ( totalsize % msgSize )
        totalParts++;

    if ( totalParts > 1 ) {
        // send multiple messages, one attachment per message.
        int     attachNbr;
        DWORD   attachSize;
        int     attachType, prevAttachType;
        char *  attachName;
        int     thisPart, partsCount;
        DWORD   startOffset;

        if ( totalParts > 1 )
            printMsg("Sending %u parts for this message.\n", totalParts );

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

        n_of_try = noftry();
        for ( k = 1; k <= n_of_try || n_of_try == -1; k++ ) {
            retcode = 0;
            if ( !serverOpen )
                retcode = say_hello( my_hostname_wanted );

            if ( retcode == 0 ) {
                serverOpen = 1;
                retcode = authenticate_smtp_user( AUTHLogin, AUTHPassword );
            }

            if ( (retcode == 0) || (retcode == -2) )
                break;
        }

        for ( attachNbr = 0; attachNbr < nbrOfAttachments; attachNbr++ ) {
            if ( retcode )
                break;

            if ( attachNbr && delayBetweenMsgs ) {
                printMsg("*** Delay %u seconds...\n", delayBetweenMsgs );
                Sleep( delayBetweenMsgs * 1000 );   // Delay user requested seconds.
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
                if ( pp ) {
                    Buf holdingBuf;

                    for ( ; *pp; namesFound++ )
                        pp += strlen( pp ) + 1;
                }

                // send the message to the SMTP server!
                for ( namesProcessed = 0; namesProcessed < namesFound; ) {
                    Buf holdingBuf;

                    if ( namesProcessed && delayBetweenMsgs ) {
                        printMsg("*** Delay %u seconds...\n", delayBetweenMsgs );
                        Sleep( delayBetweenMsgs * 1000 );   // Delay user requested seconds.
                    }

                    if ( maxNames <= 0 )
                        holdingBuf.Add( Recipients );
                    else {
                        int x;

                        pp = tmpBuf.Get();
                        for ( x = 0; x < namesProcessed; x++ )
                            pp += strlen( pp ) + 1;

                        for ( x = 0; (x < maxNames) && *pp; x++ ) {
                            if ( x )
                                holdingBuf.Add( ',' );

                            holdingBuf.Add( pp );
                            pp += strlen( pp ) + 1;
                        }
                    }

                    // send the message to the SMTP server!
                    n_of_try = noftry();
                    for ( k=1; k<=n_of_try || n_of_try == -1; k++ ) {
                        if ( n_of_try > 1 )
                            printMsg("Try number %d of %d.\n", k, n_of_try);

                        if ( k > 1 ) Sleep(15000);

                        if ( 0 == retcode )
                            retcode = prepare_smtp_message( Recipients.Get() );

                        if ( 0 == retcode ) {
                            if ( partsCount > 1 )
                                printMsg("Sending part %u of %u\n", thisPart, partsCount );

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
                }
                startOffset += length;
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
            printMsg("Message is too big to send.  Please try a smaller message.\n" );
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

        build_headers( bldHdrs );
        retcode = add_message_body( messageBuffer, msgBodySize, multipartHdrs, TRUE,
                                    attachment_boundary, 0, 0, 0 );
        if ( retcode )
            return retcode;

        retcode = add_attachments( messageBuffer, TRUE, attachment_boundary, nbrOfAttachments );
        add_msg_boundary( messageBuffer, TRUE, attachment_boundary );

        if ( retcode )
            return retcode;

        namesFound = 0;
        parse_email_addresses (Recipients.Get(), tmpBuf);
        pp = tmpBuf.Get();
        if ( pp ) {
            for ( ; *pp; namesFound++ )
                pp += strlen( pp ) + 1;
        }

        // send the message to the SMTP server!
        for ( namesProcessed = 0; namesProcessed < namesFound; ) {
            Buf holdingBuf;

            if ( namesProcessed && delayBetweenMsgs ) {
                printMsg("*** Delay %u seconds...\n", delayBetweenMsgs );
                Sleep( delayBetweenMsgs * 1000 );   // Delay user requested seconds.
            }

            holdingBuf.Clear();
            if ( maxNames <= 0 )
                holdingBuf.Add( Recipients );
            else {
                int x;

                pp = tmpBuf.Get();
                for ( x = 0; x < namesProcessed; x++ )
                    pp += strlen( pp ) + 1;

                for ( x = 0; (x < maxNames) && *pp; x++ ) {
                    if ( x )
                        holdingBuf.Add( ',' );

                    holdingBuf.Add( pp );
                    pp += strlen( pp ) + 1;
                }
            }

            n_of_try = noftry();
            for ( k=1; k<=n_of_try || n_of_try == -1; k++ ) {
                if ( n_of_try > 1 )
                    printMsg("Try number %d of %d.\n", k, n_of_try);

                if ( k > 1 ) Sleep(15000);

                if ( !serverOpen )
                    retcode = say_hello( my_hostname_wanted );

                if ( 0 == retcode ) {
                    serverOpen = 1;
                    retcode = authenticate_smtp_user( AUTHLogin, AUTHPassword );
                }

                if ( 0 == retcode )
                    retcode = prepare_smtp_message( holdingBuf.Get() );

                if ( 0 == retcode ) {
#if INCLUDE_POP3
                    if ( xtnd_xmit_supported )
                        retcode = send_edit_data( messageBuffer.Get(), NULL );
                    else
#endif
                        retcode = send_edit_data( messageBuffer.Get(), 250, NULL );
                    if ( 0 == retcode ) {
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
                close_server_socket();
                serverOpen = 0;
            }

            if ( k > n_of_try ) {
                if ( maxNames <= 0 )
                    namesProcessed = namesFound;
                else
                    namesProcessed += maxNames;
            }
        }
    }

#if SUPPORT_SALUTATIONS
    salutation.Free();
#endif

    return(retcode);
}
