#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hl_queue.h"
#include "hl_assert.h"

typedef struct
{
    uint32_t in;
    uint32_t out;
    uint32_t size;
    uint32_t length;
    uint32_t mask;
    void *buf;
} queue_t;

static inline uint32_t find_next_pow_of_2(uint32_t n);
static inline int is_pow_of_2(uint32_t n);
static inline uint32_t __queue_enqueue__(queue_t *queue, const void *data, uint32_t count);
static inline uint32_t __queue_dequeue__(queue_t *queue, void *data, uint32_t count);

int hl_queue_create(hl_queue_t *queue, uint32_t size, uint32_t length)
{
    HL_ASSERT(queue != NULL);
    HL_ASSERT(size != 0);
    HL_ASSERT(length != 0);

    queue_t *q = (queue_t *)malloc(sizeof(queue_t));

    if (q == NULL)
        return -1;

    if (!is_pow_of_2(length))
        q->length = find_next_pow_of_2(length);
    else
        q->length = length;
    q->mask = q->length - 1;
    q->size = size;
    q->in = q->out = 0;
    q->buf = malloc(q->size * q->length);

    if (q->buf == NULL)
    {
        free(q);
        return -1;
    }

    *queue = q;
    return 0;
}

void hl_queue_destory(hl_queue_t *queue)
{
    HL_ASSERT(queue != NULL);
    HL_ASSERT(*queue != NULL);
    queue_t *q = (queue_t *)(*queue);
    free(q->buf);
    free(q);
    *queue = NULL;
}

uint32_t hl_queue_enqueue(hl_queue_t queue, const void *data, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    uint32_t len = __queue_enqueue__(q, data, count);
    q->in += len;
    return len;
}

uint32_t hl_queue_dequeue(hl_queue_t queue, void *buf, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    uint32_t len = __queue_dequeue__(q, buf, count);
    q->out += len;
    return len;
}

uint32_t hl_queue_peek(hl_queue_t queue, void *buf, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    uint32_t len = __queue_dequeue__(q, buf, count);
    return len;
}

uint32_t hl_queue_skip(hl_queue_t queue, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    uint32_t len = q->in - q->out > count ? count : q->in - q->out;
    q->out += len;
    return len;
}

void hl_queue_reset(hl_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    q->in = q->out = 0;
}

uint32_t hl_queue_get_size(hl_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    return q->size;
}

uint32_t hl_queue_get_length(hl_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    return q->length;
}

uint32_t hl_queue_get_available_enqueue_length(hl_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    return q->length - q->in + q->out;
}

uint32_t hl_queue_get_available_dequeue_length(hl_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    return q->in - q->out;
}

int hl_queue_is_empty(hl_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    return q->in == q->out;
}
int hl_queue_is_full(hl_queue_t queue)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    return (q->in - q->out) == q->length;
}

void hl_queue_prepare_enqueue(hl_queue_t queue, void **data, uint32_t *count)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    uint32_t len = q->length - q->in + q->out > *count ? *count : q->length - q->in + q->out;
    uint32_t l = (q->length - (q->in & q->mask)) > len ? len : (q->length - (q->in & q->mask));
    *data = (uint8_t *)q->buf + (q->in & q->mask) * q->size;
    *count = l;
}

void hl_queue_finish_enqueue(hl_queue_t queue, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    q->in += count;
}

void hl_queue_prepare_dequeue(hl_queue_t queue, void **data, uint32_t *count)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    uint32_t len = q->in - q->out > *count ? *count : q->in - q->out;
    uint32_t l = (q->length - (q->out & q->mask)) > len ? len : (q->length - (q->out & q->mask));
    *data = (uint8_t *)q->buf + (q->out & q->mask) * q->size;
    *count = l;
}

void hl_queue_finish_dequeue(hl_queue_t queue, uint32_t count)
{
    HL_ASSERT(queue != NULL);
    queue_t *q = (queue_t *)queue;
    q->out += count;
}

static inline uint32_t find_next_pow_of_2(uint32_t n)
{
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
}

static inline int is_pow_of_2(uint32_t n)
{
    return (n & (n - 1)) == 0;
}

static inline uint32_t __queue_enqueue__(queue_t *queue, const void *data, uint32_t count)
{
    uint32_t len = (queue->length - queue->in + queue->out) > count ? count : (queue->length - queue->in + queue->out);
    uint32_t l = (queue->length - (queue->in & queue->mask)) > len ? len : (queue->length - (queue->in & queue->mask));
    if (data)
    {
        memcpy((uint8_t *)queue->buf + (queue->in & queue->mask) * queue->size, data, l * queue->size);
        memcpy((uint8_t *)queue->buf, data + l * queue->size, (len - l) * queue->size);
    }

    return len;
}

static inline uint32_t __queue_dequeue__(queue_t *queue, void *data, uint32_t count)
{
    uint32_t len = (queue->in - queue->out) > count ? count : (queue->in - queue->out);
    uint32_t l = (queue->length - (queue->out & queue->mask)) > len ? len : (queue->length - (queue->out & queue->mask));
    if (data)
    {
        memcpy(data, queue->buf + (queue->out & queue->mask) * queue->size, l * queue->size);
        memcpy(data + l * queue->size, queue->buf, (len - l) * queue->size);
    }
    return len;
}