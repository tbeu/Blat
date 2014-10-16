/*
    sendNNTP.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "blat.h"
#include "winfile.h"
#include "gensock.h"

#if INCLUDE_NNTP

extern int  open_server_socket( char *host, char *userPort, const char *defaultPort, char *portName );
extern int  get_server_response( Buf * responseStr, int * enhancedStatusCode );
extern int  put_message_line( socktag sock, const char * line );
extern int  finish_server_message( void );
extern int  close_server_socket( void );
extern void server_error (const char * message);
extern int  send_edit_data (char * message, int expected_response, Buf * responseStr );
extern int  noftry();
extern void build_headers( BLDHDRS &bldHdrs );
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

extern void printMsg(const char *p, ... );              // Added 23 Aug 2000 Craig Morrison

extern socktag ServerSocket;

extern char    NNTPHost[];
extern char    NNTPPort[];
extern Buf     groups;

extern char    AUTHLogin[];
extern char    AUTHPassword[];
extern char    mime;
extern char    base64;
extern char    uuencode;
extern char    yEnc;
extern char    boundaryPosted;

extern char    eightBitMimeSupported;
extern char    eightBitMimeRequested;
extern char    binaryMimeSupported;

  #if SUPPORT_MULTIPART
extern unsigned long multipartSize;
extern char          disableMPS;
  #endif

extern char    my_hostname[];
extern char    my_hostname_wanted[];

const char * defaultNNTPPort = "119";


static int authenticate_nntp_user(char* loginAuth, char* pwdAuth)
{
    char out_data[MAXOUTLINE];
    int  ret_temp;
    int  enhancedStatusCode;

    if ( loginAuth == NULL) {
        server_error ("The NNTP server requires authentication,\n and you did not provide a userid.");
        return(-2);
    }

    sprintf(out_data, "AUTHINFO USER %s\r\n", loginAuth);
    if ( put_message_line( ServerSocket, out_data ) ) {
        return(-1);
    }

    ret_temp = get_server_response( NULL, &enhancedStatusCode );
    if ( ret_temp == 281 ) {
        return(0);   // authentication accepted.
    }

    if ( ret_temp != 381 ) {
        server_error ("The NNTP server did not accept Auth userid/pwd value.");
        return(-2);
    }

    sprintf(out_data, "AUTHINFO PASS %s\r\n", pwdAuth);
    if ( put_message_line( ServerSocket, out_data ) ) {
        return(-1);
    }

    ret_temp = get_server_response( NULL, &enhancedStatusCode );
    if ( ret_temp != 281 ) {
        server_error ("The NNTP server did not accept Auth userid/pwd value.");
        return(-2);
    }

    return(0);
}


static int say_hello ( char* loginAuth, char* pwdAuth )
{
    int ret_temp;
    int enhancedStatusCode;

    if ( open_server_socket( NNTPHost, NNTPPort, defaultNNTPPort, "nntp" ) )
        return(-1);

    ret_temp = get_server_response( NULL, &enhancedStatusCode );
    if ( ret_temp == 201 ) {
        server_error ("NNTP server does not allow posting");
        return(-1);
    }

    if ( ret_temp == 480 ) {
        ret_temp = authenticate_nntp_user(loginAuth, pwdAuth);
        if ( ret_temp )
            return (ret_temp);
    } else {
        if ( ret_temp != 200 ) {
            server_error ("NNTP server error");
            return(-1);
        }

        if ( loginAuth[0] ) {
            ret_temp = authenticate_nntp_user(loginAuth, pwdAuth);
            if ( ret_temp )
                return (ret_temp);
        }
    }

    for ( ret_temp = 0; ret_temp != 200; ) {
        if ( put_message_line( ServerSocket, "MODE READER\r\n" ) ) {
            return(-1);
        }

        ret_temp = get_server_response( NULL, &enhancedStatusCode );
        if ( ret_temp == 201 ) {
            server_error ("NNTP server does not allow posting");
            return(-1);
        }

        if ( ret_temp == 480 ) {
            ret_temp = authenticate_nntp_user(loginAuth, pwdAuth);
            if ( ret_temp )
                return (ret_temp);
        } else {
            if ( ret_temp != 200 ) {
                server_error ("NNTP server error");
                return(-1);
            }
        }
    }

    return(0);
}


static int prepare_nntp_message( char* loginAuth, char* pwdAuth )
{
    int ret_temp;
    int enhancedStatusCode;

    for ( ret_temp = 0; ret_temp != 340; ) {
        if ( put_message_line( ServerSocket, "POST\r\n" ) ) {
            return(-1);
        }

        ret_temp = get_server_response( NULL, &enhancedStatusCode );
        if ( ret_temp == 480 ) {
            ret_temp = authenticate_nntp_user(loginAuth, pwdAuth);
            if ( ret_temp )
                return (ret_temp);
        } else {
            if ( ret_temp != 340 ) {
                server_error ("NNTP server error");
                return(-1);
            }
        }
    }

    return(0);
}


int send_news( int msgBodySize,
               Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
               char * attachment_boundary, char * multipartID,
               int nbrOfAttachments, DWORD totalsize )
{
    Buf     messageBuffer;
    Buf     multipartHdrs;
    Buf     varHeaders;
    Buf     header;
    int     n_of_try;
    int     retcode=0;
    int     k;
    char    saved8bitSupport;
    BLDHDRS bldHdrs;


    if ( !NNTPHost[0] || !groups.Length() )
        return(0);

    bldHdrs.messageBuffer         = &messageBuffer;
    bldHdrs.header                = &header;
    bldHdrs.varHeaders            = &varHeaders;
    bldHdrs.multipartHdrs         = &multipartHdrs;
    bldHdrs.buildSMTP             = FALSE;
    bldHdrs.lpszFirstReceivedData = &lpszFirstReceivedData;
    bldHdrs.lpszOtherHeader       = &lpszOtherHeader;
    bldHdrs.attachment_boundary   = attachment_boundary;
    bldHdrs.multipartID           = multipartID;
    bldHdrs.wanted_hostname       = my_hostname_wanted;
    bldHdrs.server_name           = NNTPHost;
    bldHdrs.nbrOfAttachments      = nbrOfAttachments;
    bldHdrs.addBccHeader          = FALSE;

    saved8bitSupport = eightBitMimeSupported;
/*
    if ( (nbrOfAttachments && yEnc) || eightBitMimeRequested ) {
        eightBitMimeSupported = TRUE;
        eightBitMimeRequested = FALSE;
        binaryMimeSupported = TRUE;
    }
    else
        binaryMimeSupported = FALSE;
 */
    eightBitMimeSupported = TRUE;
    eightBitMimeRequested = FALSE;
    binaryMimeSupported   = 2;

/*
    if ( yEnc ) {
        eightBitMimeSupported = TRUE;
        eightBitMimeRequested = FALSE;
    }
 */

    if ( !my_hostname[0] ) {
        // Do a quick open/close to get the local hostname before building headers.
        open_server_socket( NNTPHost, NNTPPort, defaultNNTPPort, "nntp" );
        close_server_socket();
    }

  #if SUPPORT_MULTIPART
    DWORD msgSize;

    if ( multipartSize )
        msgSize = (DWORD)multipartSize;
    else
        msgSize = (DWORD)(-1);

    getMaxMsgSize( FALSE, msgSize );

    if ( disableMPS && (totalsize > msgSize) ) {
        printMsg("Message is too big to send.  Please try a smaller message.\n" );
        finish_server_message();
        close_server_socket();
        return 14;
    }

    int totalParts = (int)(totalsize / msgSize);
    if ( totalsize % msgSize )
        totalParts++;

    if ( (totalParts > 1) || (nbrOfAttachments > 1) || (yEnc && nbrOfAttachments) ) {
        // send multiple messages, one attachment per message.
        int     attachNbr;
        DWORD   attachSize;
        int     attachType, prevAttachType;
        char *  attachName;
        int     thisPart, partsCount;
        DWORD   startOffset;

        if ( totalParts > 1 )
            printMsg("Sending %u parts for this message.\n", totalParts );

        if ( !yEnc ) {
            mime     = FALSE;
            base64   = FALSE;
            uuencode = TRUE;
        }

        n_of_try = noftry();
        for ( k = 1; k <= n_of_try || n_of_try == -1; k++ ) {
            retcode = say_hello( AUTHLogin, AUTHPassword );
            if ( (retcode == 0) || (retcode == -2) )
                break;
        }

        prevAttachType = -1;
        for ( attachNbr = 0; attachNbr < nbrOfAttachments; attachNbr++ ) {
            if ( retcode )
                break;

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

                retcode = add_message_body( messageBuffer, msgBodySize, multipartHdrs, FALSE,
                                            attachment_boundary, startOffset, thisPart, attachNbr );
                if ( retcode )
                    return retcode;

                msgBodySize = 0;    // Do not include the body in subsequent messages.

                length = msgSize;
                if ( length > (attachSize - startOffset) )
                    length = attachSize - startOffset;

                retcode = add_one_attachment( messageBuffer, FALSE, attachment_boundary,
                                              startOffset, length, thisPart, partsCount,
                                              attachNbr, &prevAttachType );
                if ( thisPart == partsCount )
                    add_msg_boundary( messageBuffer, FALSE, attachment_boundary );

                multipartHdrs.Clear();
                if ( retcode )
                    return retcode;

                // send the message to the NNTP server!
                n_of_try = noftry();
                for ( k=1; k<=n_of_try || n_of_try == -1; k++ ) {
                    if ( n_of_try > 1 )
                        printMsg("Try number %d of %d.\n", k, n_of_try);

                    if ( k > 1 ) Sleep(15000);

                    retcode = prepare_nntp_message( AUTHLogin, AUTHPassword );
                    if ( 0 == retcode ) {
                        retcode = send_edit_data( messageBuffer.Get(), 240, NULL );
                        if ( 0 == retcode ) {
                            n_of_try = 1;
                            k = 2;
                        }
                    } else if ( -2 == retcode ) {
                        // This wasn't a connection error.  The server actively denied our connection.
                        // Stop trying to send mail.
                        n_of_try = 1;
                        k = 2;
                    }
                }
                startOffset += length;
            }
        }
        finish_server_message();
        close_server_socket();
    } else
  #else
    multipartID = multipartID;
    totalsize   = totalsize;
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
        retcode = add_message_body( messageBuffer, msgBodySize, multipartHdrs, FALSE,
                                    attachment_boundary, 0, 0, 0 );
        if ( retcode ) {
            eightBitMimeSupported = saved8bitSupport;
            return retcode;
        }

        retcode = add_attachments( messageBuffer, FALSE, attachment_boundary, nbrOfAttachments );
        add_msg_boundary( messageBuffer, FALSE, attachment_boundary );

        eightBitMimeSupported = saved8bitSupport;

        if ( retcode )
            return retcode;

        // send the message to the NNTP server!
        n_of_try = noftry();
        for ( k=1; k<=n_of_try || n_of_try == -1; k++ ) {
            if ( n_of_try > 1 )
                printMsg("Try number %d of %d.\n", k, n_of_try);

            if ( k > 1 ) Sleep(15000);

            retcode = say_hello( AUTHLogin, AUTHPassword );
            if ( 0 == retcode )
                retcode = prepare_nntp_message( AUTHLogin, AUTHPassword );

            if ( 0 == retcode ) {
                retcode = send_edit_data( messageBuffer.Get(), 240, NULL );
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
        }
    }

    return(retcode);
}
#endif
