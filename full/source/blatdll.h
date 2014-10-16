#if !defined(__BLATDLL_H__)
#define __BLATDLL_H__

#include <tchar.h>

#if defined(_UNICODE) || defined(UNICODE)
#  define lcszBlatFunctionName_Send         "SendW"
#  define lcszBlatFunctionName_Blat         "BlatW"
#  define lcszBlatFunctionName_cSend        "cSendW"
#  define lcszBlatFunctionName_SetPrintFunc "SetPrintFuncW"
#else
#  define lcszBlatFunctionName_Send         "SendA"
#  define lcszBlatFunctionName_Blat         "BlatA"
#  define lcszBlatFunctionName_cSend        "cSendA"
#  define lcszBlatFunctionName_SetPrintFunc "SetPrintFuncA"
#endif

extern "C" __declspec(dllexport) int APIENTRY Send( LPCTSTR sCmd );

extern "C" __declspec(dllexport) int _stdcall Blat( int argc, LPTSTR argv[] );

extern "C" __declspec(dllexport) void _stdcall SetPrintFunc( void (__stdcall *func)(LPTSTR) );

extern "C" __declspec(dllexport) int __cdecl cSend ( LPCTSTR sCmd );

#endif          /* #if !defined(__BLATDLL_H__) */
