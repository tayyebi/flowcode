#include "flowcode.h"
#include "fc_memory.h"
#include "fc_error.h"
#include "fc_log.h"

#include <stdio.h>
#include <string.h>

#define FC_MAX_NAME_LENGTH 256
#define FC_MAX_FRAME_POOL 256

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
    fc_frame_t frame_pool[FC_MAX_FRAME_POOL];
    uint32_t frame_pool_used;
    fc_token_arena_t arena;
    fc_error_t last_error;
};

static void vm_set_error(fc_vm_t *vm, fc_error_code_t code, uint32_t ip, const char *msg) {
    vm->last_error.code = code;
    vm->last_error.instruction_index = ip;
    if (msg) {
        snprintf(vm->last_error.message, FC_ERROR_MSG_MAX, "%s", msg);
    } else {
        vm->last_error.message[0] = '\0';
    }
    vm->last_error.detail[0] = '\0';
    fc_log(FC_LOG_ERROR, "ip=%u %s: %s", ip, fc_error_name(code), msg ? msg : "");
}

/**
 * Store an error record in the state store under __error.<ip>.
 * This lets subsequent workflow steps inspect what failed.
 */
static void vm_store_error_state(fc_vm_t *vm) {
    char key[64];
    char val[FC_ERROR_MSG_MAX + 64];
    int len;
    snprintf(key, sizeof(key), "__error.%u", vm->last_error.instruction_index);
    len = snprintf(val, sizeof(val), "%s: %s",
                   fc_error_name(vm->last_error.code),
                   vm->last_error.message);
    if (len > 0) {
        fc_state_set(vm->state, key, val, (uint32_t)len, 0);
    }
}

static fc_token_t *arena_token(fc_vm_t *vm) {
    if (vm->arena.used >= vm->arena.capacity) {
        fc_log(FC_LOG_WARN, "token arena at capacity (%u/%u)",
               vm->arena.used, vm->arena.capacity);
        return NULL;
    }
    return &vm->arena.tokens[vm->arena.used++];
}

static int exec_emit(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    fc_token_t *token;
    if (ins->arg_length == 0) return 0;
    token = arena_token(vm);
    if (!token) {
        vm_set_error(vm, FC_ERR_ARENA_FULL, frame->ip, "token arena exhausted in emit");
        return -1;
    }
    token->value = &vm->program->arg_blob[ins->arg_offset];
    token->value_size = ins->arg_length;
    frame->token = token;
    return 0;
}

static int exec_call(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    char name[FC_MAX_NAME_LENGTH];
    fc_plugin_call_fn fn;
    fc_token_t out;
    if (ins->arg_length == 0 || ins->arg_length >= sizeof(name)) {
        vm_set_error(vm, FC_ERR_ARG_OVERFLOW, frame->ip, "call arg_length invalid");
        return -1;
    }
    memcpy(name, &vm->program->arg_blob[ins->arg_offset], ins->arg_length);
    name[ins->arg_length] = '\0';
    fn = fc_plugins_resolve(vm->plugins, name);
    if (!fn) {
        vm_set_error(vm, FC_ERR_PLUGIN_NOT_FOUND, frame->ip, name);
        vm_store_error_state(vm);
        /* Plugin isolation: record error but allow continuation via on_error
         * For now we return -1 (halt) since on_error routing is not yet wired.
         * Once on_error targets are in bytecode, this path will route instead. */
        return -1;
    }
    memset(&out, 0, sizeof(out));
    if (fn(frame->token, &out) != FC_PLUGIN_OK) {
        vm_set_error(vm, FC_ERR_PLUGIN_CALL, frame->ip, name);
        vm_store_error_state(vm);
        return -1;
    }
    return 0;
}

static int exec_transform(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    /* If a transform plugin is named, attempt to resolve and invoke it. */
    if (ins->arg_length > 0) {
        char name[FC_MAX_NAME_LENGTH];
        fc_plugin_call_fn fn;
        if (ins->arg_length >= sizeof(name)) {
            vm_set_error(vm, FC_ERR_ARG_OVERFLOW, frame->ip, "transform name too long");
            return -1;
        }
        memcpy(name, &vm->program->arg_blob[ins->arg_offset], ins->arg_length);
        name[ins->arg_length] = '\0';
        fn = fc_plugins_resolve(vm->plugins, name);
        if (fn) {
            fc_token_t result;
            fc_token_t *out;
            memset(&result, 0, sizeof(result));
            if (fn(frame->token, &result) != FC_PLUGIN_OK) {
                vm_set_error(vm, FC_ERR_PLUGIN_CALL, frame->ip, name);
                vm_store_error_state(vm);
                return -1;
            }
            out = arena_token(vm);
            if (!out) {
                vm_set_error(vm, FC_ERR_ARENA_FULL, frame->ip, "token arena exhausted in transform");
                return -1;
            }
            *out = result;
            frame->token = out;
            return 0;
        }
    }
    /* Identity pass-through: propagate the current token unchanged. */
    return 0;
}

static int exec_store(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    char key[FC_MAX_NAME_LENGTH];
    if (!frame->token || !frame->token->value) {
        vm_set_error(vm, FC_ERR_MISSING_TOKEN, frame->ip, "store requires a token");
        return -1;
    }
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
    if (ins->arg_length != sizeof(target)) {
        vm_set_error(vm, FC_ERR_ARG_OVERFLOW, frame->ip, "route arg_length mismatch");
        return -1;
    }
    memcpy(&target, &vm->program->arg_blob[ins->arg_offset], sizeof(target));
    if (target >= vm->program->instruction_count) {
        vm_set_error(vm, FC_ERR_ROUTE_OOB, frame->ip, "route target out of bounds");
        return -1;
    }
    frame->ip = target;
    return 1;
}

static int exec_loop(fc_vm_t *vm, const fc_instruction_t *ins, fc_frame_t *frame) {
    uint32_t target;
    if (ins->arg_length != sizeof(target)) {
        vm_set_error(vm, FC_ERR_ARG_OVERFLOW, frame->ip, "loop arg_length mismatch");
        return -1;
    }
    memcpy(&target, &vm->program->arg_blob[ins->arg_offset], sizeof(target));
    if (target >= vm->program->instruction_count) {
        vm_set_error(vm, FC_ERR_ROUTE_OOB, frame->ip, "loop target out of bounds");
        return -1;
    }
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
    vm->scheduler = NULL;
    fc_free(vm->arena.tokens);
    vm->arena.tokens = NULL;
    fc_free(vm);
}

const fc_error_t *fc_vm_last_error(const fc_vm_t *vm) {
    if (!vm) return NULL;
    return &vm->last_error;
}

int fc_vm_run(fc_vm_t *vm) {
    fc_frame_t frame;
    if (!vm || !vm->program) return -1;

    memset(&frame, 0, sizeof(frame));
    memset(&vm->last_error, 0, sizeof(vm->last_error));
    frame.program = vm->program;
    frame.ip = 0;

    fc_log(FC_LOG_INFO, "vm starting, %u instructions", vm->program->instruction_count);

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
                vm_set_error(vm, FC_ERR_INVALID_OPCODE, frame.ip, "unrecognized opcode");
                return -1;
        }
        if (status < 0) return -1;
        if (status == 0) frame.ip += 1u;
    }

    fc_log(FC_LOG_INFO, "vm completed successfully");
    return 0;
}
