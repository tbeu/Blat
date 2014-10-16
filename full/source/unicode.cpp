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

extern const char * base64table;

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
static const char SetD[] = { 39, 40, 41, 44, 45, 46, 47, 58, 63 };
static const char SetO[] = { 33, 34, 35, 36, 37, 38, 42, 59, 60, 61, 62, 64, 91, 93, 94, 95, 96, 123, 124, 125 };

void convertUnicode( Buf &sourceText, int * utf, char * charset, int utfRequested )
{
    int             utf8Found;
    int             BOM_found;         /* Byte Order Marker, for Unicode */
    unsigned char * pp;

    if ( (utfRequested != 8) && (utfRequested != 7) )
        return;

    utf8Found = FALSE;
    BOM_found = FALSE;
    if ( sourceText.Length() > 4 ) {
        pp = (unsigned char *)sourceText.Get();
        if ( (pp[0] == 0xEF) && (pp[1] == 0xBB) && (pp[2] == 0xBF) ) {
            Buf holdingPen;

            /*
             * UTF-8 BOM found.  Check if UTF-8 data actually exists, and
             * set the charset if it does.  If UTF-7 is requested, then
             * convert from UTF-8 to UTF-7, if any of the data is UTF-8.
             */
            holdingPen.Add( (char *)&pp[3] );
            if ( utf )
                *utf = 0;

            pp = (unsigned char *)holdingPen.Get();
            do {
                if ( *pp > 0x7F ) {
                    if ( charset ) {
                        strcpy(charset, "UTF-8");
                    }
                    utf8Found = TRUE;
                    break;
                }
            } while ( *(++pp) );
            if ( utf8Found && (utfRequested == 7) ) {
                int prevC;
                // int needHyphen;

                sourceText = holdingPen;
                pp = (unsigned char *)sourceText.Get();
                holdingPen.Clear();
                prevC = FALSE;
                for ( ; *pp; ) {
                    Buf tempString;

                    if ( *pp == '-' ) {
                        if ( prevC )    /* previous "character" was Unicode? */
                            holdingPen.Add( '-' );

                        holdingPen.Add( (char)*pp );
                        prevC = FALSE;
                        pp++;
                        continue;
                    }
                    if ( *pp == '+' ) {
                        holdingPen.Add( "+-" );
                        prevC = FALSE;
                        pp++;
                        continue;
                    }
                    if ( (*pp <= 0x20) || strchr(base64table, *pp) || strchr(SetD, *pp) || strchr(SetO, *pp) ) {
                        holdingPen.Add( (char)*pp );
                        prevC = FALSE;
                        pp++;
                        continue;
                    }

                    tempString.Clear();
                    prevC = TRUE;
                    // needHyphen = FALSE;
                    for ( ; ; ) {
                        if ( *pp < 0x80 ) {
                            if ( (*pp <= 0x20) || strchr(base64table, *pp) || strchr(SetD, *pp) || strchr(SetO, *pp) )
                                break;

                            tempString.Add( (char)0 );
                            tempString.Add( (char)*pp );
                            pp++;
                            continue;
                        }
                        do {
                            int            count;
                            unsigned short value;

                            if ( *pp >= 0xF0 )  /* UTF-7 does not allow 32-bit */
                                return;         /* Unicode per RFC 2152        */

                            if ( *pp < 0xC0 )   /* Marker bytes from 0x80 to   */
                                return;         /* 0xBF are invalid for UTF-8. */

                            if ( *pp < 0xE0 )
                                count = 1;
                            else
                                count = 2;

                            value = (unsigned short)(*pp & ((1 << (6 - count)) - 1));
                            pp++;
                            for ( ; count; count-- ) {
                                value = (unsigned short)((value << 6) | (*pp & ((1 << 6) - 1)));
                                pp++;
                            }
                            tempString.Add( (char)(value >> 8) );
                            tempString.Add( (char)value );
                        } while ( *pp & 0x80 );
                        for ( ; ; ) {
                            if ( *pp == '+' ) {
                                tempString.Add( (char)0 );
                                tempString.Add( (char)*pp );
                                pp++;
                            } else
                            if ( *pp == '-' ) {
                                if ( ((((tempString.Length()*8)+5)/6)+1) < ((((tempString.Length()+1)*8)+5)/6) )
                                    break;

                                tempString.Add( (char)0 );
                                tempString.Add( (char)*pp );
                                pp++;
                            } else
                            if ( strchr(base64table, *pp) ) {
                                if ( ((((tempString.Length()*8)+5)/6)+1) < ((((tempString.Length()+1)*8)+5)/6) ) {
                                    // needHyphen = TRUE;
                                    break;
                                }
                                tempString.Add( (char)0 );
                                tempString.Add( (char)*pp );
                                pp++;
                            } else
                                break;
                        }
                    }
                    holdingPen.Add('+');  /* marker for base64 encoding to follow */
                    base64_encode( tempString, holdingPen, FALSE, FALSE );
                    // if ( needHyphen )
                        holdingPen.Add('-');

                    if ( charset ) {
                        strcpy(charset, "UTF-7");
                        charset = 0;
                    }
                }
                if ( prevC )
                    holdingPen.Add('-');  /* terminate the utf-7 string */
            }
            sourceText = holdingPen;
        } else
        if ( (*(unsigned long *)pp == 0x0000FEFFl) && !(sourceText.Length() & 3) ) {
            if ( utf )
                *utf = NATIVE_32BIT_UTF;     /* Looks like Unicode 32-bit in native format */
            BOM_found = TRUE;
        } else
        if ( (*(unsigned long *)pp == 0xFFFE0000l) && !(sourceText.Length() & 3) ) {
            if ( utf )
                *utf = NON_NATIVE_32BIT_UTF; /* Looks like Unicode 32-bit in non-native format */
            BOM_found = TRUE;
        } else
        if ( (*(unsigned short *)pp == (unsigned short)0xFEFF) && !(sourceText.Length() & 1) ) {
            if ( utf )
                *utf = NATIVE_16BIT_UTF;     /* Looks like Unicode 16-bit in native format */
            BOM_found = TRUE;
        } else
        if ( (*(unsigned short *)pp == (unsigned short)0xFFFE) && !(sourceText.Length() & 1) ) {
            if ( utf )
                *utf = NON_NATIVE_16BIT_UTF; /* Looks like Unicode 16-bit in non-native format */
            BOM_found = TRUE;
        }
        else
            if ( utf && *utf && !(sourceText.Length() & 1) )
                *utf = NATIVE_16BIT_UTF; /* No BOM found, but user specified Unicode, presume 16-bit native format */
            else
                if ( utf )
                    *utf = 0;
    }
    else
        if ( utf && *utf && !(sourceText.Length() & 1) )
            *utf = NATIVE_16BIT_UTF;
        else
            if ( utf )
                *utf = 0;

    if ( utf && *utf ) {
        Buf             holdingPen;
        unsigned        bufL;
        unsigned long   value;

        holdingPen.Clear();
        pp = (unsigned char *)sourceText.Get();
        bufL = sourceText.Length() / (*utf & 0x07);
        if ( BOM_found ) {
            pp += (*utf & 0x07);
            bufL--;
        }
        if ( (utfRequested == 7) && ((*utf == NATIVE_16BIT_UTF) || (*utf == NON_NATIVE_16BIT_UTF)) ) {
            /*
             * UTF-7 requested.  This format supports only 8-/16-bit Unicode values.
             * In this case, we believe we found 16-bit Unicode.
             */
            int prevC;
            // int needHyphen;

            holdingPen.Alloc(sourceText.Length() * 3);  // maximum byte count for all 16-bit values encoded to 7-bit.
                                                        // does not include hyphens
            prevC = FALSE;
            for ( ; bufL; ) {
                Buf tempString;

                value = *(unsigned short *)pp;
                if ( *utf == NON_NATIVE_16BIT_UTF )
                    value = ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);

                if ( value == '-' ) {
                    if ( prevC )    /* previous "character" was Unicode? */
                        holdingPen.Add( '-' );

                    holdingPen.Add( (char)value );
                    prevC = FALSE;
                    pp += (*utf & 0x07);
                    bufL--;
                    continue;
                }
                if ( value == '+' ) {
                    holdingPen.Add( "+-" );
                    prevC = FALSE;
                    pp += (*utf & 0x07);
                    bufL--;
                    continue;
                }
                if ( (value < 0x7F) && ((value <= 0x20) || strchr(base64table, (char)value) || strchr(SetD, (char)value) || strchr(SetO, (char)value)) ) {
                    holdingPen.Add( (char)value );
                    prevC = FALSE;
                    pp += (*utf & 0x07);
                    bufL--;
                    continue;
                }

                tempString.Clear();
                prevC      = TRUE;
                utf8Found  = TRUE;
                // needHyphen = FALSE;
                for ( ; bufL; ) {
                    value = *(unsigned short *)pp;
                    if ( *utf == NON_NATIVE_16BIT_UTF ) {
                        value = ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);
                    }
                    if ( value < 0x80 ) {
                        if ( (value <= 0x20) || strchr(base64table, (char)value) || strchr(SetD, (char)value) || strchr(SetO, (char)value) )
                            break;

                        tempString.Add( (char)0 );
                        tempString.Add( (char)value );
                        pp += (*utf & 0x07);
                        bufL--;
                        continue;
                    }
                    for ( ; bufL && (value >= 0x80); ) {
                        tempString.Add( (char)(value >> 8) );
                        tempString.Add( (char)value );

                        pp += (*utf & 0x07);
                        bufL--;
                        value = *(unsigned short *)pp;
                        if ( *utf == NON_NATIVE_16BIT_UTF ) {
                            value = ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);
                        }
                    }

                    for ( ; bufL; ) {
                        if ( value == '+' ) {
                            tempString.Add( (char)0 );
                            tempString.Add( (char)value );
                            pp += (*utf & 0x07);
                            bufL--;
                            value = *(unsigned short *)pp;
                            if ( *utf == NON_NATIVE_16BIT_UTF ) {
                                value = ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);
                            }
                        } else
                        if ( value == '-' ) {
                            if ( ((((tempString.Length()*8)+5)/6)+1) < ((((tempString.Length()+2)*8)+5)/6) )
                                break;

                            tempString.Add( (char)0 );
                            tempString.Add( (char)value );
                            pp += (*utf & 0x07);
                            bufL--;
                            value = *(unsigned short *)pp;
                            if ( *utf == NON_NATIVE_16BIT_UTF ) {
                                value = ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);
                            }
                        } else
                        if ( (value < 0x80) && (strchr(base64table, (char)value) || (value == '-')) ) {
                            if ( ((((tempString.Length()*8)+5)/6)+1) < ((((tempString.Length()+2)*8)+5)/6) ) {
                                // needHyphen = TRUE;
                                break;
                            }
                            tempString.Add( (char)0 );
                            tempString.Add( (char)value );
                            pp += (*utf & 0x07);
                            bufL--;
                            value = *(unsigned short *)pp;
                            if ( *utf == NON_NATIVE_16BIT_UTF ) {
                                value = ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);
                            }
                        } else
                            break;
                    }
                }
                holdingPen.Add('+');  /* marker for base64 encoding to follow */
                base64_encode( tempString, holdingPen, FALSE, FALSE );
                // if ( needHyphen )
                    holdingPen.Add('-');

                if ( charset ) {
                    strcpy(charset, "UTF-7");
                    charset = 0;
                }
            }
            if ( prevC )
                holdingPen.Add('-');  /* terminate the utf-7 string */
        } else {
            /*
             * UTF-8 requested, or 32-bit Unicode found.
             * UTF-8 format supports all 16-bit and 32-bit Unicode values.
             */
            if ( (*utf == NATIVE_32BIT_UTF) || (*utf == NON_NATIVE_32BIT_UTF))
                holdingPen.Alloc(sourceText.Length() * 6);  // maximum byte count for all 32-bit values encoded to 8-bit.
            else
                holdingPen.Alloc(sourceText.Length() * 3);  // maximum byte count for all 16-bit values encoded to 8-bit.

            for ( ; bufL; ) {
                if ( *utf == NATIVE_32BIT_UTF ) {
                    value = *(unsigned long *)pp;
                } else
                if ( *utf == NATIVE_16BIT_UTF ) {
                    value = *(unsigned short *)pp;
                } else
                if ( *utf == NON_NATIVE_32BIT_UTF ) {
                    value = *(unsigned long *)pp;
                    value = ((value >> 24) & 0xFF) | ((value >> 8) & 0xFF00) | ((value & 0xFF00) << 8) | ((value & 0xFF) << 24);
                } else {
                    value = *(unsigned short *)pp;
                    value = ((value >> 8) & 0xFF) | ((value & 0xFF) << 8);
                }

                if ( value < (1 << 7) ) {
                    holdingPen.Add((char *)&value, 1);
                } else {
                    int marker;
                    int mask;
                    int extraCount = 1;
                    int x = 11;

                    if ( (*utf & 7) == 2 ) {  /* 16-bit Unicode? */
                        if ( (value & ~0x03FF) == 0xD800 ) {          /* high half of surrogate pair? */
                            unsigned short secondVal = *(unsigned short *)(pp + 2);
                            if ( *utf == NON_NATIVE_16BIT_UTF )
                                secondVal = (unsigned short)(((secondVal >> 8) & 0xFF) | ((secondVal & 0xFF) << 8));
                            if ( (secondVal & ~0x03FF) == 0xDC00 ) {  /* low half of surrogate pair? */
                                value = 0x10000L + (value & 0x03FF) * 0x400 + (secondVal & 0x03FF);
                                pp += 2;
                                bufL--;
                            }
                        }
                    }
                    utf8Found = TRUE;
                    if ( charset ) {
                        strcpy(charset, "UTF-8");
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
                        char c = (char)(((value >> (6*extraCount)) & mask) | marker);
                        holdingPen.Add( &c, 1 );
                        mask = (1 << 6) - 1;
                        marker = 0x80;
                        extraCount--;
                    } while ( extraCount );
                    value = (value & 0x3F) | 0x80;
                    holdingPen.Add( (char *)&value, 1 );
                }
                pp += (*utf & 0x07);
                bufL--;
            }
        }
        sourceText = holdingPen;
    }
    if ( utf8Found && utf )
        *utf = 1;
}
