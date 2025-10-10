#ifndef HL_LIST_H
#define HL_LIST_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    typedef void *hl_list_t;
    typedef void *hl_list_node_t;

    int hl_list_create(hl_list_t *list, uint32_t size);
    void hl_list_destory(hl_list_t *list);

    int hl_list_push_front(hl_list_t list, const void *data);
    int hl_list_push_back(hl_list_t list, const void *data);
    int hl_list_pop_front(hl_list_t list, void *buf);
    int hl_list_pop_back(hl_list_t list, void *buf);

    void hl_list_foreach(hl_list_t list,void *user_data, int (*iter)(int index, void *buf, void *user_data));
    void hl_list_foreach_reverse(hl_list_t list,void *user_data, int (*iter)(int index, void *buf, void *user_data));

    int hl_list_insert_front(hl_list_t list, hl_list_node_t node, const void *data);
    int hl_list_insert_back(hl_list_t list, hl_list_node_t node, const void *data);
    int hl_list_remove(hl_list_t list, hl_list_node_t node);

    int hl_list_is_empty(hl_list_t list);
    uint32_t hl_list_get_length(hl_list_t list);

    void *hl_list_get_data(hl_list_node_t node);
    hl_list_node_t hl_list_get_next_node(hl_list_node_t node);
    hl_list_node_t hl_list_get_prev_node(hl_list_node_t node);

    hl_list_node_t hl_list_get_front_node(hl_list_node_t node);
    hl_list_node_t hl_list_get_back_node(hl_list_node_t node);



#ifdef __cplusplus
}
#endif

#endif