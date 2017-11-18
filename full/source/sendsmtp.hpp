/*
    sendSMTP.hpp
*/

#ifndef __SENDSMTP_HPP__
#define __SENDSMTP_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"

extern LPTSTR defaultSMTPPort;

#if INCLUDE_POP3
extern LPTSTR defaultPOP3Port;
#endif
#if defined(SUBMISSION_PORT) && SUBMISSION_PORT
extern LPTSTR SubmissionPort;
#endif
#if INCLUDE_IMAP
extern LPTSTR defaultIMAPPort;
#endif

#if SUPPORT_GSSAPI

extern BOOL putline ( COMMON_DATA & CommonData, LPTSTR line );
extern BOOL getline ( COMMON_DATA & CommonData, LPTSTR line );
#endif

extern void find_and_strip_salutation ( COMMON_DATA & CommonData, Buf &email_addresses );
extern void parse_email_addresses ( COMMON_DATA & CommonData, LPTSTR email_addresses, Buf & parsed_addys );
extern void searchReplaceEmailKeyword (COMMON_DATA & CommonData, Buf & email_addresses);

extern int send_email( COMMON_DATA & CommonData, size_t msgBodySize,
                       Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                       LPTSTR attachment_boundary, LPTSTR multipartID,
                       int nbrOfAttachments, DWORD totalsize );

#endif  // #ifndef __SENDSMTP_HPP__
