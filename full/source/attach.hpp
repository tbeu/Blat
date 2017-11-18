/*
    attach.hpp
*/

#ifndef __ATTACH_HPP__
#define __ATTACH_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"

extern void getAttachmentInfo( COMMON_DATA & CommonData, int attachNbr, LPTSTR & attachName, DWORD & attachSize, int & attachType, LPTSTR & attachDescription );
extern void releaseAttachmentInfo ( COMMON_DATA & CommonData );
extern int  collectAttachmentInfo ( COMMON_DATA & CommonData, DWORD & totalsize, size_t msgBodySize );
extern void getMaxMsgSize ( COMMON_DATA & CommonData, int buildSMTP, DWORD &length );
extern int  add_one_attachment ( COMMON_DATA & CommonData, Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary,
                                 DWORD startOffset, DWORD &length,
                                 int part, int totalparts, int attachNbr, int * prevAttachType );
extern int  add_attachments ( COMMON_DATA & CommonData, Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary, int nbrOfAttachments );

#endif  // #ifndef __ATTACH_HPP__
