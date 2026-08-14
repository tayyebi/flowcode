/*
 * Built-in plugin stubs.
 *
 * Every export here is a no-I/O pass-through: it logs the call and forwards the
 * incoming token. That is enough for the workflows under samples/ to run to
 * completion with the released binary, and it keeps the stubs harmless — a
 * shipped `http.post` that actually posted would be a surprise. Replace any of
 * them by loading a plugin library that exports the same name.
 */

#include "fc_builtins.h"
#include "fc_log.h"

#include <stddef.h>

/* Payload handed to callers when nothing upstream produced a token. */
static char builtin_empty_payload[] = FC_BUILTIN_DEFAULT_TOKEN;

static fc_plugin_result_t builtin_passthrough(const char *name,
                                              const fc_token_t *in,
                                              fc_token_t *out) {
    fc_log(FC_LOG_DEBUG, "builtin plugin %s invoked (no-op pass-through)", name);
    if (!out) return FC_PLUGIN_OK;
    out->context = NULL;
    out->metadata = 0u;
    if (in && in->value && in->value_size > 0u) {
        out->value = in->value;
        out->value_size = in->value_size;
    } else {
        out->value = builtin_empty_payload;
        out->value_size = (uint32_t)(sizeof(builtin_empty_payload) - 1u);
    }
    return FC_PLUGIN_OK;
}

#define FC_BUILTIN(ident, exported_name)                                       \
    static fc_plugin_result_t builtin_##ident(const fc_token_t *in,            \
                                              fc_token_t *out) {               \
        return builtin_passthrough(exported_name, in, out);                    \
    }

FC_BUILTIN(http_get, "http.get")
FC_BUILTIN(http_post, "http.post")
FC_BUILTIN(http_put, "http.put")
FC_BUILTIN(http_delete, "http.delete")
FC_BUILTIN(email_send, "email.send")
FC_BUILTIN(crm_add_note, "crm.addNote")
FC_BUILTIN(crm_create_customer, "crm.createCustomer")
FC_BUILTIN(storage_upload, "storage.upload")
FC_BUILTIN(form_render, "form.render")
FC_BUILTIN(form_submit, "form.submit")
FC_BUILTIN(ai_generate, "ai.generate")
FC_BUILTIN(ai_extract, "ai.extract")
FC_BUILTIN(memory_fetch, "memory.fetch")
FC_BUILTIN(memory_store, "memory.store")
FC_BUILTIN(mqtt_publish, "mqtt.publish")
FC_BUILTIN(mqtt_subscribe, "mqtt.subscribe")
FC_BUILTIN(webhook, "webhook")

#undef FC_BUILTIN

static const fc_plugin_export_t builtin_exports[] = {
    {"http.get", builtin_http_get},
    {"http.post", builtin_http_post},
    {"http.put", builtin_http_put},
    {"http.delete", builtin_http_delete},
    {"email.send", builtin_email_send},
    {"crm.addNote", builtin_crm_add_note},
    {"crm.createCustomer", builtin_crm_create_customer},
    {"storage.upload", builtin_storage_upload},
    {"form.render", builtin_form_render},
    {"form.submit", builtin_form_submit},
    {"ai.generate", builtin_ai_generate},
    {"ai.extract", builtin_ai_extract},
    {"memory.fetch", builtin_memory_fetch},
    {"memory.store", builtin_memory_store},
    {"mqtt.publish", builtin_mqtt_publish},
    {"mqtt.subscribe", builtin_mqtt_subscribe},
    {"webhook", builtin_webhook}
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
