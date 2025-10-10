#ifndef PRINT_HISTORY_H
#define PRINT_HISTORY_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "common.h"
#include "utils.h"
#include "print_stats_c.h"
#include "hl_net.h"
#define PRINT_HISTORY_SIZE 50
#define PRINT_RECORD_MASK_DELETE 0x00000001
#define PRINT_RECORD_STATE_FINISH 1
#define PRINT_RECORD_STATE_ERROR 2
#define PRINT_RECORD_STATE_CANCEL 3
#define PRINT_RECORD_STATE_START 4
#define PRINT_RECORD_STATE_PAUSE 5

    typedef struct _break_resume_history_info
    {
        uint64_t start_time;
        int ntp_status;
    }break_resume_his_param_t;

    typedef struct print_history_record_tag
    {
        char filepath[PATH_MAX_LEN + 1];
        uint8_t md5[16];
        uint32_t print_state; // 独立于打印状态
        uint32_t mask;

        char local_task_id[64];
        uint64_t start_time;   // UTC时间
        uint64_t end_time;     // UTC时间
        double print_duration; // 打印时长
        int current_layer;     // 当前层数
        double filament_used;  // 已用耗材

        // 切片信息
        slice_param_t slice_param;

        // 保存缩略图文件path
        char image_dir[PATH_MAX_LEN];
        // 状态原因
        int status_r;

        // 延迟摄影路径
        char tlp_path[PATH_MAX_LEN];
        // 延迟摄影视频路径
        char video_path[PATH_MAX_LEN];
        // 延迟摄影状态
        int tlp_state;// 0未生成 1生成成功 4生成失败
        // 延迟视频时间s
        int tlp_time;
        // 时间是否同步
        int ntp_status;
    } print_history_record_t;
    void print_history_create_record(const char *filepath, char *task_id, slice_param_t *slice_param, char *image_dir);
    void print_history_clean(void);
    void print_history_update_record(double time, uint32_t state, int current_layer, double filament_used, int status_r);
    print_history_record_t *print_history_get_record(uint32_t index);
    void print_history_set_mask(uint32_t index);
    void print_history_reset_mask(uint32_t index);
    bool print_history_get_mask(uint32_t index);
    void print_history_clear_all_mask(void);
    void print_history_delete_record(void);
    void save_last_history_param(break_resume_his_param_t param);
#ifdef __cplusplus
}
#endif
#endif