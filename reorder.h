#ifndef REORDER_H
#define REORDER_H

#include <stdint.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    int      slot_id;      // Buffer slot index (-1 = empty)
    uint64_t seqnum;       // Seqnum (heap key)
    int      stream_id;    // Which stream delivered it
    uint32_t payload_len;  // Actual payload bytes in buffer
} reorder_entry_t;

typedef struct {
    reorder_entry_t *heap;         // Min-heap array
    int              capacity;
    int              count;
    uint64_t         next_seqnum;  // Next expected seqnum for output
    pthread_mutex_t  lock;
    pthread_cond_t   not_empty;    // Signaled on push
    pthread_cond_t   slot_free;    // Signaled on pop_min
    struct timespec  last_gap_check; // For NAK gap detection in outputThreadMux
} reorder_queue_t;

extern int  reorder_init(reorder_queue_t *q, int capacity);
extern void reorder_push(reorder_queue_t *q, reorder_entry_t *e);
extern int  reorder_peek(reorder_queue_t *q, reorder_entry_t *out);
extern int  reorder_pop_min(reorder_queue_t *q, reorder_entry_t *out);
extern void reorder_free(reorder_queue_t *q);

#endif
