/*
    blatext.hpp
*/
#ifndef __BLATEXT_HPP__
#define __BLATEXT_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"

extern _TCHAR  blatVersion[];
extern _TCHAR  blatVersionSuf[];
extern _TCHAR  blatBuildDate[];
extern _TCHAR  blatBuildTime[];

extern char    blatBuildDateA[];
extern char    blatBuildTimeA[];

extern LPTSTR  days[];


extern void printMsg(COMMON_DATA & CommonData, LPTSTR p, ... );

#ifdef BLATDLL_EXPORTS // this is blat.dll, not blat.exe
#if defined(__cplusplus)
extern "C" {
#endif
extern void(*pMyPrintDLL)(LPTSTR);
#if defined(__cplusplus)
}
#endif
#else
#define pMyPrintDLL _tprintf
#endif

#ifdef __cplusplus
extern "C" int _tmain( int argc, LPTSTR *argv, LPTSTR *envp );
#endif

#endif  // #ifndef __BLATEXT_HPP__
