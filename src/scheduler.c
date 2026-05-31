#include "flowcode.h"
#include "fc_memory.h"
#include "fc_log.h"

#include <string.h>

struct fc_scheduler_s {
    fc_frame_t *queue;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
};

fc_scheduler_t *fc_scheduler_create(uint32_t capacity) {
    fc_scheduler_t *s = (fc_scheduler_t *)fc_calloc(1, sizeof(fc_scheduler_t));
    if (!s) return NULL;
    s->capacity = capacity ? capacity : 128u;
    s->queue = (fc_frame_t *)fc_calloc(s->capacity, sizeof(fc_frame_t));
    if (!s->queue) {
        fc_free(s);
        return NULL;
    }
    return s;
}

void fc_scheduler_destroy(fc_scheduler_t *scheduler) {
    if (!scheduler) return;
    fc_free(scheduler->queue);
    fc_free(scheduler);
}

int fc_scheduler_enqueue(fc_scheduler_t *scheduler, const fc_frame_t *frame) {
    if (!scheduler || !frame) {
        fc_log(FC_LOG_ERROR, "scheduler enqueue called with NULL argument");
        return -1;
    }
    if (scheduler->count == scheduler->capacity) {
        fc_log(FC_LOG_ERROR, "scheduler queue full (%u/%u)", scheduler->count, scheduler->capacity);
        return -1;
    }
    /* warn when approaching capacity (>75%) */
    if (scheduler->count > 0 && scheduler->count * 4u >= scheduler->capacity * 3u) {
        fc_log(FC_LOG_WARN, "scheduler queue at %u/%u (%.0f%% full)",
               scheduler->count, scheduler->capacity,
               (double)scheduler->count / scheduler->capacity * 100.0);
    }
    scheduler->queue[scheduler->tail] = *frame;
    scheduler->tail = (scheduler->tail + 1u) % scheduler->capacity;
    scheduler->count += 1;
    return 0;
}

int fc_scheduler_dequeue(fc_scheduler_t *scheduler, fc_frame_t *out_frame) {
    if (!scheduler || !out_frame || scheduler->count == 0) return -1;
    *out_frame = scheduler->queue[scheduler->head];
    memset(&scheduler->queue[scheduler->head], 0, sizeof(fc_frame_t));
    scheduler->head = (scheduler->head + 1u) % scheduler->capacity;
    scheduler->count -= 1;
    return 0;
}
