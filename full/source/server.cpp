/*
    server.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "blat.h"

/* generic socket DLL support */
#include "gensock.h"

extern HMODULE gensock_lib;

extern void printMsg( LPTSTR p, ... );              // Added 23 Aug 2000 Craig Morrison

HANDLE  dll_module_handle = 0;
HMODULE gensock_lib = 0;

socktag ServerSocket = 0;

extern _TCHAR   Try[];
extern _TCHAR   debug;
extern _TCHAR   chunkingSupported;
#if INCLUDE_POP3
extern _TCHAR   xtnd_xmit_supported;
#endif

#define MY_HOSTNAME_SIZE    1024
_TCHAR my_hostname[MY_HOSTNAME_SIZE];


void gensock_error (LPTSTR function, int retval, LPTSTR hostname)
{
    switch ( retval ) {
    case ERR_CANT_MALLOC          : printMsg(__T("Error: Malloc failed (possibly out of memory).\n")); break;
    case ERR_SENDING_DATA         : printMsg(__T("Error: Error sending data.\n")); break;
    case ERR_INITIALIZING         : printMsg(__T("Error: Error initializing gensock.dll.\n")); break;
    case ERR_VER_NOT_SUPPORTED    : printMsg(__T("Error: Version not supported.\n")); break;
    case ERR_EINVAL               : printMsg(__T("Error: The winsock version specified by gensock is not supported by this winsock.dll.\n")); break;
    case ERR_SYS_NOT_READY        : printMsg(__T("Error: Network not ready.\n")); break;
    case ERR_CANT_RESOLVE_HOSTNAME: printMsg(__T("Error: Can't resolve hostname (%s).\n"), hostname); break;
    case ERR_CANT_GET_SOCKET      : printMsg(__T("Error: Can't create a socket (too many simultaneous links?)\n")); break;
    case ERR_READING_SOCKET       : printMsg(__T("Error: Error reading socket.\n")); break;
    case ERR_NOT_A_SOCKET         : printMsg(__T("Error: Not a socket.\n")); break;
    case ERR_BUSY                 : printMsg(__T("Error: Busy.\n")); break;
    case ERR_CLOSING              : printMsg(__T("Error: Error closing socket.\n")); break;
    case WAIT_A_BIT               : printMsg(__T("Error: Wait a bit (possible timeout).\n")); break;
    case ERR_CANT_RESOLVE_SERVICE : printMsg(__T("Error: Can't resolve service.\n")); break;
    case ERR_CANT_CONNECT         : printMsg(__T("Error: Can't connect to server (timed out if winsock.dll error 10060)\n")); break;
    case ERR_NOT_CONNECTED        : printMsg(__T("Error: Connection to server was dropped.\n")); break;
    case ERR_CONNECTION_REFUSED   : printMsg(__T("Error: Server refused connection.\n")); break;
    default:                        printMsg(__T("error %d in function '%s'\n"), retval, function);
    }
}

int open_server_socket( LPTSTR host, LPTSTR userPort, LPTSTR defaultPort, LPTSTR portName )
{
    int    retval;
    LPTSTR ptr;

    dll_module_handle = GetModuleHandle (NULL);

    ptr = _tcschr(host, __T(':'));
    if ( ptr ) {
        *ptr = __T('\0');
        _tcscpy(userPort, ptr+1);
    }

    retval = gensock_connect((LPTSTR) host,
                             (LPTSTR)((*userPort == 0) ? portName : userPort),
                             &ServerSocket);
    if ( retval ) {
        if ( retval == ERR_CANT_RESOLVE_SERVICE ) {
            retval = gensock_connect((LPTSTR)host,
                                     (LPTSTR)((*userPort == 0) ? defaultPort : userPort),
                                     &ServerSocket);
            if ( retval ) {
                gensock_error (__T("gensock_connect"), retval, host);
                return(-1);
            }
        } else { // error other than can't resolve service
            gensock_error (__T("gensock_connect"), retval, host);
            return(-1);
        }
    }

    // we wait to do this until here because WINSOCK is
    // guaranteed to be already initialized at this point.

    if ( !my_hostname[0] ) {
        // get the local hostname (needed by SMTP)
        retval = gensock_gethostname(my_hostname, MY_HOSTNAME_SIZE);
        if ( retval ) {
            gensock_error (__T("gensock_gethostname"), retval, host);
            return(-1);
        }
    }

    return(0);
}


int close_server_socket( void )
{
    int retval;

    if ( ServerSocket ) {
        retval = gensock_close(ServerSocket);
        ServerSocket = 0;

        if ( retval ) {
            gensock_error (__T("gensock_close"), retval, __T(""));
            return(-1);
        }

        if ( gensock_lib ) {
            FreeLibrary( gensock_lib );
            gensock_lib = 0;
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

int get_server_response( Buf * responseStr, int * validateResp  )
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
            retval = gensock_getchar(ServerSocket, 0, &ch);
            if ( retval ) {
                gensock_error (__T("gensock_getchar"), retval, __T(""));
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
                if ( debug ) printMsg(__T("<<<getline<<< %s\n"), in_data );
            } else {
                if ( debug ) printMsg(__T("Invalid server response received: %s\n"), in_data );
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
            if ( debug ) printMsg(__T("< <getline< < %s\n"), in_data );
        }

        /* append following lines, if there are some waiting to be received... */
        /* This is a polling call, and will return if no data is waiting.*/
        for ( x = 0; x < globaltimeout*4; x++ ) {
            retval = gensock_getchar(ServerSocket, 1, &ch);
            if ( retval != WAIT_A_BIT )
                break;

            // if ( debug ) printMsg(__T("\tgoing to sleep for a quarter of a second...zzzzz\n") );
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
                int x, y, z;
                _stscanf( received.Get() + 4, __T("%u.%u.%u"), &x, &y, &z );
                *validateResp = (x * 100) + (y * 10) + z;
            }
        }
    }
    received.Free();
    return(_tstoi(in_data));  // the RFCs say a multiline response is required to have the same value for each line.
}


#if SUPPORT_GSSAPI

// Added 2003-11-07, Joseph Calzaretta
//   overloading get_server_response so it CAN output to LPTSTR instead of Buf*
int get_server_response( LPTSTR responseStr )
{
    int enhancedStatusCode;
    int ret;
    Buf buf;

    ret = get_server_response( &buf, &enhancedStatusCode );
    if ( responseStr )
        _tcscpy(responseStr, buf.Get());

    buf.Free();
    return ret;
}
#endif

#if INCLUDE_POP3

int get_pop3_server_response( Buf * responseStr )
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
            retval = gensock_getchar(ServerSocket, 0, &ch);
            if ( retval ) {
                gensock_error (__T("gensock_getchar"), retval, __T(""));
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
        if ( debug ) printMsg(__T("<<<getline<<< %s\n"), in_data );
        if ( (memcmp( in_data, __T("+OK"),  3*sizeof(_TCHAR) ) == 0) ||
             (memcmp( in_data, __T("-ERR"), 4*sizeof(_TCHAR) ) == 0) ) {
            break;
        }

        /* append following lines, if there are some waiting to be received... */
        /* This is a polling call, and will return if no data is waiting.*/
        for ( x = 0; x < globaltimeout*4; x++ ) {
            retval = gensock_getchar(ServerSocket, 1, &ch);
            if ( retval != WAIT_A_BIT )
                break;

            // if ( debug ) printMsg(__T("\tgoing to sleep for a quarter of a second...zzzzz\n") );
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

int get_imap_untagged_server_response( Buf * responseStr  )
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
        printMsg(__T("in_data = %p, index = %p\n"), in_data, index );
 */
        while ( ch != __T('\n') ) {
            retval = gensock_getchar(ServerSocket, 0, &ch);
            if ( retval ) {
                gensock_error (__T("gensock_getchar"), retval, __T(""));
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
                _stprintf(str, __T(" %02X"), in_data[x] );
                textMsg.Add(str);
            }
            printMsg(__T("%s\n"), textMsg.Get() );
        }
 */
        received.Add( in_data );
        if ( debug ) printMsg(__T("<<<getline<<< %s\n"), in_data );
        if ( in_data[0] )
            break;

        /* append following lines, if there are some waiting to be received... */
        /* This is a polling call, and will return if no data is waiting.*/
        for ( x = 0; x < globaltimeout*4; x++ ) {
            retval = gensock_getchar(ServerSocket, 1, &ch);
            if ( retval != WAIT_A_BIT )
                break;

            // if ( debug ) printMsg(__T("\tgoing to sleep for a quarter of a second...zzzzz\n") );
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


int get_imap_tagged_server_response( Buf * responseStr, LPTSTR tag  )
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
        printMsg(__T("in_data = %p, index = %p\n"), in_data, index );
 */
        while ( ch != __T('\n') ) {
            retval = gensock_getchar(ServerSocket, 0, &ch);
            if ( retval ) {
                gensock_error (__T("gensock_getchar"), retval, __T(""));
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
                _stprintf(str, __T(" %02X"), in_data[x] );
                textMsg.Add(str);
            }
            printMsg(__T("%s\n"), textMsg.Get() );
            textMsg.Free();
        }
 */
        received.Add( in_data );
        if ( debug ) printMsg(__T("<<<getline<<< %s\n"), in_data );
        if ( memcmp( in_data, tag, _tcslen(tag)*sizeof(_TCHAR)) == 0 )
            break;

        /* append following lines, if there are some waiting to be received... */
        /* This is a polling call, and will return if no data is waiting.*/
        for ( x = 0; x < globaltimeout*4; x++ ) {
            retval = gensock_getchar(ServerSocket, 1, &ch);
            if ( retval != WAIT_A_BIT )
                break;

            // if ( debug ) printMsg(__T("\tgoing to sleep for a quarter of a second...zzzzz\n") );
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


int put_message_line( socktag sock, LPTSTR line )
{
    size_t nchars;
    int retval;

    nchars = _tcslen(line);

    retval = gensock_put_data(sock,
                              line,
                              (unsigned long) nchars);
    if ( retval ) {
        switch ( retval ) {
        case ERR_NOT_CONNECTED:
            gensock_error( __T("Server has closed the connection"), retval, __T("") );
            break;

        default:
            gensock_error (__T("gensock_put_data"), retval, __T(""));
        }
        return(-1);
    }

    if ( debug ) {
        printMsg(__T(">>>putline>>> %s\n"),line);
    }

    return(0);
}


int finish_server_message( void )
{
    int enhancedStatusCode;
    int ret;

    ret = put_message_line( ServerSocket, __T("QUIT\r\n") );

#if INCLUDE_POP3
    if ( xtnd_xmit_supported ) {
        // get_server_response( NULL );
        get_pop3_server_response( NULL );
    } else
#endif
    {
        // wait for a 221 message from the smtp server,
        // or a 205 message from the nntp server.
        get_server_response( NULL, &enhancedStatusCode );
    }
    return(ret);
}


void server_error( LPTSTR message)
{
    printMsg(__T("*** Error ***  %s\n"),message);
    finish_server_message();
}


int transform_and_send_edit_data( socktag sock, LPTSTR editptr )
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

    if ( chunkingSupported ) {
        retval = gensock_put_data_buffered(sock, editptr, msgLength);
    } else {
        msg_end = editptr + msgLength;
        index = editptr;

        header_end = _tcsstr (editptr, __T("\r\n\r\n"));
        if ( !header_end )
            header_end = msg_end;   // end of buffer

        while ( !retval && (index < msg_end) ) {
            // Loop through the data until all has been sent (index == msg_end).
            // Exit early if we encounter an error with the connection.

            if ( *index == __T('.') ) {
                if ( previous_char == __T('\n') ) {
                    /* send _two_ dots... */
                    retval = gensock_put_data_buffered(sock, index, 1);
                }
                if ( !retval ) {
                    retval = gensock_put_data_buffered(sock, index, 1);
                }
            } else
            if ( *index == __T('\r') ) {
                // watch for soft-breaks in the header, and ignore them
                if ( (index < header_end) && (memcmp(index, __T("\r\r\n"), 3*sizeof(_TCHAR)) == 0) )
                    index += 2;
                else
                if ( previous_char != __T('\r') ) {
                    retval = gensock_put_data_buffered(sock, index, 1);
                }
                // soft line-break (see EM_FMTLINES), skip extra CR */
            } else {
                if ( *index == __T('\n') )
                    if ( previous_char != __T('\r') ) {
                        retval = gensock_put_data_buffered(sock, __T("\r"), 1);
                        if ( retval )
                            break;
                    }

                retval = gensock_put_data_buffered(sock, index, 1);
            }

            previous_char = *index;
            index++;
        }
    }

    if ( !chunkingSupported &&  !retval ) {
        // no errors so far, finish up this block of data.
        // this handles the case where the user doesn't end the last
        // line with a <return>

        if ( previous_char != __T('\n') )
            retval = gensock_put_data_buffered(sock, __T("\r\n.\r\n"), 5);
        else
            retval = gensock_put_data_buffered(sock, __T(".\r\n"), 3);
    }
    /* now make sure it's all sent... */
    return (retval ? retval : gensock_put_data_flush(sock));
}


#if INCLUDE_POP3

int send_edit_data (LPTSTR message, Buf * responseStr )
{
    int retval;

    retval = transform_and_send_edit_data( ServerSocket, message );
    if ( retval )
        return(retval);

    if ( get_pop3_server_response( responseStr ) ) {
        server_error (__T("Message not accepted by server"));
        return(-1);
    }

    return(0);
}
#endif


int send_edit_data (LPTSTR message, int expected_response, Buf * responseStr )
{
    int enhancedStatusCode;
    int retval;

    retval = transform_and_send_edit_data( ServerSocket, message );
    if ( retval )
        return(retval);

    if ( get_server_response( responseStr, &enhancedStatusCode ) != expected_response ) {
        server_error (__T("Message not accepted by server"));
        return(-1);
    }

    return(0);
}


// Convert the entry "n of try" to a numeric, defaults to 1
int noftry()
{
    int n_of_try;
    int i;
    n_of_try = 0;

    for ( i = 0; i < (int)_tcslen(Try); i++ )
        Try[i] = (char) _totupper(Try[i]);

    if ( !_tcscmp(Try, __T("ONCE")    ) ) n_of_try = 1;
    if ( !_tcscmp(Try, __T("TWICE")   ) ) n_of_try = 2;
    if ( !_tcscmp(Try, __T("THRICE")  ) ) n_of_try = 3;
    if ( !_tcscmp(Try, __T("INFINITE")) ) n_of_try = -1;
    if ( !_tcscmp(Try, __T("-1")      ) ) n_of_try = -1;

    if ( n_of_try == 0 ) {
        i = _stscanf( Try, __T("%d"), &n_of_try );
        if ( !i )
            n_of_try = 1;
    }

    if ( n_of_try == 0 || n_of_try <= -2 )
        n_of_try = 1;

    return(n_of_try);
}
