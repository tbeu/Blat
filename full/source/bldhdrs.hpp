/*
    bldhdrs.hpp
*/
#ifndef __BLDHDRS_HPP__
#define __BLDHDRS_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"


extern void incrementBoundary( _TCHAR boundary[] );
extern void decrementBoundary( _TCHAR boundary[] );
extern void addStringToHeaderNoQuoting(LPTSTR string, Buf & outS, int & headerLen, int linewrap );
extern void fixup( COMMON_DATA & CommonData, LPTSTR string, Buf * outString, int headerLen, int linewrap );
extern void fixupFileName ( COMMON_DATA & CommonData, LPTSTR filename, Buf & outString, int headerLen, int linewrap );
extern void fixupEmailHeaders(COMMON_DATA & CommonData, LPTSTR string, Buf * outString, int headerLen, int linewrap );
extern LPTSTR getCharsetString( COMMON_DATA & CommonData );
extern void build_headers( COMMON_DATA & CommonData, BLDHDRS & bldHdrs );

#endif  // #ifndef __BLDHDRS_HPP__
