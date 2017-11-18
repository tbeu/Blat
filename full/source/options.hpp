/*
    options.hpp
*/
#ifndef __OPTIONS_HPP__
#define __OPTIONS_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"

extern _BLATOPTIONS blatOptionsList[];

extern void printTitleLine( COMMON_DATA & CommonData, int quiet );
extern void print_usage_line( COMMON_DATA & CommonData, LPTSTR pUsageLine );
extern int  printUsage( COMMON_DATA & CommonData, int optionPtr );
extern int  processOptions( COMMON_DATA & CommonData, int argc, LPTSTR * argv, int startargv, int preprocessing );

#endif  // #ifndef __OPTIONS_HPP__
