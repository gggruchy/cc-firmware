/**
 * @file list.h
 * @author your name (you@domain.com)
 * @brief 嵌入式双向链表
 * @version 0.1
 * @date 2022-04-06
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef LIST_H
#define LIST_H
#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
    typedef struct list_head_tag
    {
        struct list_head_tag *prev;
        struct list_head_tag *next;
    } list_head_t;

    typedef list_head_t list_node_t;

    /**
     * @brief 初始化链表
     *
     * @param head
     */
    static inline void list_head_init(list_head_t *head)
    {
        head->next = head->prev = head;
    }

    /**
     * @brief 插入节点
     *
     * @param new_node
     * @param prev_node
     * @param next_node
     */
    static inline void __list_insert__(list_node_t *new_node, list_node_t *prev_node, list_node_t *next_node)
    {
        next_node->prev = new_node;
        new_node->next = next_node;
        new_node->prev = prev_node;
        prev_node->next = new_node;
    }

    /**
     * @brief 头插
     *
     * @param head
     * @param node
     */
    static inline void list_insert_head(list_head_t *head, list_node_t *node)
    {
        __list_insert__(node, head, head->next);
    }

    /**
     * @brief 尾插
     *
     * @param head
     * @param node
     */
    static inline void list_insert_tail(list_head_t *head, list_node_t *node)
    {
        __list_insert__(node, head->prev, head);
    }

    /**
     * @brief 移除节点
     *
     * @param node
     */
    static inline void list_remove(list_node_t *node)
    {
        node->next->prev = node->prev;
        node->prev->next = node->next;
        node->next = node->prev = 0;
    }

    /**
     * @brief 替换节点
     *
     * @param old_node
     * @param new_node
     */
    static inline void list_replace(list_node_t *old_node, list_node_t *new_node)
    {
        new_node->next = old_node->next;
        new_node->prev = old_node->prev;
        old_node->next->prev = new_node;
        old_node->prev->next = new_node;
    }

    /**
     * @brief 判断节点是否位于链表头部
     *
     * @param head
     * @param node
     * @return true
     * @return false
     */
    static inline bool list_is_head(list_head_t *head, list_node_t *node)
    {
        return node->prev == head;
    }

    /**
     * @brief 判断节点是否位于链表尾部
     *
     * @param head
     * @param node
     * @return true
     * @return false
     */
    static inline bool list_is_tail(list_head_t *head, list_node_t *node)
    {
        return node->next == head;
    }

    /**
     * @brief 判断链表是否为空
     *
     * @param head
     * @return true
     * @return false
     */
    static inline bool list_is_empty(list_head_t *head)
    {
        return head->next == head;
    }

#define list_entry(ptr, type, member) ((type *)((char *)(ptr) - (char *)&(((type *)0)->member)))

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_entry(pos, head, type, member)   \
    for (pos = list_entry((head)->next, type, member); \
         &pos->member != (head);                       \
         pos = list_entry((pos)->member.next, type, member))

#define list_for_each_entry_safe(pos, n, head, type, member) \
    for (pos = list_entry((head)->next, type, member),       \
        n = list_entry((pos)->member.next, type, member);    \
         &pos->member != (head);                             \
         pos = n, n = list_entry((n)->member.next, type, member))

    /**
     * @brief 返回链表长度
     *
     * @param head
     * @return size_t
     */
    static inline size_t list_length(list_head_t *head)
    {
        list_node_t *node;
        size_t len = 0;
        list_for_each(node, head)
            len++;
        return len;
    }

#ifdef __cplusplus
} /*extern "C"*/
#endif
#endif