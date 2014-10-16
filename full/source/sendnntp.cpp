/*
    sendNNTP.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>

#include "blat.h"
#include "common_data.h"
#include "winfile.h"
#include "gensock.h"

#if INCLUDE_NNTP

extern int  open_server_socket( COMMON_DATA & CommonData, LPTSTR host, LPTSTR userPort, LPTSTR defaultPort, LPTSTR portName );
extern int  get_server_response( COMMON_DATA & CommonData, Buf * responseStr, int * enhancedStatusCode );
extern int  put_message_line( COMMON_DATA & CommonData, socktag sock, LPTSTR line );
extern int  finish_server_message( COMMON_DATA & CommonData );
extern int  close_server_socket( COMMON_DATA & CommonData );
extern void server_error( COMMON_DATA & CommonData, LPTSTR message );
extern void server_warning( COMMON_DATA & CommonData, LPTSTR message );
#ifdef BLATDLL_TC_WCX
extern int  send_edit_data (COMMON_DATA & CommonData, LPTSTR message, int expected_response, Buf * responseStr, DWORD attachmentSize );
#else
extern int  send_edit_data (COMMON_DATA & CommonData, LPTSTR message, int expected_response, Buf * responseStr );
#endif
extern int  noftry(COMMON_DATA & CommonData);
extern void build_headers( COMMON_DATA & CommonData, BLDHDRS &bldHdrs );
extern void convertPackedUnicodeToUTF( Buf & sourceText, Buf & outputText, int * utf, LPTSTR charset, int utfRequested );
  #if SUPPORT_MULTIPART
extern int  add_one_attachment( COMMON_DATA & CommonData,
                                Buf & messageBuffer, int buildSMTP, LPTSTR attachment_boundary,
                                DWORD startOffset, DWORD & length,
                                int part, int totalparts, int attachNbr, int * prevAttachType );
extern void getAttachmentInfo( COMMON_DATA & CommonData, int attachNbr, LPTSTR & attachName, DWORD & attachSize, int & attachType );
extern void getMaxMsgSize( COMMON_DATA & CommonData, int buildSMTP, DWORD & length );
  #endif
extern int  add_message_body( COMMON_DATA & CommonData,
                              Buf &messageBuffer, size_t msgBodySize, Buf &multipartHdrs, int buildSMTP,
                              LPTSTR attachment_boundary, DWORD startOffset, int part, int attachNbr );
extern int  add_attachments( COMMON_DATA & CommonData, Buf & messageBuffer, int buildSMTP, LPTSTR attachment_boundary, int nbrOfAttachments );
extern void add_msg_boundary( COMMON_DATA & CommonData, Buf & messageBuffer, int buildSMTP, LPTSTR attachment_boundary );

extern void printMsg(COMMON_DATA & CommonData, LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison

LPTSTR defaultNNTPPort = __T("119");


static int authenticate_nntp_user(COMMON_DATA & CommonData, LPTSTR loginAuth, LPTSTR pwdAuth)
{
    _TCHAR out_data[MAXOUTLINE];
    int    ret_temp;
    int    enhancedStatusCode;

    if ( loginAuth == NULL) {
        server_error (CommonData, __T("The NNTP server requires authentication,\n and you did not provide a userid.\n"));
        finish_server_message(CommonData);
        return(-2);
    }

    _stprintf(out_data, __T("AUTHINFO USER %s\r\n"), loginAuth);
    if ( put_message_line( CommonData, CommonData.ServerSocket, out_data ) ) {
        return(-1);
    }

    ret_temp = get_server_response( CommonData, NULL, &enhancedStatusCode );
    if ( ret_temp == 281 ) {
        return(0);   // authentication accepted.
    }

    if ( ret_temp != 381 ) {
        server_error (CommonData, __T("The NNTP server did not accept Auth userid/pwd value.\n"));
        finish_server_message(CommonData);
        return(-2);
    }

    _stprintf(out_data, __T("AUTHINFO PASS %s\r\n"), pwdAuth);
    if ( put_message_line( CommonData, CommonData.ServerSocket, out_data ) ) {
        return(-1);
    }

    ret_temp = get_server_response( CommonData, NULL, &enhancedStatusCode );
    if ( ret_temp != 281 ) {
        server_error (CommonData, __T("The NNTP server did not accept Auth userid/pwd value.\n"));
        finish_server_message(CommonData);
        return(-2);
    }

    return(0);
}


static int say_hello ( COMMON_DATA & CommonData, LPTSTR loginAuth, LPTSTR pwdAuth )
{
    int ret_temp;
    int enhancedStatusCode;

    if ( open_server_socket( CommonData, CommonData.NNTPHost, CommonData.NNTPPort, defaultNNTPPort, __T("nntp") ) )
        return(-1);

    ret_temp = get_server_response( CommonData, NULL, &enhancedStatusCode );
    if ( ret_temp == 201 ) {
        server_error (CommonData, __T("NNTP server does not allow posting\n"));
        finish_server_message(CommonData);
        return(-1);
    }

    if ( ret_temp == 480 ) {
        ret_temp = authenticate_nntp_user(CommonData, loginAuth, pwdAuth);
        if ( ret_temp )
            return (ret_temp);
    } else {
        if ( ret_temp != 200 ) {
            server_error (CommonData, __T("NNTP server error\n"));
            finish_server_message(CommonData);
            return(-1);
        }

        if ( loginAuth[0] ) {
            ret_temp = authenticate_nntp_user(CommonData, loginAuth, pwdAuth);
            if ( ret_temp )
                return (ret_temp);
        }
    }

    for ( ret_temp = 0; ret_temp != 200; ) {
        if ( put_message_line( CommonData, CommonData.ServerSocket, __T("MODE READER\r\n") ) ) {
            return(-1);
        }

        ret_temp = get_server_response( CommonData, NULL, &enhancedStatusCode );
        if ( ret_temp == 201 ) {
            server_error (CommonData, __T("NNTP server does not allow posting\n"));
            finish_server_message(CommonData);
            return(-1);
        }

        if ( ret_temp == 480 ) {
            ret_temp = authenticate_nntp_user(CommonData, loginAuth, pwdAuth);
            if ( ret_temp )
                return (ret_temp);
        } else {
            if ( ret_temp != 200 ) {
                server_error (CommonData, __T("NNTP server error\n"));
                finish_server_message(CommonData);
                return(-1);
            }
        }
    }

    return(0);
}


static int prepare_nntp_message( COMMON_DATA & CommonData, LPTSTR loginAuth, LPTSTR pwdAuth )
{
    int ret_temp;
    int enhancedStatusCode;

    for ( ret_temp = 0; ret_temp != 340; ) {
        if ( put_message_line( CommonData, CommonData.ServerSocket, __T("POST\r\n") ) ) {
            return(-1);
        }

        ret_temp = get_server_response( CommonData, NULL, &enhancedStatusCode );
        if ( ret_temp == 480 ) {
            ret_temp = authenticate_nntp_user(CommonData, loginAuth, pwdAuth);
            if ( ret_temp )
                return (ret_temp);
        } else {
            if ( ret_temp != 340 ) {
                server_error (CommonData, __T("NNTP server error\n"));
                finish_server_message(CommonData);
                return(-1);
            }
        }
    }

    return(0);
}


int send_news( COMMON_DATA & CommonData, size_t msgBodySize,
               Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
               LPTSTR attachment_boundary, LPTSTR multipartID,
               int nbrOfAttachments, DWORD totalsize )
{
    Buf     messageBuffer;
    Buf     multipartHdrs;
    Buf     varHeaders;
    Buf     header;
    int     n_of_try;
    int     retcode=0;
    int     k;
    _TCHAR  saved8bitSupport;
    _TCHAR  savedChunkingSupport;
    BLDHDRS bldHdrs;
    Buf     tmpBuf;


    if ( !CommonData.NNTPHost[0] || !CommonData.groups.Length() )
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
    bldHdrs.wanted_hostname       = CommonData.my_hostname_wanted;
    bldHdrs.server_name           = CommonData.NNTPHost;
    bldHdrs.nbrOfAttachments      = nbrOfAttachments;
    bldHdrs.addBccHeader          = FALSE;

    saved8bitSupport = CommonData.eightBitMimeSupported;
    savedChunkingSupport = CommonData.chunkingSupported;
/*
    if ( (nbrOfAttachments && CommonData.yEnc) || CommonData.eightBitMimeRequested ) {
        CommonData.eightBitMimeSupported = TRUE;
        CommonData.eightBitMimeRequested = FALSE;
        CommonData.binaryMimeSupported   = TRUE;
    } else
        CommonData.binaryMimeSupported   = FALSE;
 */
    CommonData.eightBitMimeSupported = TRUE;
    CommonData.eightBitMimeRequested = FALSE;
    CommonData.binaryMimeSupported   = 2;
    CommonData.chunkingSupported     = FALSE;

/*
    if ( CommonData.yEnc ) {
        CommonData.eightBitMimeSupported = TRUE;
        CommonData.eightBitMimeRequested = FALSE;
    }
 */

#if defined(_UNICODE) || defined(UNICODE)
    Buf tmpBuf2;
    int tmpUTF;

    tmpBuf.Clear();
    tmpBuf2 = CommonData.AUTHLogin;
    tmpUTF = NATIVE_16BIT_UTF;
    convertPackedUnicodeToUTF( tmpBuf2, tmpBuf, &tmpUTF, NULL, 8 );
    if ( tmpUTF )
        CommonData.AUTHLogin = tmpBuf;

    tmpBuf.Clear();
    tmpBuf2 = CommonData.AUTHPassword;
    tmpUTF = NATIVE_16BIT_UTF;
    convertPackedUnicodeToUTF( tmpBuf2, tmpBuf, &tmpUTF, NULL, 8 );
    if ( tmpUTF )
        CommonData.AUTHPassword = tmpBuf;

    tmpBuf2.Free();
#endif

    if ( !CommonData.my_hostname[0] ) {
        // Do a quick open/close to get the local hostname before building headers.
        open_server_socket( CommonData, CommonData.NNTPHost, CommonData.NNTPPort, defaultNNTPPort, __T("nntp") );
        close_server_socket(CommonData);
    }

  #if SUPPORT_MULTIPART
    DWORD msgSize;
    DWORD totalParts;

    if ( CommonData.multipartSize )
        msgSize = CommonData.multipartSize;
    else
        msgSize = (DWORD)(-1);

    getMaxMsgSize( CommonData, FALSE, msgSize );

    if ( CommonData.disableMPS && (totalsize > msgSize) ) {
        server_warning(CommonData, __T("Message is too big to send.  Please try a smaller message.\n") );
        finish_server_message(CommonData);
        close_server_socket(CommonData);
        tmpBuf.Free();
        header.Free();
        varHeaders.Free();
        multipartHdrs.Free();
        messageBuffer.Free();
        return 14;
    }

    totalParts = ((totalsize + msgSize - 1) / msgSize);

    if ( (totalParts > 1) || (nbrOfAttachments > 1) || (CommonData.yEnc && nbrOfAttachments) ) {
        // send multiple messages, one attachment per message.
        int     attachNbr;
        DWORD   attachSize;
        int     attachType, prevAttachType;
        LPTSTR  attachName;
        int     thisPart, partsCount;
        DWORD   startOffset;

        if ( totalParts > 1 )
            printMsg( CommonData, __T("Sending %lu parts for this message.\n"), totalParts );

        if ( !CommonData.yEnc ) {
            CommonData.mime     = FALSE;
            CommonData.base64   = FALSE;
            CommonData.uuencode = TRUE;
        }

        n_of_try = noftry(CommonData);
        for ( k = 1; (k <= n_of_try) || (n_of_try == -1); k++ ) {
            retcode = say_hello( CommonData, CommonData.AUTHLogin.Get(), CommonData.AUTHPassword.Get() );
            if ( (retcode == 0) || (retcode == -2) )
                break;
        }

        prevAttachType = -1;
        for ( attachNbr = 0; attachNbr < nbrOfAttachments; attachNbr++ ) {
            if ( retcode )
                break;

            getAttachmentInfo( CommonData, attachNbr, attachName, attachSize, attachType );
            partsCount = (int)(attachSize / msgSize);
            if ( attachSize % msgSize )
                partsCount++;

            header.Clear();
            startOffset = 0;
            CommonData.boundaryPosted = FALSE;
            for ( thisPart = 0; thisPart < partsCount; ) {
                DWORD length;

                if ( retcode )
                    break;

                bldHdrs.part       = ++thisPart;
                bldHdrs.totalparts = partsCount;
                bldHdrs.attachNbr  = attachNbr;
                bldHdrs.attachName = attachName;
                bldHdrs.attachSize = attachSize;
                build_headers( CommonData, bldHdrs );

                retcode = add_message_body( CommonData, messageBuffer, msgBodySize, multipartHdrs, FALSE,
                                            attachment_boundary, startOffset, thisPart, attachNbr );
                if ( retcode ) {
                    tmpBuf.Free();
                    header.Free();
                    varHeaders.Free();
                    multipartHdrs.Free();
                    messageBuffer.Free();
                    return retcode;
                }
                msgBodySize = 0;    // Do not include the body in subsequent messages.

                length = msgSize;
                if ( length > (attachSize - startOffset) )
                    length = attachSize - startOffset;

                retcode = add_one_attachment( CommonData, messageBuffer, FALSE, attachment_boundary,
                                              startOffset, length, thisPart, partsCount,
                                              attachNbr, &prevAttachType );
                if ( thisPart == partsCount )
                    add_msg_boundary( CommonData, messageBuffer, FALSE, attachment_boundary );

                multipartHdrs.Clear();
                if ( retcode ) {
                    tmpBuf.Free();
                    header.Free();
                    varHeaders.Free();
                    multipartHdrs.Free();
                    messageBuffer.Free();
                    return retcode;
                }
                // send the message to the NNTP server!
                n_of_try = noftry(CommonData);
                for ( k = 1; (k <= n_of_try) || (n_of_try == -1); k++ ) {
                    if ( n_of_try > 1 )
                        printMsg(CommonData, __T("Try number %d of %d.\n"), k, n_of_try);

                    if ( k > 1 ) Sleep(15000);

                    retcode = prepare_nntp_message( CommonData, CommonData.AUTHLogin.Get(), CommonData.AUTHPassword.Get() );
                    if ( 0 == retcode ) {
#ifdef BLATDLL_TC_WCX
                        retcode = send_edit_data( CommonData, messageBuffer.Get(), 240, NULL, totalsize - msgBodySize );
#else
                        retcode = send_edit_data( CommonData, messageBuffer.Get(), 240, NULL );
#endif
                        if ( 0 == retcode ) {
                            n_of_try = 1;
                            k = 2;
                        }
                    } else if ( -2 == retcode ) {
                        // This wasn't a connection error.  The server actively denied our connection.
                        // Stop trying to send messages.
                        n_of_try = 1;
                        k = 2;
                    }
                }
                startOffset += length;
            }
        }
        finish_server_message(CommonData);
        close_server_socket(CommonData);
    } else
  #else
    multipartID = multipartID;
    totalsize   = totalsize;
  #endif
    {
        // send a single message.
        CommonData.boundaryPosted = FALSE;

        bldHdrs.part             = 0;
        bldHdrs.totalparts       = 0;
        bldHdrs.attachNbr        = 0;
        bldHdrs.nbrOfAttachments = 0;
        bldHdrs.attachSize       = 0;
        bldHdrs.attachName       = NULL;

        build_headers( CommonData, bldHdrs );
        retcode = add_message_body( CommonData, messageBuffer, msgBodySize, multipartHdrs, FALSE,
                                    attachment_boundary, 0, 0, 0 );
        if ( retcode ) {
            CommonData.eightBitMimeSupported = saved8bitSupport;
            CommonData.chunkingSupported = savedChunkingSupport;
            tmpBuf.Free();
            header.Free();
            varHeaders.Free();
            multipartHdrs.Free();
            messageBuffer.Free();
            return retcode;
        }

        retcode = add_attachments( CommonData, messageBuffer, FALSE, attachment_boundary, nbrOfAttachments );
        add_msg_boundary( CommonData, messageBuffer, FALSE, attachment_boundary );

        CommonData.eightBitMimeSupported = saved8bitSupport;

        if ( retcode ) {
            tmpBuf.Free();
            header.Free();
            varHeaders.Free();
            multipartHdrs.Free();
            messageBuffer.Free();
            return retcode;
        }
        // send the message to the NNTP server!
        n_of_try = noftry(CommonData);
        for ( k = 1; (k <= n_of_try) || (n_of_try == -1); k++ ) {
            if ( n_of_try > 1 )
                printMsg(CommonData, __T("Try number %d of %d.\n"), k, n_of_try);

            if ( k > 1 ) Sleep(15000);

            retcode = say_hello( CommonData, CommonData.AUTHLogin.Get(), CommonData.AUTHPassword.Get() );
            if ( 0 == retcode )
                retcode = prepare_nntp_message( CommonData, CommonData.AUTHLogin.Get(), CommonData.AUTHPassword.Get() );

            if ( 0 == retcode ) {
#ifdef BLATDLL_TC_WCX
                retcode = send_edit_data( CommonData, messageBuffer.Get(), 240, NULL, totalsize - msgBodySize );
#else
                retcode = send_edit_data( CommonData, messageBuffer.Get(), 240, NULL );
#endif
                if ( 0 == retcode ) {
                    finish_server_message(CommonData);
                    n_of_try = 1;
                    k = 2;
                }
            } else if ( -2 == retcode ) {
                // This wasn't a connection error.  The server actively denied our connection.
                // Stop trying to send mail.
                n_of_try = 1;
                k = 2;
            }
            close_server_socket(CommonData);
        }
    }

    tmpBuf.Free();
    header.Free();
    varHeaders.Free();
    multipartHdrs.Free();
    messageBuffer.Free();

    return(retcode);
}
#endif
