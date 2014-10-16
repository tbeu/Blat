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
#include <stdlib.h>
#include <ctype.h>

extern "C" {
#if (WINVER > 0x0500)
    #include <winsock2.h>
#else
    #include <winsock.h>
#endif
}
#include "gensock.h"
#include "blat.h"
#if SUPPORT_GSSAPI
#include "gssfuncs.h" // Please read the comments here for information about how to use GssSession
#endif

#define MAX_PRINTF_LENGTH   1000

#ifdef SOCKET_BUFFER_SIZE
    #undef SOCKET_BUFFER_SIZE
#endif

/* This is for NT */
#ifdef WIN32

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
extern char superDebug;
extern char quiet;
#endif

extern int  noftry();
extern void printMsg(const char *p, ... );

//
//
//

void complain (char * message)
{
#ifdef _DEBUG
    OutputDebugString (message);
#else
//  MessageBox (NULL, message, "GENSOCK.DLL Error", MB_OK|MB_ICONHAND);
    printMsg("%s\n", message);
#endif
}

//
// ---------------------------------------------------------------------------
// container for a buffered SOCK_STREAM.

class connection {
private:
    SOCKET              the_socket;
    char *              in_buffer;
    char *              out_buffer;
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

    int                 get_connected (char * hostname, char * service);
    SOCKET              get_socket(void) { return(the_socket);}
    TASK_HANDLE_TYPE    get_owner_task(void) { return(owner_task);}
    int                 get_buffer(int wait);
    int                 close (void);
    int                 getachar (int wait, char * ch);
    int                 put_data (char * data, unsigned long length);
    int                 put_data_buffered (char * data, unsigned long length);
    int                 put_data_flush (void);
};

connection::connection (void)
{
    the_socket       = 0;
    in_index         = 0;
    out_index        = 0;
    in_buffer_total  = 0;
    out_buffer_total = 0;
    in_buffer        = 0;

    in_buffer   = new char[SOCKET_BUFFER_SIZE];
    out_buffer  = new char[SOCKET_BUFFER_SIZE];
    buffer_size = SOCKET_BUFFER_SIZE;

    last_winsock_error = 0;
}

connection::~connection (void)
{
    delete [] in_buffer;
    delete [] out_buffer;
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

//
// ---------------------------------------------------------------------------
//

int
     connection::get_connected (char FAR * hostname, char FAR * service)
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
    char                 message[80];
    int                  tryCount;

    // if the ctor couldn't get a buffer
    if ( !in_buffer || !out_buffer )
        return(ERR_CANT_MALLOC);

    // --------------------------------------------------
    // resolve the service name
    //

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
        serventry = getservbyname (service, (LPSTR)"tcp");

        if ( serventry )
            our_port = serventry->s_port;
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
                char savedQuiet = quiet;
                quiet = FALSE;
                printMsg("superDebug: Cannot resolve hostname <%s> to ip address.\n", hostname );
                quiet = savedQuiet;
            }
#endif
            return(ERR_CANT_RESOLVE_HOSTNAME);
        }
        ip_list_ptr = hostentry->h_addr_list;
        sa_in.sin_addr.s_addr = *(long far *)hostentry->h_addr;
#if INCLUDE_SUPERDEBUG
        if ( superDebug ) {
            char savedQuiet = quiet;
            quiet = FALSE;
            printMsg("superDebug: Hostname <%s> resolved to ip address %u.%u.%u.%u\n",
                     hostname, sa_in.sin_addr.S_un.S_un_b.s_b1,
                               sa_in.sin_addr.S_un.S_un_b.s_b2,
                               sa_in.sin_addr.S_un.S_un_b.s_b3,
                               sa_in.sin_addr.S_un.S_un_b.s_b4 );
            printMsg("superDebug: Official hostname is %s\n", hostentry->h_name );
            quiet = savedQuiet;
        }
#endif
    }


    retval = 0; // ~SOCKET_ERROR;

    for ( ; *ip_list_ptr; ip_list_ptr++ ) {

        sa_in.sin_addr.s_addr = *(u_long FAR *)*ip_list_ptr;

#if INCLUDE_SUPERDEBUG
        if ( superDebug ) {
            char savedQuiet = quiet;
            quiet = FALSE;
            printMsg("superDebug: Attempting to connect to ip address %u.%u.%u.%u\n",
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
                char savedQuiet = quiet;
                quiet = FALSE;
                printMsg("superDebug: ::connect() returned error %d, retry count remaining is %u\n",
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
            char savedQuiet = quiet;
            quiet = FALSE;
            printMsg("superDebug: Connection returned error %d\n",
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
            wsprintf(message, "unexpected error %d from winsock", err_code);
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
        char what_error[256];
        int error_code = WSAGetLastError();

        if ( error_code == WSAEINPROGRESS && wait ) {
            return(WAIT_A_BIT);
        }

        wsprintf (what_error,
                  "connection::get_buffer() unexpected error from select: %d",
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
                       in_buffer,
                       buffer_size,
                       0);

    // just in case.

    if ( bytes_read == 0 ) {
        // connection terminated (semi-) gracefully by the other side
        return(ERR_NOT_CONNECTED);
    }

    if ( bytes_read == SOCKET_ERROR ) {
        char what_error[256];
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
            wsprintf (what_error,
                      "connection::get_buffer() unexpected error: %d",
                      ws_error);
            complain (what_error);
            // return( wait ? WAIT_A_BIT : ERR_READING_SOCKET );
        }
    }
#if INCLUDE_SUPERDEBUG
    else {
        if ( superDebug ) {
            char savedQuiet = quiet;

            quiet = FALSE;
            printMsg("superDebug: Received %u bytes:\n", bytes_read );

            if ( superDebug == 2 ) {
                int    x    = bytes_read;
                char * pStr = in_buffer;

                for ( ; x > MAX_PRINTF_LENGTH; ) {
                    char * pp;
                    char * p = new char[MAX_PRINTF_LENGTH + 2];

                    strncpy( p, pStr, MAX_PRINTF_LENGTH );
                    p[MAX_PRINTF_LENGTH] = '\0';
                    pp = strrchr(p, '\n');
                    if ( pp ) {
                        pp++;
                        *pp = '\0';
                        x    -= (int)(pp - p);
                        pStr += (int)(pp - p);
                    } else {
                        x    -= MAX_PRINTF_LENGTH;
                        pStr += MAX_PRINTF_LENGTH;
                    }
                    printMsg( "%s", p );
                    if ( p[strlen(p) - 1] != '\n' )
                        printMsg( "\n" );

                    delete [] p;
                }
                if ( x ) {
                    char * p = new char[x + 2];
                    memcpy( p, pStr, x );
                    p[x] = 0;
                    printMsg( "%s", p );
                    if ( p[x - 1] != '\n' )
                        printMsg( "\n" );
                    delete [] p;
                }
            } else {
                char buf[68];
                int  x, y;

                memset( buf, ' ', 66 );
                for ( x = y = 0; y < bytes_read; x++, y++ ) {
                    char tmpstr[4];
                    unsigned char c = in_buffer[y];

                    if ( x == 16 ) {
                        buf[66] = 0;
                        printMsg( "superDebug: %05X %s\r\n", (y - 16), buf );
                        memset( buf, ' ', 66 );
                        x = 0;
                    }

                    sprintf( tmpstr, " %02X", c );
                    memcpy( &buf[x * 3], tmpstr, 3);
                    if ( (c < 0x20) || (c > 0x7e) )
                        c = '.';

                    buf[x + 50] = c;
                }
                buf[66] = 0;
                if ( x == 16 )
                    y--;

                printMsg( "superDebug: %05X %s\r\n", y - (y % 16), buf );
            }
            quiet = savedQuiet;
        }
    }
#endif

    // reset buffer indices.
    if ( bytes_read == SOCKET_ERROR )
        bytes_read = 0;

    in_buffer_total = bytes_read;
    in_index = 0;

#if SUPPORT_GSSAPI
    // Okay, we've read data sent by the server into in_buffer.  However, if pGss->IsReadyToEncrypt() is TRUE,
    // that means the message is encrypted and needs to be decrypted before we pass it back to the rest of the
    // program... so do it.

    Buf msg;

    if (pGss) {
        try {
            if (pGss->IsReadyToEncrypt()) {
                msg = pGss->Decrypt(Buf(in_buffer,in_buffer_total));//Copy in_buffer and decrypt it
                memcpy(in_buffer,(const char*)msg,msg.Length()*sizeof(char)); //save the result back to in_buffer
                in_buffer_total = msg.Length();
  #if INCLUDE_SUPERDEBUG
                if ( superDebug ) {
                    printMsg("superDebug: Decrypted\n");
                }
  #endif
            }
        }
        catch (GssSession::GssException& e) {
            char szMsg[4096];
            sprintf(szMsg, "Cannot decrypt message: %s",e.message());
            complain(szMsg);
            pGss = NULL;
            return (ERR_READING_SOCKET);
        }
    }
#endif

    return(0);

}

//
//---------------------------------------------------------------------------
// get a character from this connection.
//

int
     connection::getachar(int wait, char FAR * ch)
{
    int retval;

    if ( in_index >= in_buffer_total ) {
        retval = get_buffer(wait);
        if ( retval )
            return(retval);
    }
    *ch = in_buffer[in_index++];
    return(0);
}


//
//---------------------------------------------------------------------------
// FIXME: should try to handle the fact that send can only take
// an int, not an unsigned long.

int
     connection::put_data (char * data, unsigned long length)
{
    int num_sent;
    int retval;

#if SUPPORT_GSSAPI

    // Okay, we've got data to send to the server in char *data.  However, if pGss->IsReadyToEncrypt() is TRUE,
    // that means the message needs to be encrypted before it is sent to the server... so do it.

    Buf msg;
    if (pGss) {
        try {
            if (pGss->IsReadyToEncrypt()) {
                msg = pGss->Encrypt(Buf(data,length)); //copy data and encrypt it
                data = msg.Get(); // point char *data at the encrypted buffer
                length = msg.Length();
  #if INCLUDE_SUPERDEBUG
                if ( superDebug ) {
                    printMsg("superDebug: Encrypted via GSSAUTH_P_%s\n",
                        pGss->GetProtectionLevel()==GSSAUTH_P_PRIVACY?"PRIVACY":"INTEGRITY");
                }
  #endif
            }
        }
        catch (GssSession::GssException& e) {
            char szMsg[4096];
            sprintf(szMsg,"Cannot encrypt message: %s",e.message());
            complain(szMsg);
            pGss = NULL;
            return (ERR_SENDING_DATA);
        }
    }
#endif

    FD_ZERO (&fds);
    FD_SET  (the_socket, &fds);

    timeout.tv_sec = globaltimeout;

#if INCLUDE_SUPERDEBUG
    if ( superDebug ) {
        char savedQuiet = quiet;

        quiet = FALSE;
        printMsg("superDebug: Attempting to send %u bytes:\n", length );

        if ( superDebug == 2 ) {
            int    x    = length;
            char * pStr = data;

            for ( ; x > MAX_PRINTF_LENGTH; ) {
                char * pp;
                char * p = new char[MAX_PRINTF_LENGTH + 2];

                strncpy( p, pStr, MAX_PRINTF_LENGTH );
                p[MAX_PRINTF_LENGTH] = '\0';
                pp = strrchr(p, '\n');
                if ( pp ) {
                    pp++;
                    *pp = '\0';
                    x    -= (int)(pp - p);
                    pStr += (int)(pp - p);
                } else {
                    x    -= MAX_PRINTF_LENGTH;
                    pStr += MAX_PRINTF_LENGTH;
                }
                printMsg( "%s", p );
                if ( p[strlen(p) - 1] != '\n' )
                    printMsg( "\n" );

                delete [] p;
            }
            if ( x ) {
                char * p = new char[x + 2];
                memcpy( p, pStr, x );
                p[x] = 0;
                printMsg( "%s", p );
                if ( p[x - 1] != '\n' )
                    printMsg( "\n" );
                delete [] p;
            }
        } else {
            char          buf[68];
            int           x;
            unsigned long y;

            memset( buf, ' ', 66 );
            for ( x = 0, y = 0; y < length; x++, y++ ) {
                char          tmpstr[4];
                unsigned char c = data[y];

                if ( x == 16 ) {
                    buf[66] = 0;
                    printMsg( "superDebug: %05X %s\r\n", (y - 16), buf );
                    memset( buf, ' ', 66 );
                    x = 0;
                }

                sprintf( tmpstr, " %02X", c );
                memcpy( &buf[x * 3], tmpstr, 3);
                if ( (c < 0x20) || (c > 0x7e) )
                    c = '.';

                buf[x + 50] = c;
            }
            buf[66] = 0;
            if ( x == 16 )
                y--;

            printMsg( "superDebug: %05X %s\r\n", y - (y % 16), buf );
        }
        quiet = savedQuiet;
    }
#endif

    while ( length > 0 ) {
        retval = select (0, NULL, &fds, NULL, &timeout);
        if ( retval == SOCKET_ERROR ) {
            char what_error[256];
            int error_code = WSAGetLastError();

            wsprintf (what_error,
                      "connection::put_data() unexpected error from select: %d",
                      error_code);
            complain (what_error);
        }

        num_sent = send (the_socket,
                         data,
                         ((int)length > buffer_size) ? buffer_size : (int)length,
                         0);

        if ( num_sent == SOCKET_ERROR ) {
            char what_error[256];
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
                wsprintf (what_error,
                          "connection::put_data() unexpected error from send(): %d",
                          ws_error);
                complain (what_error);
                return(ERR_SENDING_DATA);
            }
        } else {
            length -= num_sent;
            data   += num_sent;
            if ( length > 0 )
                Sleep(1);
        }
    }

    return(0);
}

//
//
// buffered output
//

int
     connection::put_data_buffered (char * data, unsigned long length)
{
//    unsigned int sorta_sent = 0;
    int retval;

    while ( length ) {
        if ( (int)(out_index + length) < buffer_size ) {
            // we won't overflow, simply copy into the buffer
            memcpy (out_buffer + out_index, data, (size_t) length);
            out_index += (unsigned int) length;
            length = 0;
        } else {
            unsigned int orphaned_chunk = buffer_size - out_index;
            // we will overflow, handle it
            memcpy (out_buffer + out_index, data, orphaned_chunk);
            // send this buffer...
            retval = put_data (out_buffer, buffer_size);
            if ( retval )
                return(retval);

            length -= orphaned_chunk;
            out_index = 0;
            data += orphaned_chunk;
        }
    }

    return(0);
}

int
     connection::put_data_flush (void)
{
    int retval;

    retval = put_data (out_buffer, out_index);
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

int FAR
     gensock_connect (char FAR * hostname,
                      char FAR * service,
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

int FAR
     gensock_getchar (socktag st, int wait, char FAR * ch)
{
    connection * conn = (connection *) st;

    if ( !conn )
        return(ERR_NOT_A_SOCKET);

    return (conn->getachar(wait, ch));
}


//---------------------------------------------------------------------------
//
//

int FAR
     gensock_put_data (socktag st, char FAR * data, unsigned long length)
{
    connection * conn = (connection *) st;

    if ( !conn )
        return(ERR_NOT_A_SOCKET);

    return(conn->put_data(data, length));
}

//---------------------------------------------------------------------------
//
//

int FAR
     gensock_put_data_buffered (socktag st, char FAR * data, unsigned long length)
{
    connection * conn = (connection *) st;

    if ( !conn )
        return(ERR_NOT_A_SOCKET);

    return(conn->put_data_buffered (data, length));
}

//---------------------------------------------------------------------------
//
//

int FAR
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

char FAR *
     gensock_getdomainfromhostname (char FAR * name)
{
    char FAR * domainPtr;

    // --------------------------------------------------
    // resolve the hostname/ipaddress
    //

    if ( inet_addr (name) != INADDR_NONE )
        return NULL;

    if ( gethostbyname(name) == NULL ) {
        int retval = WSAGetLastError();

        domainPtr = NULL;
        if (retval == WSANOTINITIALISED) {
            retval = init_winsock();
            if ( !retval ) {
                domainPtr = gensock_getdomainfromhostname(name);
                deinit_winsock();
            }
        }
        return domainPtr;
    }

    domainPtr = strchr(name, '.');
    if (!domainPtr)
        return NULL;

    domainPtr = gensock_getdomainfromhostname( ++domainPtr );
    if (!domainPtr)
        return(name);

    return domainPtr;
}

//---------------------------------------------------------------------------
//
//
int FAR
     gensock_gethostname (char FAR * name, int namelen)
{
    int retval = gethostname(name, namelen);
    if ( retval )
        return(retval - 5000);

    return(0);
}

//---------------------------------------------------------------------------
//
//

int FAR
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

static char * errorName(int WinSockErrorCode )
{
    switch( WinSockErrorCode )
    {
        case WSABASEERR        :  return "WSABASEERR";
        case WSAEINTR          :  return "WSAEINTR";
        case WSAEBADF          :  return "WSAEBADF";
        case WSAEACCES         :  return "WSAEACCES";
        case WSAEFAULT         :  return "WSAEFAULT";
        case WSAEINVAL         :  return "WSAEINVAL";
        case WSAEMFILE         :  return "WSAEMFILE";
        case WSAEWOULDBLOCK    :  return "WSAEWOULDBLOCK";
        case WSAEINPROGRESS    :  return "WSAEINPROGRESS";
        case WSAEALREADY       :  return "WSAEALREADY";
        case WSAENOTSOCK       :  return "WSAENOTSOCK";
        case WSAEDESTADDRREQ   :  return "WSAEDESTADDRREQ";
        case WSAEMSGSIZE       :  return "WSAEMSGSIZE";
        case WSAEPROTOTYPE     :  return "WSAEPROTOTYPE";
        case WSAENOPROTOOPT    :  return "WSAENOPROTOOPT";
        case WSAEPROTONOSUPPORT:  return "WSAEPROTONOSUPPORT";
        case WSAESOCKTNOSUPPORT:  return "WSAESOCKTNOSUPPORT";
        case WSAEOPNOTSUPP     :  return "WSAEOPNOTSUPP";
        case WSAEPFNOSUPPORT   :  return "WSAEPFNOSUPPORT";
        case WSAEAFNOSUPPORT   :  return "WSAEAFNOSUPPORT";
        case WSAEADDRINUSE     :  return "WSAEADDRINUSE";
        case WSAEADDRNOTAVAIL  :  return "WSAEADDRNOTAVAIL";
        case WSAENETDOWN       :  return "WSAENETDOWN";
        case WSAENETUNREACH    :  return "WSAENETUNREACH";
        case WSAENETRESET      :  return "WSAENETRESET";
        case WSAECONNABORTED   :  return "WSAECONNABORTED";
        case WSAECONNRESET     :  return "WSAECONNRESET";
        case WSAENOBUFS        :  return "WSAENOBUFS";
        case WSAEISCONN        :  return "WSAEISCONN";
        case WSAENOTCONN       :  return "WSAENOTCONN";
        case WSAESHUTDOWN      :  return "WSAESHUTDOWN";
        case WSAETOOMANYREFS   :  return "WSAETOOMANYREFS";
        case WSAETIMEDOUT      :  return "WSAETIMEDOUT";
        case WSAECONNREFUSED   :  return "WSAECONNREFUSED";
        case WSAELOOP          :  return "WSAELOOP";
        case WSAENAMETOOLONG   :  return "WSAENAMETOOLONG";
        case WSAEHOSTDOWN      :  return "WSAEHOSTDOWN";
        case WSAEHOSTUNREACH   :  return "WSAEHOSTUNREACH";
        case WSAENOTEMPTY      :  return "WSAENOTEMPTY";
        case WSAEPROCLIM       :  return "WSAEPROCLIM";
        case WSAEUSERS         :  return "WSAEUSERS";
        case WSAEDQUOT         :  return "WSAEDQUOT";
        case WSAESTALE         :  return "WSAESTALE";
        case WSAEREMOTE        :  return "WSAEREMOTE";
        case WSAEDISCON        :  return "WSAEDISCON";
        case WSASYSNOTREADY    :  return "WSASYSNOTREADY";
        case WSAVERNOTSUPPORTED:  return "WSAVERNOTSUPPORTED";
        case WSANOTINITIALISED :  return "WSANOTINITIALISED";
    }
    return "?";
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
        char savedQuiet = quiet;
        quiet = FALSE;
        printMsg("superDebug: init_winsock(), WSAStartup() returned %i (%s)\n", retval, errorName(retval) );
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
