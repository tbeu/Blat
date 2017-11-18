/*
    sendNNTP.hpp
*/

#ifndef __SENDNNTP_HPP__
#define __SENDNNTP_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"

#if INCLUDE_NNTP

#include "winfile.h"
#include "common_data.h"
#include "gensock.h"

extern LPTSTR defaultNNTPPort;

extern int send_news( COMMON_DATA & CommonData, size_t msgBodySize,
                      Buf &lpszFirstReceivedData, Buf &lpszOtherHeader,
                      LPTSTR attachment_boundary, LPTSTR multipartID,
                      int nbrOfAttachments, DWORD totalsize );

#endif  // #if INCLUDE_NNTP

#endif  // #ifndef __SENDNNTP_HPP__
