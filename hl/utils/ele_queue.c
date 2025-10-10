#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cbd_pointer.h"
#include "cbd_queue.h"

#define QUEUE_CREATE_LOCK(queue)    pthread_mutex_init(&(queue->mutex),NULL)
#define QUEUE_RELEASE_LOCK(queue)   pthread_mutex_destroy(&(queue->mutex))
#define QUEUE_LOCK(queue)           pthread_mutex_lock(&(queue->mutex))
#define QUEUE_UNLOCK(queue)         pthread_mutex_unlock(&(queue->mutex))


typedef enum {
    POLICY_SEND_TO_BACK,
    POLICY_SEND_TO_FRONT,
    POLICY_MAX
}ENQUEUE_POLICY_E;

typedef struct {
    LIST_HEAD node;
    UCHAR_T data[];
}QUEUE_ITEM_T;

typedef struct {
    pthread_mutex_t mutex;

    UINT32_T item_size;
    UINT32_T queue_len;
    UINT32_T queue_free;
    
    LIST_HEAD head;
}CBD_QUEUE_T;

STATIC CBD_ERROR_CODE __enqueue(CBD_QUEUE_HANDLE handle, CONST VOID_T *item, ENQUEUE_POLICY_E policy)
{
    CBD_ERROR_CODE op_ret = OPRT_OK;

    if(NULL == handle || NULL == item || policy >= POLICY_MAX) {
        return OPRT_PARAM_INVALID;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;
    
    QUEUE_ITEM_T *queue_item = (QUEUE_ITEM_T *)malloc(SIZEOF(QUEUE_ITEM_T) + queue->item_size);
    if(NULL == queue_item) {
        return OPRT_MALLOC_FAILED;
    }

    INIT_LIST_HEAD(&(queue_item->node));
    memcpy(queue_item->data, (VOID_T *)item, queue->item_size);

    QUEUE_LOCK(queue);
    if(queue->queue_free > 0) {
        if(POLICY_SEND_TO_BACK == policy) {
            cbd_list_add_tail(&(queue_item->node), &(queue->head));
        } else if(POLICY_SEND_TO_FRONT == policy){
            cbd_list_add(&(queue_item->node), &(queue->head));
        }
        queue->queue_free--;
    } else {
        free(queue_item);
        op_ret = OPRT_EXCEED_UPPER_LIMIT;
    }
    QUEUE_UNLOCK(queue);

    return op_ret;
}

/**
 * @brief create and initialize a queue (FIFO)
 * 
 * @param[in] queue_len the maximum number of items that the queue can contain.
 * @param[in] item_size the number of bytes each item in the queue will require.
 * @param[out] handle the queue handle
 * 
 * @note items are queued by copy, not by reference. Each item on the queue must be the same size.
 *
 * @return OPRT_OK on success. Others on error
 */
CBD_ERROR_CODE cbd_queue_create(CONST UINT32_T queue_len, CONST UINT32_T item_size, CBD_QUEUE_HANDLE *handle)
{
    CBD_ERROR_CODE op_ret = OPRT_OK;
    CBD_QUEUE_T *queue = NULL;

    if((NULL == handle) || (0 == queue_len) || (0 == item_size)) {
        return OPRT_PARAM_INVALID;
    }

    queue = (CBD_QUEUE_T *)malloc(SIZEOF(CBD_QUEUE_T));
    if(!queue) {
        return OPRT_MALLOC_FAILED;
    }

    op_ret = QUEUE_CREATE_LOCK(queue);
    if(OPRT_OK != op_ret) {
        free(queue);
        return OPRT_COM_ERROR;
    }

    queue->item_size = item_size;
    queue->queue_len = queue_len;
    queue->queue_free = queue_len;
    INIT_LIST_HEAD(&(queue->head));

    *handle = (CBD_QUEUE_HANDLE)queue;

    return OPRT_OK;
}

/**
 * @brief enqueue
 *
 * @param[in] handle the queue handle
 * @param[in] item pointer to the item that is to be placed on the queue.
 *
 * @return OPRT_OK on success. Others on error
 */
CBD_ERROR_CODE cbd_queue_input(CBD_QUEUE_HANDLE handle, CONST VOID_T *item)
{
    return __enqueue(handle, item, POLICY_SEND_TO_BACK);
}

/**
 * @brief enqueue, instant will be dequeued first
 *
 * @param[in] handle the queue handle
 * @param[in] item pointer to the item that is to be placed on the queue.
 *
 * @return OPRT_OK on success. Others on error
 */
CBD_ERROR_CODE cbd_queue_input_instant(CBD_QUEUE_HANDLE handle, CONST VOID_T *item)
{
    return __enqueue(handle, item, POLICY_SEND_TO_FRONT);
}

/**
 * @brief dequeue
 *
 * @param[in] handle the queue handle
 * @param[in] item the dequeue item buffer, NULL indicates discard the item
 *
 * @return OPRT_OK on success
 */
CBD_ERROR_CODE cbd_queue_output(CBD_QUEUE_HANDLE handle, CONST VOID_T *item)
{
    CBD_ERROR_CODE op_ret = OPRT_OK;

    if(NULL == handle) {
        return OPRT_PARAM_INVALID;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;

    QUEUE_LOCK(queue);
    if(queue->queue_free < queue->queue_len) {
        QUEUE_ITEM_T *queue_item = cbd_list_entry(queue->head.next, QUEUE_ITEM_T, node);
        if(item) {
            memcpy((VOID_T *)item, queue_item->data, queue->item_size);
        }
        cbd_list_del(&(queue_item->node));
        free(queue_item);
        queue->queue_free++;
    } else {
        op_ret = OPRT_NOT_FOUND;
    }
    QUEUE_UNLOCK(queue);

    return op_ret;
}

/**
 * @brief get the peek item,  not dequeue
 *
 * @param[in] handle the queue handle
 * @param[out] item pointer to the buffer into which the received item will be copied.
 *
 * @return OPRT_OK on success.
 */
CBD_ERROR_CODE cbd_queue_peek(CBD_QUEUE_HANDLE handle, CONST VOID_T *item)
{
    CBD_ERROR_CODE op_ret = OPRT_OK;

    if(NULL == handle || NULL == item) {
        return OPRT_PARAM_INVALID;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;

    QUEUE_LOCK(queue);
    if(queue->queue_free < queue->queue_len) {
        QUEUE_ITEM_T *queue_item = cbd_list_entry(queue->head.next, QUEUE_ITEM_T, node);
        memcpy((VOID_T *)item, queue_item->data, queue->item_size);
    } else {
        op_ret = OPRT_NOT_FOUND;
    }
    QUEUE_UNLOCK(queue);

    return op_ret;
}

/**
 * @brief traverse the queue with specific callback
 *
 * @param[in] handle the queue handle
 *
 * @return OPRT_OK on success.
 */
CBD_ERROR_CODE cbd_queue_traverse(CBD_QUEUE_HANDLE handle, TRAVERSE_CB cb, VOID_T *ctx)
{
    if(NULL == handle || NULL == cb) {
        return OPRT_PARAM_INVALID;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;
    struct cbd_list_head *p = NULL;
    QUEUE_ITEM_T *queue_item = NULL;

    QUEUE_LOCK(queue);
    cbd_list_for_each(p, &(queue->head)) {
        queue_item = cbd_list_entry(p, QUEUE_ITEM_T, node);
        if(!cb(queue_item->data, ctx)) {
            break;
        }
    }
    QUEUE_UNLOCK(queue);

    return OPRT_OK;
}

/**
 * @brief clear all items of the queue
 *
 * @param[in] handle the queue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to cbd_error_code.h 
 */
CBD_ERROR_CODE cbd_queue_clear(CBD_QUEUE_HANDLE handle)
{
    if(NULL == handle) {
        return OPRT_PARAM_INVALID;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;
    struct cbd_list_head *p = NULL;
    struct cbd_list_head *n = NULL;
    QUEUE_ITEM_T *queue_item = NULL;

    QUEUE_LOCK(queue);
    cbd_list_for_each_safe(p, n, &(queue->head)) {
        queue_item = cbd_list_entry(p, QUEUE_ITEM_T, node);
        cbd_list_del(&queue_item->node);
        free(queue_item);
    }
    queue->queue_free = queue->queue_len;
    QUEUE_UNLOCK(queue);

    return OPRT_OK;
}

/**
 * @brief get items from start postion, not dequeue
 *
 * @param[in] handle the queue handle
 * @param[in] start the start postion
 * @param[in] items the item buffer
 * @param[in] num the item counts
 *
 * @return OPRT_OK on success.
 */
CBD_ERROR_CODE cbd_queue_get_batch(CBD_QUEUE_HANDLE handle, CONST UINT32_T start, VOID_T *items, CONST UINT32_T num)
{
    if(NULL == handle || NULL == items || 0 == num) {
        return OPRT_PARAM_INVALID;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;
    struct cbd_list_head *p = NULL;
    QUEUE_ITEM_T *queue_item = NULL;
    UINT32_T index = 0;
    UINT32_T count = 0;

    QUEUE_LOCK(queue);
    cbd_list_for_each(p, &(queue->head)) {
        if(index < start) {
            index++;
            continue;
        }

        if(count >= num) {
            break;
        }

        queue_item = cbd_list_entry(p, QUEUE_ITEM_T, node);
        memcpy((UCHAR_T*)items + count * queue->item_size, queue_item->data, queue->item_size);
        count++;
    }
    QUEUE_UNLOCK(queue);

    if(index != start || count != num) {
        return OPRT_NOT_FOUND;
    }

    return OPRT_OK;
}

/**
 * @brief delete the item from the queue position
 *
 * @param[in] handle the queue handle
 * @param[in] num the item count to be deleted from the queue
 *
 * @return OPRT_OK on success. Others on error
 */
CBD_ERROR_CODE cbd_queue_delete_batch(CBD_QUEUE_HANDLE handle, CONST UINT32_T num)
{
    CBD_ERROR_CODE op_ret = OPRT_OK;
    UINT32_T count = num;

    if(NULL == handle || 0 == num) {
        return OPRT_PARAM_INVALID;
    }

    while((count-- > 0) && (OPRT_OK == op_ret)) {
        op_ret = cbd_queue_output(handle, NULL);
    }

    return op_ret;
}

/**
 * @brief get the free queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current free item counts
 */
UINT32_T cbd_queue_get_free_num(CBD_QUEUE_HANDLE handle)
{
    if(NULL == handle) {
        return 0;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;

    return queue->queue_free;
}

/**
 * @brief get the queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current item counts 
 */
UINT32_T cbd_queue_get_used_num(CBD_QUEUE_HANDLE handle)
{
    if(NULL == handle) {
        return 0;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;
    UINT32_T used_num = 0;

    QUEUE_LOCK(queue);
    used_num = queue->queue_len - queue->queue_free;
    QUEUE_UNLOCK(queue);

    return used_num;
}

/**
 * @brief get the queue item number
 *
 * @param[in] handle the queue handle
 *
 * @return the current item counts 
 */
UINT32_T cbd_queue_get_max_num(CBD_QUEUE_HANDLE handle)
{
    if(NULL == handle) {
        return 0;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;

    return queue->queue_len;
}

/**
 * @brief release the queue
 *
 * @param[in] handle the queue handle
 *
 * @return OPRT_OK on success. Others on error, please refer to cbd_error_code.h
 */
CBD_ERROR_CODE cbd_queue_release(CBD_QUEUE_HANDLE handle)
{
    CBD_ERROR_CODE op_ret = OPRT_OK;

    if(NULL == handle) {
        return OPRT_PARAM_INVALID;
    }

    CBD_QUEUE_T *queue = (CBD_QUEUE_T *)handle;

    cbd_queue_clear(handle);

    op_ret = QUEUE_RELEASE_LOCK(queue);
    free(queue);

    return op_ret;
}