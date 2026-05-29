/* E2E test: transform-only program via full VM pipeline */
#include "flowcode.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void write_program(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F','C','B','1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 1;        /* transform only */
    uint32_t arg_size = 0;
    fc_instruction_t ins = {FC_OP_TRANSFORM, 0, 0};
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
    int rc;

    write_program("tests/e2e_transform.fcb");

    assert(fc_program_load_file("tests/e2e_transform.fcb", &program) == 0);

    state = fc_state_create();
    plugins = fc_plugins_create();
    vm = fc_vm_create(&program, state, plugins);
    assert(state && plugins && vm);

    rc = fc_vm_run(vm);
    assert(rc == 0);

    fc_vm_destroy(vm);
    fc_plugins_destroy(plugins);
    fc_state_destroy(state);
    fc_program_free(&program);
    return 0;
}
