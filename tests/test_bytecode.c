#include "flowcode.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void write_sample(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F','C','B','1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 2;
    uint32_t arg_size = 5;
    fc_instruction_t ins1 = {FC_OP_EMIT, 0, 5};
    fc_instruction_t ins2 = {FC_OP_STORE, 0, 0};
    const char payload[5] = {'h','e','l','l','o'};
    assert(f);
    fwrite(magic, 1, 4, f);
    fwrite(&version, sizeof(version), 1, f);
    fwrite(&reserved, sizeof(reserved), 1, f);
    fwrite(&count, sizeof(count), 1, f);
    fwrite(&arg_size, sizeof(arg_size), 1, f);
    fwrite(&ins1, sizeof(ins1), 1, f);
    fwrite(&ins2, sizeof(ins2), 1, f);
    fwrite(payload, 1, sizeof(payload), f);
    fclose(f);
}

int main(void) {
    fc_program_t p;
    write_sample("/tmp/workspace/tayyebi/flowcode/tests/sample.fcb");
    assert(fc_program_load_file("/tmp/workspace/tayyebi/flowcode/tests/sample.fcb", &p) == 0);
    assert(p.instruction_count == 2);
    assert(p.instructions[0].opcode == FC_OP_EMIT);
    assert(p.arg_blob_size == 5);
    assert(memcmp(p.arg_blob, "hello", 5) == 0);
    fc_program_free(&p);
    return 0;
}
