/*
 * Built-in plugin stubs.
 *
 * Every export here is a no-I/O pass-through: it logs the call and forwards
 * the incoming token. That is enough for workflows to run to completion with
 * the released binary, and it keeps the stubs harmless — a shipped
 * `MailApp.sendEmail` that actually sent mail would be a surprise. Replace any
 * of them by registering a real handler under the same name (the playground's
 * runner does exactly that over its unix-socket bridge).
 *
 * The four names are the Apps-Script-style standard apps:
 *
 *   MailApp.sendEmail   deliver mail through the host's SMTP
 *   MailApp.read        read mail through the host's IMAP
 *   UrlFetchApp.fetch   make an HTTP request
 *   Logger.log          emit a log entry to the host's log sink
 */

#include "fc_builtins.h"
#include "fc_log.h"

#include <stddef.h>

/* Payload handed to callers when nothing upstream produced a token. */
static char builtin_empty_payload[] = FC_BUILTIN_DEFAULT_TOKEN;

static fc_plugin_result_t builtin_passthrough(const fc_call_t *call,
                                              fc_token_t *out) {
    fc_log(FC_LOG_DEBUG, "builtin plugin %s invoked (no-op pass-through)", call->name);
    if (!out) return FC_PLUGIN_OK;
    out->context = NULL;
    out->metadata = 0u;
    if (call->token && call->token->value && call->token->value_size > 0u) {
        out->value = call->token->value;
        out->value_size = call->token->value_size;
    } else {
        out->value = builtin_empty_payload;
        out->value_size = (uint32_t)(sizeof(builtin_empty_payload) - 1u);
    }
    return FC_PLUGIN_OK;
}

static const fc_plugin_export_t builtin_exports[] = {
    {"MailApp.sendEmail", builtin_passthrough},
    {"MailApp.read",      builtin_passthrough},
    {"UrlFetchApp.fetch", builtin_passthrough},
    {"Logger.log",        builtin_passthrough},
};

int fc_plugins_register_builtins(fc_plugin_registry_t *registry) {
    size_t i;
    if (!registry) return -1;
    for (i = 0; i < sizeof(builtin_exports) / sizeof(builtin_exports[0]); ++i) {
        if (fc_plugins_register(registry, builtin_exports[i].name,
                                builtin_exports[i].fn) != 0) {
            fc_log(FC_LOG_ERROR, "failed to register builtin plugin %s",
                   builtin_exports[i].name);
            return -1;
        }
    }
    fc_log(FC_LOG_DEBUG, "registered %zu builtin plugins",
           sizeof(builtin_exports) / sizeof(builtin_exports[0]));
    return 0;
}
