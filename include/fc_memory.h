#ifndef FC_MEMORY_H
#define FC_MEMORY_H

#include <stddef.h>

void *fc_alloc(size_t size);
void *fc_calloc(size_t count, size_t size);
void *fc_realloc(void *ptr, size_t size);
void fc_free(void *ptr);

#endif
