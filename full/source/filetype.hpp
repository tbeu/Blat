/*
    filetype.hpp
*/
#ifndef __FILETYPE_HPP__
#define __FILETYPE_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"


extern LPTSTR getShortFileName (LPTSTR fileName);
extern void   getContentType (COMMON_DATA & CommonData, Buf & sDestBuffer, LPTSTR foundType, LPTSTR defaultType, LPTSTR sFileName);

#endif  // #ifndef __FILETYPE_HPP__
