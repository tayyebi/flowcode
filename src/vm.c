#include "flowcode.h"
#include "fc_memory.h"

#include <string.h>

typedef struct {
    fc_token_t *tokens;
    uint32_t capacity;
    uint32_t used;
} fc_token_arena_t;

struct fc_vm_s {
    fc_program_t *program;
    fc_state_store_t *state;
    fc_plugin_registry_t *plugins;
    fc_scheduler_t *scheduler;
    fc_frame_t frame_pool[256];
    uint32_t frame_pool_used;
    fc_token_arena_t arena;
};

static fc_token_t *arena_token(fc_vm_t *vm) {
    if (vm->arena.used >= vm->arena.capacity) return NULL;
    return &vm->arena.tokens[vm->arena.used++];
}

static int exec_emit(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    fc_token_t *token;
    if (ins->arg_length == 0) return 0;
    token = arena_token(vm);
    if (!token) return -1;
    token->value = &vm->program->arg_blob[ins->arg_offset];
    token->value_size = ins->arg_length;
    frame->token = token;
    return 0;
}

static int exec_call(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    char name[256];
    fc_plugin_call_fn fn;
    fc_token_t out;
    if (ins->arg_length == 0 || ins->arg_length >= sizeof(name)) return -1;
    memcpy(name, &vm->program->arg_blob[ins->arg_offset], ins->arg_length);
    name[ins->arg_length] = '\0';
    fn = fc_plugins_resolve(vm->plugins, name);
    if (!fn) return -1;
    memset(&out, 0, sizeof(out));
    if (fn(frame->token, &out) != FC_PLUGIN_OK) return -1;
    return 0;
}

static int exec_transform(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    fc_token_t *out;
    (void)ins;
    /* If a transform plugin is named, attempt to resolve and invoke it. */
    if (ins->arg_length > 0) {
        char name[256];
        fc_plugin_call_fn fn;
        if (ins->arg_length >= sizeof(name)) return -1;
        memcpy(name, &vm->program->arg_blob[ins->arg_offset], ins->arg_length);
        name[ins->arg_length] = '\0';
        fn = fc_plugins_resolve(vm->plugins, name);
        if (fn) {
            fc_token_t result;
            memset(&result, 0, sizeof(result));
            if (fn(frame->token, &result) != FC_PLUGIN_OK) return -1;
            out = arena_token(vm);
            if (!out) return -1;
            *out = result;
            frame->token = out;
            return 0;
        }
    }
    /* Identity pass-through: propagate the current token unchanged. */
    return 0;
}

static int exec_store(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    char key[256];
    if (!frame->token || !frame->token->value) return -1;
    /* Use the instruction argument as the store key when provided. */
    if (ins->arg_length > 0 && ins->arg_length < sizeof(key)) {
        memcpy(key, &vm->program->arg_blob[ins->arg_offset], ins->arg_length);
        key[ins->arg_length] = '\0';
        return fc_state_set(vm->state, key, frame->token->value, frame->token->value_size, 0);
    }
    return fc_state_set(vm->state, "last_token", frame->token->value, frame->token->value_size, 0);
}

static int exec_route(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    uint32_t target;
    (void)frame;
    if (ins->arg_length != sizeof(uint32_t)) return -1;
    memcpy(&target, &vm->program->arg_blob[ins->arg_offset], sizeof(uint32_t));
    if (target >= vm->program->instruction_count) return -1;
    frame->ip = target;
    return 1;
}

static int exec_loop(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    uint32_t target;
    if (ins->arg_length != sizeof(uint32_t)) return -1;
    memcpy(&target, &vm->program->arg_blob[ins->arg_offset], sizeof(uint32_t));
    if (target >= vm->program->instruction_count) return -1;
    frame->ip = target;
    return 1;
}

fc_vm_t *fc_vm_create(fc_program_t *program, fc_state_store_t *state, fc_plugin_registry_t *plugins) {
    fc_vm_t *vm;
    if (!program || !state || !plugins) return NULL;
    vm = (fc_vm_t *)fc_calloc(1, sizeof(fc_vm_t));
    if (!vm) return NULL;
    vm->program = program;
    vm->state = state;
    vm->plugins = plugins;
    vm->scheduler = fc_scheduler_create(128u);
    vm->arena.capacity = 1024u;
    vm->arena.tokens = (fc_token_t *)fc_calloc(vm->arena.capacity, sizeof(fc_token_t));
    if (!vm->scheduler || !vm->arena.tokens) {
        fc_vm_destroy(vm);
        return NULL;
    }
    return vm;
}

void fc_vm_destroy(fc_vm_t *vm) {
    if (!vm) return;
    fc_scheduler_destroy(vm->scheduler);
    fc_free(vm->arena.tokens);
    fc_free(vm);
}

int fc_vm_run(fc_vm_t *vm) {
    fc_frame_t frame;
    if (!vm || !vm->program) return -1;

    memset(&frame, 0, sizeof(frame));
    frame.program = vm->program;
    frame.ip = 0;

    while (frame.ip < frame.program->instruction_count) {
        const fc_instruction_t *ins = &frame.program->instructions[frame.ip];
        int status = 0;
        switch (ins->opcode) {
            case FC_OP_EMIT:
                status = exec_emit(vm, ins, &frame);
                break;
            case FC_OP_AWAIT:
                status = fc_scheduler_enqueue(vm->scheduler, &frame);
                break;
            case FC_OP_CALL:
                status = exec_call(vm, ins, &frame);
                break;
            case FC_OP_TRANSFORM:
                status = exec_transform(vm, ins, &frame);
                break;
            case FC_OP_ROUTE:
                status = exec_route(vm, ins, &frame);
                break;
            case FC_OP_LOOP:
                status = exec_loop(vm, ins, &frame);
                break;
            case FC_OP_STORE:
                status = exec_store(vm, ins, &frame);
                break;
            default:
                return -1;
        }
        if (status < 0) return -1;
        if (status == 0) frame.ip += 1u;
    }

    return 0;
}
