//
// Created by zj on 18-7-8.
//

#ifndef CFTL_LIST_H
#define CFTL_LIST_H
//添加双链表的操作，主要涉及链表的插入，遍历，节点的删除
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node{
    int lpn_num;
    struct Node *next;
    struct Node *pre;
}Node;

//以下函数是关于链表的插入，删除，遍历的函数，涉及的是双链表,Head是个没有值的空链表
Node * CreateList();
//释放链表的内存
void FreeList(Node *Head);
Node *AddNewLPNInMRU(int lpn,Node *Head);
Node *SearchLPNInList(int lpn,Node *Head);
Node *DeleteLRUInList(Node *Head);
Node *InsertNodeInListMRU(Node *Insert,Node *Head);
int IsEmptyList(Node *Head);
int ListLength(Node *Head);
//debug for print value
void PrintList(Node *Head);
Node *DeleteNodeInList(Node *DNode,Node *Head);


#endif //CFTL_LIST_H
