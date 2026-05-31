/* Test: VM error struct is populated on failure */
#include "flowcode.h"
#include "fc_error.h"
#include "fc_log.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Write a program with a CALL instruction referencing a non-existent plugin.
 * The VM should fail with FC_ERR_PLUGIN_NOT_FOUND and populate the error. */
static void write_plugin_not_found_program(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F','C','B','1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 2;          /* emit + call */
    const char plugin_name[] = "no_such_plugin";
    uint32_t name_len = (uint32_t)strlen(plugin_name);
    uint32_t arg_size = 5 + name_len; /* "hello" + plugin name */
    fc_instruction_t ins_emit  = {FC_OP_EMIT, 0, 5};
    fc_instruction_t ins_call  = {FC_OP_CALL, 5, name_len};
    const char payload[] = "hello";
    assert(f);
    fwrite(magic, 1, 4, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&reserved, sizeof(reserved), 1, f);
    fwrite(&count, sizeof(count), 1, f);
    fwrite(&arg_size, sizeof(arg_size), 1, f);
    fwrite(&ins_emit, sizeof(ins_emit), 1, f);
    fwrite(&ins_call, sizeof(ins_call), 1, f);
    fwrite(payload, 1, 5, f);
    fwrite(plugin_name, 1, name_len, f);
    fclose(f);
}

/* Write a program where route target is out of bounds. */
static void write_route_oob_program(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F','C','B','1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 2;         /* emit + route */
    uint32_t route_target = 99; /* way beyond instruction count */
    uint32_t arg_size = 5 + 4;  /* "hello" + route target */
    fc_instruction_t ins_emit  = {FC_OP_EMIT, 0, 5};
    fc_instruction_t ins_route = {FC_OP_ROUTE, 5, 4};
    const char payload[] = "hello";
    assert(f);
    fwrite(magic, 1, 4, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&reserved, sizeof(reserved), 1, f);
    fwrite(&count, sizeof(count), 1, f);
    fwrite(&arg_size, sizeof(arg_size), 1, f);
    fwrite(&ins_emit, sizeof(ins_emit), 1, f);
    fwrite(&ins_route, sizeof(ins_route), 1, f);
    fwrite(payload, 1, 5, f);
    fwrite(&route_target, sizeof(route_target), 1, f);
    fclose(f);
}

/* Write a store-only program (no emit before store → missing token). */
static void write_missing_token_program(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F','C','B','1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 1;
    uint32_t arg_size = 0;
    fc_instruction_t ins = {FC_OP_STORE, 0, 0};
    assert(f);
    fwrite(magic, 1, 4, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&reserved, sizeof(reserved), 1, f);
    fwrite(&count, sizeof(count), 1, f);
    fwrite(&arg_size, sizeof(arg_size), 1, f);
    fwrite(&ins, sizeof(ins), 1, f);
    fclose(f);
}

int main(void) {
    fc_program_t program;
    fc_state_store_t *state;
    fc_plugin_registry_t *plugins;
    fc_vm_t *vm;
    const fc_error_t *err;
    const void *val;
    uint32_t val_size;
    int rc;

    /* Suppress logging noise during tests */
    fc_log_set_level(FC_LOG_ERROR);

    printf("--- test_error_handling ---\n");

    /* Test 1: plugin not found → FC_ERR_PLUGIN_NOT_FOUND + error stored in state */
    write_plugin_not_found_program("tests/err_plugin.fcb");
    assert(fc_program_load_file("tests/err_plugin.fcb", &program) == 0);
    state = fc_state_create();
    plugins = fc_plugins_create();
    vm = fc_vm_create(&program, state, plugins);
    assert(vm);

    rc = fc_vm_run(vm);
    assert(rc != 0);
    err = fc_vm_last_error(vm);
    assert(err != NULL);
    assert(err->code == FC_ERR_PLUGIN_NOT_FOUND);
    assert(err->instruction_index == 1); /* the CALL instruction */
    assert(strstr(err->message, "no_such_plugin") != NULL);

    /* Verify error was stored in state as __error.1 */
    assert(fc_state_get(state, "__error.1", &val, &val_size) == 0);
    assert(val_size > 0);
    printf("  PASS: plugin not found → FC_ERR_PLUGIN_NOT_FOUND + state\n");

    fc_vm_destroy(vm);
    fc_plugins_destroy(plugins);
    fc_state_destroy(state);
    fc_program_free(&program);

    /* Test 2: route out of bounds → FC_ERR_ROUTE_OOB */
    write_route_oob_program("tests/err_route_oob.fcb");
    assert(fc_program_load_file("tests/err_route_oob.fcb", &program) == 0);
    state = fc_state_create();
    plugins = fc_plugins_create();
    vm = fc_vm_create(&program, state, plugins);
    assert(vm);

    rc = fc_vm_run(vm);
    assert(rc != 0);
    err = fc_vm_last_error(vm);
    assert(err != NULL);
    assert(err->code == FC_ERR_ROUTE_OOB);
    assert(err->instruction_index == 1);
    printf("  PASS: route OOB → FC_ERR_ROUTE_OOB\n");

    fc_vm_destroy(vm);
    fc_plugins_destroy(plugins);
    fc_state_destroy(state);
    fc_program_free(&program);

    /* Test 3: missing token → FC_ERR_MISSING_TOKEN */
    write_missing_token_program("tests/err_missing_token.fcb");
    assert(fc_program_load_file("tests/err_missing_token.fcb", &program) == 0);
    state = fc_state_create();
    plugins = fc_plugins_create();
    vm = fc_vm_create(&program, state, plugins);
    assert(vm);

    rc = fc_vm_run(vm);
    assert(rc != 0);
    err = fc_vm_last_error(vm);
    assert(err != NULL);
    assert(err->code == FC_ERR_MISSING_TOKEN);
    assert(err->instruction_index == 0);
    printf("  PASS: missing token → FC_ERR_MISSING_TOKEN\n");

    fc_vm_destroy(vm);
    fc_plugins_destroy(plugins);
    fc_state_destroy(state);
    fc_program_free(&program);

    /* Test 4: fc_error_name returns correct strings */
    assert(strcmp(fc_error_name(FC_ERR_OK), "FC_ERR_OK") == 0);
    assert(strcmp(fc_error_name(FC_ERR_PLUGIN_NOT_FOUND), "FC_ERR_PLUGIN_NOT_FOUND") == 0);
    assert(strcmp(fc_error_name(FC_ERR_ARENA_FULL), "FC_ERR_ARENA_FULL") == 0);
    assert(strcmp(fc_error_name(FC_ERR_COUNT), "FC_ERR_UNKNOWN") == 0);
    printf("  PASS: fc_error_name returns correct strings\n");

    /* Test 5: successful run has FC_ERR_OK */
    {
        /* Simple emit program */
        FILE *f = fopen("tests/err_ok.fcb", "wb");
        const char magic[4] = {'F','C','B','1'};
        uint16_t version = 1;
        uint16_t reserved = 0;
        uint32_t count = 1;
        uint32_t arg_size = 5;
        fc_instruction_t ins = {FC_OP_EMIT, 0, 5};
        const char payload[] = "hello";
        assert(f);
        fwrite(magic, 1, 4, f);
        fwrite(&version, sizeof(version), 1, f);
        fwrite(&reserved, sizeof(reserved), 1, f);
        fwrite(&count, sizeof(count), 1, f);
        fwrite(&arg_size, sizeof(arg_size), 1, f);
        fwrite(&ins, sizeof(ins), 1, f);
        fwrite(payload, 1, 5, f);
        fclose(f);

        assert(fc_program_load_file("tests/err_ok.fcb", &program) == 0);
        state = fc_state_create();
        plugins = fc_plugins_create();
        vm = fc_vm_create(&program, state, plugins);
        assert(vm);
        rc = fc_vm_run(vm);
        assert(rc == 0);
        err = fc_vm_last_error(vm);
        assert(err != NULL);
        assert(err->code == FC_ERR_OK);
        printf("  PASS: successful run has FC_ERR_OK\n");

        fc_vm_destroy(vm);
        fc_plugins_destroy(plugins);
        fc_state_destroy(state);
        fc_program_free(&program);
    }

    printf("--- all error handling tests passed ---\n");
    return 0;
}
