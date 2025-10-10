#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "hl_list.h"
#include "hl_assert.h"

typedef struct list
{
    struct list *prev;
    struct list *next;
    union
    {
        void *buf;
        uint32_t size;
    };
} list_t;

static inline list_t *node_create(list_t *list, const void *data);
static inline void node_destory(list_t *node);
static inline void list_insert(list_t *prev, list_t *next, list_t *node);

int hl_list_create(hl_list_t *list, uint32_t size)
{
    HL_ASSERT(list != NULL);

    list_t *lst = (list_t *)malloc(sizeof(list_t));
    if (lst == NULL)
        return -1;

    lst->prev = lst->next = lst;
    lst->size = size;

    *list = lst;
    return 0;
}

void hl_list_destory(hl_list_t *list)
{
    HL_ASSERT(list != NULL);
    HL_ASSERT(*list != NULL);
    list_t *lst = (list_t *)(*list);
    list_t *n1 = lst->prev;
    list_t *n2 = n1->prev;
    while (n1 != lst)
    {
        free(n1->buf);
        free(n1);
        n1 = n2;
        n2 = n2->prev;
    }
    free(lst);
    *list = NULL;
}

int hl_list_push_front(hl_list_t list, const void *data)
{
    HL_ASSERT(list != NULL);
    HL_ASSERT(data != NULL);
    list_t *lst = (list_t *)(list);
    list_t *n = node_create(lst, data);
    if (n == NULL)
        return -1;
    list_insert(lst, lst->next, n);
    return 0;
}

int hl_list_push_back(hl_list_t list, const void *data)
{
    HL_ASSERT(list != NULL);
    HL_ASSERT(data != NULL);
    list_t *lst = (list_t *)(list);
    list_t *n = node_create(lst, data);
    if (n == NULL)
        return -1;
    list_insert(lst->prev, lst, n);
    return 0;
}

int hl_list_pop_front(hl_list_t list, void *buf)
{
    HL_ASSERT(list != NULL);
    HL_ASSERT(buf != NULL);

    list_t *lst = (list_t *)(list);
    if (lst->next != lst)
    {
        memcpy(buf, lst->next->buf, lst->size);
        node_destory(lst->next);
    }
    else
        return -1;
    return 0;
}

int hl_list_pop_back(hl_list_t list, void *buf)
{
    HL_ASSERT(list != NULL);
    HL_ASSERT(buf != NULL);

    list_t *lst = (list_t *)(list);
    if (lst->prev != lst)
    {
        memcpy(buf, lst->prev->buf, lst->size);
        node_destory(lst->prev);
    }
    else
        return -1;
    return 0;
}

int hl_list_is_empty(hl_list_t list)
{
    HL_ASSERT(list != NULL);
    list_t *lst = (list_t *)(list);
    return lst->prev == lst->next;
}

uint32_t hl_list_get_length(hl_list_t list)
{
    HL_ASSERT(list != NULL);
    list_t *lst = (list_t *)(list);
    list_t *n = lst->next;
    uint32_t length = 0;
    while (n != lst)
    {
        n = n->next;
        length++;
    }
    return length;
}

void *hl_list_get_data(hl_list_node_t node)
{
    HL_ASSERT(node != NULL);
    list_t *nd = (list_t *)(node);
    return nd->buf;
}

hl_list_node_t hl_list_get_next_node(hl_list_node_t node)
{
    HL_ASSERT(node != NULL);
    list_t *nd = (list_t *)(node);
    return nd->next;
}

hl_list_node_t hl_list_get_prev_node(hl_list_node_t node)
{
    HL_ASSERT(node != NULL);
    list_t *nd = (list_t *)(node);
    return nd->prev;
}

hl_list_node_t hl_list_get_front_node(hl_list_t list)
{
    HL_ASSERT(list != NULL);
    list_t *lst = (list_t *)(list);
    return lst->next;
}

hl_list_node_t hl_list_get_back_node(hl_list_t list)
{
    HL_ASSERT(list != NULL);
    list_t *lst = (list_t *)(list);
    return lst->prev;
}

int hl_list_insert_front(hl_list_t list, hl_list_node_t node, const void *data)
{
    HL_ASSERT(list != NULL);
    HL_ASSERT(node != NULL);
    HL_ASSERT(data != NULL);

    list_t *lst = (list_t *)(list);
    list_t *nd = (list_t *)(node);
    list_t *n = node_create(lst, data);
    if (n == NULL)
        return -1;
    list_insert(nd, nd->next, n);
    return 0;
}

int hl_list_insert_back(hl_list_t list, hl_list_node_t node, const void *data)
{
    HL_ASSERT(list != NULL);
    HL_ASSERT(node != NULL);
    HL_ASSERT(data != NULL);

    list_t *lst = (list_t *)(list);
    list_t *nd = (list_t *)(node);
    list_t *n = node_create(lst, data);
    if (n == NULL)
        return -1;
    list_insert(nd->prev, nd, n);
    return 0;
}

int hl_list_remove(hl_list_t list, hl_list_node_t node)
{
    HL_ASSERT(list != NULL);
    HL_ASSERT(node != NULL);
    list_t *lst = (list_t *)(list);
    list_t *nd = (list_t *)(node);
    if (lst == nd)
        return -1;
    node_destory(nd);
    return 0;
}

void hl_list_foreach(hl_list_t list, void *user_data, int (*iter)(int index, void *buf, void *user_data))
{
    HL_ASSERT(list != NULL);
    list_t *lst = (list_t *)(list);
    list_t *n = lst->next;
    uint32_t index = 0;
    while (n != lst)
    {
        if (iter(index, n->buf, user_data))
        {
            n = n->next;
            index++;
        }
        else
            return;
    }
}

void hl_list_foreach_reverse(hl_list_t list, void *user_data, int (*iter)(int index, void *buf, void *user_data))
{
    HL_ASSERT(list != NULL);
    list_t *lst = (list_t *)(list);
    list_t *n = lst->prev;
    uint32_t index = 0;
    while (n != lst)
    {
        if (iter(index, n->buf, user_data))
        {
            n = n->prev;
            index++;
        }
        else
            return;
    }
}

static inline list_t *node_create(list_t *list, const void *data)
{
    list_t *n = (list_t *)malloc(sizeof(list_t));
    if (n == NULL)
        return NULL;
    n->buf = (uint8_t *)malloc(list->size);
    if (n->buf == NULL)
    {
        free(n);
        return NULL;
    }
    memcpy(n->buf, data, list->size);
    return n;
}

static inline void node_destory(list_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    free(node->buf);
    free(node);
}

static inline void list_insert(list_t *prev, list_t *next, list_t *node)
{
    prev->next = node;
    node->prev = prev;
    next->prev = node;
    node->next = next;
}