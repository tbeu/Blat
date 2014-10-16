/*
    unicode.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blat.h"

extern LPTSTR base64table;

extern void base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);

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
static const _TCHAR SetD[] = { 39, 40, 41, 44, 45, 46, 47, 58, 63 };
static const _TCHAR SetO[] = { 33, 34, 35, 36, 37, 38, 42, 59, 60, 61, 62, 64, 91, 93, 94, 95, 96, 123, 124, 125 };

void convertPackedUnicodeToUTF( Buf & sourceText, Buf & outputText, int * utf, LPTSTR charset, int utfRequested )
{
    int    utf8Found;
    int    BOM_found;         /* Byte Order Marker, for Unicode */
    int    localutf;

    unsigned short * pp;


    if ( (utfRequested != 8) && (utfRequested != 7) )
        return;

    pp = (unsigned short *)sourceText.Get();
    if ( !pp )
        return;

    if ( !sourceText.Length() )
        return;

    outputText.Clear();

    utf8Found = FALSE;
    BOM_found = FALSE;
    localutf = 0;
    if ( utf )
        localutf = *utf;

    if ( pp[0] == 0xFEFF ) {
        BOM_found = TRUE;
        if ( (pp[1] == 0x0000) && !(sourceText.Length() & 1) ) {
            localutf = NATIVE_32BIT_UTF;     /* Looks like Unicode 32-bit in native format */
        } else {
            localutf = NATIVE_16BIT_UTF;     /* Looks like Unicode 16-bit in native format */
        }
        if ( utf )
            *utf = localutf;
    }
    if ( localutf ) {
        unsigned        bufL;
        unsigned long   value;
        unsigned        incrementor;

        incrementor = (localutf & 0x07) / sizeof(_TCHAR);

        bufL = (unsigned)(sourceText.Length() / incrementor);
        if ( BOM_found ) {
            pp += incrementor;
            bufL--;
        }
        if ( (utfRequested == 7) && (localutf == NATIVE_16BIT_UTF) ) {
            /*
             * UTF-7 requested.  This format supports only 8-/16-bit Unicode values.
             * In this case, we believe we found 16-bit Unicode.
             */
            int prevC;
            // int needHyphen;

            prevC = FALSE;
            for ( ; bufL; ) {
                Buf tempString;

                value = pp[0];
                if ( value == __T('-') ) {
                    if ( prevC )    /* previous "character" was Unicode? */
                        outputText.Add( __T('-') );

                    outputText.Add( (_TCHAR)value );
                    prevC = FALSE;
                    pp += incrementor;
                    bufL--;
                    continue;
                }
                if ( value == __T('+') ) {
                    outputText.Add( __T("+-") );
                    prevC = FALSE;
                    pp += incrementor;
                    bufL--;
                    continue;
                }
                if ( (value < 0x7F) && ((value <= 0x20) || _tcschr(base64table, (_TCHAR)value) || _tcschr(SetD, (_TCHAR)value) || _tcschr(SetO, (_TCHAR)value)) ) {
                    outputText.Add( (_TCHAR)value );
                    prevC = FALSE;
                    pp += incrementor;
                    bufL--;
                    continue;
                }

                tempString.Clear();
                prevC      = TRUE;
                utf8Found  = TRUE;
                // needHyphen = FALSE;
                for ( ; bufL; ) {
                    value = pp[0];
                    if ( value < 0x80 ) {
                        if ( (value <= 0x20) || _tcschr(base64table, (_TCHAR)value) || _tcschr(SetD, (_TCHAR)value) || _tcschr(SetO, (_TCHAR)value) )
                            break;

                        tempString.Add( __T('\0') );
                        tempString.Add( (_TCHAR)value );
                        pp += incrementor;
                        bufL--;
                        continue;
                    }
                    for ( ; bufL && (value >= 0x80); ) {
                        tempString.Add( (_TCHAR)((value >> 8) & 0xFF) );
                        tempString.Add( (_TCHAR)(value & 0xFF) );

                        pp += incrementor;
                        bufL--;
                        value = pp[0];
                    }

                    for ( ; bufL; ) {
                        if ( value == __T('+') ) {
                            tempString.Add( __T('\0') );
                            tempString.Add( (_TCHAR)value );
                            pp += incrementor;
                            bufL--;
                            value = pp[0];
                        } else
                        if ( value == __T('-') ) {
                            if ( ((((tempString.Length()*8)+5)/6)+1) < ((((tempString.Length()+2)*8)+5)/6) )
                                break;

                            tempString.Add( __T('\0') );
                            tempString.Add( (_TCHAR)value );
                            pp += incrementor;
                            bufL--;
                            value = pp[0];
                        } else
                        if ( (value < 0x80) && (_tcschr(base64table, (_TCHAR)value) || (value == __T('-'))) ) {
                            if ( ((((tempString.Length()*8)+5)/6)+1) < ((((tempString.Length()+2)*8)+5)/6) ) {
                                // needHyphen = TRUE;
                                break;
                            }
                            tempString.Add( __T('\0') );
                            tempString.Add( (_TCHAR)value );
                            pp += incrementor;
                            bufL--;
                            value = pp[0];
                        } else
                            break;
                    }
                }
                outputText.Add( __T('+') );  /* marker for base64 encoding to follow */
                base64_encode( tempString, outputText, FALSE, FALSE );
                // if ( needHyphen )
                    outputText.Add( __T('-') );

                if ( charset ) {
                    _tcscpy(charset, __T("UTF-7"));
                    charset = 0;
                }

                tempString.Free();
            }
            if ( prevC )
                outputText.Add( __T('-') );  /* terminate the utf-7 string */
        } else {
            /*
             * UTF-8 requested, or 32-bit Unicode found.
             * UTF-8 format supports all 16-bit and 32-bit Unicode values.
             */
            for ( ; bufL; ) {
                if ( localutf == NATIVE_32BIT_UTF )
                    value = ((unsigned long)(_TUCHAR)pp[1] << 16) + (_TUCHAR)pp[0];
                else
                    value = pp[0];

                if ( value < (1 << 7) ) {
                    outputText.Add((LPTSTR)&value, 1);
                } else {
                    int marker;
                    int mask;
                    int extraCount = 1;
                    int x = 11;

                    if ( (localutf & 7) == NATIVE_16BIT_UTF ) {  /* 16-bit Unicode? */
                        if ( (value & ~0x03FF) == 0xD800 ) {          /* high half of surrogate pair? */
                            unsigned short secondVal;

                            secondVal = pp[1];
                            if ( (secondVal & ~0x03FF) == 0xDC00 ) {  /* low half of surrogate pair? */
                                value = 0x10000L + ((value & 0x03FF) * 0x400l) + (secondVal & 0x03FF);
                                pp += incrementor;
                                bufL--;
                            }
                        }
                    }
                    utf8Found = TRUE;
                    if ( charset ) {
                        _tcscpy(charset, __T("UTF-8"));
                        charset = 0;
                    }
                    do {
                        if ( value < (unsigned long)(1L << x) )
                            break;

                        extraCount++;
                        x += 5;
                    } while ( x < 26 );

                    marker = (unsigned char)(((1 << (extraCount + 1)) - 1) << (7 - extraCount));
                    mask   = (unsigned char)((1 << (6 - extraCount)) - 1);
                    do {
                        _TCHAR c = (_TCHAR)(((value >> (6*extraCount)) & mask) | marker);
                        outputText.Add( &c, 1 );
                        mask = (1 << 6) - 1;
                        marker = 0x80;
                        extraCount--;
                    } while ( extraCount );
                    value = (value & 0x3F) | 0x80;
                    outputText.Add( (LPTSTR)&value, 1 );
                }
                pp += incrementor;
                bufL--;
            }
        }
        outputText = outputText;
    }
    if ( utf ) {
        if ( utf8Found )
            *utf = UTF_REQUESTED;
        else
            *utf = FALSE;
    }
}


void convertUnicode( Buf &sourceText, int * utf, LPTSTR charset, int utfRequested )
{
    Buf    outputText;
    int    utf8Found;
    int    BOM_found;         /* Byte Order Marker, for Unicode */
    int    localutf;
    LPTSTR pp;

    if ( (utfRequested != 8) && (utfRequested != 7) )
        return;

    pp = sourceText.Get();
    if ( !pp )
        return;

    if ( !sourceText.Length() )
        return;

    outputText.Clear();

#if defined(_UNICODE) || defined(UNICODE)
    if ( pp[0] == 0xFEFF ) {
        convertPackedUnicodeToUTF( sourceText, outputText, utf, charset, utfRequested );
        sourceText = outputText;
        outputText.Free();
        return;
    }
#else
    if ( (pp[0] == 0xFF) && (pp[1] == 0xFE) ) {
        convertPackedUnicodeToUTF( sourceText, outputText, utf, charset, utfRequested );
        sourceText = outputText;
        outputText.Free();
        return;
    }
#endif

    utf8Found = FALSE;
    BOM_found = FALSE;
    localutf = 0;
    if ( utf )
        localutf = *utf;

    if ( sourceText.Length() > 2 ) {
        if ( (pp[0] == 0xEF) && (pp[1] == 0xBB) && (pp[2] == 0xBF) ) {
            /*
             * UTF-8 BOM found.  Check if UTF-8 data actually exists, and
             * set the charset if it does.  If UTF-7 is requested, then
             * convert from UTF-8 to UTF-7, if any of the data is UTF-8.
             */
            outputText.Add( (LPTSTR)&pp[3] );

            pp = outputText.Get();
            do {
                if ( (_TUCHAR)*pp > 0x7F ) {
                    if ( charset ) {
                        _tcscpy(charset, __T("UTF-8"));
                    }
                    utf8Found = TRUE;
                    break;
                }
            } while ( *(++pp) );
            if ( utf8Found && (utfRequested == 7) ) {
                int prevC;
                // int needHyphen;

                sourceText = outputText;
                pp = sourceText.Get();
                outputText.Clear();
                prevC = FALSE;
                for ( ; *pp; ) {
                    Buf tempString;

                    if ( *pp == __T('-') ) {
                        if ( prevC )    /* previous "character" was Unicode? */
                            outputText.Add( __T('-') );

                        outputText.Add( (_TCHAR)*pp );
                        prevC = FALSE;
                        pp++;
                        continue;
                    }
                    if ( *pp == __T('+') ) {
                        outputText.Add( __T("+-") );
                        prevC = FALSE;
                        pp++;
                        continue;
                    }
                    if ( ((_TUCHAR)*pp <= 0x20) || _tcschr(base64table, *pp) || _tcschr(SetD, *pp) || _tcschr(SetO, *pp) ) {
                        outputText.Add( (_TCHAR)*pp );
                        prevC = FALSE;
                        pp++;
                        continue;
                    }

                    tempString.Clear();
                    prevC = TRUE;
                    // needHyphen = FALSE;
                    for ( ; ; ) {
                        if ( (_TUCHAR)*pp < 0x80 ) {
                            if ( ((_TUCHAR)*pp <= 0x20) || _tcschr(base64table, *pp) || _tcschr(SetD, *pp) || _tcschr(SetO, *pp) )
                                break;

                            tempString.Add( __T('\0') );
                            tempString.Add( (_TCHAR)*pp );
                            pp++;
                            continue;
                        }
                        do {
                            int            count;
                            unsigned short value;

                            if ( (_TUCHAR)*pp >= 0xF0 ) {
                                outputText.Free();        /* UTF-7 does not allow 32-bit */
                                return;                   /* Unicode per RFC 2152        */
                            }
                            if ( (_TUCHAR)*pp < 0xC0 ) {
                                outputText.Free();        /* Marker bytes from 0x80 to   */
                                return;                   /* 0xBF are invalid for UTF-8. */
                            }
                            if ( (_TUCHAR)*pp < 0xE0 )
                                count = 1;
                            else
                                count = 2;

                            value = (unsigned short)(*pp & ((1 << (6 - count)) - 1));
                            pp++;
                            for ( ; count; count-- ) {
                                value = (unsigned short)((value << 6) | (*pp & ((1 << 6) - 1)));
                                pp++;
                            }
                            tempString.Add( (_TCHAR)((value >> 8) & 0xFF) );
                            tempString.Add( (_TCHAR)(value & 0xFF) );
                        } while ( *pp & 0x80 );
                        for ( ; ; ) {
                            if ( *pp == __T('+') ) {
                                tempString.Add( __T('\0') );
                                tempString.Add( (_TCHAR)*pp );
                                pp++;
                            } else
                            if ( *pp == __T('-') ) {
                                if ( ((((tempString.Length()*8)+5)/6)+1) < ((((tempString.Length()+1)*8)+5)/6) )
                                    break;

                                tempString.Add( __T('\0') );
                                tempString.Add( (_TCHAR)*pp );
                                pp++;
                            } else
                            if ( _tcschr(base64table, *pp) ) {
                                if ( ((((tempString.Length()*8)+5)/6)+1) < ((((tempString.Length()+1)*8)+5)/6) ) {
                                    // needHyphen = TRUE;
                                    break;
                                }
                                tempString.Add( __T('\0') );
                                tempString.Add( (_TCHAR)*pp );
                                pp++;
                            } else
                                break;
                        }
                    }
                    outputText.Add( __T('+') );  /* marker for base64 encoding to follow */
                    base64_encode( tempString, outputText, FALSE, FALSE );
                    tempString.Free();
                    // if ( needHyphen )
                        outputText.Add( __T('-') );

                    if ( charset ) {
                        _tcscpy(charset, __T("UTF-7"));
                        charset = 0;
                    }
                }
                if ( prevC )
                    outputText.Add( __T('-') );  /* terminate the utf-7 string */
            }
            sourceText = outputText;
            localutf = 0;

        } else
        if ( (pp[0] == 0xFF) && (pp[1] == 0xFE) && (pp[2] == 0x00) && (pp[3] == 0x00) && !(sourceText.Length() & 3) ) {
            BOM_found = TRUE;
            localutf = NATIVE_32BIT_UTF;     /* Looks like Unicode 32-bit in native format */
        } else
        if ( (pp[0] == 0x00) && (pp[1] == 0x00) && (pp[2] == 0xFE) && (pp[3] == 0xFF) && !(sourceText.Length() & 3) ) {
            BOM_found = TRUE;
            localutf = NON_NATIVE_32BIT_UTF; /* Looks like Unicode 32-bit in non-native format */
        } else
        if ( (pp[0] == 0xFF) && (pp[1] == 0xFE) && !(sourceText.Length() & 1) ) {
            BOM_found = TRUE;
            localutf = NATIVE_16BIT_UTF;     /* Looks like Unicode 16-bit in native format */
        } else
        if ( (pp[0] == 0xFE) && (pp[1] == 0xFF) && !(sourceText.Length() & 1) ) {
            BOM_found = TRUE;
            localutf = NON_NATIVE_16BIT_UTF; /* Looks like Unicode 16-bit in non-native format */
        } else
        if ( utf && *utf && !(sourceText.Length() & 1) )
            localutf = NATIVE_16BIT_UTF; /* No BOM found, but user specified Unicode, presume 16-bit native format */
        else
            localutf = 0;
    } else
    if ( localutf ) {
        if ( sourceText.Length() == 2 )
            localutf = NATIVE_16BIT_UTF;
        else
            localutf = 0;
    }

    if ( utf )
        *utf = localutf;

    if ( localutf ) {
        unsigned        bufL;
        unsigned long   value;
        unsigned        incrementor;
        Buf             holdingPen;

        holdingPen.Clear();

        incrementor = (localutf & 0x07);
        value = 0x0000FEFFl;
        holdingPen.Add( (LPTSTR)&value, incrementor /sizeof(_TCHAR) );

        bufL = (unsigned)(sourceText.Length() / incrementor);
        if ( BOM_found ) {
            pp += incrementor;
            bufL--;
        }
        for ( ; bufL; ) {
            if ( localutf == NATIVE_32BIT_UTF ) {
                value = ((unsigned long)pp[3] << 24) + ((unsigned long)pp[2] << 16) + ((unsigned long)pp[1] << 8) + (unsigned long)pp[0];
            } else
            if ( localutf == NATIVE_16BIT_UTF ) {
                value = (pp[1] << 8) + pp[0];
            } else
            if ( localutf == NON_NATIVE_32BIT_UTF ) {
                value = ((unsigned long)pp[0] << 24) + ((unsigned long)pp[1] << 16) + ((unsigned long)pp[2] << 8) + (unsigned long)pp[3];
            } else {
                value = (pp[0] << 8) + pp[1];
            }
            holdingPen.Add( (LPTSTR)&value, incrementor /sizeof(_TCHAR) );
            pp += incrementor;
            bufL--;
        }

        convertPackedUnicodeToUTF( holdingPen, outputText, utf, charset, utfRequested );
        sourceText = outputText;
        holdingPen.Free();
    }
    if ( utf ) {
        if ( utf8Found )
            *utf = UTF_REQUESTED;
        else
            *utf = FALSE;
    }

    outputText.Free();
}
