/*
    unicode.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "blat.h"
#include "common_data.h"

extern LPTSTR base64table;

extern void base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad);

_TCHAR utf8BOM[] = { __T('\xEF'), __T('\xBB'), __T('\xBF'), 0 };

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

void convertPackedUnicodeToUTF( Buf & sourceText, Buf & outputText, int * pUTF, LPTSTR charset, int pUTFRequested )
{
    int    pUTF8Found;
    int    BOM_found;         /* Byte Order Marker, for Unicode */
    int    tempUTF;

    unsigned short * pp;


    if ( (pUTFRequested != 8) && (pUTFRequested != 7) )
        return;

    pp = (unsigned short *)sourceText.Get();
    if ( !pp )
        return;

    if ( !sourceText.Length() )
        return;

    outputText.Clear();

    pUTF8Found = FALSE;
    BOM_found = FALSE;
    tempUTF = 0;
    if ( pUTF )
        tempUTF = *pUTF;

    if ( pp[0] == 0xFEFF ) {
        BOM_found = TRUE;
        if ( (pp[1] == 0x0000) && !(sourceText.Length() & 1) ) {
            tempUTF = NATIVE_32BIT_UTF;     /* Looks like Unicode 32-bit in native format */
        } else {
            tempUTF = NATIVE_16BIT_UTF;     /* Looks like Unicode 16-bit in native format */
        }
        if ( pUTF )
            *pUTF = tempUTF;
    }
    if ( tempUTF ) {
        unsigned        bufL;
        unsigned long   value;
        unsigned        incrementor;

        incrementor = (tempUTF & 0x07) / sizeof(_TCHAR);

        bufL = (unsigned)(sourceText.Length() / incrementor);
        if ( BOM_found ) {
            pp += incrementor;
            bufL--;
        }
        if ( (pUTFRequested == 7) && (tempUTF == NATIVE_16BIT_UTF) ) {
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
                if ( value == 0xFEFF ) {
                    pp += incrementor;
                    bufL--;
                    continue;
                }
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
                pUTF8Found = TRUE;
                // needHyphen = FALSE;
                for ( ; bufL; ) {
                    value = pp[0];
                    if ( value < 0x0080 ) {
                        if ( (value <= 0x20) || _tcschr(base64table, (_TCHAR)value) || _tcschr(SetD, (_TCHAR)value) || _tcschr(SetO, (_TCHAR)value) )
                            break;

                        tempString.Add( __T('\0') );
                        tempString.Add( (_TCHAR)value );
                        pp += incrementor;
                        bufL--;
                        continue;
                    }
                    for ( ; bufL && (value >= 0x0080); ) {
                        tempString.Add( (_TCHAR)((value >> 8) & 0xFF) );
                        tempString.Add( (_TCHAR)((value >> 0) & 0xFF) );

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
                        if ( (value < 0x0080) && (_tcschr(base64table, (_TCHAR)value) || (value == __T('-'))) ) {
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
                {
                    outputText.Add( __T('-') );
                    prevC = FALSE;
                }
                if ( charset ) {
                    _tcscpy(charset, __T("UTF-7"));
                    charset = 0;
                }

                tempString.Free();
            }
            if ( prevC )
                outputText.Add( __T('-') );  /* terminate the pUTF-7 string */
        } else {
            /*
             * UTF-8 requested, or 32-bit Unicode found.
             * UTF-8 format supports all 16-bit and 32-bit Unicode values.
             */
            for ( ; bufL; ) {
                if ( tempUTF == NATIVE_32BIT_UTF )
                    value = ((unsigned long)(_TUCHAR)pp[1] << 16) + (_TUCHAR)pp[0];
                else
                    value = pp[0];

                if ( value == 0xFEFF ) {
                    pp += incrementor;
                    bufL--;
                    continue;
                }
                if ( value < (1 << 7) ) {
                    outputText.Add((LPTSTR)&value, 1);
                } else {
                    int marker;
                    int mask;
                    int extraCount = 1;
                    int x = 11;

                    if ( (tempUTF & 7) == NATIVE_16BIT_UTF ) {  /* 16-bit Unicode? */
                        if ( (value & ~0x03FFul) == 0xD800ul ) {      /* high half of surrogate pair? */
                            unsigned short secondVal;

                            secondVal = pp[1];
                            if ( (secondVal & ~0x03FF) == 0xDC00 ) {  /* low half of surrogate pair? */
                                value = 0x10000ul + ((value & 0x03FFul) * 0x400ul) + (secondVal & 0x03FFul);
                                pp += incrementor;
                                bufL--;
                            }
                        }
                    }
                    pUTF8Found = TRUE;
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
                        marker = 0x0080;
                        extraCount--;
                    } while ( extraCount );
                    value = (value & 0x3F) | 0x0080;
                    outputText.Add( (LPTSTR)&value, 1 );
                }
                pp += incrementor;
                bufL--;
            }
        }
    }
    if ( pUTF ) {
        if ( pUTF8Found )
            *pUTF = UTF_REQUESTED;
        else
        if ( !BOM_found )
            *pUTF = FALSE;
    }
}


void convertUnicode( Buf &sourceText, int * pUTF, LPTSTR charset, int pUTFRequested )
{
    Buf    outputText;
    int    pUTF8Found;
    int    BOM_found;         /* Byte Order Marker, for Unicode */
    int    tempUTF;
    LPTSTR pp;

    if ( (pUTFRequested != 8) && (pUTFRequested != 7) )
        return;

    pp = sourceText.Get();
    if ( !pp )
        return;

    if ( !sourceText.Length() )
        return;

    outputText.Clear();

#if defined(_UNICODE) || defined(UNICODE)
    if ( pp[0] == 0xFEFF ) {
        convertPackedUnicodeToUTF( sourceText, outputText, pUTF, charset, pUTFRequested );
        sourceText = outputText;
        outputText.Free();
        return;
    }
#else
    if ( (pp[0] == 0x00FF) && (pp[1] == 0x00FE) ) {
        convertPackedUnicodeToUTF( sourceText, outputText, pUTF, charset, pUTFRequested );
        sourceText = outputText;
        outputText.Free();
        return;
    }
#endif

    pUTF8Found = FALSE;
    BOM_found = FALSE;
    tempUTF = 0;
    if ( pUTF )
        tempUTF = *pUTF;

    if ( sourceText.Length() > 2 ) {
        if ( memcmp( pp, utf8BOM, 3*sizeof(_TCHAR) ) == 0 ) {
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
                    pUTF8Found = TRUE;
                    break;
                }
            } while ( *(++pp) );
            if ( pUTF8Found && (pUTFRequested == 7) ) {
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
                        if ( (_TUCHAR)*pp < 0x0080 ) {
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

                            if ( (_TUCHAR)*pp >= 0x00F0 ) {
                                outputText.Free();        /* UTF-7 does not allow 32-bit */
                                return;                   /* Unicode per RFC 2152        */
                            }
                            if ( (_TUCHAR)*pp < 0x00C0 ) {
                                outputText.Free();        /* Marker bytes from 0x80 to   */
                                return;                   /* 0xBF are invalid for UTF-8. */
                            }
                            if ( (_TUCHAR)*pp < 0x00E0 )
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
                            tempString.Add( (_TCHAR)((value >> 0) & 0xFF) );
                        } while ( *pp & 0x0080 );
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
                    {
                        outputText.Add( __T('-') );
                        prevC = FALSE;
                    }
                    if ( charset ) {
                        _tcscpy(charset, __T("UTF-7"));
                        charset = 0;
                    }
                }
                if ( prevC )
                    outputText.Add( __T('-') );  /* terminate the pUTF-7 string */
            }
            sourceText = outputText;
            if ( pUTF8Found ) {
                if ( pUTF )
                    *pUTF = UTF_REQUESTED;
            }
            else {
                if ( pUTF )
                    *pUTF = 0;
            }

            outputText.Free();
            return;
        }

        if ( (pp[0] == 0x00FF) && (pp[1] == 0x00FE) && (pp[2] == 0x0000) && (pp[3] == 0x0000) && !((sourceText.Length() & 3) /sizeof(_TCHAR)) ) {
            BOM_found = TRUE;
            tempUTF = NATIVE_32BIT_UTF;     /* Looks like Unicode 32-bit in native format */
        } else
        if ( (pp[0] == 0x0000) && (pp[1] == 0x0000) && (pp[2] == 0x00FE) && (pp[3] == 0x00FF) && !((sourceText.Length() & 3) /sizeof(_TCHAR)) ) {
            BOM_found = TRUE;
            tempUTF = NON_NATIVE_32BIT_UTF; /* Looks like Unicode 32-bit in non-native format */
        } else
        if ( (pp[0] == 0x00FF) && (pp[1] == 0x00FE) && !((sourceText.Length() & 1) /sizeof(_TCHAR)) ) {
            BOM_found = TRUE;
            tempUTF = NATIVE_16BIT_UTF;     /* Looks like Unicode 16-bit in native format */
        } else
        if ( (pp[0] == 0x00FE) && (pp[1] == 0x00FF) && !((sourceText.Length() & 1) /sizeof(_TCHAR)) ) {
            BOM_found = TRUE;
            tempUTF = NON_NATIVE_16BIT_UTF; /* Looks like Unicode 16-bit in non-native format */
        } else
        if ( pUTF && *pUTF && !((sourceText.Length() & 1) /sizeof(_TCHAR)) )
            tempUTF = NATIVE_16BIT_UTF; /* No BOM found, but user specified Unicode, presume 16-bit native format */
        else
            tempUTF = 0;
    } else
    if ( tempUTF ) {
        if ( sourceText.Length() == 2 )
            tempUTF = NATIVE_16BIT_UTF;
        else
            tempUTF = 0;
    }

    if ( pUTF )
        *pUTF = tempUTF;

    if ( tempUTF ) {
        unsigned        bufL;
        unsigned long   value;
        unsigned        incrementor;
        Buf             holdingPen;

        holdingPen.Clear();

        incrementor = (unsigned)(tempUTF & 0x07);
        value = 0x0000FEFFl;
        holdingPen.Add( (LPTSTR)&value, incrementor /sizeof(_TCHAR) );

        bufL = (unsigned)(sourceText.Length() / incrementor);
        if ( BOM_found ) {
            pp += incrementor;
            bufL--;
        }
        for ( ; bufL; ) {
            if ( tempUTF == NATIVE_32BIT_UTF ) {
                value = ((unsigned long)pp[3] << 24) + ((unsigned long)pp[2] << 16) + ((unsigned long)pp[1] << 8) + (unsigned long)pp[0];
            } else
            if ( tempUTF == NON_NATIVE_32BIT_UTF ) {
                value = ((unsigned long)pp[0] << 24) + ((unsigned long)pp[1] << 16) + ((unsigned long)pp[2] << 8) + (unsigned long)pp[3];
            } else
            if ( tempUTF == NATIVE_16BIT_UTF ) {
                value = (unsigned long)((pp[1] << 8) + pp[0]);
            } else {
                value = (unsigned long)((pp[0] << 8) + pp[1]);
            }
            holdingPen.Add( (LPTSTR)&value, incrementor /sizeof(_TCHAR) );
            pp += incrementor;
            bufL--;
        }

        convertPackedUnicodeToUTF( holdingPen, outputText, pUTF, charset, pUTFRequested );
        sourceText = outputText;
        holdingPen.Free();
    }

    outputText.Free();
}

#if defined(_UNICODE) || defined(UNICODE)

void compactUnicodeFileData( Buf &sourceText )
{
    Buf           outputText;
    int           tempUTF;
    LPTSTR        pp;
    unsigned long value;

    pp = sourceText.Get();
    if ( !pp )
        return;

    if ( !sourceText.Length() )
        return;

    outputText.Clear();
    tempUTF = 0;

    if ( sourceText.Length() > 2 ) {
        if ( memcmp( pp, utf8BOM, 3*sizeof(_TCHAR) ) == 0 ) {
  #if 1
            /*
             * UTF-8 BOM found.
             */
            int    count;
            size_t thisLength;

            thisLength = 3;
            pp += 3;
            outputText.Add( (_TCHAR)0xFEFF );

            for ( ; thisLength < sourceText.Length(); ) {
                if ( (_TUCHAR)*pp < 0x0080 ) {
                    outputText.Add( (_TCHAR)*pp );
                    pp++;
                    thisLength++;
                    continue;
                }
                do {
                    if ( (_TUCHAR)*pp >= 0x00FE ) {
                        outputText.Free();        /* Marker bytes 0xFE and 0xFF  */
                        return;                   /* are invalid for UTF-8.      */
                    }
                    if ( (_TUCHAR)*pp < 0x00C0 ) {
                        outputText.Free();        /* Marker bytes from 0x80 to   */
                        return;                   /* 0xBF are invalid for UTF-8. */
                    }
                    if ( (_TUCHAR)*pp < 0x00E0 )
                        count = 1;
                    else
                    if ( (_TUCHAR)*pp < 0x00F0 )
                        count = 2;
                    else
                    if ( (_TUCHAR)*pp < 0x00F8 )
                        count = 3;
                    else
                    if ( (_TUCHAR)*pp < 0x00FC )
                        count = 4;
                    else
                        count = 5;

                    value = (unsigned long)(*pp & ((1 << (6 - count)) - 1));
                    pp++;
                    thisLength++;
                    for ( ; count; count-- ) {
                        value = (unsigned long)((value << 6) | (*pp & ((1 << 6) - 1)));
                        pp++;
                        thisLength++;
                    }
                    if ( value > 0x10FFFFul ) {
                        outputText.Free();        /* values greater than 0x10FFFF are invalid for Unicode */
                        return;
                    }
                    if ( (0x0D800 <= value) && (value <= 0x0FFFF) ) {
                        outputText.Free();        /* values from 0xD800 - 0xFFFF are invalid for Unicode */
                        return;
                    }
                    if ( value > 0x0FFFF ) {
                        _TCHAR surrogatePair;

                        surrogatePair = (_TCHAR)(((value - 0x10000ul) / 0x0400) + 0xD800);
                        value = (value & 0x03FF) + 0xDC00;
                        outputText.Add( surrogatePair );
                    }
                    outputText.Add( (_TCHAR)value );

                } while ( *pp & 0x0080 );
            }
            sourceText = outputText;
  #endif
            outputText.Free();
            return;
        }

        if ( (pp[0] == 0x00FF) && (pp[1] == 0x00FE) && (pp[2] == 0x0000) && (pp[3] == 0x0000) && !(sourceText.Length() & 1) ) {
            tempUTF = NATIVE_32BIT_UTF;     /* Looks like Unicode 32-bit in native format */
        } else
        if ( (pp[0] == 0x0000) && (pp[1] == 0x0000) && (pp[2] == 0x00FE) && (pp[3] == 0x00FF) && !(sourceText.Length() & 1) ) {
            tempUTF = NON_NATIVE_32BIT_UTF; /* Looks like Unicode 32-bit in non-native format */
        } else
        if ( (pp[0] == 0x00FF) && (pp[1] == 0x00FE) ) {
            tempUTF = NATIVE_16BIT_UTF;     /* Looks like Unicode 16-bit in native format */
        } else
        if ( (pp[0] == 0x00FE) && (pp[1] == 0x00FF) ) {
            tempUTF = NON_NATIVE_16BIT_UTF; /* Looks like Unicode 16-bit in non-native format */
        }
    }

    if ( tempUTF ) {
        unsigned bufL;
        unsigned incrementor;

        outputText.Clear();

        incrementor = (unsigned)(tempUTF & 0x07);
        outputText.Add( (_TCHAR)0xFEFF );

        bufL = (unsigned)(sourceText.Length() / incrementor);
        pp += incrementor;
        bufL--;

        for ( ; bufL; ) {
            if ( tempUTF == NATIVE_32BIT_UTF ) {
                value = ((unsigned long)pp[3] << 24) + ((unsigned long)pp[2] << 16) + ((unsigned long)pp[1] << 8) + (unsigned long)pp[0];
                if ( value > 0x10FFFFul ) {
                    outputText.Free();        /* values greater than 0x10FFFF are invalid for Unicode */
                    return;
                }
                if ( (0x0D800 <= value) && (value <= 0x0FFFF) ) {
                    outputText.Free();        /* values from 0xD800 - 0xFFFF are invalid for Unicode */
                    return;
                }
            } else
            if ( tempUTF == NON_NATIVE_32BIT_UTF ) {
                value = ((unsigned long)pp[0] << 24) + ((unsigned long)pp[1] << 16) + ((unsigned long)pp[2] << 8) + (unsigned long)pp[3];
                if ( value > 0x10FFFFul ) {
                    outputText.Free();        /* values greater than 0x10FFFF are invalid for Unicode */
                    return;
                }
                if ( (0x0D800 <= value) && (value <= 0x0FFFF) ) {
                    outputText.Free();        /* values from 0xD800 - 0xFFFF are invalid for Unicode */
                    return;
                }
            } else
            if ( tempUTF == NATIVE_16BIT_UTF ) {
                value = (unsigned long)((pp[1] << 8) + pp[0]);
            } else {
                value = (unsigned long)((pp[0] << 8) + pp[1]);
            }
            if ( value > 0x0FFFF ) {
                _TCHAR surrogatePair;

                surrogatePair = (_TCHAR)(((value - 0x10000ul) / 0x0400) + 0xD800);
                value = (value & 0x03FF) + 0xDC00;
                outputText.Add( surrogatePair );
            }
            outputText.Add( (_TCHAR)value );
            pp += incrementor;
            bufL--;
        }
        sourceText = outputText;
    }

    outputText.Free();
}


void checkInputForUnicode ( COMMON_DATA & CommonData, Buf & stringToCheck )
{
    size_t    length;
    size_t    x;
    _TUCHAR * pStr;
    Buf       newString;

    newString.Clear();
    compactUnicodeFileData( stringToCheck );
    stringToCheck.SetLength();
    length = stringToCheck.Length();
    pStr = (_TUCHAR *)stringToCheck.Get();
    for ( x = 0; x < length; x++ ) {
        if ( x == 0 ) {
            if ( pStr[0] == 0xFEFF )
                continue;
        }
        if ( pStr[x] > 0x00FF ) {
            if ( pStr[0] != 0xFEFF ) {
                newString.Add( (_TCHAR)0xFEFF );    /* Prepend a UTF-16 BOM */
            }
            newString.Add( (LPTSTR)pStr, length );  /* Now add the user's Unicode string */
            stringToCheck = newString;

            CommonData.utf = UTF_REQUESTED;
  #if BLAT_LITE
            CommonData.mime = TRUE;
  #else
            CommonData.eightBitMimeRequested = TRUE;
  #endif
            break;
        }
        if ( ( pStr[0]   != 0xFEFF) &&
             ( pStr[x  ] <  0x00FE) &&
             ( pStr[x  ] >= 0x00C0) &&
             ((pStr[x+1] & (_TUCHAR)~0x3F) == 0x0080) ) {

            size_t savedLength;

            newString.Add( utf8BOM );
            newString.Add( (LPTSTR)pStr, length );  /* Now add the user's UTF-8 string */
            savedLength = newString.Length();

            compactUnicodeFileData( newString );
            if ( (savedLength != newString.Length()) ||                         /* If the string length changed */
                 (memcmp( newString.Get(), utf8BOM, 3*sizeof(_TCHAR) ) != 0) || /* or if the 1st 3 bytes are not the UTF-8 BOM that I put in */
                 (_tcscmp( stringToCheck, newString.Get()+3 ) != 0) ) {         /* or if the string itself is not the same */
                stringToCheck = newString;
                if ( *newString.Get() == 0xFEFF ) {
                    CommonData.utf = UTF_REQUESTED;
  #if BLAT_LITE
                    CommonData.mime = TRUE;
  #else
                    CommonData.eightBitMimeRequested = TRUE;
  #endif
                }
                else {
                    pStr = (_TUCHAR *)stringToCheck.Get();
                    for ( savedLength = 0; savedLength < stringToCheck.Length(); savedLength++ ) {
                        if ( *pStr > 0x007F ) {
  #if BLAT_LITE
                            CommonData.mime = TRUE;
  #else
                            CommonData.eightBitMimeRequested = TRUE;
  #endif
                            break;
                        }
                    }
                }
            }
            else {
                pStr = (_TUCHAR *)stringToCheck.Get();
                for ( savedLength = 0; savedLength < stringToCheck.Length(); savedLength++ ) {
                    if ( *pStr > 0x007F ) {
  #if BLAT_LITE
                        CommonData.mime = TRUE;
  #else
                        CommonData.eightBitMimeRequested = TRUE;
  #endif
                        break;
                    }
                }
            }
            break;
        }
    }
    newString.Free();
}
#endif
