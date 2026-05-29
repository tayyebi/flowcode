#include "flowcode.h"
#include "fc_memory.h"

#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define FC_DLSYM(handle, name) GetProcAddress((HMODULE)(handle), (name))
#else
#include <dlfcn.h>
#define FC_DLSYM(handle, name) dlsym((handle), (name))
#endif

typedef fc_plugin_descriptor_t *(*fc_plugin_init_fn)(void);

typedef struct {
    char *name;
    fc_plugin_call_fn fn;
} fc_plugin_entry_t;

struct fc_plugin_registry_s {
    fc_plugin_entry_t *entries;
    uint32_t count;
    uint32_t capacity;
};

static int ensure_capacity(fc_plugin_registry_t *r, uint32_t add) {
    fc_plugin_entry_t *next;
    uint32_t i;
    uint32_t need = r->count + add;
    if (need <= r->capacity) return 0;
    while (r->capacity < need) r->capacity = r->capacity ? r->capacity * 2u : 8u;
    next = (fc_plugin_entry_t *)fc_calloc(r->capacity, sizeof(fc_plugin_entry_t));
    if (!next) return -1;
    for (i = 0; i < r->count; ++i) next[i] = r->entries[i];
    fc_free(r->entries);
    r->entries = next;
    return 0;
}

fc_plugin_registry_t *fc_plugins_create(void) {
    return (fc_plugin_registry_t *)fc_calloc(1, sizeof(fc_plugin_registry_t));
}

void fc_plugins_destroy(fc_plugin_registry_t *registry) {
    uint32_t i;
    if (!registry) return;
    for (i = 0; i < registry->count; ++i) {
        fc_free(registry->entries[i].name);
    }
    fc_free(registry->entries);
    fc_free(registry);
}

int fc_plugins_load(fc_plugin_registry_t *registry, const char *path) {
    void *handle;
    void *symbol;
    fc_plugin_init_fn init_fn;
    fc_plugin_descriptor_t *desc;
    uint32_t i;
    if (!registry || !path) return -1;
#ifdef _WIN32
    handle = (void *)LoadLibraryA(path);
#else
    handle = dlopen(path, RTLD_NOW);
#endif
    if (!handle) return -1;

    symbol = FC_DLSYM(handle, "fc_plugin_init");
    memcpy(&init_fn, &symbol, sizeof(init_fn));
    if (!init_fn) return -1;
    desc = init_fn();
    if (!desc || !desc->exports) return -1;

    if (ensure_capacity(registry, desc->export_count) != 0) return -1;

    for (i = 0; i < desc->export_count; ++i) {
        char *name = (char *)fc_alloc(strlen(desc->exports[i].name) + 1u);
        if (!name) return -1;
        strcpy(name, desc->exports[i].name);
        registry->entries[registry->count].name = name;
        registry->entries[registry->count].fn = desc->exports[i].fn;
        registry->count += 1u;
    }

    return 0;
}

fc_plugin_call_fn fc_plugins_resolve(const fc_plugin_registry_t *registry, const char *name) {
    uint32_t i;
    if (!registry || !name) return NULL;
    for (i = 0; i < registry->count; ++i) {
        if (strcmp(registry->entries[i].name, name) == 0) return registry->entries[i].fn;
    }
    return NULL;
}
