/*
    msgbody.hpp
*/
#ifndef __MSGBODY_HPP__
#define __MSGBODY_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"


extern int add_message_body ( COMMON_DATA & CommonData,
                              Buf &messageBuffer, size_t msgBodySize, Buf &multipartHdrs, int buildSMTP,
                              LPTSTR attachment_boundary, DWORD startOffset, int part,
                              int attachNbr );
extern void add_msg_boundary ( COMMON_DATA & CommonData, Buf &messageBuffer, int buildSMTP, LPTSTR attachment_boundary );

#endif  // #ifndef __MSGBODY_HPP__
