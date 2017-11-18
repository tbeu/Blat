/*
 *  RFC 1321 compliant MD5 implementation
 *
 *  Copyright (C) 2001-2003  Christophe Devine
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "declarations.h"

#include <tchar.h>
#include <windows.h>
#include <string.h>
#ifndef DWORD
#define DWORD unsigned long int
#endif

#include "md5.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GET_DWORD(n,b,i)                               \
{                                                       \
    (n) = ( (DWORD) ( (b)[(i)    ] & 0x0FF)       )  \
        | ( (DWORD) ( (b)[(i) + 1] & 0x0FF) <<  8 )  \
        | ( (DWORD) ( (b)[(i) + 2] & 0x0FF) << 16 )  \
        | ( (DWORD) ( (b)[(i) + 3] & 0x0FF) << 24 ); \
}

#define PUT_DWORD(n,b,i)                               \
{                                                       \
    (b)[(i)    ] = (_TUCHAR) (( (n)       ) & 0x0FF);   \
    (b)[(i) + 1] = (_TUCHAR) (( (n) >>  8 ) & 0x0FF);   \
    (b)[(i) + 2] = (_TUCHAR) (( (n) >> 16 ) & 0x0FF);   \
    (b)[(i) + 3] = (_TUCHAR) (( (n) >> 24 ) & 0x0FF);   \
}

void _cdecl md5_starts( md5_context *ctx )
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301ul;
    ctx->state[1] = 0xEFCDAB89ul;
    ctx->state[2] = 0x98BADCFEul;
    ctx->state[3] = 0x10325476ul;
}

void md5_process( md5_context *ctx, _TUCHAR data[64] )
{
    DWORD X[16];
    DWORD A, B, C, D;

    GET_DWORD( X[0],  data,  0 );
    GET_DWORD( X[1],  data,  4 );
    GET_DWORD( X[2],  data,  8 );
    GET_DWORD( X[3],  data, 12 );
    GET_DWORD( X[4],  data, 16 );
    GET_DWORD( X[5],  data, 20 );
    GET_DWORD( X[6],  data, 24 );
    GET_DWORD( X[7],  data, 28 );
    GET_DWORD( X[8],  data, 32 );
    GET_DWORD( X[9],  data, 36 );
    GET_DWORD( X[10], data, 40 );
    GET_DWORD( X[11], data, 44 );
    GET_DWORD( X[12], data, 48 );
    GET_DWORD( X[13], data, 52 );
    GET_DWORD( X[14], data, 56 );
    GET_DWORD( X[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFFul) >> (32 - n)))

#define P(a,b,c,d,k,s,t)                                \
{                                                       \
    a += F(b,c,d) + X[k] + t; a = S(a,s) + b;           \
}

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x,y,z) (z ^ (x & (y ^ z)))

    P( A, B, C, D,  0,  7, 0xD76AA478ul );
    P( D, A, B, C,  1, 12, 0xE8C7B756ul );
    P( C, D, A, B,  2, 17, 0x242070DBul );
    P( B, C, D, A,  3, 22, 0xC1BDCEEEul );
    P( A, B, C, D,  4,  7, 0xF57C0FAFul );
    P( D, A, B, C,  5, 12, 0x4787C62Aul );
    P( C, D, A, B,  6, 17, 0xA8304613ul );
    P( B, C, D, A,  7, 22, 0xFD469501ul );
    P( A, B, C, D,  8,  7, 0x698098D8ul );
    P( D, A, B, C,  9, 12, 0x8B44F7AFul );
    P( C, D, A, B, 10, 17, 0xFFFF5BB1ul );
    P( B, C, D, A, 11, 22, 0x895CD7BEul );
    P( A, B, C, D, 12,  7, 0x6B901122ul );
    P( D, A, B, C, 13, 12, 0xFD987193ul );
    P( C, D, A, B, 14, 17, 0xA679438Eul );
    P( B, C, D, A, 15, 22, 0x49B40821ul );

#undef F

#define F(x,y,z) (y ^ (z & (x ^ y)))

    P( A, B, C, D,  1,  5, 0xF61E2562ul );
    P( D, A, B, C,  6,  9, 0xC040B340ul );
    P( C, D, A, B, 11, 14, 0x265E5A51ul );
    P( B, C, D, A,  0, 20, 0xE9B6C7AAul );
    P( A, B, C, D,  5,  5, 0xD62F105Dul );
    P( D, A, B, C, 10,  9, 0x02441453ul );
    P( C, D, A, B, 15, 14, 0xD8A1E681ul );
    P( B, C, D, A,  4, 20, 0xE7D3FBC8ul );
    P( A, B, C, D,  9,  5, 0x21E1CDE6ul );
    P( D, A, B, C, 14,  9, 0xC33707D6ul );
    P( C, D, A, B,  3, 14, 0xF4D50D87ul );
    P( B, C, D, A,  8, 20, 0x455A14EDul );
    P( A, B, C, D, 13,  5, 0xA9E3E905ul );
    P( D, A, B, C,  2,  9, 0xFCEFA3F8ul );
    P( C, D, A, B,  7, 14, 0x676F02D9ul );
    P( B, C, D, A, 12, 20, 0x8D2A4C8Aul );

#undef F

#define F(x,y,z) (x ^ y ^ z)

    P( A, B, C, D,  5,  4, 0xFFFA3942ul );
    P( D, A, B, C,  8, 11, 0x8771F681ul );
    P( C, D, A, B, 11, 16, 0x6D9D6122ul );
    P( B, C, D, A, 14, 23, 0xFDE5380Cul );
    P( A, B, C, D,  1,  4, 0xA4BEEA44ul );
    P( D, A, B, C,  4, 11, 0x4BDECFA9ul );
    P( C, D, A, B,  7, 16, 0xF6BB4B60ul );
    P( B, C, D, A, 10, 23, 0xBEBFBC70ul );
    P( A, B, C, D, 13,  4, 0x289B7EC6ul );
    P( D, A, B, C,  0, 11, 0xEAA127FAul );
    P( C, D, A, B,  3, 16, 0xD4EF3085ul );
    P( B, C, D, A,  6, 23, 0x04881D05ul );
    P( A, B, C, D,  9,  4, 0xD9D4D039ul );
    P( D, A, B, C, 12, 11, 0xE6DB99E5ul );
    P( C, D, A, B, 15, 16, 0x1FA27CF8ul );
    P( B, C, D, A,  2, 23, 0xC4AC5665ul );

#undef F

#define F(x,y,z) (y ^ (x | ~z))

    P( A, B, C, D,  0,  6, 0xF4292244ul );
    P( D, A, B, C,  7, 10, 0x432AFF97ul );
    P( C, D, A, B, 14, 15, 0xAB9423A7ul );
    P( B, C, D, A,  5, 21, 0xFC93A039ul );
    P( A, B, C, D, 12,  6, 0x655B59C3ul );
    P( D, A, B, C,  3, 10, 0x8F0CCC92ul );
    P( C, D, A, B, 10, 15, 0xFFEFF47Dul );
    P( B, C, D, A,  1, 21, 0x85845DD1ul );
    P( A, B, C, D,  8,  6, 0x6FA87E4Ful );
    P( D, A, B, C, 15, 10, 0xFE2CE6E0ul );
    P( C, D, A, B,  6, 15, 0xA3014314ul );
    P( B, C, D, A, 13, 21, 0x4E0811A1ul );
    P( A, B, C, D,  4,  6, 0xF7537E82ul );
    P( D, A, B, C, 11, 10, 0xBD3AF235ul );
    P( C, D, A, B,  2, 15, 0x2AD7D2BBul );
    P( B, C, D, A,  9, 21, 0xEB86D391ul );

#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}

void _cdecl md5_update( md5_context *ctx, _TUCHAR *input, size_t length )
{
    DWORD left, fill;

    if( ! length ) return;

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += length;
    ctx->total[0] &= 0xFFFFFFFFul;

    if( ctx->total[0] < length )
        ctx->total[1]++;

    if( left && length >= fill )
    {
        memcpy( (void *) (ctx->buffer + left),
                (void *) input, fill*sizeof(_TCHAR) );
        md5_process( ctx, ctx->buffer );
        length -= fill;
        input  += fill;
        left = 0;
    }

    while( length >= 64 )
    {
        md5_process( ctx, input );
        length -= 64;
        input  += 64;
    }

    if( length )
    {
        memcpy( (void *) (ctx->buffer + left),
                (void *) input, length*sizeof(_TCHAR) );
    }
}

static _TUCHAR md5_padding[64] =
{
 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void _cdecl md5_finish( md5_context *ctx, _TUCHAR digest[16] )
{
    DWORD  last, padn;
    DWORD  high, low;
    _TUCHAR msglen[8];

    high = (DWORD)( ctx->total[0] >> 29 )
         | (DWORD)( ctx->total[1] <<  3 );
    low  = (DWORD)( ctx->total[0] <<  3 );

    PUT_DWORD( low,  msglen, 0 );
    PUT_DWORD( high, msglen, 4 );

    last = (DWORD)(ctx->total[0] & 0x3F);
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    md5_update( ctx, md5_padding, padn );
    md5_update( ctx, msglen, 8 );

    PUT_DWORD( ctx->state[0], digest,  0 );
    PUT_DWORD( ctx->state[1], digest,  4 );
    PUT_DWORD( ctx->state[2], digest,  8 );
    PUT_DWORD( ctx->state[3], digest, 12 );
}

#ifdef TEST

#include <stdio.h>

/*
 * these are the standard RFC 1321 test vectors
 */

static LPTSTR msg[] =
{
    __T(""),
    __T("a"),
    __T("abc"),
    __T("message digest"),
    __T("abcdefghijklmnopqrstuvwxyz"),
    __T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"),
    __T("12345678901234567890123456789012345678901234567890123456789012345678901234567890")
};

static LPTSTR val[] =
{
    __T("d41d8cd98f00b204e9800998ecf8427e"),
    __T("0cc175b9c0f1b6a831c399e269772661"),
    __T("900150983cd24fb0d6963f7d28e17f72"),
    __T("f96b697d7cb7938d525a2f31aaf161d0"),
    __T("c3fcd3d76192e4007dfb496cca67e13b"),
    __T("d174ab98d277d9f5a5611c2c9f419d9f"),
    __T("57edf4a22be3c955ac49da2e2107b67a")
};

int _tmain( int argc, LPTSTR* argv )
{
    FILE *f;
    int i, j;
    _TCHAR output[33];
    md5_context ctx;
    _TUCHAR buf[1000];
    _TUCHAR md5sum[16];

    if( argc < 2 )
    {
        _tprintf( __T("\n MD5 Validation Tests:\n\n") );

        for( i = 0; i < 7; i++ )
        {
            _tprintf( __T(" Test %d "), i + 1 );

            md5_starts( &ctx );
            md5_update( &ctx, (_TUCHAR *) msg[i], _tcslen( msg[i] ) );
            md5_finish( &ctx, md5sum );

            for( j = 0; j < 16; j++ )
            {
                _stprintf( output + j * 2, __T("%02x"), md5sum[j] );
            }

            if( memcmp( output, val[i], 32 ) )
            {
                _tprintf( __T("failed!\n") );
                return( 1 );
            }

            _tprintf( __T("passed.\n") );
        }

        _tprintf( __T("\n") );
    }
    else
    {
        if( ! ( f = _tfopen( argv[1], __T("rb") ) ) )
        {
            _tperror( __T("_tfopen") );
            return( 1 );
        }

        md5_starts( &ctx );

        while( ( i = fread( buf, 1, sizeof( buf ) / sizeof(buf[0]), f ) ) > 0 )
        {
#if defined(_UNICODE) || defined(UNICODE)
            BYTE * pByte;

            j = i;
            pByte = (BYTE *)buf;
            while ( j > 0 )
            {
                --j;
                buf[j] = pByte[j];
            }
#endif
            md5_update( &ctx, buf, i );
        }

        md5_finish( &ctx, md5sum );

        for( j = 0; j < 16; j++ )
        {
            _tprintf( __T("%02x"), md5sum[j] );
        }

        _tprintf( __T("  %s\n"), argv[1] );
    }

    return( 0 );
}

#endif

#ifdef __cplusplus
}
#endif
