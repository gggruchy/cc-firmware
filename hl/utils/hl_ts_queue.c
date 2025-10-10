#include <stdint.h>
#include <pthread.h>
#include <errno.h>

#include "hl_ts_queue.h"
#include "hl_queue.h"
#include "hl_common.h"
#include "hl_assert.h"

typedef struct ts_queue
{
    hl_queue_t queue;
    hl_easy_cond_t cond;
} ts_queue_t;

int hl_ts_queue_create(hl_ts_queue_t *queue, uint32_t size, uint32_t length)
{
    HL_ASSERT(queue != NULL);
    HL_ASSERT(size != 0);
    HL_ASSERT(length != 0);

    ts_queue_t *q = (ts_queue_t *)malloc(sizeof(ts_queue_t));
    if (q == NULL)
        return -1;

    if (hl_queue_create(&q->queue, size, length) != 0)
    {
        free(q);
        return -1;
    }

    if (hl_easy_cond_create(&q->cond) != 0)
    {
        hl_queue_destory(&q->queue);
        free(q);
        return -1;
    }

    *queue = q;

    return 0;
}

void hl_ts_queue_destory(hl_ts_queue_t *queue)
{
    HL_ASSERT(queue != NULL);
    HL_ASSERT(*queue != NULL);
    ts_queue_t *q = (ts_queue_t *)(*queue);

    hl_easy_cond_destory(&q->cond);
    hl_queue_destory(&q->queue);
    free(q);
    *queue = NULL;
}

uint32_t hl_ts_queue_enqueue(hl_ts_queue_t queue, const void *data, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    return hl_ts_queue_enqueue_wait(queue, data, count, 0);
}

uint32_t hl_ts_queue_dequeue(hl_ts_queue_t queue, void *buf, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    return hl_ts_queue_dequeue_wait(queue, buf, count, 0);
}

uint32_t hl_ts_queue_peek(hl_ts_queue_t queue, void *buf, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    return hl_ts_queue_peek_wait(queue, buf, count, 0);
}

uint32_t hl_ts_queue_enqueue_try(hl_ts_queue_t queue, const void *data, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    if (hl_easy_cond_trylock(q->cond) == 0)
    {
        len = hl_queue_enqueue(q->queue, data, count);
        hl_easy_cond_unlock(q->cond);
    }
    if (len > 0)
        hl_easy_cond_signal(q->cond);
    return len;
}

uint32_t hl_ts_queue_dequeue_try(hl_ts_queue_t queue, void *buf, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    if (hl_easy_cond_trylock(q->cond) == 0)
    {
        len = hl_queue_dequeue(q->queue, buf, count);
        hl_easy_cond_unlock(q->cond);
    }
    if (len > 0)
        hl_easy_cond_signal(q->cond);
    return len;
}

uint32_t hl_ts_queue_peek_try(hl_ts_queue_t queue, void *buf, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    if (hl_easy_cond_trylock(q->cond) == 0)
    {
        len = hl_queue_peek(q->queue, buf, count);
        hl_easy_cond_unlock(q->cond);
    }
    return len;
}

uint32_t hl_ts_queue_enqueue_wait(hl_ts_queue_t queue, const void *data, uint32_t count, uint64_t millisecond)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    struct timespec timeout = hl_calculate_timeout(millisecond);
    hl_easy_cond_lock(q->cond);
    while ((len += hl_queue_enqueue(q->queue, (const uint8_t *)data + len * hl_queue_get_size(q->queue), count - len)) != count)
    {
        if (millisecond == 0)
            hl_easy_cond_wait(q->cond);
        else if (hl_easy_cond_timewait(q->cond, &timeout) == ETIMEDOUT)
            break;
    }
    hl_easy_cond_unlock(q->cond);
    if (len > 0)
        hl_easy_cond_signal(q->cond);
    return len;
}

uint32_t hl_ts_queue_dequeue_wait(hl_ts_queue_t queue, void *buf, uint32_t count, uint64_t millisecond)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    struct timespec timeout = hl_calculate_timeout(millisecond);
    hl_easy_cond_lock(q->cond);
    while ((len += hl_queue_dequeue(q->queue, (uint8_t *)buf + len * hl_queue_get_size(q->queue), count - len)) != count)
    {
        if (millisecond == 0)
            hl_easy_cond_wait(q->cond);
        else if (hl_easy_cond_timewait(q->cond, &timeout) == ETIMEDOUT)
            break;
    }
    hl_easy_cond_unlock(q->cond);
    if (len > 0)
        hl_easy_cond_signal(q->cond);
    return len;
}

uint32_t hl_ts_queue_peek_wait(hl_ts_queue_t queue, void *buf, uint32_t count, uint64_t millisecond)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    struct timespec timeout = hl_calculate_timeout(millisecond);
    hl_easy_cond_lock(q->cond);
    while (hl_queue_get_available_dequeue_length(q->queue) != count)
    {
        if (millisecond == 0)
            hl_easy_cond_wait(q->cond);
        else if (hl_easy_cond_timewait(q->cond, &timeout) == ETIMEDOUT)
            break;
    }
    len = hl_queue_peek(q->queue, buf, count);
    hl_easy_cond_unlock(q->cond);
    return len;
}

uint32_t hl_ts_queue_skip(hl_ts_queue_t queue, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    hl_easy_cond_lock(q->cond);
    len = hl_queue_skip(q->queue, count);
    hl_easy_cond_unlock(q->cond);
    return len;
}

void hl_ts_queue_reset(hl_ts_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    hl_easy_cond_lock(q->cond);
    hl_queue_reset(q->queue);
    hl_easy_cond_unlock(q->cond);
}

uint32_t hl_ts_queue_get_size(hl_ts_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    return hl_queue_get_size(q->queue);
}

uint32_t hl_ts_queue_get_length(hl_ts_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    return hl_ts_queue_get_length(q->queue);
}

uint32_t hl_ts_queue_get_available_enqueue_length(hl_ts_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    hl_easy_cond_lock(q->cond);
    len = hl_queue_get_available_enqueue_length(q->queue);
    hl_easy_cond_unlock(q->cond);
    return len;
}

uint32_t hl_ts_queue_get_available_dequeue_length(hl_ts_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    uint32_t len = 0;
    hl_easy_cond_lock(q->cond);
    len = hl_queue_get_available_dequeue_length(q->queue);
    hl_easy_cond_unlock(q->cond);
    return len;
}

int hl_ts_queue_is_empty(hl_ts_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    int ret;
    hl_easy_cond_lock(q->cond);
    ret = hl_queue_is_empty(q->queue);
    hl_easy_cond_unlock(q->cond);
    return ret;
}

int hl_ts_queue_is_full(hl_ts_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    ts_queue_t *q = (ts_queue_t *)queue;
    int ret;
    hl_easy_cond_lock(q->cond);
    ret = hl_queue_is_full(q->queue);
    hl_easy_cond_unlock(q->cond);
    return ret;
}