/**
 * @file utils_ehsm.h
 * @author your name (you@domain.com)
 * @brief 基于事件驱动的层次状态机
 * @version 0.1
 * @date 2021-08-09
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifndef UTILS_EHSM_H
#define UTILS_EHSM_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "utils.h"
#include "ts_queue.h"

#define UTILS_EHSM_MAX_DEPTH 8 //状态机最大层次深度

    enum
    {
        UTILS_EHSM_ENTRY_EVENT = 0, //状态机内部事件,由状态机维护
        UTILS_EHSM_EXIT_EVENT,
        UTILS_EHSM_INIT_EVENT,
        UTILS_EHSM_IDLE_EVENT,
        UTILS_EHSM_USER_EVENT, //用户自定义事件
    };

    typedef struct utils_ehsm_event_tag utils_ehsm_event_t;
    typedef struct utils_ehsm_tag utils_ehsm_t;
    typedef struct utils_ehsm_state_tag utils_ehsm_state_t;
    typedef int (*utils_ehsm_handler_t)(utils_ehsm_t *ehsm, utils_ehsm_event_t *event);

    /**
     * @brief 事件结构体
     * @param event 事件值
     * @param data 事件携带参数
     * @param size 事件参数大小
     */
    struct utils_ehsm_event_tag
    {
        uint32_t event;
        void *data;
        uint32_t size;
    };

    /**
     * @brief 状态结构体
     * @param parent 父状态
     * @param handler 状态对应的处理句柄
     * @param 状态名称
     * @param 状态序号
     * @param 深度值
     */
    struct utils_ehsm_state_tag
    {
        struct utils_ehsm_state_tag *parent;
        utils_ehsm_handler_t handler;
        const char *name;
        uint8_t depth;
    };

    /**
     * @brief 状态机结构体
     * @param name 状态机名称
     * @param event_queue 事件队列
     * @param current_state  当前状态
     * @param 顶层状态机句柄,当事件被所有子状态机忽略时候会送达至此进行处理
     *
     */
    struct utils_ehsm_tag
    {
        const char *name;
        ts_queue_t *event_queue;
        utils_ehsm_state_t *current_state;
        utils_ehsm_state_t *last_state;
        utils_ehsm_handler_t root_handler;
        void *user_data;
        bool init;
    };

    /**
     * @brief 创建状态机
     *
     * @param ehsm 状态机句柄
     * @param name 状态机名称
     * @param initial_state 初始状态
     * @param root_handler 顶层处理函数
     * @param event_queue_size 消息队列最大值
     * @return int
     */
    int utils_ehsm_create(utils_ehsm_t *ehsm, const char *name, utils_ehsm_state_t *initial_state, utils_ehsm_handler_t root_handler, uint32_t event_queue_size, void *user_data);

    int utils_ehsm_destroy(utils_ehsm_t *ehsm);

    /**
     * @brief 创建状态
     *
     * @param state 状态句柄
     * @param parent 状态父状态
     * @param handler 状态处理函数
     * @param name 状态名称
     * @return int
     */
    int utils_ehsm_create_state(utils_ehsm_state_t *state, utils_ehsm_state_t *parent, utils_ehsm_handler_t handler, const char *name);

    int utils_ehsm_event(utils_ehsm_t *ehsm, int event, void *data, size_t size);

    int utils_ehsm_run(utils_ehsm_t *ehsm);

    int utils_ehsm_tran(utils_ehsm_t *ehsm, utils_ehsm_state_t *next_state);

    utils_ehsm_state_t *utils_ehsm_get_state(utils_ehsm_t *ehsm);

    utils_ehsm_state_t *utils_ehsm_get_last_state(utils_ehsm_t *ehsm);
#ifdef __cplusplus
}
#endif
#endif