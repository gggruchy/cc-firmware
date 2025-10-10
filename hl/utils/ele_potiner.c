/***********************************************************
*  File: uni_poter.c
*  Author: Doon
*  Date: 240326
***********************************************************/
#define _UNI_POTER_GLOBAL
#include "cbd_pointer.h"
/***********************************************************
*************************micro defe***********************
***********************************************************/

/***********************************************************
*************************variable defe********************
***********************************************************/

/***********************************************************
*************************function defe********************
***********************************************************/

/***********************************************************
*  Function: __list_add 前后两节点之间插入一个新的节点
*  put: pNew->新加入的节点
*         pPrev->前节点
*         pNext->后节点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
static void __list_add( const P_LIST_HEAD pNew,  const P_LIST_HEAD pPrev,\
                        const P_LIST_HEAD pNext)
{
    pNext->prev = pNew;
    pNew->next = pNext;
    pNew->prev = pPrev;
    pPrev->next = pNew;
}

/***********************************************************
*  Function: __list_del 将前后两节点之间的节点移除
*  put: pPrev->前节点
*         pNext->后节点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
static void __list_del( const P_LIST_HEAD pPrev,  const P_LIST_HEAD pNext)
{
    pNext->prev = pPrev;
    pPrev->next = pNext;
}

/***********************************************************
*  Function: cbd_list_empty 判断该链表是否为空
*  put: pHead
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
int cbd_list_empty( const P_LIST_HEAD pHead)
{
    return pHead->next == pHead;
}

/***********************************************************
*  Function: cbd_list_add 插入一个新的节点
*  put: pNew->新节点
*         pHead->插入点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
void cbd_list_add( const P_LIST_HEAD pNew,  const P_LIST_HEAD pHead)
{
    __list_add(pNew, pHead, pHead->next);
}

/***********************************************************
*  Function: cbd_list_add_tail 反向插入一个新的节点
*  put: pNew->新节点
*         pHead->插入点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
void cbd_list_add_tail( const P_LIST_HEAD pNew,  const P_LIST_HEAD pHead)
{
    __list_add(pNew, pHead->prev, pHead);
}

/***********************************************************
*  Function: cbd_list_splice 接合两条链表
*  put: pList->被接合链
*         pHead->接合链
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
void cbd_list_splice( const P_LIST_HEAD pList,  const P_LIST_HEAD pHead)
{
    P_LIST_HEAD pFirst = pList->next;

    if (pFirst != pList) // 该链表不为空
    {
        P_LIST_HEAD pLast = pList->prev;
        P_LIST_HEAD pAt = pHead->next; // list接合处的节点

        pFirst->prev = pHead;
        pHead->next = pFirst;
        pLast->next = pAt;
        pAt->prev = pLast;
    }
}

/***********************************************************
*  Function: cbd_list_del 链表中删除节点
*  put: pEntry->待删除节点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
void cbd_list_del( const P_LIST_HEAD pEntry)
{
    __list_del(pEntry->prev, pEntry->next);
}

/***********************************************************
*  Function: list_del 链表中删除某节点并初始化该节点
*  put: pEntry->待删除节点
*  Output: none
*  Return: none
*  Date: 120427
***********************************************************/
void cbd_list_del_it( const P_LIST_HEAD pEntry)
{
    __list_del(pEntry->prev, pEntry->next);
    INIT_LIST_HEAD(pEntry);
}