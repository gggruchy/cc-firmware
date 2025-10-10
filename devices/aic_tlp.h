
#ifndef AIC_TLP_H
#define AIC_TLP_H

/* 最多 40 个文件 */
#ifndef TLP_FILE_NUM_MAX
#define TLP_FILE_NUM_MAX 40
#endif

/* 最多 200Mb */
#ifndef TLP_FILE_TOTAL_SIZE
#define TLP_FILE_TOTAL_SIZE 200
#endif

// 分区限制M
#define TLP_FILE_TLP_PARTITION ((TLP_FILE_TOTAL_SIZE + 30) * 1024 * (uint64_t)1024)

#define TLP_FRAME_RATE 20         // 帧率
// #define TLP_RESOLUTION_WIDTH 1280 // 宽
// #define TLP_RESOLUTION_HEIGHT 720 // 高
#define TLP_RESOLUTION_WIDTH 640 // 宽
#define TLP_RESOLUTION_HEIGHT 360 // 高

#ifdef __cplusplus
extern "C"
{
#endif
#include "config.h"
#if CONFIG_SUPPORT_TLP
#include <stdint.h>

    int aic_tlp_early_init();

    int aic_tlp_init(const char *name);
    void aic_tlp_complte(int make_video, uint32_t tlp_total_frame);
    void aic_tlp_delete(const char *name);
    void aic_tlp_export(const char *name, int video_export_enable);
    void aic_tlp_capture(uint32_t layer);
    void aic_tlp_get_path(char *path, uint32_t size);
    void aic_tlp_snapshot(); // 快照
    uint8_t get_aic_tlp_progress(void);

#endif

#ifdef __cplusplus
}
#endif
#endif
