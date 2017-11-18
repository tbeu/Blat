/*
    server.hpp
*/
#ifndef __SERVER_HPP__
#define __SERVER_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"


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


// validateResp pulls double duty.  If NULL, then the server response is not checked.
// If set to a pointer, then any enhanced status codes that are received get stored there.

extern int get_server_response( COMMON_DATA & CommonData, Buf * responseStr, int * validateResp  );


#if SUPPORT_GSSAPI

// Added 2003-11-07, Joseph Calzaretta
//   overloading get_server_response so it CAN output to LPTSTR instead of Buf*
extern int get_server_response( COMMON_DATA & CommonData, LPTSTR responseStr );
#endif

#if INCLUDE_POP3

extern int get_pop3_server_response( COMMON_DATA & CommonData, Buf * responseStr );
#endif

#if INCLUDE_IMAP

extern int get_imap_untagged_server_response( COMMON_DATA & CommonData, Buf * responseStr  );
extern int get_imap_tagged_server_response( COMMON_DATA & CommonData, Buf * responseStr, LPTSTR tag  );
#endif


extern int  gensockGetLastError (COMMON_DATA & CommonData);
extern void gensockSaveLastError (COMMON_DATA & CommonData, int retval);
extern void gensock_error (COMMON_DATA & CommonData, LPTSTR function, int retval, LPTSTR hostname);
extern int  open_server_socket( COMMON_DATA & CommonData, LPTSTR host, LPTSTR userPort, LPTSTR defaultPort, LPTSTR portName );
extern int  close_server_socket( COMMON_DATA & CommonData );
extern int  put_message_line( COMMON_DATA & CommonData, socktag sock, LPTSTR line );
extern int  finish_server_message( COMMON_DATA & CommonData );
extern void server_error( COMMON_DATA & CommonData, LPTSTR message );
extern void server_warning( COMMON_DATA & CommonData, LPTSTR message);


#ifdef BLATDLL_TC_WCX
extern int transform_and_send_edit_data( COMMON_DATA & CommonData, LPTSTR editptr, DWORD attachmentSize );
#else
extern int transform_and_send_edit_data( COMMON_DATA & CommonData, LPTSTR editptr );
#endif


#if INCLUDE_POP3

#ifdef BLATDLL_TC_WCX
extern int send_edit_data (COMMON_DATA & CommonData, LPTSTR message, Buf * responseStr, DWORD attachmentSize );
#else
extern int send_edit_data (COMMON_DATA & CommonData, LPTSTR message, Buf * responseStr );
#endif
#endif  // #if INCLUDE_POP3


#ifdef BLATDLL_TC_WCX
extern int send_edit_data ( COMMON_DATA & CommonData, LPTSTR message, int expected_response, Buf * responseStr, DWORD attachmentSize );
#else
extern int send_edit_data ( COMMON_DATA & CommonData, LPTSTR message, int expected_response, Buf * responseStr );
#endif


// Convert the entry "n of try" to a numeric, defaults to 1
extern int noftry( COMMON_DATA & CommonData );

#endif  // #ifndef __SERVER_HPP__
