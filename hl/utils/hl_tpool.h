#ifndef HL_TPOOL_H
#define HL_TPOOL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef void *hl_tpool_thread_t;
    typedef void (*hl_tpool_function_t)(hl_tpool_thread_t thread, void *args);

    void hl_tpool_init(int thread_count);

    int hl_tpool_create_thread(hl_tpool_thread_t *thread, hl_tpool_function_t function, void *args, uint32_t msg_in_size, uint32_t msg_in_length, uint32_t msg_out_size, uint32_t msg_out_length);
    void hl_tpool_destory_thread(hl_tpool_thread_t *thread);
    void hl_tpool_cancel_thread(hl_tpool_thread_t thread);
    int hl_tpool_wait_started(hl_tpool_thread_t thread, uint64_t millisecond);
    int hl_tpool_wait_completed(hl_tpool_thread_t thread, uint64_t millisecond);
    int hl_tpool_is_started(hl_tpool_thread_t thread);
    int hl_tpool_is_completed(hl_tpool_thread_t thread);

    int hl_tpool_send_msg(hl_tpool_thread_t thread, const void *msg);
    int hl_tpool_recv_msg(hl_tpool_thread_t thread, void *buf);
    int hl_tpool_send_msg_wait(hl_tpool_thread_t thread, const void *msg, uint64_t millisecond);
    int hl_tpool_recv_msg_wait(hl_tpool_thread_t thread, void *buf, uint64_t millisecond);
    int hl_tpool_send_msg_try(hl_tpool_thread_t thread, const void *msg);
    int hl_tpool_recv_msg_try(hl_tpool_thread_t thread, void *buf);
    
    int hl_tpool_thread_test_cancel(hl_tpool_thread_t thread);
    int hl_tpool_thread_send_msg(hl_tpool_thread_t thread, const void *msg);
    int hl_tpool_thread_recv_msg(hl_tpool_thread_t thread, void *buf);
    int hl_tpool_thread_send_msg_wait(hl_tpool_thread_t thread, const void *msg, uint64_t millisecond);
    int hl_tpool_thread_recv_msg_wait(hl_tpool_thread_t thread, void *buf, uint64_t millisecond);
    int hl_tpool_thread_send_msg_try(hl_tpool_thread_t thread, const void *msg);
    int hl_tpool_thread_recv_msg_try(hl_tpool_thread_t thread, void *buf);
    int hl_tpool_send_msg_exit(hl_tpool_thread_t thread, const void *msg);
#ifdef __cplusplus
}
#endif

#endif