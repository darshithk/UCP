#include "sequencer.h"
#include <stdlib.h>
#include <string.h>

typedef struct __sequencer_item_t {
    uint32_t firstSeqNo;
    uint32_t lastSeqNo;
} sequencer_item_t;

// Initialize the sequencer
sequencer_t* sequencer_init(void) {
    sequencer_t* seq = (sequencer_t*) malloc(sizeof(sequencer_t));
    if (!seq) {
        perror("malloc");
        return NULL;
    }
    memset(seq, 0, sizeof(sequencer_t));
    LinkedListInit(&seq->seq);
    seq->expectedLastSeqNo = UINT32_MAX;
    return seq;
}

static sequencer_item_t* create_sequencer_range(uint32_t first_seq, uint32_t last_seq) {
    sequencer_item_t* item = (sequencer_item_t*) malloc(sizeof(sequencer_item_t));
    if (!item) {
        perror("malloc");
        return NULL;
    }
    item->firstSeqNo = first_seq;
    item->lastSeqNo = last_seq;
    return item;
}

// Add a sequence number to the sequencer
int sequencer_add(sequencer_t* seq, uint32_t seqNo, bool isLast) {
    // printf("Adding sequence to a list of %d fragments \n", LinkedListLength(&seq->seq));

    if (isLast) {
        seq->expectedLastSeqNo = seqNo;
        seq->maxSeqNo = seqNo;
    } else if (seq->maxSeqNo < seqNo) {
        seq->maxSeqNo = seqNo;
    }

    if (sequencer_check(seq, seqNo)) {
        // Check if the sequence number is already in the sequencer
        return FALSE;
    } else if (LinkedListEmpty(&seq->seq)) {
        // if the sequence number is the first in the sequence, insert it at the beginning of the sequencer
        LinkedListAppend(&seq->seq, create_sequencer_range(seqNo, seqNo));
        return TRUE;
    }

    // Add the sequence number to the sequencer
    for (LinkedListElem* elem = LinkedListFirst(&seq->seq); elem != NULL; elem = LinkedListNext(&seq->seq, elem)) {
        // If the sequence number is the next in the sequence, extend the last sequence number
        sequencer_item_t* item = (sequencer_item_t*)elem->obj;
        if (item->lastSeqNo + 1 == seqNo) {
            // printf("Extending (%d->%d) to (%d->%d)\n", item->firstSeqNo, item->lastSeqNo, item->firstSeqNo, seqNo);
            item->lastSeqNo = seqNo;
            // Check if the sequence number can be merged with the next sequence number
            LinkedListElem* nextElem = LinkedListNext(&seq->seq, elem);
            if (nextElem != NULL) {
                sequencer_item_t* nextItem = (sequencer_item_t*)nextElem->obj;
                if (nextItem->firstSeqNo - 1 == item->lastSeqNo) {
                    // printf("Merging (%d->%d) with (%d->%d)\n", item->firstSeqNo, item->lastSeqNo, nextItem->firstSeqNo, nextItem->lastSeqNo);
                    item->lastSeqNo = nextItem->lastSeqNo;
                    LinkedListUnlink(&seq->seq, nextElem);
                    free(nextItem);
                }
            }
            return TRUE;
        }
        // Else If the sequence number is the previous in the sequence, extend the first sequence number
        else if (item->firstSeqNo - 1 == seqNo) {
            // printf("Extending (%d->%d) to (%d->%d)\n", item->firstSeqNo, item->lastSeqNo, seqNo, item->lastSeqNo);
            item->firstSeqNo = seqNo;
            // Check if the sequence number can be merged with the previous sequence number
            LinkedListElem* prevElem = LinkedListPrev(&seq->seq, elem);
            if (prevElem != NULL) {
                sequencer_item_t* prevItem = (sequencer_item_t*)prevElem->obj;
                if (prevItem->lastSeqNo + 1 == item->firstSeqNo) {
                    // printf("Merging (%d->%d) with (%d->%d)\n", prevItem->firstSeqNo, prevItem->lastSeqNo, item->firstSeqNo, item->lastSeqNo);
                    item->firstSeqNo = prevItem->firstSeqNo;
                    LinkedListUnlink(&seq->seq, prevElem);
                    free(prevItem);
                }
            }
            return TRUE;
        } else {
            // Else if the sequence number is between two sequences, but cannot be merged with any of them 
            // Make it a new sequence number and insert it between two sequences
            LinkedListElem* nextElem = LinkedListNext(&seq->seq, elem);
            if (nextElem != NULL) {
                sequencer_item_t* nextItem = (sequencer_item_t*)nextElem->obj;
                if (item->lastSeqNo < seqNo && seqNo < nextItem->firstSeqNo) {
                    LinkedListInsertBefore(&seq->seq, create_sequencer_range(seqNo, seqNo), nextElem);
                    return TRUE;
                }
            } else {
                if (item->lastSeqNo < seqNo) {
                    LinkedListAppend(&seq->seq, create_sequencer_range(seqNo, seqNo));
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

// Check if a sequence number is in the sequencer
int sequencer_check(sequencer_t* seq, uint32_t seqNo) {
    for (LinkedListElem* elem = LinkedListFirst(&seq->seq); elem != NULL; elem = LinkedListNext(&seq->seq, elem)) {
        sequencer_item_t* item = (sequencer_item_t*)elem->obj;
        if (item->firstSeqNo <= seqNo && seqNo <= item->lastSeqNo) {
            return TRUE;
        }
    }
    return FALSE;
}

uint32_t sequencer_complete(sequencer_t* seq) {
    if (LinkedListEmpty(&seq->seq)) {
        return FALSE;
    }
    // Iterate through the link list and check if the blocks are contiguous
    for (LinkedListElem* elem = LinkedListFirst(&seq->seq); elem != NULL; elem = LinkedListNext(&seq->seq, elem)) {
        sequencer_item_t* item = (sequencer_item_t*)elem->obj;
        LinkedListElem* next_elem = LinkedListNext(&seq->seq, elem);
        if (next_elem) {
            sequencer_item_t* next_item = (sequencer_item_t*)next_elem->obj;
            if (item->lastSeqNo + 1 == next_item->firstSeqNo) {
                item->lastSeqNo = next_item->lastSeqNo;
                LinkedListUnlink(&seq->seq, next_elem);
                free(next_item);
            }
        }
    }

    if (LinkedListLength(&seq->seq) == 1) {
        LinkedListElem* elem = LinkedListFirst(&seq->seq);
        sequencer_item_t* item = (sequencer_item_t*)elem->obj;
        if (item->firstSeqNo == 0 && item->lastSeqNo == seq->expectedLastSeqNo) {
            return TRUE;
        }
        
    }
    return FALSE;
}

// Get the next missing sequence number in the sequencer
void sequencer_iterate_missing_segments(sequencer_t* seq, void (*callback)(uint32_t, void*), void* ctx) {
    if (LinkedListEmpty(&seq->seq)) {
        return;
    }

    uint32_t req_seq_no = 0;

    while(req_seq_no < seq->maxSeqNo) {
        if (!sequencer_check(seq, req_seq_no)) {
            callback(req_seq_no, ctx);
        }
        req_seq_no++;
    }
}

// Free the sequencer
void sequencer_destroy(sequencer_t* seq) {
    for (LinkedListElem* elem = LinkedListFirst(&seq->seq); elem != NULL; elem = LinkedListNext(&seq->seq, elem)) {
        free(elem->obj);
    }
    LinkedListUnlinkAll(&seq->seq);
    free(seq);
}
