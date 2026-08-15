/*
 * Regression test: a plugin invoked as FC_OP_CALL must have its returned
 * token installed as the current token, exactly like FC_OP_TRANSFORM does.
 *
 * Before this fix, exec_call() ran the plugin and discarded `out`, so a
 * `store` immediately after a call always saw whatever token existed before
 * the call (or none at all) rather than what the plugin produced.
 */
#include "flowcode.h"
#include "fc_log.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char produced[] = "produced-by-call";

static fc_plugin_result_t echo_producer(const fc_token_t *in, fc_token_t *out) {
    (void)in;
    assert(out);
    out->value = (void *)(uintptr_t)produced;
    out->value_size = (uint32_t)(sizeof(produced) - 1u);
    out->context = NULL;
    out->metadata = 0u;
    return FC_PLUGIN_OK;
}

/* Program: CALL "test.produce" (no incoming token), then STORE "out". */
static void write_program(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F', 'C', 'B', '1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 2;
    const char plugin_name[] = "test.produce";
    const char store_key[] = "out";
    uint32_t name_len = (uint32_t)strlen(plugin_name);
    uint32_t key_len = (uint32_t)strlen(store_key);
    uint32_t arg_size = name_len + key_len;
    fc_instruction_t ins_call = {FC_OP_CALL, 0, name_len};
    fc_instruction_t ins_store = {FC_OP_STORE, name_len, key_len};

    assert(f);
    fwrite(magic, 1, 4, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&reserved, sizeof(reserved), 1, f);
    fwrite(&count, sizeof(count), 1, f);
    fwrite(&arg_size, sizeof(arg_size), 1, f);
    fwrite(&ins_call, sizeof(ins_call), 1, f);
    fwrite(&ins_store, sizeof(ins_store), 1, f);
    fwrite(plugin_name, 1, name_len, f);
    fwrite(store_key, 1, key_len, f);
    fclose(f);
}

int main(void) {
    fc_program_t program;
    fc_state_store_t *state;
    fc_plugin_registry_t *plugins;
    fc_vm_t *vm;
    const void *val;
    uint32_t val_size;

    fc_log_set_level(FC_LOG_ERROR);
    printf("--- test_call_result ---\n");

    write_program("tests/call_result.fcb");
    assert(fc_program_load_file("tests/call_result.fcb", &program) == 0);

    state = fc_state_create();
    plugins = fc_plugins_create();
    assert(fc_plugins_register(plugins, "test.produce", echo_producer) == 0);

    vm = fc_vm_create(&program, state, plugins);
    assert(vm);
    assert(fc_vm_run(vm) == 0);

    assert(fc_state_get(state, "out", &val, &val_size) == 0);
    assert(val_size == sizeof(produced) - 1u);
    assert(memcmp(val, produced, val_size) == 0);
    printf("  PASS: a call's plugin result becomes the current token\n");

    fc_vm_destroy(vm);
    fc_plugins_destroy(plugins);
    fc_state_destroy(state);
    fc_program_free(&program);

    printf("--- all call-result tests passed ---\n");
    return 0;
}
