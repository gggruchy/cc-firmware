#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "hl_callback.h"
#include "hl_list.h"
#include "hl_assert.h"

typedef struct
{
    hl_callback_function_t function;
    void *user_data;
} cb_unit_t;

typedef struct
{
    hl_list_t list;
    pthread_mutex_t lock;
} cb_t;

int hl_callback_create(hl_callback_t *cb)
{
    HL_ASSERT(cb != NULL);
    cb_t *c = (cb_t *)malloc(sizeof(cb_t));
    if (c == NULL)
        return -1;
    memset(c, 0, sizeof(cb_t));

    if (pthread_mutex_init(&c->lock, NULL) != 0)
    {
        free(c);
        return -1;
    }

    if (hl_list_create(&c->list, sizeof(cb_unit_t)) != 0)
    {
        pthread_mutex_destroy(&c->lock);
        free(c);
        return -1;
    }

    *cb = c;
    return 0;
}

void hl_callback_destory(hl_callback_t *cb)
{
    HL_ASSERT(cb != NULL);
    HL_ASSERT(*cb != NULL);
    cb_t *c = (cb_t *)(*cb);

    pthread_mutex_destroy(&c->lock);
    free(c);
    hl_list_destory(&c->list);

    *cb = NULL;
}

void hl_callback_call(hl_callback_t cb, const void *data)
{
    HL_ASSERT(cb != NULL);
    cb_t *c = (cb_t *)(cb);
    cb_unit_t *unit;

    pthread_mutex_lock(&c->lock);
    hl_list_node_t n = hl_list_get_next_node(c->list);
    while (n != c->list)
    {
        unit = (cb_unit_t *)hl_list_get_data(n);
        unit->function(data, unit->user_data);
        n = hl_list_get_next_node(n);
    }
    pthread_mutex_unlock(&c->lock);
}

void hl_callback_register(hl_callback_t cb, hl_callback_function_t function, void *user_data)
{
    HL_ASSERT(cb != NULL);
    HL_ASSERT(function != NULL);

    cb_t *c = (cb_t *)(cb);
    cb_unit_t unit;

    unit.function = function;
    unit.user_data = user_data;

    pthread_mutex_lock(&c->lock);
    hl_list_push_back(c->list, &unit);
    pthread_mutex_unlock(&c->lock);
}

void hl_callback_unregister(hl_callback_t cb, hl_callback_function_t function, void *user_data)
{
    HL_ASSERT(cb != NULL);
    cb_t *c = (cb_t *)(cb);
    cb_unit_t *unit;
    pthread_mutex_lock(&c->lock);
    hl_list_node_t n = hl_list_get_next_node(c->list);
    hl_list_node_t n1 = hl_list_get_next_node(n);

    while (n != c->list)
    {
        unit = (cb_unit_t *)hl_list_get_data(n);
        if (unit->function == function && unit->user_data == user_data)
            hl_list_remove(c->list, n);
        else if (unit->function == function && unit->user_data == NULL)
            hl_list_remove(c->list, n);
        else if (unit->function == NULL)
            hl_list_remove(c->list, n);
        n = n1;
        n1 = hl_list_get_next_node(n);
    }
    pthread_mutex_unlock(&c->lock);
}
