/*
    unicode.hpp
*/

#ifndef __UNICODE_HPP__
#define __UNICODE_HPP__ 1

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"
#include "common_data.h"

extern _TCHAR const utf8BOM[];

/*
 * This routine will convert 16-bit and 32-bit Unicode files into UTF-7 or
 * UTF-8 format, suitable for email transmission.  If the file is already
 * in UTF-8 format, and there is a Byte Order Marker (BOM), the BOM will
 * be removed and the text scanned for 8-bit data, in order to determine
 * if the character set needs to be changed to UTF-8.
 *
 * If there is no BOM for this text, and if UTF is requested by the user,
 * this code will assume 16-bit Unicode since this is what Microsoft uses
 * with Notepad, regedit, and others.  All text input must contain a BOM
 * if the text is to be recognized properly, rather than guessing.
 */
extern void convertPackedUnicodeToUTF( Buf & sourceText, Buf & outputText, int * pUTF, LPTSTR charset, int pUTFRequested );

extern void convertUnicode( Buf &sourceText, int * pUTF, LPTSTR charset, int pUTFRequested );

#if defined(_UNICODE) || defined(UNICODE)

extern void compactUnicodeFileData( Buf &sourceText );
extern void checkInputForUnicode ( COMMON_DATA & CommonData, Buf & stringToCheck );

#endif  // #if defined(_UNICODE) || defined(UNICODE)
#endif  // #ifndef __UNICODE_HPP__
