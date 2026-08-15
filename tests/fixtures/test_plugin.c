/*
 * Minimal loadable plugin used only by tests/test_plugin_load.c. Exports
 * "test.echo" returning a fixed payload distinct from any in-process stub,
 * so a test can tell whether the loaded version or a stub answered a call.
 */
#include "flowcode.h"

static char loaded_payload[] = "LOADED";

static fc_plugin_result_t loaded_echo(const fc_token_t *in, fc_token_t *out) {
    (void)in;
    if (!out) return FC_PLUGIN_OK;
    out->value = loaded_payload;
    out->value_size = (uint32_t)(sizeof(loaded_payload) - 1u);
    out->context = NULL;
    out->metadata = 0u;
    return FC_PLUGIN_OK;
}

static fc_plugin_export_t exports[] = {
    { "test.echo", loaded_echo },
};

static fc_plugin_descriptor_t descriptor = {
    sizeof(exports) / sizeof(exports[0]),
    exports
};

fc_plugin_descriptor_t *fc_plugin_init(void) {
    return &descriptor;
}
