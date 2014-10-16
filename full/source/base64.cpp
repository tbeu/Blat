/*
    base64.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <string.h>

#include "blat.h"

// MIME base64 Content-Transfer-Encoding

#define PADCHAR '='

const char * base64table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


static unsigned char base64_dec( const unsigned char Ch )
{
    if ( Ch == PADCHAR )
        return 0;

    if ( Ch ) {
        char * p = (char *)strchr( base64table, Ch );

        if ( p )
            return (unsigned char)( p - base64table );
    }

    // Any other character returns 0xFF since the character would be invalid.
    return(0xFF);
}

// Changed 2003-11-07, Joseph Calzaretta
//
// base64_decode needs to return an accurate length.
//   It cannot assume that the output is a null-terminated string.
//   It might be an arbitrary byte sequence with nulls in the middle.
//
// Also, if a malformed base64 string is passed to it (wrong length, etc),
//   base64_encode should not overrun the in buffer.  This version
//   assumes every input byte might be the last.  In this case, the
//   malformed portion will be treated as representing zero.
//
int base64_decode(const unsigned char *in, char *out)
{
    char        * outstart = out;
    unsigned char cnew, cold;
    int           b;

    cold = 0;   // Fix a compiler warning.
    for ( b = 0; (*in && (*in != PADCHAR)); in++ ) {
        if ( (*in == '\r') || (*in == '\n') )
            continue;

        cnew = base64_dec( *in );
        if ( cnew == 0xFF ) {
            if (b)
                *out++ = (char)(cold << (2*b));
            break;
        }

        if (b)
            *out++ = (char)((cold << (2*b)) + (cnew >> (6-2*b)));

        cold = cnew;
        if (++b == 4)
            b = 0;
    }
    *out = 0;
    return (int)(out-outstart);
}


#define B64_Mask(Ch) (char) base64table[ (Ch) & 0x3F ]

void base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad )
{
    DWORD           length;
    DWORD           tempLength;
    unsigned char * in;
    int             bytes_out;
    char            tmpstr[80];
    unsigned long   bitStream;

    in     = (unsigned char *) source.Get();
    length = source.Length();
    tempLength = (((length *8)+5)/6);
    tempLength += (((tempLength + 71) / 72) * 2) + 1;
    out.Alloc( out.Length() + tempLength );

    // Ensure the data is padded with NULL to work the for() loop.
    in[length] = '\0';

    tmpstr[ 72 ] = '\0';

    for ( bytes_out = 0; length > 2; length -= 3, in += 3 ) {
        if ( bytes_out == 72 ) {
            out.Add( tmpstr );
            bytes_out = 0;
            if ( inclCrLf )
                out.Add( "\r\n" );
        }

        bitStream = (in[0] << 16) | (in[1] << 8) | in[2];
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >> 18 );
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >> 12 );
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >>  6 );
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream       );
    }

    /* If length == 0, then we're done.
     * If length == 1 at this point, then the in[0] is the last byte of the file, and in[1] is a binary zero.
     * If length == 2 at this point, then in[0] and in[1] are the last bytes, while in[2] is a binary zero.
     * In all cases, in[2] is not needed.
     */
    if ( length ) {
        if ( bytes_out == 72 ) {
            out.Add( tmpstr );
            if ( inclCrLf )
                out.Add( "\r\n" );

            bytes_out = 0;
        }

        bitStream = (in[0] << 8) | in[1];
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >> 10 );
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >>  4 );

        if ( length == 2 )
            tmpstr[ bytes_out++ ] = B64_Mask( bitStream << 2 );
        else
            if ( inclPad )
                tmpstr[ bytes_out++ ] = PADCHAR;

        if ( inclPad )
            tmpstr[ bytes_out++ ] = PADCHAR;
    }

    if ( bytes_out ) {
        tmpstr[ bytes_out ] = '\0';
        out.Add( tmpstr );
        if ( inclCrLf )
            out.Add( "\r\n" );
    }
}


void base64_encode(const unsigned char *in, int length, char *out, int inclCrLf)
{
    Buf inBuf;
    Buf outBuf;

    *out = 0;
    if ( !length )
        return;

    inBuf.Add( (const char *)in, length );
    base64_encode( inBuf, outBuf, inclCrLf, TRUE );
    strcpy( out, outBuf.Get() );
}
