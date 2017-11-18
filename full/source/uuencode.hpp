/*
    uuencode.hpp
*/
#ifndef __UUENCODE_HPP__
#define __UUENCODE_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"

#if BLAT_LITE
#else

  #define INCLUDE_XXENCODE  FALSE

extern void douuencode(COMMON_DATA & CommonData, Buf & source, Buf & out, LPTSTR filename, int part, int lastpart);

  #if INCLUDE_XXENCODE

extern void xxencode(Buf & source, Buf & out, LPTSTR filename, int part, int lastpart );
  #endif // #if INCLUDE_XXENCODE
#endif  // #if BLAT_LITE
#endif  // #ifndef __UUENCODE_HPP__
