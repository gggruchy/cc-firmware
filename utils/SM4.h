
/**
 * \file sm4.h
 */
#ifndef XYSSL_SM4_H
#define XYSSL_SM4_H
#include <stdint.h>

#define SM4_ENCRYPT     1
#define SM4_DECRYPT     0

typedef struct
{
    int mode;                   /*!<  encrypt/decrypt   */
    unsigned long sk[32];       /*!<  SM4 subkeys       */
}
sm4_context;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief          SM4 key schedule (128-bit, encryption)
 *
 * \param ctx      SM4 context to be initialized
 * \param key      16-byte secret key
 */
void sm4_setkey_enc( sm4_context *ctx, unsigned char key[16] );

/**
 * \brief          SM4 key schedule (128-bit, decryption)
 *
 * \param ctx      SM4 context to be initialized
 * \param key      16-byte secret key
 */
void sm4_setkey_dec( sm4_context *ctx, unsigned char key[16] );

/**
 * \brief          SM4-ECB block encryption/decryption
 * \param ctx      SM4 context
 * \param mode     SM4_ENCRYPT or SM4_DECRYPT
 * \param length   length of the input data
 * \param input    input block
 * \param output   output block
 */
void sm4_crypt_ecb( sm4_context *ctx,
				     int mode,
					 int length,
                     unsigned char *input,
                     unsigned char *output);

void sm4_en_de_crypt_ecb(int mode, unsigned char *key, unsigned char *input, unsigned char *output, int length);
void sm4_en_de_crypt_cbc(int mode, unsigned char *key, unsigned char *Iv, unsigned char *input, unsigned char *output, int length);




#ifdef __cplusplus
}
#endif



/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                             \
{                                                       \
    (n) = ( (unsigned long) (b)[(i)    ] << 24 )        \
        | ( (unsigned long) (b)[(i) + 1] << 16 )        \
        | ( (unsigned long) (b)[(i) + 2] <<  8 )        \
        | ( (unsigned long) (b)[(i) + 3]       );       \
}
#endif


#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                             \
{                                                       \
    (b)[(i)    ] = (unsigned char) ( (n) >> 24 );       \
    (b)[(i) + 1] = (unsigned char) ( (n) >> 16 );       \
    (b)[(i) + 2] = (unsigned char) ( (n) >>  8 );       \
    (b)[(i) + 3] = (unsigned char) ( (n)       );       \
}
#endif


/*
 *rotate shift left marco definition
 *
 */
#define  SHL(x,n) (((x) & 0xFFFFFFFF) << n)
#define ROTL(x,n) (SHL((x),n) | ((x) >> (32 - n)))


#define SWAP(a,b) { unsigned long t = a; a = b; b = t; t = 0; }

#endif 
