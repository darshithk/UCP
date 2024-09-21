#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "linked_list.h"

typedef struct __sequencer {
    LinkedList seq;
    uint32_t maxSeqNo;
    uint32_t expectedLastSeqNo;
} sequencer_t;

// Initialize the sequencer
sequencer_t* sequencer_init(void);

// Add a sequence number to the sequencer
int sequencer_add(sequencer_t* seq, uint32_t seqNo, bool isLast);

// Check if a sequence number is in the sequencer
int sequencer_check(sequencer_t* seq, uint32_t seqNo);

// Get the next missing sequence number in the sequencer
void sequencer_iterate_missing_segments(sequencer_t* seq, void (*callback)(uint32_t, void*), void* ctx);

// Check if the sequencer is complete
uint32_t sequencer_complete(sequencer_t* seq);

// Free the sequencer
void sequencer_destroy(sequencer_t* seq);

#endif // SEQUENCER_H
