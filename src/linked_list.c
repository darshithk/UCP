/*
 * Author:      Rutuparna K Patkar (rpatkar@usc.edu)
 */

#include <stdlib.h>
#include "linked_list.h"

/*
 * This returns the number of elements in the list.
 * @param list Reference to list
 * @return Number of elements in the list 
 */
int LinkedListLength(LinkedList* list) {
    return list->num_members;
}

/*
 * This function returns TRUE if the list is empty, else returns FALSE.
 * @param list Reference to list
 * @return TRUE if the list is empty, else returns FALSE
 */
int LinkedListEmpty(LinkedList* list) {
    if (list->anchor.next == &(list->anchor) && list->anchor.prev == &(list->anchor))
        return TRUE;
    else 
        return FALSE;
}

/*
 * This function adds object to the list, if list is empty. Else adds object after Last(). 
 * And, returns TRUE if the operation is performed successfully and returns FALSE otherwise.
 * @param list Reference to list
 * @param obj  Reference to object
 * @return TRUE if the operation is performed successfully and returns FALSE otherwise
 */
int LinkedListAppend(LinkedList* list, void* obj) {

    if (list == NULL) // Or object NULL 
        return FALSE;

    LinkedListElem *new_elem = (LinkedListElem *)malloc(sizeof(LinkedListElem));
    new_elem->obj = obj;

    // If list empty
    if (LinkedListEmpty(list)) {
        list->anchor.next = new_elem;
        list->anchor.prev = new_elem;
        new_elem->next = &(list->anchor);
        new_elem->prev = &(list->anchor);
    } else {
        // Not Empty
        new_elem->next = &(list->anchor);
        new_elem->prev = list->anchor.prev;

        list->anchor.prev->next = new_elem;
        list->anchor.prev = new_elem;
    }
    list->num_members++;
    return TRUE;
}

/*
 * This function adds object to the list, if list is empty. Else adds object after First(). 
 * And, returns TRUE if the operation is performed successfully and returns FALSE otherwise.
 * @param list Reference to list
 * @param obj  Reference to object
 * @return TRUE if the operation is performed successfully and returns FALSE otherwise
 */
int LinkedListPrepend(LinkedList* list, void* obj) {
    if (list == NULL) // Or object NULL 
        return FALSE;

    LinkedListElem* new_elem = (LinkedListElem *)malloc(sizeof(LinkedListElem));
    new_elem->obj = obj;

    //If list empty
    if (LinkedListEmpty(list)) {
        list->anchor.next = new_elem;
        list->anchor.prev = new_elem;
        new_elem->next = &(list->anchor);
        new_elem->prev = &(list->anchor);
    } else {
        // Not empty
        new_elem->next = list->anchor.next;
        new_elem->prev = &(list->anchor);

        list->anchor.next->prev = new_elem;
        list->anchor.next = new_elem;
    }
    list->num_members++;
    return TRUE;
}

/*
 * This function unlinks and free() elem from the list. Object is not freed.
 * @param list Reference to list
 * @param elem  Reference to the element of the list
 */
void LinkedListUnlink(LinkedList* list, LinkedListElem* elem) {
    if (LinkedListEmpty(list)) {
        return;
    }

    //LinkedListElem* temp;
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;

    list->num_members--;
    free(elem);
}

/*
 * This function unlinks and free() all elements from the list and make the list empty. 
 * Objects are not freed.
 * @param list Reference to list
 * @param elem  Reference to the element of the list
 */
void LinkedListUnlinkAll(LinkedList* list) {
    LinkedListElem* elem = NULL;
    LinkedListElem* next_elem = NULL;

    elem = LinkedListFirst(list);

    while (elem != NULL) {
        next_elem = LinkedListNext(list, elem);
        LinkedListUnlink(list, elem);
        elem = next_elem;
    }
}

/*
 * This function inserts obj between elem and elem->next. 
 * If elem is NULL, then this function acts same as Append(). 
 * It returns TRUE if the operation is performed successfully and returns FALSE otherwise. 
 * @param list Reference to list
 * @param elem  Reference to the element of the list
 * @return TRUE if the operation is performed successfully and returns FALSE otherwise
 */
int LinkedListInsertAfter(LinkedList* list, void* obj, LinkedListElem* elem) {
    if (elem == NULL) {
        return LinkedListAppend(list, obj);
    }

    // Insert After
    LinkedListElem* new_elem = (LinkedListElem *)malloc(sizeof(LinkedListElem));
    if (new_elem == NULL) {
        return FALSE;
    }

    new_elem->obj = obj;
    new_elem->next = elem->next;
    new_elem->prev = elem;

    elem->next->prev = new_elem;
    elem->next = new_elem;

    list->num_members++;
    return TRUE;
}

/*
 * This function inserts obj between elem and elem->prev. 
 * If elem is NULL, then this function acts same as Prepend(). 
 * It returns TRUE if the operation is performed successfully and returns FALSE otherwise. 
 * @param list Reference to list
 * @param elem  Reference to the element of the list
 * @return TRUE if the operation is performed successfully and returns FALSE otherwise
 */
int LinkedListInsertBefore(LinkedList* list, void* obj, LinkedListElem* elem) {
    if (elem == NULL) {
        return LinkedListPrepend(list, obj);
    }
    
    LinkedListElem* new_elem = (LinkedListElem *)malloc(sizeof(LinkedListElem));
    if (new_elem == NULL) {
        return FALSE;
    }

    new_elem->obj = obj;
    new_elem->next = elem;
    new_elem->prev = elem->prev;

    elem->prev->next = new_elem;
    elem->prev = new_elem;

    list->num_members++;
    return TRUE;
}

/*
 * This function returns the first list element or NULL if the list is empty.
 * @param list Reference to list
 * @return Reference of First list element or NULL if the list is empty.
 */
LinkedListElem *LinkedListFirst(LinkedList* list) {
    if (LinkedListEmpty(list))
        return NULL;
    else 
        return list->anchor.next;
}

/*
 * This function returns the last list element or NULL if the list is empty.
 * @param list Reference to list
 * @return Reference of last list element or NULL if the list is empty.
 */
LinkedListElem *LinkedListLast(LinkedList* list) {
    if (LinkedListEmpty(list))
        return NULL;
    else 
        return list->anchor.prev;
}

/*
 * This function returns elem->next or NULL if elem is the last item on the list
 * @param list Reference to list
 * @param elem  Reference to the element of the list
 * @return Reference of elem->next or NULL if elem is the last item on the list
 */
LinkedListElem *LinkedListNext(LinkedList* list, LinkedListElem* elem) {
    if (elem->next == &(list->anchor))
        return NULL;
    else
        return elem->next;
}

/*
 * This function returns elem->prev or NULL if elem is the first item on the list
 * @param list Reference to list
 * @param elem  Reference to the element of the list
 * @return Reference of elem->prev or NULL if elem is the first item on the list
 */
LinkedListElem *LinkedListPrev(LinkedList* list, LinkedListElem* elem) {
    if (elem->prev == &(list->anchor))
        return NULL;
    else
        return elem->prev;
}

/*
 * This function returns the list element elem such that elem->obj == obj. Returns NULL if no such element can be found.
 * @param list Reference to list
 * @param elem  Reference to the element of the list
 * @return Reference of the list element elem such that elem->obj == obj or NULL if no such element can be found.
 */
LinkedListElem *LinkedListFind(LinkedList* list, void* obj) {

    // If list is emplty, return NULL
    if (LinkedListEmpty(list)) {
        return NULL;
    }

    //Get first Element
    LinkedListElem* elem;

    for (elem = LinkedListFirst(list); elem != NULL; elem = LinkedListNext(list, elem)) {
        if (elem->obj == obj) {
            return elem;
        }
    }
    return NULL;
}

/*
 * This function initialize the list into an empty list. 
 * @param list Reference to list
 * @return TRUE if all is well and returns FALSE if there is an error initializing the list.
 */
int LinkedListInit(LinkedList* list) {
    list->anchor.obj = NULL;

    list->anchor.next = list->anchor.prev = &(list->anchor);
    return TRUE;
}
