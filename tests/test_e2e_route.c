/* E2E test: route instruction via full VM pipeline */
#include "flowcode.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void write_program(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F','C','B','1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 3;             /* emit, route, store */
    uint32_t arg_size = 4 + 5;     /* route target (4 bytes) + "hello" (5 bytes) */
    /* ins0: emit "hello" (arg_offset=4, arg_length=5) */
    fc_instruction_t ins_emit = {FC_OP_EMIT, 4, 5};
    /* ins1: route to instruction 2 (arg_offset=0, arg_length=4) */
    fc_instruction_t ins_route = {FC_OP_ROUTE, 0, 4};
    /* ins2: store last token */
    fc_instruction_t ins_store = {FC_OP_STORE, 0, 0};
    uint32_t route_target = 2;
    const char payload[] = "hello";
    assert(f);
    fwrite(magic, 1, 4, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&reserved, sizeof(reserved), 1, f);
    fwrite(&count, sizeof(count), 1, f);
    fwrite(&arg_size, sizeof(arg_size), 1, f);
    fwrite(&ins_emit, sizeof(ins_emit), 1, f);
    fwrite(&ins_route, sizeof(ins_route), 1, f);
    fwrite(&ins_store, sizeof(ins_store), 1, f);
    /* arg blob: route target first, then payload */
    fwrite(&route_target, sizeof(route_target), 1, f);
    fwrite(payload, 1, 5, f);
    fclose(f);
}

int main(void) {
    fc_program_t program;
    fc_state_store_t *state;
    fc_plugin_registry_t *plugins;
    fc_vm_t *vm;
    const void *val;
    uint32_t val_size;
    int rc;

    write_program("tests/e2e_route.fcb");

    assert(fc_program_load_file("tests/e2e_route.fcb", &program) == 0);
    assert(fc_program_validate(&program) == 0);

    state = fc_state_create();
    plugins = fc_plugins_create();
    vm = fc_vm_create(&program, state, plugins);
    assert(state && plugins && vm);

    rc = fc_vm_run(vm);
    assert(rc == 0);

    /* verify the route executed and store ran */
    assert(fc_state_get(state, "last_token", &val, &val_size) == 0);
    assert(val_size == 5);
    assert(memcmp(val, "hello", 5) == 0);

    fc_vm_destroy(vm);
    fc_plugins_destroy(plugins);
    fc_state_destroy(state);
    fc_program_free(&program);
    return 0;
}
