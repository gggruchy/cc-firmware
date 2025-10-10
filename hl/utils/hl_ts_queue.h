#ifndef HL_TS_QUEUE_H
#define HL_TS_QUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef void *hl_ts_queue_t;

    int hl_ts_queue_create(hl_ts_queue_t *queue, uint32_t size, uint32_t length);
    void hl_ts_queue_destory(hl_ts_queue_t *queue);

    uint32_t hl_ts_queue_enqueue(hl_ts_queue_t queue, const void *data, uint32_t count);
    uint32_t hl_ts_queue_dequeue(hl_ts_queue_t queue, void *buf, uint32_t count);
    uint32_t hl_ts_queue_peek(hl_ts_queue_t queue, void *buf, uint32_t count);

    uint32_t hl_ts_queue_enqueue_try(hl_ts_queue_t queue, const void *data, uint32_t count);
    uint32_t hl_ts_queue_dequeue_try(hl_ts_queue_t queue, void *buf, uint32_t count);
    uint32_t hl_ts_queue_peek_try(hl_ts_queue_t queue, void *buf, uint32_t count);

    uint32_t hl_ts_queue_enqueue_wait(hl_ts_queue_t queue, const void *data, uint32_t count, uint64_t millisecond);
    uint32_t hl_ts_queue_dequeue_wait(hl_ts_queue_t queue, void *buf, uint32_t count, uint64_t millisecond);
    uint32_t hl_ts_queue_peek_wait(hl_ts_queue_t queue, void *buf, uint32_t count, uint64_t millisecond);

    uint32_t hl_ts_queue_skip(hl_ts_queue_t queue, uint32_t count);
    void hl_ts_queue_reset(hl_ts_queue_t queue);

    uint32_t hl_ts_queue_get_size(hl_ts_queue_t queue);
    uint32_t hl_ts_queue_get_length(hl_ts_queue_t queue);
    uint32_t hl_ts_queue_get_available_enqueue_length(hl_ts_queue_t queue);
    uint32_t hl_ts_queue_get_available_dequeue_length(hl_ts_queue_t queue);
    int hl_ts_queue_is_empty(hl_ts_queue_t queue);
    int hl_ts_queue_is_full(hl_ts_queue_t queue);

#ifdef __cplusplus
}
#endif

#endif