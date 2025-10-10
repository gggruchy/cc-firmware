#ifndef EXPLORER_H
#define EXPLORER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "common.h"
#include "utils.h"

    #pragma pack(push, 1)
    typedef struct history_item_tag
    {
        char path[NAME_MAX_LEN + 1]; // 文件路径
        char *name;                                         // 文件名
        char date[64];                                     // 文件开始时间
        uint64_t time_consumption;      // 文件总计耗时
        bool print_state;                               // 打印完成状态
        uint8_t is_exist;
    }history_item_t;
    #pragma pack(pop)

    typedef struct explorer_item_tag
    {
        char name[NAME_MAX_LEN + 1];
        char path[PATH_MAX_LEN + 1];
        char date[64];                                     // 文件开始时间
        bool print_state;                               // 打印完成状态
        uint64_t time_consumption;
        uint32_t magic;
        uint8_t is_dir;
        time_t item_mtime;                              //文件修改时间
        void *userdata;
        int type;
    } explorer_item_t;

    enum EXPLORER_OPERATION_STATE
    {
        EXPLORER_OPERATION_IDLE,
        EXPLORER_OPERATION_COPYING,
        EXPLORER_OPERATION_MOVING,
        EXPLORER_OPERATION_COPY_DONE,
        EXPLORER_OPERATION_MOVE_DONE,
        EXPLORER_OPERATION_COPY_STOP,
        EXPLORER_OPERATION_MOVE_STOP,
        EXPLORER_OPERATION_DELETE,
        EXPLORER_OPERATION_DELETE_DONE,
        EXPLORER_OPERATION_COPY_FAIL,
        EXPLORER_OPERATION_VERIFYING,
    };

    enum
    {
        EXPLORER_ITEM_CALLBACK_STATUS_START = 0,
        EXPLORER_ITEM_CALLBACK_STATUS_CONTINUE,
        EXPLORER_ITEM_CALLBACK_STATUS_END,
    };

    typedef struct explorer_operation_context_tag
    {
        pthread_t thread;
        pthread_attr_t thread_attr;
        sem_t stop_sem; // 发送停止命令的信号量
        uint64_t copy_state;  // 复制状态 -1：失败 length：成功
        char src_path[PATH_MAX_LEN + 1];
        char dst_path[PATH_MAX_LEN + 1];
    } explorer_operation_context_t;

    typedef struct explorer_tag
    {
        explorer_item_t *item_array;
        char current_path[PATH_MAX_LEN + 1]; // 当前目录
        int total_number;                    // 项总数
        int dir_number;
        int depth;                                                                               // 目录深度
        explorer_operation_context_t operation_ctx;                                              // 负责复制线程的上下文信息
        uint8_t opertaion_state;                                                                 // 当前资源管理器的状态,用于互斥操作和决定是否在完成复制后删除源文件
        bool (*item_callback)(struct explorer_tag *explorer, explorer_item_t *item, int status); // 每一个项更新回调函数，返回true会保存到内存中，false会被过滤
        void (*update_callback)(struct explorer_tag *explorer);                                  // 项更新回调函数
        void (*operation_callback)(struct explorer_tag *explorer, uint64_t size, uint64_t offset, int state);
    } explorer_t;

    int utils_explorer_init(explorer_t *explorer);
    int utils_explorer_deinit(explorer_t *explorer);
    int utils_explorer_set_path(explorer_t *explorer, const char *path);
    int utils_explorer_set_current_path(explorer_t *explorer, const char *path);
    int utils_explorer_opendir(explorer_t *explorer, const char *dirpath);
    int utils_explorer_get_item(explorer_t *explorer, int index, explorer_item_t **item);
    int utils_explore_operation_start(explorer_t *explorer, char *src_path, char *dst_path, uint8_t isDelete);
    int utils_explore_operation_stop(explorer_t *explorer);
    int utils_explorer_global_search(explorer_t *explorer, const char *key);
    void utils_init_history_list(history_item_t* history_explorer);
    void utils_add_history(explorer_item_t *file_item, history_item_t* history_explorer);

    static inline int utils_explorer_set_update_callback(explorer_t *explorer, void (*update_callback)(explorer_t *explorer))
    {
        if (UTILS_CHECK(explorer != NULL))
            return -1;
        if (UTILS_CHECK(update_callback != NULL))
            return -2;
        explorer->update_callback = update_callback;
        return 0;
    }

    static inline int utils_explorer_set_item_callback(explorer_t *explorer, bool (*item_callback)(struct explorer_tag *explorer, explorer_item_t *item, int status))
    {
        if (UTILS_CHECK(explorer != NULL))
            return -1;
        if (UTILS_CHECK(item_callback != NULL))
            return -2;
        explorer->item_callback = item_callback;
        return 0;
    }

    static inline int utils_explorer_set_operation_callback(explorer_t *explorer, void (*operation_callback)(explorer_t *explorer, uint64_t size, uint64_t offset, int state))
    {
        if (UTILS_CHECK(explorer != NULL))
            return -1;
        if (UTILS_CHECK(operation_callback != NULL))
            return -2;
        explorer->operation_callback = operation_callback;
        return 0;
    }

#ifdef __cplusplus
}
#endif

#endif