#ifndef __HL_MD5_H  
#define __HL_MD5_H  

# ifdef  __cplusplus
extern "C" {
# endif

#define MD5_DECRYPT_LEN   16

typedef struct  
{  
    unsigned int count[2];  
    unsigned int state[4];  
    unsigned char buffer[64];     
}HL_MD5_CTX_S;  

/**
 * @brief hl_md5_init 
 *
 * @param context
 */
void hl_md5_init(HL_MD5_CTX_S *context);

/**
 * @brief hl_md5_update 
 *
 * @param context
 * @param[in] input
 * @param[in] inputlen
 */
void hl_md5_update(HL_MD5_CTX_S *context,const unsigned char *input,const unsigned int inputlen);

/**
 * @brief hl_md5_final 
 *
 * @param context
 * @param digest[16]
 */
void hl_md5_final(HL_MD5_CTX_S *context,unsigned char digest[16]);

# ifdef  __cplusplus
}
# endif

#endif 
