#include "flowcode.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void write_sample(const char *path) {
    FILE *f = fopen(path, "wb");
    const char magic[4] = {'F','C','B','1'};
    uint16_t version = 1;
    uint16_t reserved = 0;
    uint32_t count = 2;
    uint32_t arg_size = 4;
    fc_instruction_t ins1 = {FC_OP_EMIT, 0, 4};
    fc_instruction_t ins2 = {FC_OP_STORE, 0, 0};
    const char payload[4] = {'d','o','n','e'};
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
    int rc;
    write_sample("tests/cli_sample.fcb");
    rc = system("./flowcode run tests/cli_sample.fcb");
    assert(rc == 0);
    return 0;
}
