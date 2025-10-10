/**
 * @file queue.h
 * @author your name (you@domain.com)
 * @brief 基于数组的循环队列
 * @version 0.1
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef QUEUE_H
#define QUEUE_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

    typedef struct
    {
        void *data;
        size_t ecount;
        size_t size;
        size_t in;
        size_t out;
    } queue_t;

    /**
     * @brief 创建队列
     *
     * @param size 队列元素的大小
     * @param ecount 队列元素最大个数
     * @return queue_t* 队列句柄
     */
    static inline queue_t *queue_create(size_t size, size_t ecount)
    {
        queue_t *queue;
        if ((queue = (queue_t *)malloc(sizeof(queue_t))) == NULL)
            return NULL;
        memset(queue, 0, sizeof(queue_t));
        if ((queue->data = (void *)malloc(ecount * size)) == NULL)
        {
            free(queue);
            return NULL;
        }
        memset(queue->data, 0, ecount * size);
        queue->ecount = ecount;
        queue->size = size;
        return queue;
    }

    /**
     * @brief 销毁队列
     *
     * @param queue 队列句柄
     * @return void*
     */
    static inline void queue_destroy(queue_t *queue)
    {
        free(queue->data);
        free(queue);
    }

    /**
     * @brief 入队
     *
     * @param queue 队列句柄
     * @param data 入队数据
     * @param ecount 入队元素个数
     * @return int 实际入队元素个数
     */
    static inline size_t queue_enqueue(queue_t *queue, void *data, size_t ecount)
    {
        size_t len = (queue->ecount - queue->in + queue->out) > ecount ? ecount : (queue->ecount - queue->in + queue->out);
        size_t l = (queue->ecount - queue->in % queue->ecount) > len ? len : (queue->ecount - queue->in % queue->ecount);
        if (data)
        {
            memcpy((uint8_t *)queue->data + (queue->in % queue->ecount) * queue->size, data, l * queue->size);
            memcpy(queue->data, (uint8_t *)data + l * queue->size, (len - l) * queue->size);
        }
        queue->in += len;
        return len;
    }

    /**
     * @brief 出队
     *
     * @param queue 队列句柄
     * @param data 出队数据
     * @param ecount 出队元素个数
     * @return int 实际出队元素个数
     */
    static inline size_t queue_dequeue(queue_t *queue, void *data, size_t ecount)
    {
        size_t len = queue->in - queue->out > ecount ? ecount : queue->in - queue->out;
        size_t l = (queue->ecount - queue->out % queue->ecount) > len ? len : (queue->ecount - queue->out % queue->ecount);
        if (data)
        {
            memcpy(data, (uint8_t *)queue->data + (queue->out % queue->ecount) * queue->size, l * queue->size);
            memcpy((uint8_t *)data + l * queue->size, queue->data, (len - l) * queue->size);
        }
        queue->out += len;
        return len;
    }

    /**
     * @brief 查看队列
     *
     * @param queue 队列句柄
     * @param data 出队数据
     * @param ecount 出队元素个数
     * @return int 实际出队元素个数
     */
    static inline size_t queue_peek(queue_t *queue, void *data, size_t ecount)
    {
        size_t len = queue->in - queue->out > ecount ? ecount : queue->in - queue->out;
        size_t l = (queue->ecount - queue->out % queue->ecount) > len ? len : (queue->ecount - queue->out % queue->ecount);
        if (data)
        {
            memcpy(data, (uint8_t *)queue->data + (queue->out % queue->ecount) * queue->size, l * queue->size);
            memcpy((uint8_t *)data + l * queue->size, queue->data, (len - l) * queue->size);
        }
        return len;
    }

    /**
     * @brief 重置队列
     *
     * @param queue 队列句柄
     */
    static inline void queue_reset(queue_t *queue)
    {
        queue->in = queue->out = 0;
    }

    /**
     * @brief 获取队列可入队长度
     *
     * @param queue 队列句柄
     * @return size_t 可入队长度
     */
    static inline size_t queue_available_enqueue_length(queue_t *queue)
    {
        return queue->ecount - queue->in + queue->out;
    }

    /**
     * @brief 获取队列可出队长度
     *
     * @param queue 队列句柄
     * @return size_t 可出队长度
     */
    static inline size_t queue_available_dequeue_length(queue_t *queue)
    {
        return queue->in - queue->out;
    }

    static uint8_t queue_is_empty(queue_t *queue)
    {
        return queue_available_dequeue_length(queue) == 0;
    }

    static uint8_t queue_is_full(queue_t *queue)
    {
        return queue_available_enqueue_length(queue) == 0;
    }

#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif