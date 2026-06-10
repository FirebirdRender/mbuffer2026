#ifndef READY_POOL_H
#define READY_POOL_H

#include <semaphore.h>
#include <pthread.h>
#include <stddef.h>

typedef struct {
    sem_t            sem;      // count of ready buffers
    pthread_mutex_t  lock;     // protects ring indices
    int             *indices;  // ring buffer of slot indices
    int              head;
    int              tail;
    size_t           capacity;
} ready_pool_t;

extern int  ready_pool_init(ready_pool_t *rp, size_t capacity);
extern void ready_pool_push(ready_pool_t *rp, int slot_id);
extern int  ready_pool_try_pop(ready_pool_t *rp, int *out, long timeout_ms);
extern int  ready_pool_pop_nonblock(ready_pool_t *rp, int *out);
extern void ready_pool_free(ready_pool_t *rp);
extern int  ready_pool_count(ready_pool_t *rp);

#endif
