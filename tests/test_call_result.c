/*
 * Regression test: a plugin invoked as FC_OP_CALL must have its returned
 * token installed as the current token, exactly like FC_OP_TRANSFORM does.
 *
 * Before this fix, exec_call() ran the plugin and discarded `out`, so a
 * `store` immediately after a call always saw whatever token existed before
 * the call (or none at all) rather than what the plugin produced.
 *
 * Also covers the ABI v2 call contract: compiled key=value parameters and
 * the host pointer reach the plugin through fc_call_t, and a legacy
 * name-only image (no interior NUL) still loads and runs with no params.
 */
#include "flowcode.h"
#include "fc_log.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char produced[] = "produced-by-call";

/* What the last echo_producer invocation saw. */
static const char *seen_greeting;
static int saw_greeting;
static void *seen_host;

static fc_plugin_result_t echo_producer(const fc_call_t *call, fc_token_t *out) {
    assert(out);
    assert(call && call->name && strcmp(call->name, "test.produce") == 0);
    seen_host = call->host;
    saw_greeting = 0;
    seen_greeting = NULL;
    if (call->params) {
        size_t i;
        for (i = 0; call->params[i].key; i++) {
            if (strcmp(call->params[i].key, "greeting") == 0) {
                saw_greeting = 1;
                seen_greeting = call->params[i].value;
            }
        }
    }
    out->value = (void *)(uintptr_t)produced;
    out->value_size = (uint32_t)(sizeof(produced) - 1u);
    out->context = NULL;
    out->metadata = 0u;
    return FC_PLUGIN_OK;
}

/* Program: CALL "test.produce" (no incoming token), then STORE "out".
 * The call's arg blob is the v2 encoding when with_params: name\0k\0v\0. */
static void write_program(const char *path, int with_params) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F', 'C', 'B', '1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 2;
    const char plugin_name[] = "test.produce";
    const char params[] = "greeting\0hi-from-params\0";
    const char store_key[] = "out";
    uint32_t name_len = (uint32_t)strlen(plugin_name);
    uint32_t params_len = with_params ? (uint32_t)(sizeof(params) - 1u) : 0u;
    uint32_t key_len = (uint32_t)strlen(store_key);
    uint32_t arg_size = name_len + 1u + params_len + key_len;
    fc_instruction_t ins_call = {FC_OP_CALL, 0, name_len + 1u + params_len};
    fc_instruction_t ins_store = {FC_OP_STORE, name_len + 1u + params_len, key_len};

    assert(f);
    fwrite(magic, 1, 4, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&reserved, sizeof(reserved), 1, f);
    fwrite(&count, sizeof(count), 1, f);
    fwrite(&arg_size, sizeof(arg_size), 1, f);
    fwrite(&ins_call, sizeof(ins_call), 1, f);
    fwrite(&ins_store, sizeof(ins_store), 1, f);
    fwrite(plugin_name, 1, name_len, f);
    fputc('\0', f);
    if (params_len) fwrite(params, 1, params_len, f);
    fwrite(store_key, 1, key_len, f);
    fclose(f);
}

static void run_case(const char *path, int with_params, void *host) {
    fc_program_t program;
    fc_state_store_t *state;
    fc_plugin_registry_t *plugins;
    fc_vm_t *vm;
    const void *val;
    uint32_t val_size;

    write_program(path, with_params);
    assert(fc_program_load_file(path, &program) == 0);

    state = fc_state_create();
    plugins = fc_plugins_create();
    assert(fc_plugins_register(plugins, "test.produce", echo_producer) == 0);

    vm = fc_vm_create(&program, state, plugins);
    assert(vm);
    fc_vm_set_host(vm, host);
    assert(fc_vm_run(vm) == 0);

    assert(fc_state_get(state, "out", &val, &val_size) == 0);
    assert(val_size == sizeof(produced) - 1u);
    assert(memcmp(val, produced, val_size) == 0);

    assert(seen_host == host);
    if (with_params) {
        assert(saw_greeting == 1);
        assert(seen_greeting && strcmp(seen_greeting, "hi-from-params") == 0);
    } else {
        assert(saw_greeting == 0);
    }

    fc_vm_destroy(vm);
    fc_plugins_destroy(plugins);
    fc_state_destroy(state);
    fc_program_free(&program);
}

int main(void) {
    int host_marker = 0;
    void *host = &host_marker;

    fc_log_set_level(FC_LOG_ERROR);
    printf("--- test_call_result ---\n");

    run_case("tests/call_result.fcb", 0, host);
    printf("  PASS: a call's plugin result becomes the current token (legacy blob)\n");
    printf("  PASS: a legacy name-only image carries no params and still runs\n");

    run_case("tests/call_params.fcb", 1, host);
    printf("  PASS: compiled params and the host pointer reach the plugin\n");

    printf("--- all call-result tests passed ---\n");
    return 0;
}
