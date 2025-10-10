#ifndef HL_COMMON_H
#define HL_COMMON_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <time.h>

    typedef void *hl_easy_cond_t;
    int hl_easy_cond_create(hl_easy_cond_t *cond);
    void hl_easy_cond_destory(hl_easy_cond_t *cond);
    int hl_easy_cond_lock(hl_easy_cond_t cond);
    int hl_easy_cond_trylock(hl_easy_cond_t cond);
    int hl_easy_cond_unlock(hl_easy_cond_t cond);
    int hl_easy_cond_wait(hl_easy_cond_t cond);
    int hl_easy_cond_timewait(hl_easy_cond_t cond, const struct timespec *timeout);
    int hl_easy_cond_signal(hl_easy_cond_t cond);
    int hl_easy_cond_boardcast(hl_easy_cond_t cond);

    int hl_system(const char *fmt, ...);
    int hl_echo(const char *path, const char *buf, uint32_t bufsize);

    struct timespec hl_calculate_timeout(uint64_t millisecond);
    uint64_t hl_get_tick_ns(void);
    uint64_t hl_get_tick_us(void);
    uint64_t hl_get_tick_ms(void);
    int hl_tick_is_overtime(uint64_t start_tick, uint64_t current_tick, uint64_t timeout_tick);
    uint64_t hl_get_utc_second(void);
    struct tm hl_get_time_from_utc_second(uint64_t utc_second);

    int hl_parse_usb_id(const char *product, uint16_t *vid, uint16_t *pid);
    int hl_parse_usb_id2(const char *product, uint16_t *vid, uint16_t *pid);

    int hl_get_line(char *buf, uint32_t bufsize, char **saveptr);

    int hl_copy_create(void **ctx, const char *src, const char *dest);
    void hl_copy_destory(void **ctx);
    int hl_copy(void *ctx, uint64_t *size, uint64_t *offset);

    void hl_convert_version_string_to_version(const char *version_string, char *version, uint32_t size);
    int hl_version_compare(const char *v1, const char *v2);
    const char *hl_get_name_from_path(const char *path);

    int hl_md5(const char *filepath, uint8_t *digest);
    int hl_str2hex(char *str, int len, uint8_t *digest);
    int hl_hex2str(uint8_t *digest, int len, char *str);
    uint64_t hl_get_utc_ms(void);
    int is_process_running(const char *process_name);
    struct tm hl_get_localtime_time_from_utc_second(uint64_t utc_second);
    int is_capacity_enough(const char *usb_path, __fsblkcnt64_t file_size);
    int file_decrypt_aes(const char *input_file, const char *output_file, const char *hex_key, const char *hex_iv, uint32_t file_seek);
    int file_encrypt_aes(const char *input_file, const char *output_file, const char *hex_key, const char *hex_iv);
    int file_is_zip(const char *file_path, uint32_t file_seek);
    int hl_md5_seek(const char *filepath, uint8_t *digest, uint32_t file_seek);
#ifdef __cplusplus
}
#endif

#endif