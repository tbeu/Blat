/*
    base64.cpp
*/

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <string.h>

#include "blat.h"

// MIME base64 Content-Transfer-Encoding

#define PADCHAR __T('=')

LPTSTR base64table = __T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");


static _TUCHAR base64_dec( _TUCHAR Ch )
{
    if ( Ch == PADCHAR )
        return 0;

    if ( Ch ) {
        LPTSTR p = (LPTSTR)_tcschr( base64table, Ch );

        if ( p )
            return (_TUCHAR)(p - base64table);
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
int base64_decode( Buf & in, Buf & out )
{
    _TUCHAR * pIn;
    _TUCHAR   cnew, cold;
    int       b;

    if ( !in.Get() )
        return 0;

    pIn = (_TUCHAR *)in.Get();
    out.Clear();
    cold = 0;   // Fix a compiler warning.
    for ( b = 0; *pIn && (*pIn != PADCHAR); pIn++ ) {
        if ( (*pIn == __T('\r')) || (*pIn == __T('\n')) )
            continue;

        cnew = base64_dec( *pIn );
        if ( cnew == 0xFF ) {
            if (b)
                out.Add( (_TCHAR)((cold << (2*b)) & 0xFF) );
            break;
        }

        if (b)
            out.Add( (_TCHAR)(((cold << (2*b)) + (cnew >> (6-2*b)))& 0xFF) );

        cold = cnew;
        if (++b == 4)
            b = 0;
    }
    *out.GetTail() = __T('\0');
    return (int)out.Length();
}

int base64_decode( _TUCHAR * in, LPTSTR out )
{
    Buf bufIn;
    Buf bufOut;
    int len;

    if ( !in || !out )
        return 0;

    bufIn = (LPTSTR)in;
    len = base64_decode( bufIn, bufOut );
    if ( bufOut.Length() )
        _tcscpy( out, bufOut.Get() );

    return len;
}


#define B64_Mask(Ch) (_TCHAR) base64table[ (Ch) & 0x3F ]

void base64_encode(Buf & source, Buf & out, int inclCrLf, int inclPad )
{
    size_t    length;
    size_t    tempLength;
    _TUCHAR * in;
    int       bytes_out;
    _TCHAR    tmpstr[80];
    DWORD     bitStream;

    in = (_TUCHAR *) source.Get();
    if ( !in )
        return;

    length = source.Length();
    tempLength = (((length *8)+5)/6);
    tempLength += (((tempLength + 71) / 72) * 2) + 1;
    out.Alloc( out.Length() + tempLength );

    // Ensure the data is padded with NULL to work the for() loop.
    in[length] = __T('\0');

    tmpstr[ 72 ] = __T('\0');

    for ( bytes_out = 0; length > 2; length -= 3, in += 3 ) {
        if ( bytes_out == 72 ) {
            out.Add( tmpstr );
            bytes_out = 0;
            if ( inclCrLf )
                out.Add( __T("\r\n") );
        }

        bitStream = (DWORD)(in[0] << 16) | (DWORD)(in[1] << 8) | in[2];
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >> 18 );
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >> 12 );
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >>  6 );
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream       );
    }

    /* If length == 0, then we're done.
     * If length == 1 at this point, then in[0] is the last byte of the string/file, and in[1] is a binary zero.
     * If length == 2 at this point, then in[0] and in[1] are the last bytes, while in[2] is a binary zero.
     * In all cases, in[2] is not needed.
     */
    if ( length ) {
        if ( bytes_out == 72 ) {
            out.Add( tmpstr );
            if ( inclCrLf )
                out.Add( __T("\r\n") );

            bytes_out = 0;
        }

        bitStream = (DWORD)((in[0] << 8) | in[1]);
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >> 10 );
        tmpstr[ bytes_out++ ] = B64_Mask( bitStream >>  4 );

        if ( length == 2 )
            tmpstr[ bytes_out++ ] = B64_Mask( bitStream << 2 );
        else {
            if ( inclPad )
                tmpstr[ bytes_out++ ] = PADCHAR;
        }

        if ( inclPad )
            tmpstr[ bytes_out++ ] = PADCHAR;
    }

    if ( bytes_out ) {
        tmpstr[ bytes_out ] = __T('\0');
        out.Add( tmpstr );
        if ( inclCrLf )
            out.Add( __T("\r\n") );
    }
}


void base64_encode(_TUCHAR * in, size_t length, LPTSTR out, int inclCrLf)
{
    Buf inBuf;
    Buf outBuf;

    *out = __T('\0');
    if ( !length )
        return;

    inBuf.Add( (LPTSTR)in, length );
    base64_encode( inBuf, outBuf, inclCrLf, TRUE );
    _tcscpy( out, outBuf.Get() );
}
