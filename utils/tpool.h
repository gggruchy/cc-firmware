#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "ts_queue.h"
#include <stdbool.h>

    typedef int64_t routine_id_t;
    typedef int (*tpool_routine_t)(void *args, ts_queue_t *cmd, ts_queue_t *in, ts_queue_t *out);

    /**
     * @brief 创建线程池
     *
     * @param thread_count
     * @return void*
     */
    void *tpool_create(int thread_count);

    /**
     * @brief 销毁线程池
     *
     * @param handle
     */
    void tpool_destroy(void *handle);

    /**
     * @brief 获取线程池句柄
     *
     * @return void*
     */
    void *tpool_get_handle();

    /**
     * @brief 推入线程
     *
     * @param handle 句柄
     * @param routine 线程实例
     * @param arg 线程传入参数
     * @param ret 线程返回值
     * @param in 线程通信队列
     * @param out 线程通信队列
     * @return routine_id_t
     */
    routine_id_t tpool_enqueue_routine(void *handle, tpool_routine_t routine, void *arg, int *ret, ts_queue_t *in, ts_queue_t *out);

    /**
     * @brief 取消线程
     *
     * @param handle
     * @param id
     * @return true
     * @return false
     */
    bool tpool_cancel_routine(void *handle, routine_id_t id);

    /**
     * @brief 测试是否取消线程
     *
     * @param cmd_queue
     * @return true
     * @return false
     */
    bool tpool_test_cancel(ts_queue_t *cmd_queue);
#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif