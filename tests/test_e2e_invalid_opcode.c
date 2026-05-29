/* E2E test: invalid opcode detection via VM */
#include "flowcode.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void write_program(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F','C','B','1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 1;
    uint32_t arg_size = 0;
    /* opcode 0xFF is invalid */
    fc_instruction_t ins = {0xFF, 0, 0};
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
    int rc;

    write_program("tests/e2e_invalid_opcode.fcb");

    /* load should fail validation because 0xFF is not a valid opcode */
    rc = fc_program_load_file("tests/e2e_invalid_opcode.fcb", &program);
    assert(rc != 0);

    return 0;
}
