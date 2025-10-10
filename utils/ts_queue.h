#ifndef TS_QUEUE_H
#define TS_QUEUE_H
#ifdef __cplusplus
extern "C"
{
#endif
/**
 * @file ts_queue.h
 * @author your name (you@domain.com)
 * @brief 用于多线程的队列版本
 * @version 0.1
 * @date 2022-03-03
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include "queue.h"
    typedef struct
    {
        queue_t *queue;
        pthread_mutex_t cond_mutex;
        pthread_cond_t cond;
        pthread_condattr_t cond_attr;
    } ts_queue_t;

    static inline ts_queue_t *ts_queue_create(size_t size, size_t ecount)
    {
        ts_queue_t *ts_queue;
        if ((ts_queue = (ts_queue_t *)malloc(sizeof(ts_queue_t))) == NULL)
            return NULL;

        memset(ts_queue, 0, sizeof(ts_queue_t));

        if ((ts_queue->queue = queue_create(size, ecount)) == NULL)
        {
            free(ts_queue);
            return NULL;
        }

        if (pthread_mutex_init(&ts_queue->cond_mutex, NULL) != 0)
        {
            queue_destroy(ts_queue->queue);
            free(ts_queue);
            return NULL;
        }

        if (pthread_condattr_init(&ts_queue->cond_attr) != 0)
        {
            pthread_mutex_destroy(&ts_queue->cond_mutex);
            queue_destroy(ts_queue->queue);
            free(ts_queue);
            return NULL;
        }

        if (pthread_condattr_setclock(&ts_queue->cond_attr, CLOCK_MONOTONIC) != 0)
        {
            pthread_condattr_destroy(&ts_queue->cond_attr);
            pthread_mutex_destroy(&ts_queue->cond_mutex);
            queue_destroy(ts_queue->queue);
            free(ts_queue);
            return NULL;
        }

        if (pthread_cond_init(&ts_queue->cond, &ts_queue->cond_attr) != 0)
        {
            pthread_condattr_destroy(&ts_queue->cond_attr);
            pthread_mutex_destroy(&ts_queue->cond_mutex);
            queue_destroy(ts_queue->queue);
            free(ts_queue);
            return NULL;
        }

        return ts_queue;
    }

    static inline void ts_queue_destroy(ts_queue_t *ts_queue)
    {
        pthread_cond_destroy(&ts_queue->cond);
        pthread_condattr_destroy(&ts_queue->cond_attr);
        pthread_mutex_destroy(&ts_queue->cond_mutex);
        queue_destroy(ts_queue->queue);
        free(ts_queue);
    }

    static inline size_t ts_queue_enqueue(ts_queue_t *ts_queue, void *data, size_t ecount)
    {
        size_t len = 0;
        pthread_mutex_lock(&ts_queue->cond_mutex);
        pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
        while ((len += queue_enqueue(ts_queue->queue, (uint8_t *)data + len, ecount - len)) != ecount)
            pthread_cond_wait(&ts_queue->cond, &ts_queue->cond_mutex);
        if (len > 0)
            pthread_cond_signal(&ts_queue->cond);
        pthread_cleanup_pop(1);
        return len;
    }

    static inline size_t ts_queue_try_enqueue(ts_queue_t *ts_queue, void *data, size_t ecount)
    {
        size_t len = 0;
        if (pthread_mutex_trylock(&ts_queue->cond_mutex) == 0)
        {
            pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
            len = queue_enqueue(ts_queue->queue, data, ecount);
            if (len > 0)
                pthread_cond_signal(&ts_queue->cond);
            pthread_cleanup_pop(1);
        }
        return len;
    }

    static inline size_t ts_queue_enqueue_timeout(ts_queue_t *ts_queue, void *data, size_t ecount, uint32_t millisecond)
    {
        size_t len = 0;
        struct timespec now;
        struct timespec timeout;
        clock_gettime(CLOCK_MONOTONIC, &now);
        timeout.tv_sec = now.tv_sec + millisecond / 1000;
        timeout.tv_nsec = now.tv_nsec + (millisecond - (millisecond / 1000) * 1000) * 1000000;
        if (timeout.tv_nsec > 1000000000)
        {
            timeout.tv_sec += 1;
            timeout.tv_nsec = -1000000000UL;
        }

        pthread_mutex_lock(&ts_queue->cond_mutex);
        pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
        while ((len += queue_enqueue(ts_queue->queue, (uint8_t *)data + len, ecount - len)) != ecount)
        {
            if (pthread_cond_timedwait(&ts_queue->cond, &ts_queue->cond_mutex, &timeout) == ETIMEDOUT)
                break;
        }
        if (len > 0)
            pthread_cond_signal(&ts_queue->cond);
        pthread_cleanup_pop(1);
        return len;
    }

    static inline size_t ts_queue_dequeue(ts_queue_t *ts_queue, void *data, size_t ecount)
    {
        size_t len = 0;
        pthread_mutex_lock(&ts_queue->cond_mutex);
        pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
        while ((len += queue_dequeue(ts_queue->queue, (uint8_t *)data + len, ecount - len)) != ecount)
        {
            pthread_cond_wait(&ts_queue->cond, &ts_queue->cond_mutex);
        }
        if (len > 0)
            pthread_cond_signal(&ts_queue->cond);
        pthread_cleanup_pop(1);
        return len;
    }

    static inline size_t ts_queue_try_dequeue(ts_queue_t *ts_queue, void *data, size_t ecount)
    {
        size_t len = 0;
        if (pthread_mutex_trylock(&ts_queue->cond_mutex) == 0)
        {
            pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
            len = queue_dequeue(ts_queue->queue, data, ecount);
            if (len > 0)
                pthread_cond_signal(&ts_queue->cond);
            pthread_cleanup_pop(1);
        }
        return len;
    }

    static inline size_t ts_queue_dequeue_timeout(ts_queue_t *ts_queue, void *data, size_t ecount, uint32_t millisecond)
    {
        size_t len = 0;
        struct timespec now;
        struct timespec timeout;
        clock_gettime(CLOCK_MONOTONIC, &now);
        timeout.tv_sec = now.tv_sec + millisecond / 1000;
        timeout.tv_nsec = now.tv_nsec + (millisecond - (millisecond / 1000) * 1000) * 1000000;
        if (timeout.tv_nsec > 1000000000)
        {
            timeout.tv_sec += 1;
            timeout.tv_nsec = -1000000000;
        }
        pthread_mutex_lock(&ts_queue->cond_mutex);
        pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
        while ((len += queue_dequeue(ts_queue->queue, (uint8_t *)data + len, ecount - len)) != ecount)
        {
            if (pthread_cond_timedwait(&ts_queue->cond, &ts_queue->cond_mutex, &timeout) == ETIMEDOUT)
                break;
        }
        if (len > 0)
            pthread_cond_signal(&ts_queue->cond);
        pthread_cleanup_pop(1);
        return len;
    }

    static inline size_t ts_queue_peek(ts_queue_t *ts_queue, void *data, size_t ecount)
    {
        size_t len = 0;
        pthread_mutex_lock(&ts_queue->cond_mutex);
        pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
        len = queue_peek(ts_queue->queue, data, ecount);
        pthread_cleanup_pop(1);
        return len;
    }

    static inline size_t ts_queue_try_peek(ts_queue_t *ts_queue, void *data, size_t ecount)
    {
        size_t len = 0;
        if (pthread_mutex_trylock(&ts_queue->cond_mutex) == 0)
        {
            pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
            len = queue_peek(ts_queue->queue, data, ecount);
            pthread_cleanup_pop(1);
        }
        return len;
    }

    static inline void ts_queue_reset(ts_queue_t *ts_queue)
    {
        pthread_mutex_lock(&ts_queue->cond_mutex);
        pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
        queue_reset(ts_queue->queue);
        pthread_cleanup_pop(1);
    }

    static inline size_t ts_queue_available_enqueue_length(ts_queue_t *ts_queue)
    {
        size_t len;
        pthread_mutex_lock(&ts_queue->cond_mutex);
        pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
        len = queue_available_enqueue_length(ts_queue->queue);
        pthread_cleanup_pop(1);
        return len;
    }

    static inline size_t ts_queue_available_dequeue_length(ts_queue_t *ts_queue)
    {
        size_t len;
        pthread_mutex_lock(&ts_queue->cond_mutex);
        pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &ts_queue->cond_mutex);
        len = queue_available_dequeue_length(ts_queue->queue);
        pthread_cleanup_pop(1);
        return len;
    }

    static bool ts_queue_is_empty(ts_queue_t *ts_queue)
    {
        return ts_queue_available_dequeue_length(ts_queue) == 0;
    }

    static bool ts_queue_is_full(ts_queue_t *ts_queue)
    {
        return ts_queue_available_enqueue_length(ts_queue) == 0;
    }

#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif