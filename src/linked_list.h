#ifndef _LINKED_LIST_H_
#define _LINKED_LIST_H_

#ifndef NULL
#define NULL 0L
#endif /* ~NULL */

#ifndef TRUE
#define FALSE 0
#define TRUE 1
#endif /* ~TRUE */

#ifdef WIN32
#define DIR_SEP '\\'
#else /* ~WIN32 */
#define DIR_SEP '/'
#endif /* WIN32 */

#ifndef max
#define max(A,B) (((A)>(B)) ? (A) : (B))
#define min(A,B) (((A)>(B)) ? (B) : (A))
#endif /* ~max */

#ifndef round
#define round(X) (((X) >= 0) ? (int)((X)+0.5) : (int)((X)-0.5))
#endif /* ~round */

#ifndef MAXPATHLENGTH
#define MAXPATHLENGTH 256
#endif /* ~MAXPATHLENGTH */

typedef struct tagLinkedListElem {
    void *obj;
    struct tagLinkedListElem *next;
    struct tagLinkedListElem *prev;
} LinkedListElem;

typedef struct tagLinkedList {
    int num_members;
    LinkedListElem anchor;
} LinkedList;

int  LinkedListLength(LinkedList*);
int  LinkedListEmpty(LinkedList*);

int  LinkedListAppend(LinkedList*, void*);
int  LinkedListPrepend(LinkedList*, void*);
void LinkedListUnlink(LinkedList*, LinkedListElem*);
void LinkedListUnlinkAll(LinkedList*);
int  LinkedListInsertAfter(LinkedList*, void*, LinkedListElem*);
int  LinkedListInsertBefore(LinkedList*, void*, LinkedListElem*);

LinkedListElem *LinkedListFirst(LinkedList*);
LinkedListElem *LinkedListLast(LinkedList*);
LinkedListElem *LinkedListNext(LinkedList*, LinkedListElem*);
LinkedListElem *LinkedListPrev(LinkedList*, LinkedListElem*);

LinkedListElem *LinkedListFind(LinkedList*, void*);

int LinkedListInit(LinkedList*);

#endif /*_LINKED_LIST_H_*/
