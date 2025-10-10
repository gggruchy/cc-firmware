#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "hl_tpool.h"
#include "hl_assert.h"
#include "hl_common.h"
#include "hl_ts_queue.h"
#include "hl_list.h"
#define LOG_TAG "hl_tpool"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"
typedef enum
{
    THREAD_STATE_QUEUE,
    THREAD_STATE_EXECUTING,
    THREAD_STATE_COMPLETED,
} thread_state_t;

typedef struct thread
{
    hl_tpool_function_t function;
    void *args;

    thread_state_t state;
    hl_ts_queue_t msg_in;
    hl_ts_queue_t msg_out;

    hl_easy_cond_t cond;
    sem_t sem_stop;

    hl_list_node_t node;
} thread_t;

typedef struct
{
    hl_list_t thread_list;
    hl_easy_cond_t cond;
    pthread_t *pid;
} tpool_t;

static void *tpool_thread(void *args);
static tpool_t tp;

void hl_tpool_init(int thread_count)
{
    HL_ASSERT(hl_list_create(&tp.thread_list, sizeof(thread_t *)) == 0);
    HL_ASSERT(hl_easy_cond_create(&tp.cond) == 0);
    HL_ASSERT((tp.pid = malloc(sizeof(pthread_t) * thread_count)) != NULL);

    // 将线程池中的线程优先级调至最低
    pthread_attr_t attr;
    struct sched_param param;
    // Initialize thread attribute
    pthread_attr_init(&attr);
    // Set scheduling policy
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    // Set thread priority
    param.sched_priority = sched_get_priority_min(SCHED_RR);;
    pthread_attr_setschedparam(&attr, &param);
    for (int i = 0; i < thread_count; i++)
        HL_ASSERT(pthread_create(&tp.pid[i], &attr, tpool_thread, NULL) == 0);
}

int hl_tpool_create_thread(hl_tpool_thread_t *thread, hl_tpool_function_t function, void *args, uint32_t msg_in_size, uint32_t msg_in_length, uint32_t msg_out_size, uint32_t msg_out_length)
{
    HL_ASSERT(thread != NULL);
    HL_ASSERT(function != NULL);
    LOG_I("hl_tpool_create_thread %p\n", function);
    thread_t *th = (thread_t *)malloc(sizeof(thread_t));
    if (th == NULL)
    {
        LOG_E("can't allocate thread\n");
        return -1;
    }

    memset(th, 0, sizeof(thread_t));
    th->function = function;
    th->args = args;
    th->state = THREAD_STATE_QUEUE;
    if (hl_easy_cond_create(&th->cond) != 0)
    {
        free(th);
        return -1;
    }
    if (msg_in_size > 0 && msg_in_length > 0)
    {
        if (hl_ts_queue_create(&th->msg_in, msg_in_size, msg_in_length) != 0)
        {
            hl_easy_cond_destory(&th->cond);
            free(th);
            return -1;
        }
    }
    if (msg_out_size > 0 && msg_out_length > 0)
    {
        if (hl_ts_queue_create(&th->msg_out, msg_out_size, msg_out_length) != 0)
        {
            if (th->msg_in)
                hl_ts_queue_destory(&th->msg_in);
            hl_easy_cond_destory(&th->cond);
            free(th);
            return -1;
        }
    }
    hl_easy_cond_lock(tp.cond);
    hl_list_push_back(tp.thread_list, &th);
    th->node = hl_list_get_back_node(tp.thread_list);
    *thread = th;
    hl_easy_cond_unlock(tp.cond);
    hl_easy_cond_signal(tp.cond);
    return 0;
}

void hl_tpool_destory_thread(hl_tpool_thread_t *thread)
{
    HL_ASSERT(thread != NULL);
    HL_ASSERT(*thread != NULL);
    thread_t *th = (thread_t *)(*thread);
    hl_easy_cond_lock(tp.cond);
    if (th->msg_out)
        hl_ts_queue_destory(&th->msg_out);
    if (th->msg_in)
        hl_ts_queue_destory(&th->msg_in);
    hl_easy_cond_destory(&th->cond);
    hl_easy_cond_unlock(tp.cond);
    free(th);
    *thread = NULL;
}

void hl_tpool_cancel_thread(hl_tpool_thread_t thread)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;

    hl_easy_cond_lock(tp.cond);
    hl_easy_cond_lock(th->cond);
    if (th->state == THREAD_STATE_QUEUE)
    {
        hl_list_remove(tp.thread_list, th->node);
        th->state = THREAD_STATE_COMPLETED;
    }
    else if (th->state == THREAD_STATE_EXECUTING)
    {
        sem_post(&th->sem_stop);
    }
    hl_easy_cond_unlock(th->cond);
    hl_easy_cond_boardcast(th->cond);
    hl_easy_cond_unlock(tp.cond);
}

int hl_tpool_thread_test_cancel(hl_tpool_thread_t thread)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    if (sem_trywait(&th->sem_stop) == 0)
        return 1;
    return 0;
}

int hl_tpool_thread_send_msg(hl_tpool_thread_t thread, const void *msg)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_out != NULL);
    return hl_ts_queue_enqueue(th->msg_out, msg, 1) == 1 ? 0 : -1;
}

int hl_tpool_thread_recv_msg(hl_tpool_thread_t thread, void *buf)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_in != NULL);
    return hl_ts_queue_dequeue(th->msg_in, buf, 1) == 1 ? 0 : -1;
}

int hl_tpool_thread_send_msg_wait(hl_tpool_thread_t thread, const void *msg, uint64_t millisecond)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_out != NULL);
    return hl_ts_queue_enqueue_wait(th->msg_out, msg, 1, millisecond) == 1 ? 0 : -1;
}

int hl_tpool_thread_recv_msg_wait(hl_tpool_thread_t thread, void *buf, uint64_t millisecond)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_in != NULL);
    return hl_ts_queue_dequeue_wait(th->msg_in, buf, 1, millisecond) == 1 ? 0 : -1;
}

int hl_tpool_thread_send_msg_try(hl_tpool_thread_t thread, const void *msg)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_out != NULL);
    return hl_ts_queue_enqueue_try(th->msg_out, msg, 1) == 1 ? 0 : -1;
}

int hl_tpool_thread_recv_msg_try(hl_tpool_thread_t thread, void *buf)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_in != NULL);
    return hl_ts_queue_dequeue_try(th->msg_in, buf, 1) == 1 ? 0 : -1;
}

int hl_tpool_wait_started(hl_tpool_thread_t thread, uint64_t millisecond)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    struct timespec timeout = hl_calculate_timeout(millisecond);

    hl_easy_cond_lock(th->cond);
    while (th->state == THREAD_STATE_QUEUE)
    {
        if (millisecond == 0)
            hl_easy_cond_wait(th->cond);
        else if (hl_easy_cond_timewait(th->cond, &timeout) == ETIMEDOUT)
        {
            hl_easy_cond_unlock(th->cond);
            hl_easy_cond_unlock(tp.cond);
            return 0;
        }
    }
    hl_easy_cond_unlock(th->cond);
    return 1;
}

int hl_tpool_wait_completed(hl_tpool_thread_t thread, uint64_t millisecond)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    struct timespec timeout = hl_calculate_timeout(millisecond);
    hl_easy_cond_lock(th->cond);
    while (th->state != THREAD_STATE_COMPLETED)
    {
        if (millisecond == 0)
            hl_easy_cond_wait(th->cond);
        else if (hl_easy_cond_timewait(th->cond, &timeout) == ETIMEDOUT)
        {
            hl_easy_cond_unlock(th->cond);
            return 0;
        }
    }
    hl_easy_cond_unlock(th->cond);
    return 1;
}

int hl_tpool_is_started(hl_tpool_thread_t thread)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    thread_state_t state;

    hl_easy_cond_lock(th->cond);
    state = th->state;
    hl_easy_cond_unlock(th->cond);

    return state != THREAD_STATE_QUEUE;
}

int hl_tpool_is_completed(hl_tpool_thread_t thread)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    thread_state_t state;

    hl_easy_cond_lock(th->cond);
    state = th->state;
    hl_easy_cond_unlock(th->cond);

    return state == THREAD_STATE_COMPLETED;
}

int hl_tpool_send_msg(hl_tpool_thread_t thread, const void *msg)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_in != NULL);
    return hl_ts_queue_enqueue(th->msg_in, msg, 1) == 1 ? 0 : -1;
}

int hl_tpool_send_msg_exit(hl_tpool_thread_t thread, const void *msg)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_in != NULL);
    if (hl_ts_queue_is_full(th->msg_in))
        return -1;
    else
        return hl_ts_queue_enqueue(th->msg_in, msg, 1) == 1 ? 0 : -1;
}

int hl_tpool_recv_msg(hl_tpool_thread_t thread, void *buf)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_out != NULL);
    return hl_ts_queue_dequeue(th->msg_out, buf, 1) == 1 ? 0 : -1;
}

int hl_tpool_send_msg_wait(hl_tpool_thread_t thread, const void *msg, uint64_t millisecond)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_in != NULL);
    return hl_ts_queue_enqueue_wait(th->msg_in, msg, 1, millisecond) == 1 ? 0 : -1;
}

int hl_tpool_recv_msg_wait(hl_tpool_thread_t thread, void *buf, uint64_t millisecond)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_out != NULL);
    return hl_ts_queue_dequeue_wait(th->msg_out, buf, 1, millisecond) == 1 ? 0 : -1;
}

int hl_tpool_send_msg_try(hl_tpool_thread_t thread, const void *msg)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_in != NULL);
    return hl_ts_queue_enqueue_try(th->msg_in, msg, 1) == 1 ? 0 : -1;
}

int hl_tpool_recv_msg_try(hl_tpool_thread_t thread, void *buf)
{
    HL_ASSERT(thread != NULL);
    thread_t *th = (thread_t *)thread;
    HL_ASSERT(th->msg_out != NULL);
    return hl_ts_queue_dequeue_try(th->msg_out, buf, 1) == 1 ? 0 : -1;
}

static void *tpool_thread(void *args)
{
    thread_t *th;
    for (;;)
    {
        hl_easy_cond_lock(tp.cond);
        while (hl_list_pop_front(tp.thread_list, &th) != 0)
            hl_easy_cond_wait(tp.cond);
        hl_easy_cond_lock(th->cond);
        th->state = THREAD_STATE_EXECUTING;
        hl_easy_cond_unlock(th->cond);
        hl_easy_cond_boardcast(th->cond);
        hl_easy_cond_unlock(tp.cond);

        th->function(th, th->args);

        hl_easy_cond_lock(tp.cond);
        hl_easy_cond_lock(th->cond);
        th->state = THREAD_STATE_COMPLETED;
        hl_easy_cond_unlock(th->cond);
        hl_easy_cond_boardcast(th->cond);
        hl_easy_cond_unlock(tp.cond);
        usleep(10000);
    }

    return 0;
}
