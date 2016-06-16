/*
    server.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "blat.h"
#include "common_data.h"

/* generic socket DLL support */
#include "gensock.h"

extern void printMsg(COMMON_DATA & CommonData, LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison

#ifdef BLATDLL_TC_WCX
#ifdef BLATDLL_EXPORTS // this is blat.dll, not blat.exe
extern tProcessDataProcW pProcessDataProcW;
extern tProcessDataProc  pProcessDataProc;
#endif
#endif

int gensockGetLastError (COMMON_DATA & CommonData)
{
    return CommonData.lastServerSocketError;
}

void gensockSaveLastError (COMMON_DATA & CommonData, int retval)
{
    CommonData.lastServerSocketError = retval;
}

void gensock_error (COMMON_DATA & CommonData, LPTSTR function, int retval, LPTSTR hostname)
{
    gensockSaveLastError(CommonData, retval);

    switch ( retval ) {
    case ERR_CANT_MALLOC          : printMsg(CommonData, __T("Error: Malloc failed (possibly out of memory).\n")); break;
    case ERR_SENDING_DATA         : printMsg(CommonData, __T("Error: Error sending data.\n")); break;
    case ERR_INITIALIZING         : printMsg(CommonData, __T("Error: Error initializing gensock.dll.\n")); break;
    case ERR_VER_NOT_SUPPORTED    : printMsg(CommonData, __T("Error: Version not supported.\n")); break;
    case ERR_EINVAL               : printMsg(CommonData, __T("Error: The winsock version specified by gensock is not supported by this winsock.dll.\n")); break;
    case ERR_SYS_NOT_READY        : printMsg(CommonData, __T("Error: Network not ready.\n")); break;
    case ERR_CANT_RESOLVE_HOSTNAME: printMsg(CommonData, __T("Error: Can't resolve hostname (%s).\n"), hostname); break;
    case ERR_CANT_GET_SOCKET      : printMsg(CommonData, __T("Error: Can't create a socket (too many simultaneous links?)\n")); break;
    case ERR_READING_SOCKET       : printMsg(CommonData, __T("Error: Error reading socket.\n")); break;
    case ERR_NOT_A_SOCKET         : printMsg(CommonData, __T("Error: Not a socket.\n")); break;
    case ERR_BUSY                 : printMsg(CommonData, __T("Error: Busy.\n")); break;
    case ERR_CLOSING              : printMsg(CommonData, __T("Error: Error closing socket.\n")); break;
    case WAIT_A_BIT               : printMsg(CommonData, __T("Error: Wait a bit (possible timeout).\n")); break;
    case ERR_CANT_RESOLVE_SERVICE : printMsg(CommonData, __T("Error: Can't resolve service.\n")); break;
    case ERR_CANT_CONNECT         : printMsg(CommonData, __T("Error: Can't connect to server (timed out if winsock.dll error 10060)\n")); break;
    case ERR_NOT_CONNECTED        : printMsg(CommonData, __T("Error: Connection to server was dropped.\n")); break;
    case ERR_CONNECTION_REFUSED   : printMsg(CommonData, __T("Error: Server refused connection.\n")); break;
    default:                        printMsg(CommonData, __T("error %d in function '%s'\n"), retval, function);
    }
}

int open_server_socket( COMMON_DATA & CommonData, LPTSTR host, LPTSTR userPort, LPTSTR defaultPort, LPTSTR portName )
{
    int    retval;
    LPTSTR ptr;

    CommonData.dll_module_handle = GetModuleHandle (NULL);

    ptr = _tcschr(host, __T(':'));
    if ( ptr ) {
        *ptr = __T('\0');
        _tcscpy(userPort, ptr+1);
    }

    retval = gensock_connect(CommonData, (LPTSTR) host,
                             (LPTSTR)((*userPort == 0) ? portName : userPort),
                             &CommonData.ServerSocket);
    gensockSaveLastError(CommonData, retval);
    if ( retval ) {
        if ( retval == ERR_CANT_RESOLVE_SERVICE ) {
            retval = gensock_connect(CommonData, (LPTSTR)host,
                                     (LPTSTR)((*userPort == 0) ? defaultPort : userPort),
                                     &CommonData.ServerSocket);
            gensockSaveLastError(CommonData, retval);
            if ( retval ) {
                gensock_error (CommonData, __T("gensock_connect"), retval, host);
                return(-1);
            }
        } else { // error other than can't resolve service
            gensock_error (CommonData, __T("gensock_connect"), retval, host);
            return(-1);
        }
    }

    // we wait to do this until here because WINSOCK is
    // guaranteed to be already initialized at this point.

    if ( !CommonData.my_hostname.Get()[0] ) {
        CommonData.my_hostname.Alloc(MY_HOSTNAME_SIZE);
        // get the local hostname (needed by SMTP)
        retval = gensock_gethostname(CommonData.my_hostname.Get(), MY_HOSTNAME_SIZE);
        CommonData.my_hostname.SetLength();
        gensockSaveLastError(CommonData, retval);
        if ( retval ) {
            gensock_error (CommonData, __T("gensock_gethostname"), retval, host);
            return(-1);
        }
    }

    return(0);
}


int close_server_socket( COMMON_DATA & CommonData )
{
    int retval;

    if ( CommonData.ServerSocket ) {
        retval = gensock_close(CommonData, CommonData.ServerSocket);
        CommonData.ServerSocket = 0;
        gensockSaveLastError(CommonData, 0);

        if ( CommonData.gensock_lib ) {
            FreeLibrary( CommonData.gensock_lib );
            CommonData.gensock_lib = 0;
        }

        if ( retval ) {
            gensock_error (CommonData, __T("gensock_close"), retval, __T(""));
            return(-1);
        }
    }

    return(0);
}


/*
 * get_server_response() already knows how to test the first three digits for a response,
 * this is a snapshop of a session where enhanced status codes were available.
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
//#define _istdigit(c)  (((c) >= __T('0')) && ((c) <= __T('9')))


// validateResp pulls double duty.  If NULL, then the server response is not checked.
// If set to a pointer, then any enhanced status codes that are received get stored there.

int get_server_response( COMMON_DATA & CommonData, Buf * responseStr, int * validateResp  )
{
    _TCHAR ch;
    _TCHAR in_data [MAXOUTLINE];
    Buf    received;
    LPTSTR index;
    int    x;
    int    retval = 0;

// Comment: If the fourth byte of the received line is a hyphen,
//          then more lines are to follow.  If the fourth byte
//          is a space, then that is the last line.

// Minor change in 1.8.1 to allow multiline responses.
// Courtesy of Wolfgang Schwart <schwart@wmd.de>
// (the do{}while(!retval) and the if(!retval) are new...

    if ( responseStr ) {
        responseStr->Clear();
    }

    received.Clear();
    index = in_data;
    do {
        in_data[3] = __T('\0'); // Avoid a random "nnn-" being here and confusing the multiline detection below...
        ch = __T('.');          // Must reinitialize to avoid '\n' being here in the event of a multi-line response
        while ( ch != __T('\n') ) {
            retval = gensock_getchar(CommonData, CommonData.ServerSocket, 0, &ch);
            gensockSaveLastError(CommonData, retval);
            if ( retval ) {
                gensock_error (CommonData, __T("gensock_getchar"), retval, __T(""));
                received.Free();
                return(-1);
            }

            if ( ch == __T('\0') )
                continue;

            if ( index != &in_data[MAXOUTLINE - 1] ) {
                *index++ = ch;
            }
        }

        *index = __T('\0');  // terminate the string we received.
        if ( validateResp ) {
            if ( _istdigit(in_data[0]) &&
                 _istdigit(in_data[1]) &&
                 _istdigit(in_data[2]) &&
                 (in_data[3] == __T(' ')) || (in_data[3] == __T('-')) ) {
                received.Add( in_data );
                if ( CommonData.debug ) printMsg( CommonData, __T("<<<getline<<< %s\n"), in_data );
            } else {
                if ( CommonData.debug ) printMsg( CommonData, __T("Invalid server response received: %s\n"), in_data );
                in_data[3] = __T('\0');
            }

            /* Added in, version 1.8.8.  Multiline ESMTP responses (Tim Charron) */
            /* Look for 250- at the start of the response*/
            /* Actually look for xxx- at the start.  xxx can be any response code... */
            /* This typically an indicator of a multi-line ESMTP response.*/
            /* ((*(rowstart) == __T('2')) && (*(rowstart+1) == __T('5')) && (*(rowstart+2) == __T('0')) && (*(rowstart+3)== __T('-')) ) */

            if ( in_data[3] == __T(' ')) // If we have a space, this is the last line to be received.
                break;
        } else {
            received.Add( in_data );
            if ( CommonData.debug ) printMsg( CommonData, __T("< <getline< < %s\n"), in_data );
        }

        /* append following lines, if there are some waiting to be received... */
        /* This is a polling call, and will return if no data is waiting.*/
        for ( x = 0; x < CommonData.globaltimeout*4; x++ ) {
            retval = gensock_getchar(CommonData, CommonData.ServerSocket, 1, &ch);
            if ( retval != WAIT_A_BIT )
                break;

            // if ( CommonData.debug ) printMsg( CommonData, __T("\tgoing to sleep for a quarter of a second...zzzzz\n") );
            Sleep( 250 );
        }

        if ( retval )
            break;  // if no data is waiting, time to exit.

        index = in_data;
        if ( (ch != __T('\r')) && (ch != __T('\n')) )
            *index++ = ch;
    } while ( !retval );

    if ( responseStr ) {
        if (  received.Get() ) {
            for ( index = received.Get(); *index; index++ ) {
                if ( *index == __T('\r') )
                    continue;

                if ( *index == __T('\n') )
                    responseStr->Add( __T('\0') );
                else
                if ( *index == __T('\t') )
                    responseStr->Add( __T(' ') );
                else
                    responseStr->Add( index, 1 );
            }
        }
        responseStr->Add( __T('\0') );
        responseStr->Add( __T('\0') );
    }

    if ( validateResp ) {
        *validateResp = _tstoi(received.Get());

        if ( received.Length() > 9 ) {
            index = received.Get() + 4;
            if ( _istdigit(*index++) && (*index++ == __T('.')) &&
                 _istdigit(*index++) && (*index++ == __T('.')) &&
                 _istdigit(*index++) ) {
                unsigned hundredsDigit, tensDigit, onesDigit;
                _stscanf( received.Get() + 4, __T("%u.%u.%u"), &hundredsDigit, &tensDigit, &onesDigit );
                *validateResp = (hundredsDigit * 100) + (tensDigit * 10) + onesDigit;
            }
        }
    }
    received.Free();
    return(_tstoi(in_data));  // the RFCs say a multiline response is required to have the same value for each line.
}


#if SUPPORT_GSSAPI

// Added 2003-11-07, Joseph Calzaretta
//   overloading get_server_response so it CAN output to LPTSTR instead of Buf*
int get_server_response( COMMON_DATA & CommonData, LPTSTR responseStr )
{
    int enhancedStatusCode;
    int ret;
    Buf buf;

    ret = get_server_response( CommonData, &buf, &enhancedStatusCode );
    if ( responseStr )
        _tcscpy(responseStr, buf.Get());

    buf.Free();
    return ret;
}
#endif

#if INCLUDE_POP3

int get_pop3_server_response( COMMON_DATA & CommonData, Buf * responseStr )
{
    _TCHAR ch;
    _TCHAR in_data [MAXOUTLINE];
    Buf    received;
    LPTSTR index;
    int    x;
    int    retval = 0;


    if ( responseStr ) {
        responseStr->Clear();
    }

    received.Clear();
    index = in_data;
    in_data[0] = __T('\0'); // Avoid a random "+" being here and confusing the multiline detection below...
    do {
        ch = __T('.');      // Must reinitialize to avoid '\n' being here in the event of a multi-line response
        while ( ch != __T('\n') ) {
            retval = gensock_getchar(CommonData, CommonData.ServerSocket, 0, &ch);
            gensockSaveLastError(CommonData, retval);
            if ( retval ) {
                gensock_error (CommonData, __T("gensock_getchar"), retval, __T(""));
                received.Free();
                return(-1);
            }

            if ( !ch )
                continue;

            if ( index != &in_data[MAXOUTLINE - 1] ) {
                *index++ = ch;
            }
        }

        *index = __T('\0');  // terminate the string we received.
        received.Add( in_data );
        if ( CommonData.debug ) printMsg( CommonData, __T("<<<getline<<< %s\n"), in_data );
        if ( (memcmp( in_data, __T("+OK"),  3*sizeof(_TCHAR) ) == 0) ||
             (memcmp( in_data, __T("-ERR"), 4*sizeof(_TCHAR) ) == 0) ) {
            break;
        }

        /* append following lines, if there are some waiting to be received... */
        /* This is a polling call, and will return if no data is waiting.*/
        for ( x = 0; x < CommonData.globaltimeout*4; x++ ) {
            retval = gensock_getchar(CommonData, CommonData.ServerSocket, 1, &ch);
            gensockSaveLastError(CommonData, retval);
            if ( retval != WAIT_A_BIT )
                break;

            // if ( CommonData.debug ) printMsg( CommonData, __T("\tgoing to sleep for a quarter of a second...zzzzz\n") );
            Sleep( 250 );
        }

        if ( retval )
            break;  // if no data is waiting, time to exit.

        index = in_data;
        if ( (ch != __T('\r')) && (ch != __T('\n')) )
            *index++ = ch;
    } while ( !retval );

    if ( responseStr && received.Get() ) {
        for ( index = received.Get(); *index; index++ ) {
            if ( *index == __T('\r') )
                continue;

            if ( *index == __T('\n') )
                responseStr->Add( __T('\0') );
            else
            if ( *index == __T('\t') )
                responseStr->Add( __T(' ') );
            else
                responseStr->Add( index, 1 );
        }

        responseStr->Add( __T('\0') );
        responseStr->Add( __T('\0') );
    }

    if ( memcmp( in_data, __T("+OK"),  3*sizeof(_TCHAR) ) == 0 )
        retval = 0;
    else
    if ( memcmp( in_data, __T("-ERR"), 4*sizeof(_TCHAR) ) == 0 )
        retval = 1;
    else
        retval = 2;

    received.Free();
    return retval;
}
#endif

#if INCLUDE_IMAP

int get_imap_untagged_server_response( COMMON_DATA & CommonData, Buf * responseStr  )
{
    _TCHAR ch;
    _TCHAR in_data [MAXOUTLINE];
    Buf    received;
    LPTSTR index;
    int    x;
    int    retval = 0;

    if ( responseStr ) {
        responseStr->Clear();
    }

    received.Clear();
    index = in_data;
    in_data[0] = __T('\0');
    do {
        ch = __T('\0'); // Must reinitialize to avoid '\n' being here in the event of a multi-line response
/*
        printMsg( CommonData, __T("in_data = %p, index = %p\n"), in_data, index );
 */
        while ( ch != __T('\n') ) {
            retval = gensock_getchar(CommonData, CommonData.ServerSocket, 0, &ch);
            gensockSaveLastError(CommonData, retval);
            if ( retval ) {
                gensock_error (CommonData, __T("gensock_getchar"), retval, __T(""));
                received.Free();
                return(-1);
            }

            if ( !ch )
                continue;

            if ( index != &in_data[MAXOUTLINE - 1] ) {
                *index++ = ch;
            }
        }

        *index = __T('\0');  // terminate the string we received.
/*
        {
            _TCHAR str[4];
            Buf textMsg;
            int x;

            textMsg = __T("in_data =");
            for ( x = 0; index > &in_data[x]; x++ ) {
                _stprintf(str, __T(" %02") _TCHAR_PRINTF_FORMAT __T("X"), in_data[x] );
                textMsg.Add(str);
            }
            printMsg( CommonData, __T("%s\n"), textMsg.Get() );
        }
 */
        received.Add( in_data );
        if ( CommonData.debug ) printMsg( CommonData, __T("<<<getline<<< %s\n"), in_data );
        if ( in_data[0] )
            break;

        /* append following lines, if there are some waiting to be received... */
        /* This is a polling call, and will return if no data is waiting.*/
        for ( x = 0; x < CommonData.globaltimeout*4; x++ ) {
            retval = gensock_getchar(CommonData, CommonData.ServerSocket, 1, &ch);
            gensockSaveLastError(CommonData, retval);
            if ( retval != WAIT_A_BIT )
                break;

            // if ( CommonData.debug ) printMsg( CommonData, __T("\tgoing to sleep for a quarter of a second...zzzzz\n") );
            Sleep( 250 );
        }

        if ( retval )
            break;  // if no data is waiting, time to exit.

        index = in_data;
        if ( ch && (ch != __T('\r')) && (ch != __T('\n')) )
            *index++ = ch;
    } while ( !retval );

    if ( responseStr && received.Get() ) {
        for ( index = received.Get(); *index; index++ ) {
            if ( *index == __T('\r') )
                continue;

            if ( *index == __T('\n') )
                responseStr->Add( __T('\0') );
            else
            if ( *index == __T('\t') )
                responseStr->Add( __T(' ') );
            else
                responseStr->Add( index, 1 );
        }

        responseStr->Add( __T('\0') );
        responseStr->Add( __T('\0') );
    }

    received.Free();
    return (in_data[1] == __T(' '));
}


int get_imap_tagged_server_response( COMMON_DATA & CommonData, Buf * responseStr, LPTSTR tag  )
{
    _TCHAR ch;
    _TCHAR in_data [MAXOUTLINE];
    Buf    received;
    LPTSTR index;
    int    x;
    int    retval = 0;

    if ( responseStr ) {
        responseStr->Clear();
    }

    received.Clear();
    index = in_data;
    in_data[0] = __T('\0');
    do {
        ch = __T('\0'); // Must reinitialize to avoid '\n' being here in the event of a multi-line response
/*
        printMsg( CommonData, __T("in_data = %p, index = %p\n"), in_data, index );
 */
        while ( ch != __T('\n') ) {
            retval = gensock_getchar(CommonData, CommonData.ServerSocket, 0, &ch);
            gensockSaveLastError(CommonData, retval);
            if ( retval ) {
                gensock_error (CommonData, __T("gensock_getchar"), retval, __T(""));
                received.Free();
                return(-1);
            }

            if ( !ch )
                continue;

            if ( index != &in_data[MAXOUTLINE - 1] ) {
                *index++ = ch;
            }
        }

        *index = __T('\0');  // terminate the string we received.
/*
        {
            _TCHAR str[4];
            Buf textMsg;
            int x;

            textMsg = __T("in_data =");
            for ( x = 0; index > &in_data[x]; x++ ) {
                _stprintf(str, __T(" %02") _TCHAR_PRINTF_FORMAT __T("X"), in_data[x] );
                textMsg.Add(str);
            }
            printMsg( CommonData, __T("%s\n"), textMsg.Get() );
            textMsg.Free();
        }
 */
        received.Add( in_data );
        if ( CommonData.debug ) printMsg( CommonData, __T("<<<getline<<< %s\n"), in_data );
        if ( memcmp( in_data, tag, _tcslen(tag)*sizeof(_TCHAR)) == 0 )
            break;

        /* append following lines, if there are some waiting to be received... */
        /* This is a polling call, and will return if no data is waiting.*/
        for ( x = 0; x < CommonData.globaltimeout*4; x++ ) {
            retval = gensock_getchar(CommonData, CommonData.ServerSocket, 1, &ch);
            gensockSaveLastError(CommonData, retval);
            if ( retval != WAIT_A_BIT )
                break;

            // if ( CommonData.debug ) printMsg( CommonData, __T("\tgoing to sleep for a quarter of a second...zzzzz\n") );
            Sleep( 250 );
        }

        if ( retval )
            break;  // if no data is waiting, time to exit.

        index = in_data;
        if ( ch && (ch != __T('\r')) && (ch != __T('\n')) )
            *index++ = ch;
    } while ( !retval );

    if ( responseStr && received.Get() ) {
        for ( index = received.Get(); *index; index++ ) {
            if ( *index == __T('\r') )
                continue;

            if ( *index == __T('\n') )
                responseStr->Add( __T('\0') );
            else
            if ( *index == __T('\t') )
                responseStr->Add( __T(' ') );
            else
                responseStr->Add( index, 1 );
        }

        responseStr->Add( __T('\0') );
        responseStr->Add( __T('\0') );
    }

    received.Free();
    return ( (memcmp( in_data, tag, _tcslen(tag)*sizeof(_TCHAR)) != 0) );
}
#endif


int put_message_line( COMMON_DATA & CommonData, socktag sock, LPTSTR line )
{
    size_t nchars;
    int retval;

    retval = gensockGetLastError( CommonData );
    if ( retval == 0 ) {

        nchars = _tcslen(line);

        retval = gensock_put_data(CommonData,
                                  sock,
                                  line,
                                  (unsigned long) nchars);
        gensockSaveLastError(CommonData, retval);
        if ( retval ) {
            switch ( retval ) {
            case ERR_NOT_CONNECTED:
                gensock_error( CommonData, __T("Server has closed the connection"), retval, __T("") );
                break;

            default:
                gensock_error (CommonData, __T("gensock_put_data"), retval, __T(""));
            }
            return(-1);
        }

        if ( CommonData.debug ) {
            _TCHAR c;
            LPTSTR pStr;
            static _TCHAR maskedLine[] = __T(">>>putline>>> %s *****\n");

            if ( ((pStr = _tcsstr( line, __T("AUTH LOGIN ") )) != NULL) ||
                 ((pStr = _tcsstr( line, __T("AUTH PLAIN ") )) != NULL) ) {
                c = pStr[11];
                pStr[11] = __T('\0');
                printMsg(CommonData, maskedLine, line);
                pStr[11] = c;
            }
            else
            if ( (pStr = _tcsstr( line, __T("PASS ") )) != NULL ) {
                c = pStr[4];
                pStr[4] = __T('\0');
                printMsg(CommonData, maskedLine, line);
                pStr[4] = c;
            }
            else
                printMsg(CommonData, __T(">>>putline>>> %s\n"),line);
        }
    }
    else
        retval = -retval;

    return(retval);
}


int finish_server_message( COMMON_DATA & CommonData )
{
    int enhancedStatusCode;
    int ret;

    ret = gensockGetLastError( CommonData );
    if ( ret == 0 ) {

        ret = put_message_line( CommonData, CommonData.ServerSocket, __T("QUIT\r\n") );

#if INCLUDE_POP3
        if ( CommonData.xtnd_xmit_supported ) {
            // get_server_response( CommonData, NULL );
            get_pop3_server_response( CommonData, NULL );
        } else
#endif
        {
            // wait for a 221 message from the smtp server,
            // or a 205 message from the nntp server.
            get_server_response( CommonData, NULL, &enhancedStatusCode );
        }
    }
    else
        ret = -ret;

    return(ret);
}


void server_error( COMMON_DATA & CommonData, LPTSTR message )
{
    Buf      outString;
    _TCHAR * pChar;

    if ( message && (message[0] != __T('\0')) ) {
        outString.Clear();
        for ( ; ; ) {
            pChar = _tcschr( message, __T('\n') );
            if ( !pChar )
                break;

            outString.Add( __T("*** Error ***  ") );
            outString.Add( message, (&pChar[1] - message) );
            message = &pChar[1];
        }
        if ( message[0] != __T('\0') ) {
            outString.Add( __T("*** Error ***  ") );
            outString.Add( message );
        }
        if ( outString.Get() && *outString.Get() )
            printMsg( CommonData, outString.Get() );
        outString.Free();
    }
}


void server_warning( COMMON_DATA & CommonData, LPTSTR message)
{
    Buf      outString;
    _TCHAR * pChar;

    if ( message && (message[0] != __T('\0')) ) {
        outString.Clear();
        for ( ; ; ) {
            pChar = _tcschr( message, __T('\n') );
            if ( !pChar )
                break;

            outString.Add( __T("*** Warning ***  ") );
            outString.Add( message, (&pChar[1] - message) );
            message = &pChar[1];
        }
        if ( message[0] != __T('\0') ) {
            outString.Add( __T("*** Warning ***  ") );
            outString.Add( message );
        }
        if ( outString.Get() && *outString.Get() )
            printMsg( CommonData, outString.Get() );
        outString.Free();
    }
}


#ifdef BLATDLL_TC_WCX
int transform_and_send_edit_data( COMMON_DATA & CommonData, LPTSTR editptr, DWORD attachmentSize )
#else
int transform_and_send_edit_data( COMMON_DATA & CommonData, LPTSTR editptr )
#endif
{
    LPTSTR index;
    LPTSTR header_end;
    LPTSTR msg_end;
    _TCHAR previous_char;
    int    retval;
    DWORD  msgLength;

    previous_char = __T('\0');
    msgLength = (DWORD)_tcslen(editptr);
    retval = 0;

    if ( CommonData.chunkingSupported ) {
        retval = gensock_put_data_buffered(CommonData, CommonData.ServerSocket, editptr, msgLength);
        gensockSaveLastError(CommonData, retval);
    } else {
        msg_end = editptr + msgLength;
        index = editptr;

#ifdef BLATDLL_TC_WCX
        int currPos = 0;
        int lastPos = -1;
        LPTSTR initialIndex = index;
        LPTSTR lastIndex = index;
#endif

        header_end = _tcsstr (editptr, __T("\r\n\r\n"));
        if ( !header_end )
            header_end = msg_end;   // end of buffer

        while ( !retval && (index < msg_end) ) {
            // Loop through the data until all has been sent (index == msg_end).
            // Exit early if we encounter an error with the connection.

            if ( *index == __T('.') ) {
                if ( previous_char == __T('\n') ) {
                    /* send _two_ dots... */
                    retval = gensock_put_data_buffered(CommonData, CommonData.ServerSocket, index, 1);
                    gensockSaveLastError(CommonData, retval);
                }
                if ( !retval ) {
                    retval = gensock_put_data_buffered(CommonData, CommonData.ServerSocket, index, 1);
                    gensockSaveLastError(CommonData, retval);
                }
            } else
            if ( *index == __T('\r') ) {
                // watch for soft-breaks in the header, and ignore them
                if ( (index < header_end) && (memcmp(index, __T("\r\r\n"), 3*sizeof(_TCHAR)) == 0) )
                    index += 2;
                else
                if ( previous_char != __T('\r') ) {
                    retval = gensock_put_data_buffered(CommonData, CommonData.ServerSocket, index, 1);
                    gensockSaveLastError(CommonData, retval);
                }
                // soft line-break (see EM_FMTLINES), skip extra CR */
            } else {
                if ( *index == __T('\n') )
                    if ( previous_char != __T('\r') ) {
                        retval = gensock_put_data_buffered(CommonData, CommonData.ServerSocket, __T("\r"), 1);
                        gensockSaveLastError(CommonData, retval);
                        if ( retval )
                            break;
                    }

                retval = gensock_put_data_buffered(CommonData, CommonData.ServerSocket, index, 1);
                gensockSaveLastError(CommonData, retval);
            }

            previous_char = *index;

#ifdef BLATDLL_TC_WCX
#ifdef BLATDLL_EXPORTS // this is blat.dll, not blat.exe
            // call external progress function
            if ( !retval ) {
                if ( pProcessDataProcW || pProcessDataProc ) {
                    currPos = min( 100, MulDiv( 100, (int)(index - initialIndex), msgLength ) );
                    if (currPos > lastPos)  {
                        int size = MulDiv( (int)(index - lastIndex), attachmentSize, msgLength );
                        if ( pProcessDataProcW ) {
                            retval = (pProcessDataProcW( NULL, max( 0, size ) ) == 0) ? 20 : 0;
                        }
                        else
                        if ( pProcessDataProc ) {
                            retval = (pProcessDataProc( NULL, max( 0, size ) ) == 0) ? 20 : 0;
                        }
                        lastIndex = index;
                        lastPos = currPos;
                    }
                }
            }
#endif
#endif

            index++;
        }
    }

    if ( !CommonData.chunkingSupported &&  !retval ) {
        // no errors so far, finish up this block of data.
        // this handles the case where the user doesn't end the last
        // line with a <return>

        if ( previous_char != __T('\n') )
            retval = gensock_put_data_buffered(CommonData, CommonData.ServerSocket, __T("\r\n.\r\n"), 5);
        else
            retval = gensock_put_data_buffered(CommonData, CommonData.ServerSocket, __T(".\r\n"), 3);
        gensockSaveLastError(CommonData, retval);
    }
    /* now make sure it's all sent... */
    return (retval ? retval : gensock_put_data_flush(CommonData, CommonData.ServerSocket));
}


#if INCLUDE_POP3

#ifdef BLATDLL_TC_WCX
int send_edit_data (COMMON_DATA & CommonData, LPTSTR message, Buf * responseStr, DWORD attachmentSize )
#else
int send_edit_data (COMMON_DATA & CommonData, LPTSTR message, Buf * responseStr )
#endif
{
    int retval;

#ifdef BLATDLL_TC_WCX
    retval = transform_and_send_edit_data( CommonData, message, attachmentSize );
#else
    retval = transform_and_send_edit_data( CommonData, message );
#endif
    if ( retval )
        return(retval);

    if ( get_pop3_server_response( CommonData, responseStr ) ) {
        server_error (CommonData, __T("Message not accepted by server\n"));
        finish_server_message(CommonData);
        return(-1);
    }

    return(0);
}
#endif


#ifdef BLATDLL_TC_WCX
int send_edit_data ( COMMON_DATA & CommonData, LPTSTR message, int expected_response, Buf * responseStr, DWORD attachmentSize )
#else
int send_edit_data ( COMMON_DATA & CommonData, LPTSTR message, int expected_response, Buf * responseStr )
#endif
{
    int enhancedStatusCode;
    int retval;
    int serverResponse;

#ifdef BLATDLL_TC_WCX
    retval = transform_and_send_edit_data( CommonData, message, attachmentSize );
#else
    retval = transform_and_send_edit_data( CommonData, message );
#endif
    if ( retval )
        return(retval);

    serverResponse = get_server_response( CommonData, responseStr, &enhancedStatusCode );
    if ( serverResponse != expected_response ) {
        server_error (CommonData, __T("Message not accepted by server\n"));
        finish_server_message(CommonData);
        return(-1);
    }

    return(0);
}


// Convert the entry "n of try" to a numeric, defaults to 1
int noftry( COMMON_DATA & CommonData )
{
    int    n_of_try;
    int    i;

    n_of_try = 0;
    _tcsupr( CommonData.Try.Get() );

    if ( !_tcscmp(CommonData.Try.Get(), __T("ONCE")    ) ) n_of_try = 1;
    if ( !_tcscmp(CommonData.Try.Get(), __T("TWICE")   ) ) n_of_try = 2;
    if ( !_tcscmp(CommonData.Try.Get(), __T("THRICE")  ) ) n_of_try = 3;
    if ( !_tcscmp(CommonData.Try.Get(), __T("INFINITE")) ) n_of_try = -1;
    if ( !_tcscmp(CommonData.Try.Get(), __T("-1")      ) ) n_of_try = -1;

    if ( n_of_try == 0 ) {
        i = _stscanf( CommonData.Try.Get(), __T("%d"), &n_of_try );
        if ( !i )
            n_of_try = 1;
    }

    if ( n_of_try == 0 || n_of_try <= -2 )
        n_of_try = 1;

    return(n_of_try);
}
