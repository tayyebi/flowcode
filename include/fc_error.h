#ifndef FC_ERROR_H
#define FC_ERROR_H

#include <stdint.h>

/**
 * Structured error codes for the Flowcode runtime.
 *
 * Every subsystem returns an fc_error_code_t instead of a bare -1,
 * making errors inspectable and actionable.
 */
typedef enum {
    FC_ERR_OK = 0,

    /* Generic / memory */
    FC_ERR_ALLOC,            /* malloc/calloc/realloc returned NULL      */
    FC_ERR_NULL_ARG,         /* required argument was NULL               */

    /* Bytecode / program */
    FC_ERR_INVALID_MAGIC,    /* FCB header magic mismatch                */
    FC_ERR_INVALID_VERSION,  /* FCB header version unsupported           */
    FC_ERR_TRUNCATED,        /* file too short / unexpected EOF          */
    FC_ERR_INVALID_OPCODE,   /* opcode out of valid range                */
    FC_ERR_INVALID_ARG_REF,  /* arg_offset+arg_length exceeds blob       */
    FC_ERR_FILE_OPEN,        /* fopen failed                             */

    /* Scheduler */
    FC_ERR_QUEUE_FULL,       /* scheduler ring buffer at capacity        */
    FC_ERR_QUEUE_EMPTY,      /* dequeue on empty queue                   */

    /* State store */
    FC_ERR_KEY_NOT_FOUND,    /* state key does not exist                 */
    FC_ERR_KEY_EXPIRED,      /* state key existed but TTL expired        */

    /* Plugin */
    FC_ERR_PLUGIN_LOAD,      /* dlopen / LoadLibrary failed              */
    FC_ERR_PLUGIN_INIT,      /* fc_plugin_init symbol not found          */
    FC_ERR_PLUGIN_DESCRIPTOR,/* plugin returned NULL descriptor          */
    FC_ERR_PLUGIN_NOT_FOUND, /* named plugin not in registry             */
    FC_ERR_PLUGIN_CALL,      /* plugin function returned FC_PLUGIN_ERR   */

    /* VM */
    FC_ERR_ARENA_FULL,       /* token arena exhausted                    */
    FC_ERR_ARG_OVERFLOW,     /* arg_length exceeds local buffer          */
    FC_ERR_ROUTE_OOB,        /* route/loop target >= instruction_count   */
    FC_ERR_MISSING_TOKEN,    /* store requires a token but none exists   */
    FC_ERR_VM_RUN,           /* generic VM execution failure             */

    FC_ERR_COUNT             /* sentinel — number of error codes         */
} fc_error_code_t;

/**
 * Human-readable error context attached to the VM.
 *
 * After fc_vm_run returns non-zero the caller can inspect this struct
 * to learn *what* failed, *where*, and *why*.
 */
#define FC_ERROR_MSG_MAX 256
#define FC_ERROR_DETAIL_MAX 256

typedef struct {
    fc_error_code_t code;
    char            message[FC_ERROR_MSG_MAX];
    char            detail[FC_ERROR_DETAIL_MAX];
    uint32_t        instruction_index;   /* ip when error occurred  */
} fc_error_t;

/** Return a static string for a given error code (never NULL). */
const char *fc_error_name(fc_error_code_t code);

#endif
