#ifndef _OTA_UPDATE_H
#define _OTA_UPDATE_H
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint.h>
#include <malloc.h>
#include <assert.h>
#include "string.h"
#ifdef __cplusplus
extern "C"
{
#endif

typedef void *hl_ota_update_ctx_t;
typedef void *hl_ota_dec_ctx_t;
typedef enum
{
    HL_OTA_DEC_IDLE = 0,
    HL_OTA_DEC_RUNNING,
    HL_OTA_DEC_COMPLETED,
    HL_OTA_DEC_FAILED,
} hl_ota_dec_state_t;
int ota_update(const char *path);
int ota_update2(char *root_path);
int ota_progress_thread_create(hl_ota_update_ctx_t *ctx);
struct progress_msg *get_ota_progress_msg(hl_ota_update_ctx_t *ctx);
void ota_progress_thread_destory(hl_ota_update_ctx_t *ctx);
int update_firmware_check(const char *input_file, uint8_t *md5, uint32_t *len, uint8_t *info);
int ota_dec_thread_create(hl_ota_dec_ctx_t *ctx, char *input_file, char *output_file);
void ota_dec_thread_destory(hl_ota_dec_ctx_t *ctx);
hl_ota_dec_state_t ota_dec_get_state(hl_ota_dec_ctx_t *ctx);
#ifdef __cplusplus
}
#endif
#endif