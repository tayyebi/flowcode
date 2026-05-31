#include "flowcode.h"
#include "fc_error.h"
#include "fc_log.h"

#include <stdio.h>
#include <string.h>

static int run_fcb(const char *path) {
    fc_program_t program;
    fc_state_store_t *state;
    fc_plugin_registry_t *plugins;
    fc_vm_t *vm;
    int rc;

    if (fc_program_load_file(path, &program) != 0) {
        fprintf(stderr, "error: failed to load bytecode file: %s\n", path);
        return 1;
    }

    state = fc_state_create();
    plugins = fc_plugins_create();
    vm = fc_vm_create(&program, state, plugins);
    if (!state || !plugins || !vm) {
        fprintf(stderr, "error: failed to initialize runtime\n");
        fc_program_free(&program);
        fc_state_destroy(state);
        fc_plugins_destroy(plugins);
        fc_vm_destroy(vm);
        return 1;
    }

    rc = fc_vm_run(vm);
    if (rc != 0) {
        const fc_error_t *err = fc_vm_last_error(vm);
        if (err && err->code != FC_ERR_OK) {
            fprintf(stderr, "error: workflow failed at instruction %u: %s",
                    err->instruction_index, fc_error_name(err->code));
            if (err->message[0])
                fprintf(stderr, " — %s", err->message);
            fprintf(stderr, "\n");
        } else {
            fprintf(stderr, "error: workflow execution failed\n");
        }
    }

    fc_vm_destroy(vm);
    fc_plugins_destroy(plugins);
    fc_state_destroy(state);
    fc_program_free(&program);
    return rc == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc != 3 || strcmp(argv[1], "run") != 0) {
        fprintf(stderr, "usage: flowcode run <file.fcb>\n");
        return 1;
    }
    return run_fcb(argv[2]);
}
