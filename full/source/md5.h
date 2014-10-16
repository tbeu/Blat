#ifndef _MD5_H
#define _MD5_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef uint8_t
#define uint8_t unsigned char
#endif

#ifndef uint32_t
#define uint32_t unsigned long int
#endif

typedef struct
{
    uint32_t total[2];
    uint32_t state[4];
    _TUCHAR  buffer[64];
}
md5_context;

void _cdecl md5_starts( md5_context *ctx );
void _cdecl md5_update( md5_context *ctx, _TUCHAR *input, uint32_t length );
void _cdecl md5_finish( md5_context *ctx, _TUCHAR  digest[16] );

#ifdef __cplusplus
}
#endif

#endif /* md5.h */
