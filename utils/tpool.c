#include "tpool.h"
#include "list.h"
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct
{
    tpool_routine_t routine;
    void *arg;
    int *ret;
    ts_queue_t *in;
    ts_queue_t *out;
    routine_id_t id;
    list_node_t list_node;
} tpool_thread_info_t;

typedef struct tpool_tag tpool_t;
typedef struct
{
    pthread_t tid;
    sem_t started;
    sem_t exit;
    ts_queue_t *cmd;
    ts_queue_t *ack;
    tpool_t *tpool;
    routine_id_t id;
} tpool_tcb_t;

struct tpool_tag
{
    list_head_t list_head;
    tpool_tcb_t *tcb_list;
    int thread_count;
    int global_count;
    pthread_mutex_t cond_mutex;
    pthread_cond_t cond;
};

static int tpool_tcb_init(tpool_t *tpool, tpool_tcb_t *tcb);
static void tpool_tcb_deinit(tpool_t *tpool, tpool_tcb_t *tcb);
static void *tpool_tcb_routine(void *args);

void *tpool_create(int thread_count)
{
    tpool_t *tpool;

    if ((tpool = (tpool_t *)malloc(sizeof(tpool_t))) == NULL)
        goto Exit;
    memset(tpool, 0, sizeof(tpool_t));

    list_head_init(&tpool->list_head);

    if (pthread_mutex_init(&tpool->cond_mutex, NULL) != 0)
        goto FreePool;

    if (pthread_cond_init(&tpool->cond, NULL) != 0)
        goto FreeCondMutex;

    if ((tpool->tcb_list = (tpool_tcb_t *)malloc(thread_count * sizeof(tpool_tcb_t))) == NULL)
    {
        printf("malloc for tcb_list failed\n");
        goto FreeCond;
    }
    memset(tpool->tcb_list, 0, thread_count * sizeof(tpool_tcb_t));

    for (int i = 0; i < thread_count; i++)
    {
        if (tpool_tcb_init(tpool, &tpool->tcb_list[i]) != 0)
        {
            printf("tpool tcb init failed\n");
            goto FreeTcb;
        }
        tpool->thread_count++;
    }

    return tpool;

FreeTcb:
    for (int i = 0; i < tpool->thread_count; i++)
    {
        sem_wait(&tpool->tcb_list[i].started);
        tpool_tcb_deinit(tpool, &tpool->tcb_list[i]);
    }
    free(tpool->tcb_list);
FreeCond:
    pthread_cond_destroy(&tpool->cond);
FreeCondMutex:
    pthread_mutex_destroy(&tpool->cond_mutex);
FreePool:
    free(tpool);
Exit:
    return NULL;
}

void tpool_destroy(void *handle)
{
    tpool_t *tpool = (tpool_t *)handle;

    for (int i = 0; i < tpool->thread_count; i++)
    {
        sem_wait(&tpool->tcb_list[i].started);
        sem_post(&tpool->tcb_list[i].exit);
        pthread_cancel(tpool->tcb_list[i].tid);
        tpool_tcb_deinit(tpool, &tpool->tcb_list[i]);
    }
    free(tpool->tcb_list);
    pthread_cond_destroy(&tpool->cond);
    pthread_mutex_destroy(&tpool->cond_mutex);
    tpool_thread_info_t *pos;
    tpool_thread_info_t *next;
    list_for_each_entry_safe(pos, next, &tpool->list_head, tpool_thread_info_t, list_node)
    {
        list_remove(&pos->list_node);
        free(pos);
    }

    free(tpool);
}

void *tpool_get_handle()
{
    static void *handle = NULL;
    if (handle == NULL)
        handle = tpool_create(16);
    return handle;
}

routine_id_t tpool_enqueue_routine(void *handle, tpool_routine_t routine, void *arg, int *ret, ts_queue_t *in, ts_queue_t *out)
{
    tpool_t *tpool = (tpool_t *)handle;
    tpool_thread_info_t *thread_info = (tpool_thread_info_t *)malloc(sizeof(tpool_thread_info_t));
    if (thread_info == NULL)
        return -1;

    memset(thread_info, 0, sizeof(tpool_thread_info_t));
    
    thread_info->routine = routine;
    thread_info->arg = arg;
    thread_info->ret = ret;
    thread_info->in = in;
    thread_info->out = out;

    pthread_mutex_lock(&tpool->cond_mutex);
    pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &tpool->cond_mutex);
    list_insert_tail(&tpool->list_head, &thread_info->list_node);
    thread_info->id = tpool->global_count++;
    pthread_cond_signal(&tpool->cond);
    pthread_cleanup_pop(1);

    return thread_info->id;
}

bool tpool_cancel_routine(void *handle, routine_id_t id)
{
    tpool_t *tpool = (tpool_t *)handle;
    tpool_thread_info_t *pos;
    tpool_thread_info_t *next;
    bool found = false;
    int tcb_id = 0;
    pthread_mutex_lock(&tpool->cond_mutex);

    //任务在队列排队则直接移除
    list_for_each_entry_safe(pos, next, &tpool->list_head, tpool_thread_info_t, list_node)
    {
        if (pos->id == id)
        {
            list_remove(&pos->list_node);
            free(pos);
            pthread_mutex_unlock(&tpool->cond_mutex);
            return true;
        }
    }

    //任务已经在执行则通过队列通知任务使任务退出
    for (int i = 0; i < tpool->thread_count; i++)
    {
        if (tpool->tcb_list[i].id == id)
        {
            tcb_id = i;
            ts_queue_reset(tpool->tcb_list[tcb_id].ack);
            if (ts_queue_enqueue(tpool->tcb_list[tcb_id].cmd, NULL, 1) == 1)
                found = true;
            break;
        }
    }
    pthread_mutex_unlock(&tpool->cond_mutex);

    if (found)
        ts_queue_dequeue(tpool->tcb_list[tcb_id].ack, NULL, 1);
    return true;
}

bool tpool_test_cancel(ts_queue_t *cmd_queue)
{
    if (ts_queue_try_peek(cmd_queue, NULL, 1) == 1)
        return true;
    return false;
}

static int tpool_tcb_init(tpool_t *tpool, tpool_tcb_t *tcb)
{
    memset(tcb, 0, sizeof(tpool_tcb_t));
    tcb->tpool = tpool;
    sem_init(&tcb->started, 0, 0);
    sem_init(&tcb->exit, 0, 0);
    tcb->cmd = ts_queue_create(sizeof(int), 1);
    tcb->ack = ts_queue_create(sizeof(int), 1);
    if (pthread_create(&tcb->tid, NULL, tpool_tcb_routine, tcb) != 0)
    {
        free(tcb);
        return -1;
    }
    return 0;
}

static void tpool_tcb_deinit(tpool_t *tpool, tpool_tcb_t *tcb)
{
    pthread_join(tcb->tid, NULL);
    ts_queue_destroy(tcb->ack);
    ts_queue_destroy(tcb->cmd);
    sem_destroy(&tcb->exit);
    sem_destroy(&tcb->started);
}

static void *tpool_tcb_routine(void *args)
{
    tpool_thread_info_t *thread_info = NULL;
    tpool_tcb_t *tcb = (tpool_tcb_t *)args;
    tpool_t *tpool = tcb->tpool;
    sem_post(&tcb->started);

    while (1)
    {
        if (sem_trywait(&tcb->exit) != 0)
        {
            thread_info = NULL;
            pthread_mutex_lock(&tpool->cond_mutex);
            pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, &tpool->cond_mutex);
            while (list_is_empty(&tpool->list_head))
                pthread_cond_wait(&tpool->cond, &tpool->cond_mutex);
            thread_info = list_entry(tpool->list_head.next, tpool_thread_info_t, list_node);
            list_remove(&thread_info->list_node);
            tcb->id = thread_info->id;
            ts_queue_reset(tcb->cmd);
            ts_queue_reset(tcb->ack);
            pthread_cleanup_pop(1);
            if (thread_info)
            {
                int ret = thread_info->routine(thread_info->arg, tcb->cmd, thread_info->in, thread_info->out);
                if (thread_info->ret)
                    *(thread_info->ret) = ret;
                free(thread_info);
                ts_queue_enqueue(tcb->ack, NULL, 1);
            }
        }
        else
        {
            pthread_exit((void *)0);
        }
    }
    return (void *)0;
}
