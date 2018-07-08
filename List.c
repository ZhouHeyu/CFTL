//
// Created by zj on 18-7-8.
//

#include "List.h"
#include <assert.h>


Node *CreateList()
{
    Node *head;
    head=(Node *)malloc(sizeof(Node));
    if(head==NULL){
        printf("(CreateList)malloc for head is failed\n");
        assert(0);
    }
    //init
    head->lpn_num=-1;
    head->next=head;
    head->pre=head;
    return head;
}

void FreeList(Node * Head)
{
    Node *t1,*t2;
    t1=Head->next;

    while(t1!=Head){
        t2=t1;
        t1=t1->next;
        free(t2);
    }

    free(Head);
}

Node *SearchLPNInList(int lpn,Node *Head)
{
    Node *temp;
    temp=Head->next;
    while(temp !=Head){
        if(temp->lpn_num==lpn){
            return temp;
        }
        temp=temp->next;
    }
//   没有找到则返回空的Node
    return NULL;
}


int IsEmptyList(Node *Head)
{
    if(Head->next==Head){
        return 1;
    }else{
        return 0;
    }
}

int ListLength(Node *Head)
{
    int L=0;
    Node *temp=Head->next;
    while(temp!=Head){
        L++;
        temp=temp->next;
    }
    return L;
}

Node *AddNewLPNInMRU(int lpn,Node *Head)
{
    Node *new;
    new=(Node *)malloc(sizeof(Node));
    if(new==NULL){
        printf("AddNewLPNInMRU  malloc for new node is failed\n");
        assert(0);
    }

//    insert
    new->lpn_num=lpn;
    new->next=Head->next;
    new->pre=Head;

    Head->next->pre=new;
    Head->next=new;

    return new;
}

//从链表删除尾部的节点，但是返回的是被删除的节点的指针
Node *DeleteLRUInList(Node *Head)
{
    Node *temp;
    if(IsEmptyList(Head)){
        printf("List is Empty,can not delete\n");
        assert(0);
    }
    temp=Head->pre;
//    收尾衔接
    temp->pre->next=Head;
    Head->pre=temp->pre;

//    将该节点剥离出list
    temp->pre=NULL;
    temp->next=NULL;

    return temp;
}


Node *InsertNodeInListMRU(Node *Insert,Node *Head)
{
    Node *temp;
//    remove
    Insert->pre->next=Insert->next;
    Insert->next->pre=Insert->pre;

//    Insert->next=NULL;
//    Insert->pre=NULL;
//    Insert
    Insert->pre=Head;
    Insert->next=Head->next;

    Head->next->pre=Insert;
    Head->next=Insert;

    return Head;
}

Node *DeleteNodeInList(Node *DNode,Node *Head)
{
//    debug code
    Node *temp;
    int flag=0;
    temp=Head->pre;
    while(temp!=Head){
        if(temp==DNode){
            flag=1;
            break;
        }
        temp=temp->pre;
    }
    if(flag==0){
        printf("can not find delete node in List\n");
        assert(0);
    }
//     debug end

    DNode->pre->next=DNode->next;
    DNode->next->pre=DNode->pre;

    DNode->pre=NULL;
    DNode->next=NULL;
    return DNode;
}

//debug for print value
void PrintList(Node *Head)
{
    int Count=0;
    Node *temp;
    temp=Head->next;
    while(temp!=Head){
        printf("The %d Node->LPN :%d\n",Count,temp->lpn_num);
        temp=temp->next;
        Count++;
    }
}


/****************debug for List function********************************/
/*
 * Node *Head;

int main()
{
    int i,lpn;
    Node *Temp;
    Head=CreateList();
    for(i=0;i<10;i++){
        AddNewLPNInMRU(i,Head);
    }
//   test print
    printf("AddNewLPNInMRU\n");
    PrintList(Head);

    lpn=6;
    Temp=SearchLPNInList(lpn,Head);
    if(Temp!=NULL){
        printf("search %d in List,the Node ->lpn %d\n",lpn,Temp->lpn_num);
        printf("***************Insert before*************\n");
        PrintList(Head);
        InsertNodeInListMRU(Temp,Head);
        printf("***************Insert after*************\n");
        PrintList(Head);
    }


    lpn=11;
    Temp=SearchLPNInList(lpn,Head);
    if(Temp==NULL){
        printf("can not search %d in List\n",lpn);
        printf("***************Add before*************\n");
        PrintList(Head);
        AddNewLPNInMRU(lpn,Head);
        printf("***************Add after*************\n");
        PrintList(Head);
    }

    printf("***************Delete before*************\n");
    PrintList(Head);
    Temp=DeleteLRUInList(Head);
    printf("delete Node ->LPN %d \n",Temp->lpn_num);
    printf("***************Add after*************\n");
    PrintList(Head);

    FreeList(Head);
    return 0;
}

 */