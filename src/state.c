#include "flowcode.h"
#include "fc_memory.h"

#include <string.h>
#include <time.h>

typedef struct {
    char *key;
    void *value;
    uint32_t size;
    uint64_t expires_at;
    int in_use;
} fc_state_entry_t;

struct fc_state_store_s {
    fc_state_entry_t *entries;
    uint32_t count;
    uint32_t capacity;
};

static uint64_t now_seconds(void) {
    return (uint64_t)time(NULL);
}

static int ensure_capacity(fc_state_store_t *s) {
    uint32_t i;
    fc_state_entry_t *next;
    if (s->count < s->capacity) return 0;
    next = (fc_state_entry_t *)fc_calloc(s->capacity ? s->capacity * 2u : 16u, sizeof(fc_state_entry_t));
    if (!next) return -1;
    for (i = 0; i < s->capacity; ++i) next[i] = s->entries[i];
    fc_free(s->entries);
    s->entries = next;
    s->capacity = s->capacity ? s->capacity * 2u : 16u;
    return 0;
}

fc_state_store_t *fc_state_create(void) {
    return (fc_state_store_t *)fc_calloc(1, sizeof(fc_state_store_t));
}

void fc_state_destroy(fc_state_store_t *store) {
    uint32_t i;
    if (!store) return;
    for (i = 0; i < store->capacity; ++i) {
        fc_free(store->entries[i].key);
        fc_free(store->entries[i].value);
    }
    fc_free(store->entries);
    fc_free(store);
}

int fc_state_set(fc_state_store_t *store, const char *key, const void *value, uint32_t size, uint64_t ttl_seconds) {
    uint32_t i;
    char *new_key;
    void *new_value;
    if (!store || !key || (!value && size > 0)) return -1;

    for (i = 0; i < store->capacity; ++i) {
        if (store->entries[i].in_use && strcmp(store->entries[i].key, key) == 0) {
            new_value = size ? fc_alloc(size) : NULL;
            if (size && !new_value) return -1;
            if (size) memcpy(new_value, value, size);
            fc_free(store->entries[i].value);
            store->entries[i].value = new_value;
            store->entries[i].size = size;
            store->entries[i].expires_at = ttl_seconds ? now_seconds() + ttl_seconds : 0;
            return 0;
        }
    }

    if (ensure_capacity(store) != 0) return -1;
    for (i = 0; i < store->capacity; ++i) {
        if (!store->entries[i].in_use) {
            new_key = (char *)fc_alloc(strlen(key) + 1u);
            if (!new_key) return -1;
            strcpy(new_key, key);
            new_value = size ? fc_alloc(size) : NULL;
            if (size && !new_value) {
                fc_free(new_key);
                return -1;
            }
            if (size) memcpy(new_value, value, size);

            store->entries[i].key = new_key;
            store->entries[i].value = new_value;
            store->entries[i].size = size;
            store->entries[i].expires_at = ttl_seconds ? now_seconds() + ttl_seconds : 0;
            store->entries[i].in_use = 1;
            store->count += 1;
            return 0;
        }
    }
    return -1;
}

int fc_state_get(fc_state_store_t *store, const char *key, const void **value, uint32_t *size) {
    uint32_t i;
    if (!store || !key || !value || !size) return -1;
    for (i = 0; i < store->capacity; ++i) {
        if (store->entries[i].in_use && strcmp(store->entries[i].key, key) == 0) {
            if (store->entries[i].expires_at && store->entries[i].expires_at <= now_seconds()) {
                fc_free(store->entries[i].key);
                fc_free(store->entries[i].value);
                memset(&store->entries[i], 0, sizeof(store->entries[i]));
                store->count -= 1;
                return -1;
            }
            *value = store->entries[i].value;
            *size = store->entries[i].size;
            return 0;
        }
    }
    return -1;
}
