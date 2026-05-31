#include "fc_error.h"

static const char *error_names[] = {
    "FC_ERR_OK",
    "FC_ERR_ALLOC",
    "FC_ERR_NULL_ARG",
    "FC_ERR_INVALID_MAGIC",
    "FC_ERR_INVALID_VERSION",
    "FC_ERR_TRUNCATED",
    "FC_ERR_INVALID_OPCODE",
    "FC_ERR_INVALID_ARG_REF",
    "FC_ERR_FILE_OPEN",
    "FC_ERR_QUEUE_FULL",
    "FC_ERR_QUEUE_EMPTY",
    "FC_ERR_KEY_NOT_FOUND",
    "FC_ERR_KEY_EXPIRED",
    "FC_ERR_PLUGIN_LOAD",
    "FC_ERR_PLUGIN_INIT",
    "FC_ERR_PLUGIN_DESCRIPTOR",
    "FC_ERR_PLUGIN_NOT_FOUND",
    "FC_ERR_PLUGIN_CALL",
    "FC_ERR_ARENA_FULL",
    "FC_ERR_ARG_OVERFLOW",
    "FC_ERR_ROUTE_OOB",
    "FC_ERR_MISSING_TOKEN",
    "FC_ERR_VM_RUN",
};

const char *fc_error_name(fc_error_code_t code) {
    if (code >= 0 && code < FC_ERR_COUNT)
        return error_names[code];
    return "FC_ERR_UNKNOWN";
}
