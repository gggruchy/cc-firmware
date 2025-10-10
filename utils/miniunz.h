#ifndef MINIUNZ_H
#define MINIUNZ_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "unzip.h"
#include "stdint.h"
    typedef void *hl_unz_ctx_t;
    typedef enum
    {
        HL_UNZ_STATE_IDLE,
        HL_UNZ_STATE_RUNNING,
        HL_UNZ_STATE_COMPLETED,
        HL_UNZ_STATE_FAILED,
    } hl_unz_state_t;
    int hl_unz_task_create(hl_unz_ctx_t *ctx, const char *path, const char *decompression_path, const char *password);
    hl_unz_state_t hl_unz_get_state(hl_unz_ctx_t ctx, uint64_t *offset, uint64_t *size);
    void hl_unz_task_destory(hl_unz_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
#endif