#ifndef FC_BUILTINS_H
#define FC_BUILTINS_H

#include "flowcode.h"

/**
 * Register the built-in plugin stubs (http.*, email.*, crm.*, storage.*, form.*,
 * ai.*, memory.*, mqtt.*, webhook) into `registry`.
 *
 * The stubs perform no I/O: they log the call at DEBUG level and pass the
 * incoming token through unchanged. They exist so that workflows written
 * against the standard plugin names — the `samples/` directory in particular —
 * execute end to end with the shipped binary, before any real plugin library is
 * loaded with fc_plugins_load(). Loading a real plugin that exports the same
 * name replaces the stub.
 *
 * Returns 0 on success, -1 if any registration failed.
 */
int fc_plugins_register_builtins(fc_plugin_registry_t *registry);

/** Default token payload seeded by the CLI so token-less runs can `store`. */
#define FC_BUILTIN_DEFAULT_TOKEN "{}"

#endif
