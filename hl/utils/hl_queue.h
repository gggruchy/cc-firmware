#ifndef HL_QUEUE_H
#define HL_QUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef void *hl_queue_t;

    int hl_queue_create(hl_queue_t *queue, uint32_t size, uint32_t length);
    void hl_queue_destory(hl_queue_t *queue);

    uint32_t hl_queue_enqueue(hl_queue_t queue, const void *data, uint32_t count);
    uint32_t hl_queue_dequeue(hl_queue_t queue, void *buf, uint32_t count);
    uint32_t hl_queue_peek(hl_queue_t queue, void *buf, uint32_t count);
    uint32_t hl_queue_skip(hl_queue_t queue, uint32_t count);
    void hl_queue_reset(hl_queue_t queue);

    uint32_t hl_queue_get_size(hl_queue_t queue);
    uint32_t hl_queue_get_length(hl_queue_t queue);
    uint32_t hl_queue_get_available_enqueue_length(hl_queue_t queue);
    uint32_t hl_queue_get_available_dequeue_length(hl_queue_t queue);
    int hl_queue_is_empty(hl_queue_t queue);
    int hl_queue_is_full(hl_queue_t queue);

    void hl_queue_prepare_enqueue(hl_queue_t queue, void **data, uint32_t *count);
    void hl_queue_finish_enqueue(hl_queue_t queue, uint32_t count);
    void hl_queue_prepare_dequeue(hl_queue_t queue, void **data, uint32_t *count);
    void hl_queue_finish_dequeue(hl_queue_t queue, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif