#ifndef __APP_PRINT_H__
#define __APP_PRINT_H__
#include "app.h"

enum
{
    UI_AIC_FOREIGN_START_MONITOR = 0, // 启动异物监测
    UI_AIC_FOREIGN_NORMAL_MONITOR,    // 正常异物监测
    UI_AIC_FOREIGN_FINALLY_MONITOR,   // 最后一次异物监测

    UI_AIC_CHOW_START_MONITOR,   // 启动炒面监测
    UI_AIC_CHOW_NORMAL_MONITOR,  // 正常炒面监测
    UI_AIC_CHOW_FINALLY_MONITOR, // 最后一次炒面监测

    UI_AIC_MONITOR_STATE_NULL, //空
};
typedef struct aic_print_info
{
    char print_name[256];
    char tlp_test_path[256];
    int print_state; // 1完成 2取消 3错误 4打印中(不包含暂停/暂停中/停止中)
    int current_layer;
    int total_layer;
    bool tlp_start_state;
    bool monitor_abnormal_state;
    int monitor_abnormal_index; // 1炒面 2异物
} aic_print_info_t;
extern aic_print_info_t aic_print_info;


void app_print_callback(widget_t **widget_list, widget_t *widget, void *e);
bool app_print_get_print_state(void);
bool app_print_get_print_busy(void);
bool app_print_get_print_completed_msgbox(void);
void aic_tlp_printing_generate_tlp(void);
bool app_print_reset_status(void);

/*
    @brief：打印暂停或停止命令已经入队等待处理
    @return:
        @1:已入队列
        @0:未入队列
*/
uint8_t is_print_pause_cancel_enqueue(void);
#if CONFIG_SUPPORT_AIC
void app_print_foreign_capture_detection(void);
#endif
#endif