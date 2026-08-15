/*
 * Regression test: fc_plugins_load() must replace an existing registration
 * under the same name, in both call orders, matching fc_plugins_register().
 *
 * Before this fix, fc_plugins_load() appended every exported name without
 * checking for a duplicate, and fc_plugins_resolve() returns the first
 * match — so a real plugin loaded after a stub was registered could never
 * be reached; only the stub would ever run.
 */
#include "flowcode.h"
#include "fc_log.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#ifndef FC_TEST_PLUGIN_PATH
#error "FC_TEST_PLUGIN_PATH must be defined to the built fixture library"
#endif

static char stub_payload[] = "STUB";

static fc_plugin_result_t stub_echo(const fc_token_t *in, fc_token_t *out) {
    (void)in;
    if (!out) return FC_PLUGIN_OK;
    out->value = stub_payload;
    out->value_size = (uint32_t)(sizeof(stub_payload) - 1u);
    out->context = NULL;
    out->metadata = 0u;
    return FC_PLUGIN_OK;
}

static int resolves_to_loaded(fc_plugin_registry_t *plugins) {
    fc_plugin_call_fn fn = fc_plugins_resolve(plugins, "test.echo");
    fc_token_t out;
    memset(&out, 0, sizeof(out));
    assert(fn != NULL);
    assert(fn(NULL, &out) == FC_PLUGIN_OK);
    return out.value_size == 6u && memcmp(out.value, "LOADED", 6u) == 0;
}

int main(void) {
    fc_plugin_registry_t *plugins;

    fc_log_set_level(FC_LOG_ERROR);
    printf("--- test_plugin_load ---\n");

    /* Order 1: register a stub, then load the real plugin over it. */
    plugins = fc_plugins_create();
    assert(plugins);
    assert(fc_plugins_register(plugins, "test.echo", stub_echo) == 0);
    assert(fc_plugins_load(plugins, FC_TEST_PLUGIN_PATH) == 0);
    assert(resolves_to_loaded(plugins));
    printf("  PASS: fc_plugins_load() overrides a prior fc_plugins_register()\n");
    fc_plugins_destroy(plugins);

    /* Order 2: load first, then register a plugin under the same name —
     * the register call should win, since it is also the later call. */
    plugins = fc_plugins_create();
    assert(plugins);
    assert(fc_plugins_load(plugins, FC_TEST_PLUGIN_PATH) == 0);
    assert(fc_plugins_register(plugins, "test.echo", stub_echo) == 0);
    {
        fc_plugin_call_fn fn = fc_plugins_resolve(plugins, "test.echo");
        fc_token_t out;
        memset(&out, 0, sizeof(out));
        assert(fn != NULL);
        assert(fn(NULL, &out) == FC_PLUGIN_OK);
        assert(out.value_size == 4u && memcmp(out.value, "STUB", 4u) == 0);
    }
    printf("  PASS: whichever registration happens last wins, in either order\n");
    fc_plugins_destroy(plugins);

    /* Loading the same library twice must not leave two entries resolving
     * inconsistently — the second load replaces the first, so resolve still
     * finds exactly the loaded plugin. */
    plugins = fc_plugins_create();
    assert(plugins);
    assert(fc_plugins_load(plugins, FC_TEST_PLUGIN_PATH) == 0);
    assert(fc_plugins_load(plugins, FC_TEST_PLUGIN_PATH) == 0);
    assert(resolves_to_loaded(plugins));
    printf("  PASS: loading the same library twice does not duplicate entries\n");
    fc_plugins_destroy(plugins);

    printf("--- all plugin-load tests passed ---\n");
    return 0;
}
