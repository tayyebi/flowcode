#include "fc_memory.h"

#include <stdlib.h>

void *fc_alloc(size_t size) { return malloc(size); }
void *fc_calloc(size_t count, size_t size) { return calloc(count, size); }
void *fc_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
void fc_free(void *ptr) { free(ptr); }
