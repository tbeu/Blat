/*
    yEnc.hpp
*/

#ifndef __YENC_HPP__
#define __YENC_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"

#if SUPPORT_YENC

#include "common_data.h"

extern void yEncode( COMMON_DATA & CommonData, Buf & source, Buf & out, LPTSTR filename, long full_len,
                     int part, int lastpart, unsigned long & full_crc_val );
#endif
#endif  // #ifndef __YENC_HPP__