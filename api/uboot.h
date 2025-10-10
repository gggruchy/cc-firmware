#ifndef UBOOT_H
#define UBOOT_H


#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#define CONFIG_UBOOT_FWENV "/etc/fw_env.config"

char *fw_getenv(char *name);
int fw_env_open(struct env_opts *opts);
int fw_env_close(struct env_opts *opts);
char *bootloader_env_get(const char *name);
int bootloader_env_set(const char *name, const char *value);
int get_image_selector(char *cmd_str );		//
int get_update_status(  );	

#define AES_KEY_LENGTH  (128 / 8)
struct env_opts {
        char *config_file;
        int aes_flag; /* Is AES encryption used? */
        uint8_t aes_key[AES_KEY_LENGTH];
};

int fw_parse_script(char *fname, struct env_opts *opts);
int fw_env_write(char *name, char *value);
int fw_env_flush(struct env_opts *opts);

// extern unsigned	long  crc32	 (unsigned long, const unsigned char *, unsigned);


#ifdef __cplusplus
}
#endif
#endif