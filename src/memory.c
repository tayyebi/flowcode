#include "fc_memory.h"
#include "fc_log.h"

#include <stdlib.h>

void *fc_alloc(size_t size) {
    void *p = malloc(size);
    if (!p && size > 0) fc_log(FC_LOG_ERROR, "fc_alloc failed for %zu bytes", size);
    return p;
}

void *fc_calloc(size_t count, size_t size) {
    void *p = calloc(count, size);
    if (!p && count > 0 && size > 0) fc_log(FC_LOG_ERROR, "fc_calloc failed for %zu * %zu bytes", count, size);
    return p;
}

void *fc_realloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p && size > 0) fc_log(FC_LOG_ERROR, "fc_realloc failed for %zu bytes", size);
    return p;
}

void fc_free(void *ptr) { free(ptr); }
