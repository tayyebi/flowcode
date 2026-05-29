#include "flowcode.h"
#include "fc_memory.h"

#include <stdio.h>
#include <string.h>

#define FC_MAGIC "FCB1"
#define FC_VERSION 1u

typedef struct {
    char magic[4];
    uint16_t version;
    uint16_t reserved;
    uint32_t instr_count;
    uint32_t arg_size;
} fc_header_t;

static int valid_opcode(uint8_t opcode) {
    return opcode >= FC_OP_EMIT && opcode <= FC_OP_STORE;
}

int fc_program_validate(const fc_program_t *program) {
    uint32_t i;
    if (!program) return -1;
    for (i = 0; i < program->instruction_count; ++i) {
        const fc_instruction_t *ins = &program->instructions[i];
        if (!valid_opcode(ins->opcode)) return -1;
        if (ins->arg_offset > program->arg_blob_size) return -1;
        if (ins->arg_length > program->arg_blob_size - ins->arg_offset) return -1;
    }
    return 0;
}

int fc_program_load_file(const char *path, fc_program_t *out_program) {
    FILE *fp;
    fc_header_t h;
    size_t n;
    if (!path || !out_program) return -1;
    memset(out_program, 0, sizeof(*out_program));

    fp = fopen(path, "rb");
    if (!fp) return -1;

    n = fread(&h, 1, sizeof(h), fp);
    if (n != sizeof(h) || memcmp(h.magic, FC_MAGIC, 4) != 0 || h.version != FC_VERSION) {
        fclose(fp);
        return -1;
    }

    out_program->instruction_count = h.instr_count;
    out_program->arg_blob_size = h.arg_size;

    if (h.instr_count > 0) {
        out_program->instructions = (fc_instruction_t *)fc_calloc(h.instr_count, sizeof(fc_instruction_t));
        if (!out_program->instructions) {
            fclose(fp);
            return -1;
        }
        n = fread(out_program->instructions, sizeof(fc_instruction_t), h.instr_count, fp);
        if (n != h.instr_count) {
            fclose(fp);
            fc_program_free(out_program);
            return -1;
        }
    }

    if (h.arg_size > 0) {
        out_program->arg_blob = (uint8_t *)fc_alloc(h.arg_size);
        if (!out_program->arg_blob) {
            fclose(fp);
            fc_program_free(out_program);
            return -1;
        }
        n = fread(out_program->arg_blob, 1, h.arg_size, fp);
        if (n != h.arg_size) {
            fclose(fp);
            fc_program_free(out_program);
            return -1;
        }
    }

    fclose(fp);
    if (fc_program_validate(out_program) != 0) {
        fc_program_free(out_program);
        return -1;
    }
    return 0;
}

void fc_program_free(fc_program_t *program) {
    if (!program) return;
    fc_free(program->instructions);
    fc_free(program->arg_blob);
    memset(program, 0, sizeof(*program));
}
