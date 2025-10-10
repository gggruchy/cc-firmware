#ifndef UTILS_H
#define UTILS_H

#include "openssl/md5.h"
#include "common.h"
#ifdef __cplusplus
extern "C"
{
#endif
    /**
     * @union : byte_bit 是一个字节大小的联合体数据
     * @details : 每一位都规划出来
    */
    typedef union
    {
        struct
        {
            unsigned char b0 : 1;
            unsigned char b1 : 1;
            unsigned char b2 : 1;
            unsigned char b3 : 1;
            unsigned char b4 : 1;
            unsigned char b5 : 1;
            unsigned char b6 : 1;
            unsigned char b7 : 1;
        } bit;

        unsigned char byte;
    } byte_bit;

    /**
     * @union : short_bit 是双字节大小的联合体数据
     * @details : 每一位都规划出来
    */
    typedef union
    {
        struct
        {
            char b8 : 1;
            char b9 : 1;
            char b10 : 1;
            char b11 : 1;
            char b12 : 1;
            char b13 : 1;
            char b14 : 1;
            char b15 : 1;

            char b0 : 1;
            char b1 : 1;
            char b2 : 1;
            char b3 : 1;
            char b4 : 1;
            char b5 : 1;
            char b6 : 1;
            char b7 : 1;
        } bit;

        struct
        {
            unsigned char by1;
            unsigned char by0;
        } byte;

        unsigned short int all;
    } short_byte;

    /**
     * @union : word_bit 是四个字节大小的联合体数据
     * @details : 每一位都规划出来
    */
    typedef union
    {
        struct
        {
            char b24 : 1;
            char b25 : 1;
            char b26 : 1;
            char b27 : 1;
            char b28 : 1;
            char b29 : 1;
            char b30 : 1;
            char b31 : 1;

            char b16 : 1;
            char b17 : 1;
            char b18 : 1;
            char b19 : 1;
            char b20 : 1;
            char b21 : 1;
            char b22 : 1;
            char b23 : 1;

            char b8 : 1;
            char b9 : 1;
            char b10 : 1;
            char b11 : 1;
            char b12 : 1;
            char b13 : 1;
            char b14 : 1;
            char b15 : 1;

            char b0 : 1;
            char b1 : 1;
            char b2 : 1;
            char b3 : 1;
            char b4 : 1;
            char b5 : 1;
            char b6 : 1;
            char b7 : 1;
        } bt;

        struct
        {
            unsigned char by3;
            unsigned char by2;
            unsigned char by1;
            unsigned char by0;
        } byte;

        struct
        {
            unsigned short int wd1;
            unsigned short int wd0;
        } short_word;

        unsigned long all;
    } word_byte;

#define PATH_MAX_LEN 1023
#define NAME_MAX_LEN 255

#define FILE_MAGIC_BASE 0x12fd0000
#define BUILD_FILE_MAGIC_NUM(id) (FILE_MAGIC_BASE | id)

#define __UTILS_CHECK(cond, func, line)                                       \
    (                                                                         \
        {                                                                     \
            int ret = (cond);                                                 \
            if (!ret)                                                         \
            {                                                                 \
                printf("ERR: FUNC %s LINE %d  COND %s\n", func, line, #cond); \
            }                                                                 \
            !ret;                                                             \
        })

#define __UTILS_CHECK_ERRNO(cond, func, line)                                                          \
    (                                                                                                  \
        {                                                                                              \
            int ret = (cond);                                                                          \
            if (!ret)                                                                                  \
            {                                                                                          \
                printf("ERR: FUNC %s LINE %d ERRNO %s COND %s\n", func, line, strerror(errno), #cond); \
            }                                                                                          \
            !ret;                                                                                      \
        })

#define UTILS_CHECK(cond) __UTILS_CHECK((cond), __FUNCTION__, __LINE__)
#define UTILS_CHECK_ERRNO(cond) __UTILS_CHECK_ERRNO((cond), __FUNCTION__, __LINE__)
#define UTILS_CHECK_VAL_INVALID(val, min, max) (!((val) > (min) || (val) < (max)))

    typedef struct common_header_tag
    {
#define DLP_FILE_MAGIC BUILD_FILE_MAGIC_NUM(25)
#define RBF_FILE_MAGIC BUILD_FILE_MAGIC_NUM(34)
#define RBF_ENCRYPTION_FILE_MAGIC BUILD_FILE_MAGIC_NUM(37)
#define LT9711_EDP_CONFIG_FILE_MAGIC BUILD_FILE_MAGIC_NUM(64)
#define HSC32C1_FIRMWARE_FILE_MAGIC BUILD_FILE_MAGIC_NUM(286)
#define CTB_DISCARD_FILE_MAGIC BUILD_FILE_MAGIC_NUM(134)
#define CTB_CLASSIC_FILE_MAGIC BUILD_FILE_MAGIC_NUM(262)
#define CTB_ENCRYPTION_FILE_MAGIC BUILD_FILE_MAGIC_NUM(263)
#define PARAMS_FILE_MAGIC BUILD_FILE_MAGIC_NUM(282)
#define MASK_FILE_MAGIC BUILD_FILE_MAGIC_NUM(284)
#define EXPOSURE_IMAGE_FILE_MAGIC BUILD_FILE_MAGIC_NUM(285)
#define ZIP_FILE_MAGIC 0x04034B50
#define CRT_FILE_MAGIC 0xff220625
#define JXS2_FILE_MAGIC 0xff220810

        uint32_t magic;
        uint32_t size;
    } common_header_t;

    uint64_t utils_get_size(const char *filepath);
    uint64_t utils_get_dir_size(const char *dirpath);
    uint64_t utils_get_dir_available_size(const char *dirpath);
    int utils_get_header(common_header_t *header, const char *filepath);
    int utils_copy_at(const char *dst_file, uint64_t dst_offset, const char *src_file, uint64_t src_offset, void *data, void (*cb)(void *data, uint64_t size, uint64_t offset, int end));
    const char *utils_get_suffix(const char *filepath);
    const char *utils_get_file_name(const char *filepath);
    void utils_get_prefix(const char *filepath, char *prefix);
    int utils_get_bmp_info(const char *filepath, uint32_t *w, uint32_t *h, uint16_t *biBitCount);
#define utils_copy_file_cb(dst_file, src_file, data, cb) utils_copy_at(dst_file, 0, src_file, 0, data, cb)
#define utils_copy_file(dst_file, src_file) utils_copy_at(dst_file, 0, src_file, 0, NULL, NULL)
    uint64_t utils_get_current_tick(void);
    uint64_t utils_get_current_tick_us(void);
    void utils_second2time(uint64_t tsecond, uint32_t *hour, uint32_t *minute, uint32_t *second);
    void utils_second2timestr(uint64_t tsecond, char *str, uint32_t len);
    int utils_system(const char *format, ...);
    int utils_vfork_system(const char *format, ...);
    int utils_generate_md5(const char *filepath, char *digest);
    void utils_get_file_dir(const char *filepath, char *dir);
    int utils_file_path_handle(char *src_path, const char *path);
    bool utils_ipv4_verify(char *ip);

    int utils_path_to_name(char *name, const char *path);
    int eliminate_string_blank(char *name, const char *whole_mame);
    void utils_get_udisk_prefix(const char *filepath, char *prefix);
    int rsa_encrypt_string_with_cert_str(const char *plaintext, const char *cert_str, char *ciphertext, int *ciphertext_len);
    int rsa_encrypt_string_with_cert_file(const char *plaintext, const char *cert_file, char *ciphertext_base64, int *ciphertext_base64_len);
    int utils_hex2str(uint8_t *data, char *s, int len);

    void utils_string_toupper(const char *in, char *out, int outsize);
    
#ifdef __cplusplus
}
#endif

#endif
