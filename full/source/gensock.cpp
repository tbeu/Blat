// -*- C++ -*-
// generic socket DLL, winsock version
// disclaimer:  a C programmer wrote this.

// $Id: gensock.cpp 1.15 1994/11/23 22:38:10 rushing Exp $

#include "declarations.h"

#ifdef __WATCOMC__
//    #define WINVER 0x0501
    #define WIN32
#endif

#include <tchar.h>
// #include <windows.h> Is included by the Winsock include files.
#include <stdio.h>
#include <ctype.h>

extern "C" {
#if (WINVER > 0x0500)
    #include <winsock2.h>
#else
    #include <winsock.h>
#endif
}
//#include <tchar.h>

#include "gensock.h"
#include "blat.h"
#include "punycode.h"
#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
#endif

#define MAX_PRINTF_LENGTH   1000
#define MAX_HOSTNAME_LENGTH  512

#ifdef SOCKET_BUFFER_SIZE
    #undef SOCKET_BUFFER_SIZE
#endif

/* This is for NT */
#if defined(WIN32) || defined(WIN64)

    #define SOCKET_BUFFER_SIZE  64240   // 1460 * 44

    extern HANDLE   dll_module_handle;

    #define GET_CURRENT_TASK    dll_module_handle
    #define TASK_HANDLE_TYPE    HANDLE

/* This is for WIN16 */
#else
    #define SOCKET_BUFFER_SIZE  32120   // 1460 * 22

    HINSTANCE dll_module_handle;
    #define GET_CURRENT_TASK    GetCurrentTask()
    #define TASK_HANDLE_TYPE    HTASK
#endif

int  init_winsock (void);
void deinit_winsock (void);

int  globaltimeout = 30;

#if INCLUDE_SUPERDEBUG
extern _TCHAR superDebug;
extern _TCHAR quiet;
#endif

extern int  noftry();
extern void printMsg(LPTSTR p, ... );

//
//
//

void complain (LPTSTR message)
{
#ifdef _DEBUG
    OutputDebugString (message);
#else
//  MessageBox (NULL, message, __T("GENSOCK.DLL Error"), MB_OK|MB_ICONHAND);
    printMsg( __T("%s\n"), message );
#endif
}

//
// ---------------------------------------------------------------------------
// container for a buffered SOCK_STREAM.

class connection {
private:
    SOCKET              the_socket;
    char *              pInBuffer;
    char *              pOutBuffer;
    unsigned int        in_index;
    unsigned int        out_index;
    unsigned int        in_buffer_total;
    unsigned int        out_buffer_total;
    unsigned int        last_winsock_error;
    int                 buffer_size;
    TASK_HANDLE_TYPE    owner_task;
    fd_set              fds;
    struct timeval      timeout;

public:

    connection (void);
    ~connection (void);

    int                 get_connected (LPTSTR hostname, LPTSTR service);
    SOCKET              get_socket(void) { return(the_socket);}
    TASK_HANDLE_TYPE    get_owner_task(void) { return(owner_task);}
    int                 get_buffer(int wait);
    int                 close (void);
    int                 getachar (int wait, LPTSTR ch);
    int                 put_data (LPTSTR pData, unsigned long length);
#if defined(_UNICODE) || defined(UNICODE)
    int                 put_data (char * pData, unsigned long length);
#endif
    int                 put_data_buffered (LPTSTR pData, unsigned long length);
    int                 put_data_flush (void);
};

connection::connection (void)
{
    the_socket       = 0;
    in_index         = 0;
    out_index        = 0;
    in_buffer_total  = 0;
    out_buffer_total = 0;
    pInBuffer        = 0;

    pInBuffer   = new char[SOCKET_BUFFER_SIZE];
    pOutBuffer  = new char[SOCKET_BUFFER_SIZE];
    buffer_size = SOCKET_BUFFER_SIZE;

    last_winsock_error = 0;
}

connection::~connection (void)
{
    delete [] pInBuffer;
    delete [] pOutBuffer;
}

int
     gensock_is_a_number (char * string)
{
    while ( *string ) {
        if ( !isdigit (*string) ) {
            return(0);
        }
        string++;
    }
    return(1);
}

#if defined(_UNICODE) || defined(UNICODE)

#define unicode_max_length  256
#define ace_max_length      256


static void convertHostnameUsingPunycode( LPTSTR _Thostname, char * punycodeName )
{
    Buf             tmpBuf;
    punycode_uint   input[unicode_max_length];
    punycode_uint   input_length;
    BYTE            case_flags[ace_max_length+1];
    _TCHAR          output[ace_max_length+1];
    punycode_uint   output_length;
    size_t          c;
    LPTSTR          there;
    LPTSTR          start;
    BYTE            needPunycode;
    Buf             finishedURL;
    _TCHAR          oldC;
    punycode_status status;


    /* Read the input code points: */

    tmpBuf = _Thostname;
    there  = tmpBuf.Get();
    start  = there;
    _tcslwr(there);
    finishedURL.Clear();
    needPunycode = 0;
    for (;;) {
        if ((*there == __T('\0')) || (*there == __T('.'))) {
            oldC = *there;
            *there = __T('\0');
            if ( needPunycode ) {
                input_length = 0;
                while ( start < there ) {
                    case_flags[input_length] = 0;
                    input[input_length] = (punycode_uint)*start;
                    if ((input[input_length] >= 0xD800) && (input[input_length] <= 0xDBFF) &&
                        (start[1]            >= 0xDC00) && (start[1]            <= 0xDFFF)) {
                        input[input_length] = 0x10000l + ((input[input_length] & 0x3FFl) << 10) + (start[1] & 0x3FFl);
                        start++;
                    }
                    input_length++;
                    start++;
                }
                /* Encode: */
                output_length = ace_max_length;
                status = punycode_encode(input_length, input, case_flags,
                                         &output_length, output);
                finishedURL.Add( __T("xn--") );
                finishedURL.Add( output, output_length );
                needPunycode = 0;
            } else
                finishedURL.Add( start );

            *there = oldC;
            if (*there == __T('\0'))
                break;

            finishedURL.Add( oldC );
            start = there + 1;
        } else
        if ( *there > 0x7F )
            needPunycode = 1;

        there++;
    }
    there = finishedURL.Get();
    c = 0;
    do {
        punycodeName[c] = (char)(there[c] & 0x0FF);
        if ( punycodeName[c] == '\0' )
            break;

    } while ( ++c < 511 );
    punycodeName[511] = '\0';

    finishedURL.Free();
    tmpBuf.Free();
}

#endif
//
// ---------------------------------------------------------------------------
//

int
     connection::get_connected (LPTSTR _Thostname, LPTSTR _Tservice)
{
    struct hostent FAR * hostentry;
    struct servent FAR * serventry;
    unsigned long        ip_address;
    char FAR *           ip_list[2];
    char FAR * FAR *     ip_list_ptr;
    struct sockaddr_in   sa_in;
    unsigned short       our_port;
    int                  dummyVal = 0;
    int                  retval, err_code;
    unsigned int         maxMsgSize;
    unsigned long        ioctl_blocking = 1;
    _TCHAR               message[80];
    int                  tryCount;
    char                 service[MAX_HOSTNAME_LENGTH];
    char                 hostname[MAX_HOSTNAME_LENGTH];

    // if the ctor couldn't get a buffer
    if ( !pInBuffer || !pOutBuffer )
        return(ERR_CANT_MALLOC);

    // --------------------------------------------------
    // resolve the service name
    //

#if defined(_UNICODE) || defined(UNICODE)
    convertHostnameUsingPunycode( _Thostname, hostname );
#else
    strncpy( hostname, _Thostname, sizeof(hostname)-1 );
    hostname[sizeof(hostname)] = '\0';
#endif

    for ( dummyVal = 0; _Tservice[dummyVal] && (dummyVal < (sizeof(service)-1)); dummyVal++ ) {
        service[dummyVal] = (char)(_Tservice[dummyVal] & 0xFF);
    }
    service[dummyVal] = '\0';

    dummyVal = 0;
    // If they've specified a number, just use it.
    if ( gensock_is_a_number (service) ) {
        char * tail;
        our_port = (unsigned short) strtol (service, &tail, 10);
        if ( tail == service ) {
            return(ERR_CANT_RESOLVE_SERVICE);
        }
        our_port = htons (our_port);
    } else {
        // we have a name, we must resolve it.
        serventry = getservbyname (service, "tcp");

        if ( serventry )
            our_port = (unsigned short)serventry->s_port;
        else {
            retval = WSAGetLastError();
            // Chicago beta is throwing a WSANO_RECOVERY here...
            if ( (retval == WSANO_DATA) || (retval == WSANO_RECOVERY) ) {
                return(ERR_CANT_RESOLVE_SERVICE);
            }

            return(retval - 5000);
        }
    }

    // --------------------------------------------------
    // resolve the hostname/ipaddress
    //

    if ( (ip_address = inet_addr (hostname)) != INADDR_NONE ) {
        ip_list[0]  = (char FAR *)&ip_address;
        ip_list[1]  = NULL;
        ip_list_ptr = ip_list;
    } else {
        if ( (hostentry = gethostbyname(hostname)) == NULL ) {
#if INCLUDE_SUPERDEBUG
            if ( superDebug ) {
                _TCHAR savedQuiet = quiet;
                quiet = FALSE;
                printMsg( __T("superDebug: Cannot resolve hostname <%s> to ip address.\n"), _Thostname );
                quiet = savedQuiet;
            }
#endif
            return(ERR_CANT_RESOLVE_HOSTNAME);
        }
        ip_list_ptr = hostentry->h_addr_list;
        sa_in.sin_addr.s_addr = *(ULONG far *)hostentry->h_addr;
#if INCLUDE_SUPERDEBUG
        if ( superDebug ) {
            Buf      _tprtline;
            unsigned tx;
            _TCHAR   savedQuiet = quiet;

            quiet = FALSE;
            printMsg( __T("superDebug: Hostname <%s> resolved to ip address %hhu.%hhu.%hhu.%hhu\n"),
                      _Thostname, sa_in.sin_addr.S_un.S_un_b.s_b1,
                                  sa_in.sin_addr.S_un.S_un_b.s_b2,
                                  sa_in.sin_addr.S_un.S_un_b.s_b3,
                                  sa_in.sin_addr.S_un.S_un_b.s_b4 );
            _tprtline.Clear();
            for ( tx = 0; hostentry->h_name[tx]; tx++ )
                _tprtline.Add( hostentry->h_name[tx] );
            _tprtline.Add( __T('\0') );
            printMsg( __T("superDebug: Official hostname is %s\n"), _tprtline.Get() );
            _tprtline.Free();
            quiet = savedQuiet;
        }
#endif
    }


    retval = 0; // ~SOCKET_ERROR;

    for ( ; *ip_list_ptr; ip_list_ptr++ ) {

        sa_in.sin_addr.s_addr = *(u_long FAR *)*ip_list_ptr;

#if INCLUDE_SUPERDEBUG
        if ( superDebug ) {
            _TCHAR savedQuiet = quiet;
            quiet = FALSE;
            printMsg( __T("superDebug: Attempting to connect to ip address %hhu.%hhu.%hhu.%hhu\n"),
                      sa_in.sin_addr.S_un.S_un_b.s_b1,
                      sa_in.sin_addr.S_un.S_un_b.s_b2,
                      sa_in.sin_addr.S_un.S_un_b.s_b3,
                      sa_in.sin_addr.S_un.S_un_b.s_b4 );
            quiet = savedQuiet;
        }
#endif
        // --------------------------------------------------
        // get a socket
        //

        if ( (the_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET ) {
            return(ERR_CANT_GET_SOCKET);
        }

        sa_in.sin_family = AF_INET;
        sa_in.sin_port = our_port;

        // set socket options.  DONTLINGER will give us a more graceful disconnect

        setsockopt(the_socket,
                   SOL_SOCKET,
                   SO_DONTLINGER,
                   (char *) &dummyVal, sizeof(dummyVal));

        // get a connection

        for ( tryCount = noftry(); tryCount; ) {
            retval = connect (the_socket,
                              (struct sockaddr *)&sa_in,
                              sizeof(struct sockaddr_in));
            if ( retval != SOCKET_ERROR )
                break;
#if INCLUDE_SUPERDEBUG
            if ( superDebug ) {
                _TCHAR savedQuiet = quiet;
                quiet = FALSE;
                printMsg( __T("superDebug: ::connect() returned error %d, retry count remaining is %d\n"),
                          WSAGetLastError(), tryCount - 1 );
                quiet = savedQuiet;
            }
#endif
            if ( --tryCount )
                Sleep(1000);
        }

        if ( retval != SOCKET_ERROR )
            break;

#if INCLUDE_SUPERDEBUG
        if ( superDebug ) {
            _TCHAR savedQuiet = quiet;
            quiet = FALSE;
            printMsg( __T("superDebug: Connection returned error %d\n"),
                      WSAGetLastError() );
            quiet = savedQuiet;
        }
#endif
    }

    if ( retval == SOCKET_ERROR ) {
        switch ( (err_code = WSAGetLastError()) ) {
        /* twiddle our thumbs until the connect succeeds */
        case WSAEWOULDBLOCK:
            break;

        case WSAECONNREFUSED:
            return(ERR_CONNECTION_REFUSED);

        default:
            _stprintf(message, __T("unexpected error %d from winsock"), err_code);
            complain(message);

        case WSAETIMEDOUT:
            return(ERR_CANT_CONNECT);
        }
    }

    owner_task = GET_CURRENT_TASK;

    // Make this a non-blocking socket
    ioctlsocket (the_socket, FIONBIO, &ioctl_blocking);

    // make the FD_SET and timeout structures for later operations...

    FD_ZERO (&fds);
    FD_SET  (the_socket, &fds);

    // normal timeout, can be changed by the wait option.
    timeout.tv_sec = globaltimeout;
    timeout.tv_usec = 0;

    maxMsgSize = SOCKET_BUFFER_SIZE;
    dummyVal = sizeof(maxMsgSize);
    getsockopt(the_socket,
               SOL_SOCKET,
               SO_SNDBUF,
               (char *)&maxMsgSize, &dummyVal);
    if ( maxMsgSize < SOCKET_BUFFER_SIZE )
        buffer_size = (int)maxMsgSize;

    return(0);
}

#if INCLUDE_SUPERDEBUG

void debugOutputThisData( char * pData, unsigned long length, LPTSTR msg )
{
    _TCHAR        _tprtline[MAX_PRINTF_LENGTH + 2];
    unsigned      tx;
    BYTE          buf[68];
    unsigned long x;
    unsigned long y;
    unsigned long offset;
    BYTE        * pStr;
    _TCHAR        savedQuiet;
    BYTE        * pp;
    BYTE        * p;
    char          tmpstr[4];
    BYTE          c;


    if ( superDebug ) {
        savedQuiet = quiet;

        quiet = FALSE;
        printMsg( msg, length );

        if ( superDebug == 2 ) {
            x    = length;
            pStr = (BYTE *)pData;

            for ( ; x > MAX_PRINTF_LENGTH; ) {
                p = new BYTE[MAX_PRINTF_LENGTH + 2];

                strncpy( (char *)p, (char *)pStr, MAX_PRINTF_LENGTH );
                p[MAX_PRINTF_LENGTH] = '\0';
                pp = (BYTE *)strrchr((char *)p, '\n');
                if ( pp ) {
                    pp++;
                    *pp = '\0';
                    x    -= (unsigned long)(pp - p);
                    pStr += (int)(pp - p);
                } else {
                    x    -= MAX_PRINTF_LENGTH;
                    pStr += MAX_PRINTF_LENGTH;
                }
                for ( tx = 0; p[tx]; tx++ )
                    _tprtline[tx] = p[tx];
                _tprtline[tx] = __T('\0');
                printMsg( __T("%s"), _tprtline );
                if ( p[strlen((char *)p) - 1] != '\n' )
                    printMsg( __T("\n") );

                delete [] p;
            }
            if ( x ) {
                p = new BYTE[x + 2];
                memcpy( p, pStr, x );
                p[x] = '\0';
                for ( tx = 0; p[tx]; tx++ )
                    _tprtline[tx] = p[tx];
                _tprtline[tx] = __T('\0');
                printMsg( __T("%s"), _tprtline );
                if ( p[x - 1] != '\n' )
                    printMsg( __T("\n") );
                delete [] p;
            }
        } else {
            memset( buf, ' ', 66 );
            offset = 0;
            y = 0;
            for ( x = 0; y < length; x++, y++ ) {
                c = (BYTE)pData[y];

                if ( x == 16 ) {
                    buf[66] = '\0';
                    for ( tx = 0; buf[tx]; tx++ )
                        _tprtline[tx] = buf[tx];
                    _tprtline[tx] = __T('\0');
                    printMsg( __T("superDebug: %05lX %s\r\n"), offset, _tprtline );
                    memset( buf, ' ', 66 );
                    x = 0;
                    offset += 16;
                }

                sprintf( tmpstr, " %02hhX", c );
                memcpy( &buf[x * 3], tmpstr, 3);
                if ( (c < 0x20) || (c > 0x7e) )
                    c = '.';

                buf[x + 50] = c;
            }
            buf[66] = '\0';
            for ( tx = 0; buf[tx]; tx++ )
                _tprtline[tx] = buf[tx];
            _tprtline[tx] = __T('\0');
            printMsg( __T("superDebug: %05lX %s\r\n"), offset, _tprtline );
        }
        quiet = savedQuiet;
    }
}
#endif

//
//---------------------------------------------------------------------------
//
// The 'wait' parameter, if set, says to return WAIT_A_BIT
// if there's no data waiting to be read.

int
     connection::get_buffer(int wait)
{
    int retval;
    int bytes_read = 0;
//    unsigned long ready_to_read = 0;

    // Use select to see if data is waiting...

    FD_ZERO (&fds);
    FD_SET  (the_socket, &fds);

    // if wait is set, we are polling, return immediately
    if ( wait ) {
        timeout.tv_sec = 0;
    } else {
        timeout.tv_sec = globaltimeout;
    }

    if ( (retval = select (0, &fds, NULL, NULL, &timeout))
         == SOCKET_ERROR ) {
        _TCHAR what_error[256];
        int error_code = WSAGetLastError();

        if ( error_code == WSAEINPROGRESS && wait ) {
            return(WAIT_A_BIT);
        }

        _stprintf( what_error,
                   __T("connection::get_buffer() unexpected error from select: %d"),
                   error_code);
        complain (what_error);
        // return( wait ? WAIT_A_BIT : ERR_READING_SOCKET );
    }

    // if we don't want to wait
    if ( !retval && wait ) {
        return(WAIT_A_BIT);
    }

    // we have data waiting...
    bytes_read = recv (the_socket,
                       pInBuffer,
                       buffer_size,
                       0);

    // just in case.

    if ( bytes_read == 0 ) {
        // connection terminated (semi-) gracefully by the other side
        return(ERR_NOT_CONNECTED);
    }

    if ( bytes_read == SOCKET_ERROR ) {
        _TCHAR what_error[256];
        int ws_error = WSAGetLastError();
        switch ( ws_error ) {
        // all these indicate loss of connection (are there more?)
        case WSAENOTCONN:
        case WSAENETDOWN:
        case WSAENETUNREACH:
        case WSAENETRESET:
        case WSAECONNABORTED:
        case WSAECONNRESET:
            return(ERR_NOT_CONNECTED);

        case WSAEWOULDBLOCK:
            return(WAIT_A_BIT);

        default:
            _stprintf( what_error,
                       __T("connection::get_buffer() unexpected error: %d"),
                       ws_error);
            complain ( what_error );
            // return( wait ? WAIT_A_BIT : ERR_READING_SOCKET );
        }
    }
#if INCLUDE_SUPERDEBUG
    else {
        debugOutputThisData( pInBuffer, (unsigned long)bytes_read, __T("superDebug: Received %lu bytes:\n") );
    }
#endif

    // reset buffer indices.
    if ( bytes_read == SOCKET_ERROR )
        bytes_read = 0;

    in_buffer_total = (unsigned int)bytes_read;
    in_index = 0;

#if SUPPORT_GSSAPI
    // Okay, we've read data sent by the server into pInBuffer.  However, if pGss->IsReadyToEncrypt() is TRUE,
    // that means the message is encrypted and needs to be decrypted before we pass it back to the rest of the
    // program... so do it.

    Buf msg;
    Buf msg1;

    if (pGss) {
        try {
            if (pGss->IsReadyToEncrypt()) {
                msg1.Clear();
                msg1.Add(pInBuffer,in_buffer_total);
                msg = pGss->Decrypt(msg1);//Copy pInBuffer and decrypt it
  #if defined(_UNICODE) || defined(UNICODE)
                {
                    size_t len = msg.Length();
                    LPTSTR pStr = msg.Get();

                    pInBuffer[len] = '\0';
                    while ( len ) {
                        --len;
                        pInBuffer[len] = (char)(pStr[len] & 0xFF);
                    }
                }
  #else
                memcpy(pInBuffer,(const char*)msg,msg.Length()); //save the result back to pInBuffer
  #endif
                in_buffer_total = (int)msg.Length();
  #if INCLUDE_SUPERDEBUG
                if ( superDebug ) {
                    printMsg( __T("superDebug: Decrypted\n") );
                }
  #endif
            }
        }
        catch (GssSession::GssException& e) {
            _TCHAR szMsg[4096];
            _stprintf(szMsg, __T("Cannot decrypt message: %s"),e.message());
            complain(szMsg);
            pGss = NULL;
            msg1.Free();
            msg.Free();
            return (ERR_READING_SOCKET);
        }
    }
    msg1.Free();
    msg.Free();
#endif

    return(0);

}

//
//---------------------------------------------------------------------------
// get a character from this connection.
//

int
     connection::getachar(int wait, LPTSTR ch)
{
    int retval;

    if ( in_index >= in_buffer_total ) {
        retval = get_buffer(wait);
        if ( retval )
            return(retval);
    }
    *ch = (_TCHAR)pInBuffer[in_index++];
    return(0);
}


//
//---------------------------------------------------------------------------
// FIXME: should try to handle the fact that send can only take
// an int, not an unsigned long.

int
     connection::put_data (char * pData, unsigned long length)
{
    int num_sent;
    int retval;

#if SUPPORT_GSSAPI
    // Okay, we've got data to send to the server in char *pData.  However, if pGss->IsReadyToEncrypt() is TRUE,
    // that means the message needs to be encrypted before it is sent to the server... so do it.

    Buf msg;
    Buf msg1;

    if (pGss) {
        try {
            if (pGss->IsReadyToEncrypt()) {
                msg1.Clear();
                msg1.Add(pData,length);
                msg = pGss->Encrypt(msg1); //copy pData and encrypt it
  #if defined(_UNICODE) || defined(UNICODE)
                LPTSTR pEncryptedData = msg.Get(); // point at the encrypted buffer

                pData = (char *)pEncryptedData;
                size_t ix = 0;
                while ( ix < msg.Length() ) {
                    pData[ix] = (char)(pEncryptedData[ix] & 0xFF);
                    ix++;
                }
  #else
                pData = (char *)msg.Get(); // point char *pData at the encrypted buffer
  #endif
                length = (int)msg.Length();
  #if INCLUDE_SUPERDEBUG
                if ( superDebug ) {
                    printMsg( __T("superDebug: Encrypted via GSSAUTH_P_%s\n"),
                              (pGss->GetProtectionLevel() == GSSAUTH_P_PRIVACY) ? __T("PRIVACY") : __T("INTEGRITY") );
                }
  #endif
            }
        }
        catch (GssSession::GssException& e) {
            _TCHAR szMsg[4096];
            _stprintf(szMsg, __T("Cannot encrypt message: %s"), e.message() );
            complain(szMsg);
            pGss = NULL;
            msg1.Free();
            msg.Free();
            return (ERR_SENDING_DATA);
        }
    }
    msg1.Free();
    msg.Free();
#endif

    FD_ZERO (&fds);
    FD_SET  (the_socket, &fds);

    timeout.tv_sec = globaltimeout;

#if INCLUDE_SUPERDEBUG
    debugOutputThisData( pData, length, __T("superDebug: Attempting to send %lu bytes:\n") );
#endif

    while ( length > 0 ) {
        retval = select (0, NULL, &fds, NULL, &timeout);
        if ( retval == SOCKET_ERROR ) {
            _TCHAR what_error[256];
            int error_code = WSAGetLastError();

            _stprintf( what_error,
                       __T("connection::put_pData() unexpected error from select: %d"),
                       error_code );
            complain ( what_error );
        }

        num_sent = send( the_socket,
                         pData,
                         ((int)length > buffer_size) ? buffer_size : (int)length,
                         0 );

        if ( num_sent == SOCKET_ERROR ) {
            _TCHAR what_error[256];
            int ws_error = WSAGetLastError();
            switch ( ws_error ) {
            // this is the only error we really expect to see.
            case WSAENOTCONN:
                return(ERR_NOT_CONNECTED);

                // seems that we can still get a block
            case WSAEWOULDBLOCK:
            case WSAEINPROGRESS:
                Sleep(1);
                break;

            default:
                _stprintf( what_error,
                           __T("connection::put_pData() unexpected error from send(): %d"),
                           ws_error );
                complain (what_error);
                return(ERR_SENDING_DATA);
            }
        } else {
            length -= num_sent;
            pData   += num_sent;
            if ( length > 0 )
                Sleep(1);
        }
    }

    return(0);
}

#if defined(_UNICODE) || defined(UNICODE)

int
     connection::put_data (LPTSTR pData, unsigned long length)
{
    char * pCharData;
    int    retval;
    size_t ix;

    retval = ERR_SENDING_DATA;
    pCharData = new char[length];
    if ( pCharData ) {
        for ( ix = 0; ix < length; ix++ ) {
            pCharData[ix] = (char)(pData[ix] & 0xFF);
        }
        retval = put_data( pCharData, length );
        delete [] pCharData;
    }
    return( retval );
}
#endif

//
//
// buffered output
//

int
     connection::put_data_buffered (LPTSTR pData, unsigned long length)
{
//    unsigned int sorta_sent = 0;
    int retval;

    while ( length ) {
        if ( (int)(out_index + length) < buffer_size ) {
            // we won't overflow, simply copy into the buffer
            do {
                pOutBuffer[out_index] = (char)(*pData & 0xFF);
                pData++;
                out_index++;
                length--;
            } while ( length );
        } else {
            unsigned int ix;
            unsigned int orphaned_chunk = buffer_size - out_index;
            LPTSTR       tempPData = pData;

            // we will overflow, handle it
            ix = orphaned_chunk;
            while ( ix ) {
                pOutBuffer[out_index] = (char)(*tempPData & 0xFF);
                tempPData++;
                out_index++;
                ix--;
            }

            // send this buffer...
            retval = put_data (pOutBuffer, (unsigned long)buffer_size);
            if ( retval )
                return(retval);

            length -= orphaned_chunk;
            out_index = 0;
            pData += orphaned_chunk;
        }
    }

    return(0);
}

int
     connection::put_data_flush (void)
{
    int retval;

    retval = put_data (pOutBuffer, out_index);
    if ( retval )
        return(retval);

    out_index = 0;

    return(0);
}

//
//---------------------------------------------------------------------------
//

int
     connection::close (void)
{
    if ( closesocket(the_socket) == SOCKET_ERROR )
        return(ERR_CLOSING);

    return(0);
}


//
//---------------------------------------------------------------------------
// we keep lists of connections in this class

class connection_list {
private:
    connection *      data;
    connection_list * next;

public:
    connection_list   (void);
    ~connection_list  (void);
    void push         (connection & conn);

    // should really use pointer-to-memberfun for these
    connection * find (SOCKET sock);
    int how_many_are_mine (void);

    void remove       (socktag sock);
};

connection_list::connection_list (void)
{
    next = 0;
}

connection_list::~connection_list(void)
{
    if ( data )
        delete data;
}

// add a new connection to the list

void
     connection_list::push (connection & conn)
{
    connection_list * new_conn;

    new_conn = new connection_list();

    new_conn->data = data;
    new_conn->next = next;

    data = &conn;
    next = new_conn;

}

int
     connection_list::how_many_are_mine(void)
{
    TASK_HANDLE_TYPE  current_task = GET_CURRENT_TASK;
    connection_list * iter = this;
    int num = 0;

    while ( iter->data ) {
        if ( iter->data->get_owner_task() == current_task )
            num++;
        iter = iter->next;
    }
    return(num);
}

// find a particular socket's connection object.

connection *
     connection_list::find (SOCKET sock)
{
    connection_list * iter = this;

    while ( iter->data ) {
        if ( iter->data->get_socket() == sock )
            return(iter->data);
        iter = iter->next;
    }
    return(0);
}

void
     connection_list::remove (socktag sock)
{
    // at the end
    if ( !data )
        return;

    // we can assume next is valid because
    // the last node is always {0,0}
    if ( data == sock ) {
        connection_list * pnext = next;
        delete data;
        data = next->data;
        next = next->next;                       // 8^)
        delete pnext;
        return;
    }

    // recurse
    next->remove(sock);
}

//
// ---------------------------------------------------------------------------
// global variables (shared by all DLL users)

connection_list global_socket_list;
int network_initialized;

// ---------------------------------------------------------------------------
// C/DLL interface
//

int
     gensock_connect (LPTSTR hostname,
                      LPTSTR service,
                      socktag FAR * pst)
{
    int retval;
    connection * conn;

    conn = new connection;
    if ( !conn )
        return(ERR_INITIALIZING);

    // if this task hasn't opened any sockets yet, then
    // call WSAStartup()

    if ( global_socket_list.how_many_are_mine() < 1 ) {
        retval = init_winsock();
        if ( retval )
            return(retval);
    }

    global_socket_list.push(*conn);

    retval = conn->get_connected (hostname, service);
    if ( retval ) {
        if ( gensock_close(conn) )
            delete conn;
        *pst = 0;
        return(retval);
    }
    *pst = (void FAR *) conn;

    return(0);
}

//
//
//

int
     gensock_getchar (socktag st, int wait, LPTSTR ch)
{
    connection * conn = (connection *) st;

    if ( !conn )
        return(ERR_NOT_A_SOCKET);

    return (conn->getachar(wait, ch));
}


//---------------------------------------------------------------------------
//
//

int
     gensock_put_data (socktag st, LPTSTR pData, unsigned long length)
{
    connection * conn = (connection *) st;

    if ( !conn )
        return(ERR_NOT_A_SOCKET);

    return(conn->put_data(pData, length));
}

//---------------------------------------------------------------------------
//
//

int
     gensock_put_data_buffered (socktag st, LPTSTR data, unsigned long length)
{
    connection * conn = (connection *) st;

    if ( !conn )
        return(ERR_NOT_A_SOCKET);

    return(conn->put_data_buffered (data, length));
}

//---------------------------------------------------------------------------
//
//

int
     gensock_put_data_flush (socktag st)
{
    connection * conn = (connection *) st;

    if ( !conn )
        return(ERR_NOT_A_SOCKET);

    return(conn->put_data_flush());
}

//
// ---------------------------------------------------------------------------
//
char *
     gensock_getdomainfromhostnameA (char * pName)
{
    char * domainPtr;

    // --------------------------------------------------
    // resolve the hostname/ipaddress
    //

    domainPtr = NULL;
    for ( ; ; ) {
        if ( inet_addr (pName) != INADDR_NONE )
            break;

        if ( gethostbyname(pName) == NULL ) {
            int retval = WSAGetLastError();

            if (retval == WSANOTINITIALISED) {
                retval = init_winsock();
                if ( !retval ) {
                    domainPtr = gensock_getdomainfromhostnameA(pName);
                    deinit_winsock();
                }
            }
            break;
        }

        domainPtr = strchr(pName, '.');
        if (!domainPtr)
            break;

        domainPtr = gensock_getdomainfromhostnameA( ++domainPtr );
        if (!domainPtr)
            domainPtr = pName;

        break;
    }
    return domainPtr;
}

#if defined(_UNICODE) || defined(UNICODE)
LPTSTR
     gensock_getdomainfromhostnameW (LPTSTR pName)
{
    unsigned x;
    char   * pDomainName;
    LPTSTR   pTDomainName;
    char     hostname[MAX_HOSTNAME_LENGTH];
    static _TCHAR _Thostname[MAX_HOSTNAME_LENGTH];

    convertHostnameUsingPunycode( pName, hostname );
    pTDomainName = NULL;
    pDomainName = gensock_getdomainfromhostnameA( hostname );
    if ( pDomainName ) {
        pTDomainName = _Thostname;
        for ( x = 0; x < (MAX_HOSTNAME_LENGTH-1); x++ ) {
            _Thostname[x] = (_TCHAR)pDomainName[x];
            if ( pDomainName[x] == '\0' )
                break;
        }
    }
    return pTDomainName;
}
#endif

//---------------------------------------------------------------------------
//
//
int
     gensock_gethostname (LPTSTR pName, int namelen)
{
    char name[MAX_HOSTNAME_LENGTH];
    int  retval;

    ZeroMemory(name, sizeof(name));
    retval = gethostname(name, namelen);
    if ( retval )
        return(retval - 5000);

    for ( retval = 0; (retval < namelen) && (retval < sizeof(name)); retval++ ) {
        if ( !name[retval] )
            break;

        pName[retval] = (_TCHAR)name[retval];
    }
    pName[retval] = __T('\0');
    return(0);
}

//---------------------------------------------------------------------------
//
//

int
     gensock_close (socktag st)
{
    connection * conn;
    int retval;

    conn = (connection *) st;

    if ( !conn )
        return(ERR_NOT_A_SOCKET);

    retval = conn->close();
    if ( retval )
        return(retval);

    global_socket_list.remove((connection *)st);

    if ( global_socket_list.how_many_are_mine() < 1 ) {
        deinit_winsock();
    }

    return(0);
}

#if INCLUDE_SUPERDEBUG

static LPTSTR errorName(int WinSockErrorCode )
{
    switch( WinSockErrorCode )
    {
        case 0                 :  return __T("NO_ERROR");
        case WSABASEERR        :  return __T("WSABASEERR");
        case WSAEINTR          :  return __T("WSAEINTR");
        case WSAEBADF          :  return __T("WSAEBADF");
        case WSAEACCES         :  return __T("WSAEACCES");
        case WSAEFAULT         :  return __T("WSAEFAULT");
        case WSAEINVAL         :  return __T("WSAEINVAL");
        case WSAEMFILE         :  return __T("WSAEMFILE");
        case WSAEWOULDBLOCK    :  return __T("WSAEWOULDBLOCK");
        case WSAEINPROGRESS    :  return __T("WSAEINPROGRESS");
        case WSAEALREADY       :  return __T("WSAEALREADY");
        case WSAENOTSOCK       :  return __T("WSAENOTSOCK");
        case WSAEDESTADDRREQ   :  return __T("WSAEDESTADDRREQ");
        case WSAEMSGSIZE       :  return __T("WSAEMSGSIZE");
        case WSAEPROTOTYPE     :  return __T("WSAEPROTOTYPE");
        case WSAENOPROTOOPT    :  return __T("WSAENOPROTOOPT");
        case WSAEPROTONOSUPPORT:  return __T("WSAEPROTONOSUPPORT");
        case WSAESOCKTNOSUPPORT:  return __T("WSAESOCKTNOSUPPORT");
        case WSAEOPNOTSUPP     :  return __T("WSAEOPNOTSUPP");
        case WSAEPFNOSUPPORT   :  return __T("WSAEPFNOSUPPORT");
        case WSAEAFNOSUPPORT   :  return __T("WSAEAFNOSUPPORT");
        case WSAEADDRINUSE     :  return __T("WSAEADDRINUSE");
        case WSAEADDRNOTAVAIL  :  return __T("WSAEADDRNOTAVAIL");
        case WSAENETDOWN       :  return __T("WSAENETDOWN");
        case WSAENETUNREACH    :  return __T("WSAENETUNREACH");
        case WSAENETRESET      :  return __T("WSAENETRESET");
        case WSAECONNABORTED   :  return __T("WSAECONNABORTED");
        case WSAECONNRESET     :  return __T("WSAECONNRESET");
        case WSAENOBUFS        :  return __T("WSAENOBUFS");
        case WSAEISCONN        :  return __T("WSAEISCONN");
        case WSAENOTCONN       :  return __T("WSAENOTCONN");
        case WSAESHUTDOWN      :  return __T("WSAESHUTDOWN");
        case WSAETOOMANYREFS   :  return __T("WSAETOOMANYREFS");
        case WSAETIMEDOUT      :  return __T("WSAETIMEDOUT");
        case WSAECONNREFUSED   :  return __T("WSAECONNREFUSED");
        case WSAELOOP          :  return __T("WSAELOOP");
        case WSAENAMETOOLONG   :  return __T("WSAENAMETOOLONG");
        case WSAEHOSTDOWN      :  return __T("WSAEHOSTDOWN");
        case WSAEHOSTUNREACH   :  return __T("WSAEHOSTUNREACH");
        case WSAENOTEMPTY      :  return __T("WSAENOTEMPTY");
        case WSAEPROCLIM       :  return __T("WSAEPROCLIM");
        case WSAEUSERS         :  return __T("WSAEUSERS");
        case WSAEDQUOT         :  return __T("WSAEDQUOT");
        case WSAESTALE         :  return __T("WSAESTALE");
        case WSAEREMOTE        :  return __T("WSAEREMOTE");
        case WSAEDISCON        :  return __T("WSAEDISCON");
        case WSASYSNOTREADY    :  return __T("WSASYSNOTREADY");
        case WSAVERNOTSUPPORTED:  return __T("WSAVERNOTSUPPORTED");
        case WSANOTINITIALISED :  return __T("WSANOTINITIALISED");
    }
    return __T("?");
}
#endif
//---------------------------------------------------------------------------
//
//

int
     init_winsock(void)
{
    int retval;
    WSADATA winsock_data;
    WORD version_required = 0x0101;              /* Version 1.1 */

    network_initialized = 0;
    retval = WSAStartup (version_required, &winsock_data);
#if INCLUDE_SUPERDEBUG
    if ( superDebug ) {
        _TCHAR savedQuiet = quiet;
        quiet = FALSE;
        printMsg( __T("superDebug: init_winsock(), WSAStartup() returned %i (%s)\n"), retval, errorName(retval) );
        quiet = savedQuiet;
    }
#endif
    switch ( retval ) {
//    case 0:           // defaults to break anyway.
//        /* successful */
//        break;
    case WSASYSNOTREADY:
        return(ERR_SYS_NOT_READY);

    case WSAEINVAL:
        return(ERR_EINVAL);

    case WSAVERNOTSUPPORTED:
        return(ERR_VER_NOT_SUPPORTED);

    case WSAEINPROGRESS:        /* A blocking Windows Sockets 1.1 operation is in progress. */
        return(ERR_BUSY);

    case WSAEPROCLIM:           /* Limit on the number of tasks supported by the Windows Sockets implementation has been reached. */
        return(ERR_CANT_GET_SOCKET);
    }
    network_initialized = 1;
    return(0);
}

void
     deinit_winsock(void)
{
    network_initialized = 0;
    WSACleanup();
}
