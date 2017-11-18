/*
    base64.hpp
*/

#ifndef __BASE64_HPP__
#define __BASE64_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"

extern LPTSTR base64table;

extern int  base64_decode( Buf & in, Buf & out );
extern int  base64_decode( _TUCHAR * in, LPTSTR out );
extern void base64_encode( Buf & source, Buf & out, int inclCrLf, int inclPad );
extern void base64_encode( _TUCHAR * in, size_t length, LPTSTR out, int inclCrLf );

#endif  // #ifndef __BASE64_HPP__
