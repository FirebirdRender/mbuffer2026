#include "ready_pool.h"
#include <stdlib.h>
#include <errno.h>
#include <time.h>

int ready_pool_init(ready_pool_t *rp, size_t capacity)
{
    if (sem_init(&rp->sem, 0, 0) != 0)
        return -1;
    pthread_mutex_init(&rp->lock, NULL);
    rp->indices = (int*)malloc(capacity * sizeof(int));
    if (!rp->indices) {
        sem_destroy(&rp->sem);
        return -1;
    }
    rp->head = 0;
    rp->tail = 0;
    rp->capacity = capacity;
    return 0;
}

void ready_pool_push(ready_pool_t *rp, int slot_id)
{
    pthread_mutex_lock(&rp->lock);
    rp->indices[rp->tail] = slot_id;
    rp->tail = (rp->tail + 1) % (int)rp->capacity;
    pthread_mutex_unlock(&rp->lock);
    sem_post(&rp->sem);
}

// Returns 0 on success, -1 on timeout, -2 on error
int ready_pool_try_pop(ready_pool_t *rp, int *out, long timeout_ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }
    int ret;
    while ((ret = sem_timedwait(&rp->sem, &ts)) == -1 && errno == EINTR)
        ;
    if (ret == -1) {
        if (errno == ETIMEDOUT)
            return -1;
        return -2;
    }
    pthread_mutex_lock(&rp->lock);
    *out = rp->indices[rp->head];
    rp->head = (rp->head + 1) % (int)rp->capacity;
    pthread_mutex_unlock(&rp->lock);
    return 0;
}

int ready_pool_pop_nonblock(ready_pool_t *rp, int *out)
{
    if (sem_trywait(&rp->sem) != 0)
        return -1;
    pthread_mutex_lock(&rp->lock);
    *out = rp->indices[rp->head];
    rp->head = (rp->head + 1) % (int)rp->capacity;
    pthread_mutex_unlock(&rp->lock);
    return 0;
}

int ready_pool_count(ready_pool_t *rp)
{
    int val;
    sem_getvalue(&rp->sem, &val);
    return val;
}

void ready_pool_free(ready_pool_t *rp)
{
    free(rp->indices);
    rp->indices = NULL;
    sem_destroy(&rp->sem);
    pthread_mutex_destroy(&rp->lock);
}
