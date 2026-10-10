#ifndef FC_BUILTINS_H
#define FC_BUILTINS_H

#include "flowcode.h"

/**
 * Register the built-in plugin stubs (MailApp.sendEmail, MailApp.read,
 * UrlFetchApp.fetch, Logger.log) into `registry`.
 *
 * The stubs perform no I/O: they log the call at DEBUG level and pass the
 * incoming token through unchanged. They exist so that workflows written
 * against the standard app names execute end to end with the shipped binary,
 * before a real handler is registered — the playground's runner replaces all
 * four with its unix-socket bridge. Registering a real handler under the same
 * name replaces the stub.
 *
 * Returns 0 on success, -1 if any registration failed.
 */
int fc_plugins_register_builtins(fc_plugin_registry_t *registry);

/** Default token payload seeded by the CLI so token-less runs can `store`. */
#define FC_BUILTIN_DEFAULT_TOKEN "{}"

#endif
