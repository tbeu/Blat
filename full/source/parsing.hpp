/*
    parsing.hpp

    This file is for parsing comma delimited strings into individual strings.
    This provides a central source for a common function used for email addresses,
    and attachment names.
*/
#ifndef __PARSING_HPP__
#define __PARSING_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"

extern void parseCommaDelimitString ( COMMON_DATA & CommonData, LPTSTR source, Buf & parsed_strings, int pathNames );

#endif  // #ifndef __PARSING_HPP__
