/*
    reg.hpp
*/
#ifndef __REG_HPP__
#define __REG_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"


extern int  CreateRegEntry( COMMON_DATA & CommonData, int useHKCU );
extern void ShowRegHelp   ( COMMON_DATA & CommonData );
extern int  DeleteRegEntry( COMMON_DATA & CommonData, HKEY rootKeyLevel, LPTSTR pstrSubDirectory, Buf & pstrProfile );
extern int  GetRegEntry   ( COMMON_DATA & CommonData, Buf & pstrProfile );
extern void ListProfiles  ( COMMON_DATA & CommonData, LPTSTR pstrProfile );

#endif  // #ifndef __REG_HPP__
