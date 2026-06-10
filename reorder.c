#include "reorder.h"
#include <stdlib.h>
#include <string.h>

static void sift_up(reorder_entry_t *h, int i)
{
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h[i].seqnum >= h[p].seqnum)
            break;
        reorder_entry_t tmp = h[i]; h[i] = h[p]; h[p] = tmp;
        i = p;
    }
}

static void sift_down(reorder_entry_t *h, int i, int n)
{
    while (1) {
        int small = i;
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        if (l < n && h[l].seqnum < h[small].seqnum) small = l;
        if (r < n && h[r].seqnum < h[small].seqnum) small = r;
        if (small == i) break;
        reorder_entry_t tmp = h[i]; h[i] = h[small]; h[small] = tmp;
        i = small;
    }
}

int reorder_init(reorder_queue_t *q, int capacity)
{
    q->heap = (reorder_entry_t*)calloc((size_t)capacity, sizeof(reorder_entry_t));
    if (!q->heap) return -1;
    q->capacity = capacity;
    q->count = 0;
    q->next_seqnum = 1;
    clock_gettime(CLOCK_MONOTONIC, &q->last_gap_check);
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->slot_free, NULL);
    for (int i = 0; i < capacity; i++) {
        q->heap[i].slot_id = -1;
        q->heap[i].seqnum = 0;
    }
    return 0;
}

void reorder_push(reorder_queue_t *q, reorder_entry_t *e)
{
    pthread_mutex_lock(&q->lock);
    if (q->count >= q->capacity) {
        pthread_mutex_unlock(&q->lock);
        return;
    }
    q->heap[q->count] = *e;
    sift_up(q->heap, q->count);
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

int reorder_peek(reorder_queue_t *q, reorder_entry_t *out)
{
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    *out = q->heap[0];
    pthread_mutex_unlock(&q->lock);
    return 0;
}

int reorder_pop_min(reorder_queue_t *q, reorder_entry_t *out)
{
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    *out = q->heap[0];
    q->heap[0] = q->heap[--q->count];
    q->heap[q->count].slot_id = -1;
    if (q->count > 0)
        sift_down(q->heap, 0, q->count);
    pthread_cond_signal(&q->slot_free);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

void reorder_free(reorder_queue_t *q)
{
    if (q->heap) {
        free(q->heap);
        q->heap = NULL;
    }
    q->capacity = 0;
    q->count = 0;
}
