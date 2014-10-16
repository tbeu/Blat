/*
    uuencode.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>

#include "blat.h"

#if BLAT_LITE
#else

  #define INCLUDE_XXENCODE  FALSE

unsigned int uuencodeBytesLine = 45;

static unsigned int encodedLineLength;

extern LPCSTR GetNameWithoutPath(LPCSTR lpFn);


  #define UU_Mask(Ch) (char) ((((Ch) - 1) & 0x3F) + 0x21)

void douuencode(Buf & source, Buf & out, const char * filename, int part, int lastpart)
{
    DWORD           filesize;
    DWORD           tempLength;
    char            tmpstr[88];
    unsigned char * p;
    unsigned int    bytes_out;
    unsigned long   bitStream;

    if ( part < 2 ) {
        out.Add( "begin 644 " );
        out.Add( GetNameWithoutPath(filename) );
        out.Add( "\r\n" );
    }

    p = (unsigned char *) source.Get();
    filesize = source.Length();
    tempLength = (((filesize *8)+5)/6);
    tempLength += (((tempLength + (uuencodeBytesLine * 4/3) - 1) / (uuencodeBytesLine * 4/3)) * 3) + 1;
    out.Alloc( out.Length() + tempLength );

    // Ensure the data is padded with NULL to work the for() loop.
    p[filesize] = '\0';

    encodedLineLength = (uuencodeBytesLine * 4) / 3;

    for ( bytes_out = 0; filesize > 2; filesize -= 3, p += 3 ) {
        if ( bytes_out > encodedLineLength ) {
            tmpstr[ bytes_out ] = 0;
            out.Add( tmpstr );
            out.Add( "\r\n" );
            bytes_out = 0;
        }

        if ( bytes_out == 0 )
            tmpstr[ bytes_out++ ] = UU_Mask((filesize > uuencodeBytesLine) ? uuencodeBytesLine : filesize);

        bitStream = (p[0] << 16) | (p[1] << 8) | p[2];
        tmpstr[ bytes_out++ ] = UU_Mask( bitStream >> 18 );
        tmpstr[ bytes_out++ ] = UU_Mask( bitStream >> 12 );
        tmpstr[ bytes_out++ ] = UU_Mask( bitStream >>  6 );
        tmpstr[ bytes_out++ ] = UU_Mask( bitStream       );
    }

    /* If filesize == 0, then we're done.
     * If filesize == 1 at this point, then the p[0] is the last byte of the file, and p[1] is a binary zero.
     * If filesize == 2 at this point, then p[0] and p[1] are the last bytes, while p[2] is a binary zero.
     * In all cases, p[2] is not needed.
     */
    if ( filesize ) {
        if ( bytes_out > encodedLineLength ) {
            tmpstr[ bytes_out ] = 0;
            out.Add( tmpstr );
            out.Add( "\r\n" );
            bytes_out = 0;
        }

        if ( bytes_out == 0 )
            tmpstr[ bytes_out++ ] = UU_Mask( filesize );

        bitStream = (p[0] << 8) | p[1];
        tmpstr[ bytes_out++ ] = UU_Mask( bitStream >> 10 );
        tmpstr[ bytes_out++ ] = UU_Mask( bitStream >>  4 );

        if ( filesize == 2 )
            tmpstr[ bytes_out++ ] = UU_Mask( bitStream << 2 );
    }

    if ( bytes_out ) {
        tmpstr[ bytes_out ] = '\0';
        out.Add( tmpstr );
        out.Add( "\r\n" );
    }

    if ( part == lastpart )
        out.Add( "\x60\r\nend\r\n" );   // x60 represents a zero length record, marking the end.
}


  #if INCLUDE_XXENCODE

  #define XX_Mask(Ch) (char) xxenctable[ (Ch) & 0x3F ]

static const char xxenctable[]  = "+-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void xxencode(Buf & source, Buf & out, char * filename, int part, int lastpart )
{
    DWORD           filesize;
    char            tmpstr[88];
    unsigned char * p;
    unsigned int    bytes_out;
    unsigned long   bitStream;

    if ( part < 2 ) {
        out.Add( "begin 700 " );
        out.Add( GetNameWithoutPath(filename) );
        out.Add( "\r\n" );
    }

    p = (unsigned char *) source.Get();
    filesize = source.Length();
    out.Alloc( out.Length() + (filesize*2) + 4 );

    // Ensure the data is padded with NULL to work the for() loop.
    p[filesize] = '\0';

    encodedLineLength = (uuencodeBytesLine * 4) / 3;

    for ( bytes_out = 0; filesize > 2; filesize -= 3, p += 3 ) {
        if ( bytes_out > encodedLineLength ) {
            tmpstr[ bytes_out ] = 0;
            out.Add( tmpstr );
            out.Add( "\r\n" );
            bytes_out = 0;
        }

        if ( bytes_out == 0 )
            tmpstr[ bytes_out++ ] = XX_Mask((filesize > uuencodeBytesLine) ? uuencodeBytesLine : filesize);

        bitStream = (p[0] << 16) | (p[1] << 8) | p[2];
        tmpstr[ bytes_out++ ] = XX_Mask( bitStream >> 18 );
        tmpstr[ bytes_out++ ] = XX_Mask( bitStream >> 12 );
        tmpstr[ bytes_out++ ] = XX_Mask( bitStream >>  6 );
        tmpstr[ bytes_out++ ] = XX_Mask( bitStream       );
    }

    /* If filesize == 0, then we're done.
     * If filesize == 1 at this point, then the p[0] is the last byte of the file, and p[1] is a binary zero.
     * If filesize == 2 at this point, then p[0] and p[1] are the last bytes, while p[2] is a binary zero.
     * In all cases, p[2] is not needed.
     */
    if ( filesize ) {
        if ( bytes_out > encodedLineLength ) {
            tmpstr[ bytes_out ] = 0;
            out.Add( tmpstr );
            out.Add( "\r\n" );
            bytes_out = 0;
        }

        if ( bytes_out == 0 )
            tmpstr[ bytes_out++ ] = XX_Mask( filesize );

        bitStream = (p[0] << 8) | p[1];
        tmpstr[ bytes_out++ ] = XX_Mask( bitStream >> 10 );
        tmpstr[ bytes_out++ ] = XX_Mask( bitStream >>  4 );

        if ( filesize == 2 )
            tmpstr[ bytes_out++ ] = XX_Mask( bitStream << 2 );
    }

    if ( bytes_out ) {
        tmpstr[ bytes_out ] = '\0';
        out.Add( tmpstr );
        out.Add( "\r\n" );
    }

    if ( part == lastpart )
        out.Add( "end\r\n" );
}
  #endif
#endif
