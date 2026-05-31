#ifndef FLOWCODE_H
#define FLOWCODE_H

#include <stddef.h>
#include <stdint.h>

#include "fc_error.h"

typedef enum {
    FC_OP_EMIT = 0x01,
    FC_OP_AWAIT = 0x02,
    FC_OP_CALL = 0x03,
    FC_OP_TRANSFORM = 0x04,
    FC_OP_ROUTE = 0x05,
    FC_OP_LOOP = 0x06,
    FC_OP_STORE = 0x07
} fc_opcode_t;

#if defined(_MSC_VER)
#pragma pack(push, 1)
typedef struct {
    uint8_t opcode;
    uint32_t arg_offset;
    uint32_t arg_length;
} fc_instruction_t;
#pragma pack(pop)
#else
typedef struct __attribute__((packed)) {
    uint8_t opcode;
    uint32_t arg_offset;
    uint32_t arg_length;
} fc_instruction_t;
#endif

typedef struct fc_token_s {
    void *value;
    uint32_t value_size;
    void *context;
    uint64_t metadata;
} fc_token_t;

typedef struct {
    fc_instruction_t *instructions;
    uint32_t instruction_count;
    uint8_t *arg_blob;
    uint32_t arg_blob_size;
} fc_program_t;

typedef struct {
    fc_program_t *program;
    uint32_t ip;
    fc_token_t *token;
} fc_frame_t;

typedef enum {
    FC_PLUGIN_OK = 0,
    FC_PLUGIN_ERR = 1
} fc_plugin_result_t;

typedef fc_plugin_result_t (*fc_plugin_call_fn)(const fc_token_t *in, fc_token_t *out);

typedef struct {
    const char *name;
    fc_plugin_call_fn fn;
} fc_plugin_export_t;

typedef struct {
    uint32_t export_count;
    fc_plugin_export_t *exports;
} fc_plugin_descriptor_t;

typedef struct fc_state_store_s fc_state_store_t;
typedef struct fc_plugin_registry_s fc_plugin_registry_t;
typedef struct fc_scheduler_s fc_scheduler_t;
typedef struct fc_vm_s fc_vm_t;

int fc_program_load_file(const char *path, fc_program_t *out_program);
void fc_program_free(fc_program_t *program);
int fc_program_validate(const fc_program_t *program);

fc_state_store_t *fc_state_create(void);
void fc_state_destroy(fc_state_store_t *store);
int fc_state_set(fc_state_store_t *store, const char *key, const void *value, uint32_t size, uint64_t ttl_seconds);
int fc_state_get(fc_state_store_t *store, const char *key, const void **value, uint32_t *size);

fc_plugin_registry_t *fc_plugins_create(void);
void fc_plugins_destroy(fc_plugin_registry_t *registry);
int fc_plugins_load(fc_plugin_registry_t *registry, const char *path);
fc_plugin_call_fn fc_plugins_resolve(const fc_plugin_registry_t *registry, const char *name);

fc_scheduler_t *fc_scheduler_create(uint32_t capacity);
void fc_scheduler_destroy(fc_scheduler_t *scheduler);
int fc_scheduler_enqueue(fc_scheduler_t *scheduler, const fc_frame_t *frame);
int fc_scheduler_dequeue(fc_scheduler_t *scheduler, fc_frame_t *out_frame);

fc_vm_t *fc_vm_create(fc_program_t *program, fc_state_store_t *state, fc_plugin_registry_t *plugins);
void fc_vm_destroy(fc_vm_t *vm);
int fc_vm_run(fc_vm_t *vm);

/** Inspect the last error after fc_vm_run returns non-zero. */
const fc_error_t *fc_vm_last_error(const fc_vm_t *vm);

#endif
