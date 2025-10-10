#ifndef HL_DECOMPRESSION_H
#define HL_DECOMPRESSION_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef enum
    {
        HL_GUNZIP_STATE_FAILED,
        HL_GUNZIP_STATE_RUNNING,
        HL_GUNZIP_STATE_COMPLETED,
    } hl_gunzip_state_t;

    typedef int (*hl_gunzip_callback_t)(const char *filename);

    typedef void *hl_gunzip_ctx_t;
    int hl_gunzip_ctx_create(hl_gunzip_ctx_t *ctx, const char *filepath, const char *dir, hl_gunzip_callback_t callback);
    void hl_gunzip_ctx_destory(hl_gunzip_ctx_t *ctx);
    void hl_gunzip(hl_gunzip_ctx_t ctx);
    hl_gunzip_state_t hl_gunzip_get_state(hl_gunzip_ctx_t ctx, uint64_t *offset, uint64_t *size);
    int hl_gunzip_indentify(const char *filepath);

#ifdef __cplusplus
}
#endif

#endif