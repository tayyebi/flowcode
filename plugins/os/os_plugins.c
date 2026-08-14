/*
 * os plugin — the two primitives everything else in this library is built
 * from, plus two workflow-facing plugins assembled out of them:
 *
 *   os.print   write the token to stdout, pass it through unchanged
 *   os.exec    run the token's text as a shell command, capture stdout
 *   http.get   os.exec, trying curl / wget / python3 in that order
 *   webhook    os.exec, trying nc / python3 in that order, waits for
 *              exactly one request and returns its body
 *
 * This is deliberately not built into the `flowcode` CLI. The stubs the CLI
 * registers by default do no I/O on purpose — see fc_builtins.h — and that
 * guarantee would be false the moment a shipped binary could run whatever a
 * workflow's token happens to contain. Load this on purpose:
 *
 *     fc_plugins_load(plugins, "./libfc_os.so");
 *
 * Because Flowcode's call ABI passes only the current token (see
 * doc/Plugins-and-Integrations.md — call parameters like `url = ...` are
 * discarded by the compiler and never reach a plugin), every plugin here
 * takes its one argument *as the token text*: `http.get`'s token is the URL,
 * `webhook`'s token is the port to listen on.
 *
 * Every exec here runs through `/bin/sh -c`, so anything that reaches a
 * token here is executed. That is the feature, not an accident — treat any
 * workflow wired to this plugin as running arbitrary shell commands with
 * whatever data flows into it, exactly like an Ansible `shell:` task or a
 * Node-RED exec node. Do not load it for workflows built from untrusted
 * input.
 */

#include "flowcode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Hard ceilings so a runaway command or a hostile token cannot exhaust
 * memory. Output beyond this is truncated, not rejected. */
#define FC_OS_MAX_ARG_LEN     4096u
#define FC_OS_MAX_OUTPUT_LEN  (1u << 20) /* 1 MiB */
#define FC_OS_MAX_CMD_LEN     8192u

/* ------------------------------------------------------------------ */
/* os.exec — the one primitive that touches the outside world          */
/* ------------------------------------------------------------------ */

/*
 * Quote `s` for safe embedding as a single `sh -c` argument: wrap it in
 * single quotes, and escape any single quote inside it as '\'' (close the
 * quote, emit an escaped quote, reopen the quote). This is the standard
 * technique for passing arbitrary bytes through a shell without letting
 * them break out of the argument they are meant to be.
 *
 * Returns 0 on success, -1 if `out` is too small for the quoted result.
 */
static int shell_quote(const char *s, size_t s_len, char *out, size_t out_cap) {
    size_t oi = 0;
    size_t i;
    if (!s || !out || out_cap == 0) return -1;
    if (oi >= out_cap) return -1;
    out[oi++] = '\'';
    for (i = 0; i < s_len; i++) {
        if (s[i] == '\'') {
            if (oi + 4 >= out_cap) return -1;
            out[oi++] = '\'';
            out[oi++] = '\\';
            out[oi++] = '\'';
            out[oi++] = '\'';
        } else {
            if (oi + 1 >= out_cap) return -1;
            out[oi++] = s[i];
        }
    }
    if (oi + 2 > out_cap) return -1;
    out[oi++] = '\'';
    out[oi] = '\0';
    return 0;
}

/*
 * Run `cmd` through the shell and capture its stdout into a heap buffer.
 * On success, out_buf and out_len describe the captured bytes and the
 * caller owns *out_buf (free() it). Returns 0 if the command started and exited
 * with status 0, -1 otherwise (popen failure, read failure, or nonzero
 * exit — including "command not found", which every caller here treats as
 * "try the next tool in the fallback list").
 */
static int exec_capture(const char *cmd, char **out_buf, size_t *out_len) {
    FILE *pipe;
    char *buf;
    size_t cap = 4096;
    size_t len = 0;
    int status;

    if (!cmd || !out_buf || !out_len) return -1;
    *out_buf = NULL;
    *out_len = 0;

    buf = (char *)malloc(cap);
    if (!buf) return -1;

    pipe = popen(cmd, "r");
    if (!pipe) {
        free(buf);
        return -1;
    }

    for (;;) {
        size_t n;
        if (len >= FC_OS_MAX_OUTPUT_LEN) break; /* truncate; still a success if exit==0 */
        if (len == cap) {
            size_t new_cap = cap * 2;
            char *tmp;
            if (new_cap > FC_OS_MAX_OUTPUT_LEN + 1) new_cap = FC_OS_MAX_OUTPUT_LEN + 1;
            tmp = (char *)realloc(buf, new_cap);
            if (!tmp) {
                free(buf);
                pclose(pipe);
                return -1;
            }
            buf = tmp;
            cap = new_cap;
        }
        n = fread(buf + len, 1, cap - len, pipe);
        if (n == 0) break;
        len += n;
    }

    status = pclose(pipe);
    if (status != 0) {
        free(buf);
        return -1;
    }

    *out_buf = buf;
    *out_len = len;
    return 0;
}

/*
 * Build a null-terminated copy of the token's text, bounded and safe to pass
 * to shell_quote / snprintf. Returns 0 on success, -1 if the token is empty
 * or too large.
 */
static int token_text(const fc_token_t *in, char *out, size_t out_cap) {
    size_t n;
    if (!in || !in->value || in->value_size == 0) return -1;
    n = in->value_size;
    if (n >= out_cap || n > FC_OS_MAX_ARG_LEN) return -1;
    memcpy(out, in->value, n);
    out[n] = '\0';
    return 0;
}

static fc_plugin_result_t set_out(fc_token_t *out, char *buf, size_t len) {
    if (!out) {
        free(buf);
        return FC_PLUGIN_OK;
    }
    out->value = buf;      /* ownership transfers to the token; leaked for the
                             * run's lifetime, matching the plugin ABI contract
                             * that a token's value stays alive as long as the
                             * VM does. The process exits shortly after. */
    out->value_size = (uint32_t)len;
    out->context = NULL;
    out->metadata = 0;
    return FC_PLUGIN_OK;
}

/* os.exec — token in is the command line, token out is captured stdout. */
static fc_plugin_result_t os_exec(const fc_token_t *in, fc_token_t *out) {
    char cmd[FC_OS_MAX_ARG_LEN + 1];
    char *buf;
    size_t len;

    if (token_text(in, cmd, sizeof(cmd)) != 0) return FC_PLUGIN_ERR;
    if (exec_capture(cmd, &buf, &len) != 0) return FC_PLUGIN_ERR;
    return set_out(out, buf, len);
}

/* os.print — token in is written to stdout as-is, token out == token in. */
static fc_plugin_result_t os_print(const fc_token_t *in, fc_token_t *out) {
    if (in && in->value && in->value_size > 0) {
        fwrite(in->value, 1, in->value_size, stdout);
        fputc('\n', stdout);
        fflush(stdout);
    }
    if (out && in) {
        out->value = in->value;
        out->value_size = in->value_size;
        out->context = NULL;
        out->metadata = 0;
    } else if (out) {
        memset(out, 0, sizeof(*out));
    }
    return FC_PLUGIN_OK;
}

/* ------------------------------------------------------------------ */
/* Fallback-chain runner: try each command; first clean exit wins.     */
/* ------------------------------------------------------------------ */

static fc_plugin_result_t run_fallback_chain(const char *const *cmds, size_t n,
                                              fc_token_t *out) {
    size_t i;
    for (i = 0; i < n; i++) {
        char *buf;
        size_t len;
        if (exec_capture(cmds[i], &buf, &len) == 0) {
            return set_out(out, buf, len);
        }
        /* This tool was missing, or failed; fall through to the next one. */
    }
    return FC_PLUGIN_ERR;
}

/* ------------------------------------------------------------------ */
/* http.get — token in is a URL, token out is the response body.       */
/* ------------------------------------------------------------------ */

static fc_plugin_result_t http_get(const fc_token_t *in, fc_token_t *out) {
    char url[FC_OS_MAX_ARG_LEN + 1];
    char quoted[FC_OS_MAX_ARG_LEN + 8];
    char cmd_curl[FC_OS_MAX_CMD_LEN];
    char cmd_wget[FC_OS_MAX_CMD_LEN];
    char cmd_py[FC_OS_MAX_CMD_LEN];
    const char *chain[3];
    int n;

    if (token_text(in, url, sizeof(url)) != 0) return FC_PLUGIN_ERR;
    if (shell_quote(url, strlen(url), quoted, sizeof(quoted)) != 0) return FC_PLUGIN_ERR;

    n = snprintf(cmd_curl, sizeof(cmd_curl), "curl -fsSL -- %s 2>/dev/null", quoted);
    if (n < 0 || (size_t)n >= sizeof(cmd_curl)) return FC_PLUGIN_ERR;
    n = snprintf(cmd_wget, sizeof(cmd_wget), "wget -qO- -- %s 2>/dev/null", quoted);
    if (n < 0 || (size_t)n >= sizeof(cmd_wget)) return FC_PLUGIN_ERR;
    /* No `--` here: unlike curl/wget, `python3 -c script arg1` does not strip
     * a leading `--` from its own argv — it would land in sys.argv[1] as a
     * literal two-character string instead of the URL. */
    n = snprintf(cmd_py, sizeof(cmd_py),
                 "python3 -c 'import sys,urllib.request as u;"
                 "sys.stdout.buffer.write(u.urlopen(sys.argv[1], timeout=30).read())' %s",
                 quoted);
    if (n < 0 || (size_t)n >= sizeof(cmd_py)) return FC_PLUGIN_ERR;

    chain[0] = cmd_curl;
    chain[1] = cmd_wget;
    chain[2] = cmd_py;
    return run_fallback_chain(chain, 3, out);
}

/* ------------------------------------------------------------------ */
/* webhook — token in is a port number, token out is one request body. */
/* ------------------------------------------------------------------ */

/*
 * Blocks until exactly one HTTP request arrives on the given port, then
 * returns. This is a single-shot listener, not a server: the workflow is
 * a program that runs once, so "wait for a webhook" here means "wait for
 * the next call and stop". Run flowcode again to accept another.
 */
static fc_plugin_result_t webhook(const fc_token_t *in, fc_token_t *out) {
    char port[FC_OS_MAX_ARG_LEN + 1];
    char cmd_nc[FC_OS_MAX_CMD_LEN];
    char cmd_py[FC_OS_MAX_CMD_LEN];
    const char *chain[2];
    int n;
    long portnum;
    char *end;

    if (token_text(in, port, sizeof(port)) != 0) {
        /* No port supplied: default to 8080, the same default `form.render`
         * examples in the samples imply for local testing. */
        memcpy(port, "8080", 5);
    }

    errno = 0;
    portnum = strtol(port, &end, 10);
    if (errno != 0 || end == port || *end != '\0' || portnum < 1 || portnum > 65535) {
        return FC_PLUGIN_ERR;
    }

    /* `nc`: accept one connection, print whatever it sends until the peer
     * closes. Good enough to capture a POSTed body for a single call. */
    n = snprintf(cmd_nc, sizeof(cmd_nc), "nc -l %ld 2>/dev/null", portnum);
    if (n < 0 || (size_t)n >= sizeof(cmd_nc)) return FC_PLUGIN_ERR;

    /* python3 fallback: a one-shot HTTP server that reads exactly one
     * request, writes its body to stdout, and exits. */
    n = snprintf(cmd_py, sizeof(cmd_py),
                 "python3 -c '"
                 "import http.server as h,sys\n"
                 "class Handler(h.BaseHTTPRequestHandler):\n"
                 "    def do_POST(self):\n"
                 "        n=int(self.headers.get(\"Content-Length\",0))\n"
                 "        sys.stdout.buffer.write(self.rfile.read(n))\n"
                 "        self.send_response(200); self.end_headers()\n"
                 "    def do_GET(self):\n"
                 "        sys.stdout.buffer.write(self.path.encode())\n"
                 "        self.send_response(200); self.end_headers()\n"
                 "    def log_message(self,*a): pass\n"
                 "s=h.HTTPServer((\"0.0.0.0\",%ld),Handler)\n"
                 "s.handle_request()\n"
                 "'",
                 portnum);
    if (n < 0 || (size_t)n >= sizeof(cmd_py)) return FC_PLUGIN_ERR;

    chain[0] = cmd_nc;
    chain[1] = cmd_py;
    return run_fallback_chain(chain, 2, out);
}

/* ------------------------------------------------------------------ */
/* Plugin descriptor                                                   */
/* ------------------------------------------------------------------ */

static fc_plugin_export_t exports[] = {
    { "os.print", os_print },
    { "os.exec",  os_exec },
    { "http.get", http_get },
    { "webhook",  webhook },
};

static fc_plugin_descriptor_t descriptor = {
    sizeof(exports) / sizeof(exports[0]),
    exports
};

fc_plugin_descriptor_t *fc_plugin_init(void) {
    return &descriptor;
}
