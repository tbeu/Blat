/*
    mime.hpp
*/
#ifndef __MIME_HPP__
#define __MIME_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"


extern int  CheckIfNeedQuotedPrintable(COMMON_DATA & CommonData, LPTSTR pszStr, int inHeader);
extern int  GetLengthQuotedPrintable(COMMON_DATA & CommonData, LPTSTR pszStr, int inHeader);
extern void ConvertToQuotedPrintable(COMMON_DATA & CommonData, Buf & source, Buf & out, int inHeader);

#endif  // #ifndef __MIME_HPP__
