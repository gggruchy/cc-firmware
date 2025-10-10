#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

#include "hl_sm.h"
#include "hl_ts_queue.h"
#include "hl_assert.h"
#include "hl_common.h"

typedef struct
{
    int event_id;
    void *event_data;
    uint32_t event_data_size;
} sm_event_t;

typedef struct
{
    hl_sm_state_t prev_state;
    hl_sm_state_t current_state;
    hl_sm_state_t next_state;

    void *entry_event_data;
    uint32_t entry_event_data_size;

    void *user_data;

    hl_ts_queue_t event_queue;
    pthread_rwlock_t state_lock;
} sm_t;

int hl_sm_create(hl_sm_t *sm, hl_sm_state_t init_state, void *entry_event_data, uint32_t entry_event_data_size, uint32_t event_queue_length, void *user_data)
{
    HL_ASSERT(sm != NULL);
    HL_ASSERT(init_state != NULL);
    HL_ASSERT(event_queue_length != 0);

    sm_t *s = (sm_t *)malloc(sizeof(sm_t));
    if (s == NULL)
        return -1;
    memset(s, 0, sizeof(sm_t));

    if (pthread_rwlock_init(&s->state_lock, NULL) != 0)
    {
        free(s);
        return -1;
    }

    if (hl_ts_queue_create(&s->event_queue, sizeof(sm_event_t), event_queue_length) != 0)
    {
        pthread_rwlock_unlock(&s->state_lock);
        free(s);
        return -1;
    }

    s->next_state = init_state;
    s->current_state = s->prev_state = NULL;

    if (entry_event_data)
    {
        s->entry_event_data = malloc(entry_event_data_size);
        if (s->entry_event_data == NULL)
        {
            hl_ts_queue_destory(&s->event_queue);
            pthread_rwlock_unlock(&s->state_lock);
            free(s);
            return -1;
        }
        memcpy(s->entry_event_data, entry_event_data, entry_event_data_size);
        s->entry_event_data_size = entry_event_data_size;
    }
    s->user_data = user_data;

    *sm = s;
    return 0;
}

void hl_sm_destroy(hl_sm_t *sm)
{
    HL_ASSERT(sm != NULL);
    HL_ASSERT(*sm != NULL);

    sm_t *s = (sm_t *)(*sm);
    if (s->entry_event_data)
        free(s->entry_event_data);

    sm_event_t event = {0};
    while (hl_ts_queue_get_available_dequeue_length(s->event_queue) > 0)
    {
        if (hl_ts_queue_dequeue(s->event_queue, &event, 1) == 1)
        {
            if (event.event_data)
            {
                free(event.event_data);
                event.event_data = NULL;
            }
        }
    }

    hl_ts_queue_destory(&s->event_queue);
    pthread_rwlock_unlock(&s->state_lock);
    free(s);
    *sm = NULL;
}

void hl_sm_dispatch_event(hl_sm_t sm)
{
    HL_ASSERT(sm != NULL);

    sm_t *s = (sm_t *)(sm);
    sm_event_t event = {0};

    pthread_rwlock_wrlock(&s->state_lock);
    if (s->next_state != s->current_state)
    {
        pthread_rwlock_unlock(&s->state_lock);
        if (s->current_state)
            s->current_state(s, HL_SM_EVENT_ID_EXIT, NULL, 0);

        pthread_rwlock_wrlock(&s->state_lock);
        s->prev_state = s->current_state;
        s->current_state = s->next_state;

        pthread_rwlock_unlock(&s->state_lock);

        s->current_state(s, HL_SM_EVENT_ID_ENTRY, s->entry_event_data, s->entry_event_data_size);
        if (s->entry_event_data)
        {
            free(s->entry_event_data);
            s->entry_event_data = NULL;
            s->entry_event_data_size = 0;
        }
    }
    else
        pthread_rwlock_unlock(&s->state_lock);

    if (hl_ts_queue_dequeue_try(s->event_queue, &event, 1) == 1)
    {
        s->current_state(s, event.event_id, event.event_data, event.event_data_size);
        if (event.event_data)
            free(event.event_data);
    }
    else
    {
        s->current_state(s, HL_SM_EVENT_ID_IDLE, NULL, 0);
    }
}

int hl_sm_send_event(hl_sm_t sm, int event_id, const void *event_data, uint32_t event_data_size)
{
    HL_ASSERT(sm != NULL);

    sm_t *s = (sm_t *)(sm);
    sm_event_t event = {0};

    event.event_id = event_id;
    if (event_data)
    {
        event.event_data = malloc(event_data_size);
        if (event.event_data == NULL)
            return -1;
        memcpy(event.event_data, event_data, event_data_size);
        event.event_data_size = event_data_size;
    }

    if (hl_ts_queue_enqueue(s->event_queue, &event, 1) != 1)
    {
        if (event.event_data)
            free(event.event_data);
        return -1;
    }
    return 0;
}

int hl_sm_get_event_queue_length(hl_sm_t sm)
{
    HL_ASSERT(sm != NULL);
    sm_t *s = (sm_t *)(sm);
    hl_ts_queue_get_available_enqueue_length(s->event_queue);
}

int hl_sm_trans_state(hl_sm_t sm, hl_sm_state_t state, const void *entry_event_data, uint32_t entry_event_data_size)
{
    HL_ASSERT(sm != NULL);
    sm_t *s = (sm_t *)(sm);

    if (entry_event_data)
    {
        s->entry_event_data = malloc(entry_event_data_size);
        if (s->entry_event_data == NULL)
            return -1;
        memcpy(s->entry_event_data, entry_event_data, entry_event_data_size);
        s->entry_event_data_size = entry_event_data_size;
    }
    else
    {
        s->entry_event_data = NULL;
        s->entry_event_data_size = 0;
    }

    pthread_rwlock_wrlock(&s->state_lock);
    s->next_state = state;
    pthread_rwlock_unlock(&s->state_lock);

    return 0;
}

hl_sm_state_t hl_sm_get_current_state(hl_sm_t sm)
{
    HL_ASSERT(sm != NULL);
    sm_t *s = (sm_t *)(sm);
    hl_sm_state_t state;
    pthread_rwlock_rdlock(&s->state_lock);
    state = s->current_state;
    pthread_rwlock_unlock(&s->state_lock);
    return state;
}

hl_sm_state_t hl_sm_get_prev_state(hl_sm_t sm)
{
    HL_ASSERT(sm != NULL);
    sm_t *s = (sm_t *)(sm);
    hl_sm_state_t state;
    pthread_rwlock_rdlock(&s->state_lock);
    state = s->prev_state;
    pthread_rwlock_unlock(&s->state_lock);
    return state;
}

void hl_sm_set_user_data(hl_sm_t sm, void *user_data)
{
    HL_ASSERT(sm != NULL);
    sm_t *s = (sm_t *)(sm);
    s->user_data = user_data;
}

void *hl_sm_get_user_data(hl_sm_t sm)
{
    HL_ASSERT(sm != NULL);
    sm_t *s = (sm_t *)(sm);
    return s->user_data;
}