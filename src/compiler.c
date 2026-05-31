/*
 * Flowcode compiler — C implementation.
 *
 * Translates Flowcode source (.fc) to bytecode (.fcb).
 * This is a faithful port of compiler/index.ts.
 */

#define _POSIX_C_SOURCE 200809L

#include "fc_compiler.h"
#include "fc_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* Opcodes (must match flowcode.h / the TypeScript compiler)          */
/* ------------------------------------------------------------------ */
#define OP_EMIT      0x01
#define OP_AWAIT     0x02
#define OP_CALL      0x03
#define OP_TRANSFORM 0x04
#define OP_ROUTE     0x05
#define OP_LOOP      0x06
#define OP_STORE     0x07

/* ------------------------------------------------------------------ */
/* Internal helpers: growable buffers                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  opcode;
    uint32_t arg_offset;
    uint32_t arg_length;
} instr_entry_t;

typedef struct {
    instr_entry_t *items;
    size_t         count;
    size_t         cap;
} instr_vec_t;

static void instr_vec_init(instr_vec_t *v) {
    v->items = NULL;
    v->count = 0;
    v->cap   = 0;
}

static int instr_vec_push(instr_vec_t *v, instr_entry_t e) {
    if (v->count == v->cap) {
        size_t newcap = v->cap ? v->cap * 2 : 16;
        instr_entry_t *tmp = (instr_entry_t *)fc_realloc(v->items, newcap * sizeof(*tmp));
        if (!tmp) return -1;
        v->items = tmp;
        v->cap   = newcap;
    }
    v->items[v->count++] = e;
    return 0;
}

static void instr_vec_free(instr_vec_t *v) {
    fc_free(v->items);
    v->items = NULL;
    v->count = v->cap = 0;
}

/* Byte buffer for the argument blob. */
typedef struct {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} byte_buf_t;

static void byte_buf_init(byte_buf_t *b) {
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

static int byte_buf_append(byte_buf_t *b, const void *src, size_t n) {
    if (b->len + n > b->cap) {
        size_t newcap = b->cap ? b->cap * 2 : 256;
        while (newcap < b->len + n) newcap *= 2;
        uint8_t *tmp = (uint8_t *)fc_realloc(b->data, newcap);
        if (!tmp) return -1;
        b->data = tmp;
        b->cap  = newcap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 0;
}

static void byte_buf_free(byte_buf_t *b) {
    fc_free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

/* Diagnostics vector. */
typedef struct {
    fc_diagnostic_t *items;
    size_t           count;
    size_t           cap;
} diag_vec_t;

static void diag_vec_init(diag_vec_t *v) {
    v->items = NULL;
    v->count = 0;
    v->cap   = 0;
}

static int diag_vec_push(diag_vec_t *v, fc_diag_level_t level, int line, const char *msg) {
    if (v->count == v->cap) {
        size_t newcap = v->cap ? v->cap * 2 : 8;
        fc_diagnostic_t *tmp = (fc_diagnostic_t *)fc_realloc(v->items, newcap * sizeof(*tmp));
        if (!tmp) return -1;
        v->items = tmp;
        v->cap   = newcap;
    }
    fc_diagnostic_t *d = &v->items[v->count++];
    d->level = level;
    d->line  = line;
    snprintf(d->message, sizeof(d->message), "%s", msg);
    return 0;
}

static void diag_vec_free(diag_vec_t *v) {
    fc_free(v->items);
    v->items = NULL;
    v->count = v->cap = 0;
}

/* Route/loop target tracking for post-pass validation. */
typedef struct {
    size_t   instr_index;
    uint32_t target_instr;
    int      src_line;
} route_target_t;

typedef struct {
    route_target_t *items;
    size_t          count;
    size_t          cap;
} route_vec_t;

static void route_vec_init(route_vec_t *v) { v->items = NULL; v->count = v->cap = 0; }

static int route_vec_push(route_vec_t *v, route_target_t e) {
    if (v->count == v->cap) {
        size_t newcap = v->cap ? v->cap * 2 : 8;
        route_target_t *tmp = (route_target_t *)fc_realloc(v->items, newcap * sizeof(*tmp));
        if (!tmp) return -1;
        v->items = tmp;
        v->cap   = newcap;
    }
    v->items[v->count++] = e;
    return 0;
}

static void route_vec_free(route_vec_t *v) { fc_free(v->items); v->items = NULL; v->count = v->cap = 0; }

/* Step name tracking for duplicate detection. */
typedef struct {
    char name[128];
    int  line;
} step_entry_t;

typedef struct {
    step_entry_t *items;
    size_t        count;
    size_t        cap;
} step_vec_t;

static void step_vec_init(step_vec_t *v) { v->items = NULL; v->count = v->cap = 0; }

static int step_vec_push(step_vec_t *v, const char *name, int line) {
    if (v->count == v->cap) {
        size_t newcap = v->cap ? v->cap * 2 : 8;
        step_entry_t *tmp = (step_entry_t *)fc_realloc(v->items, newcap * sizeof(*tmp));
        if (!tmp) return -1;
        v->items = tmp;
        v->cap   = newcap;
    }
    step_entry_t *e = &v->items[v->count++];
    snprintf(e->name, sizeof(e->name), "%s", name);
    e->line = line;
    return 0;
}

static int step_vec_find(const step_vec_t *v, const char *name) {
    for (size_t i = 0; i < v->count; i++) {
        if (strcmp(v->items[i].name, name) == 0) return v->items[i].line;
    }
    return 0;
}

static void step_vec_free(step_vec_t *v) { fc_free(v->items); v->items = NULL; v->count = v->cap = 0; }

/* ------------------------------------------------------------------ */
/* Line splitting                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    char  **lines;
    size_t  count;
} line_array_t;

/* Trim leading/trailing whitespace in-place (modifies string). */
static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)*(end - 1))) end--;
    *end = '\0';
    return s;
}

static line_array_t split_lines(const char *source) {
    line_array_t la;
    la.lines = NULL;
    la.count = 0;
    size_t cap = 0;

    const char *p = source;
    while (*p) {
        const char *eol = p;
        while (*eol && *eol != '\n' && *eol != '\r') eol++;

        /* Extract this line */
        size_t len = (size_t)(eol - p);
        char *buf = (char *)fc_alloc(len + 1);
        if (!buf) return la;
        memcpy(buf, p, len);
        buf[len] = '\0';

        /* Trim in-place */
        char *trimmed = trim(buf);

        /* Grow array if needed */
        if (la.count == cap) {
            size_t newcap = cap ? cap * 2 : 64;
            char **tmp = (char **)fc_realloc(la.lines, newcap * sizeof(char *));
            if (!tmp) { fc_free(buf); return la; }
            la.lines = tmp;
            cap = newcap;
        }

        /* Store a trimmed copy (trimmed points into buf) */
        if (trimmed != buf) {
            char *dup = (char *)fc_alloc(strlen(trimmed) + 1);
            if (!dup) { fc_free(buf); return la; }
            strcpy(dup, trimmed);
            fc_free(buf);
            la.lines[la.count++] = dup;
        } else {
            la.lines[la.count++] = buf;
        }

        /* Advance past EOL */
        if (*eol == '\r') eol++;
        if (*eol == '\n') eol++;
        p = eol;
    }
    return la;
}

static void free_lines(line_array_t *la) {
    for (size_t i = 0; i < la->count; i++) fc_free(la->lines[i]);
    fc_free(la->lines);
    la->lines = NULL;
    la->count = 0;
}

/* ------------------------------------------------------------------ */
/* Pattern matching helpers                                           */
/* ------------------------------------------------------------------ */

static int str_starts_with(const char *s, const char *prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int str_ends_with(const char *s, const char *suffix) {
    size_t slen = strlen(s);
    size_t plen = strlen(suffix);
    if (plen > slen) return 0;
    return strcmp(s + slen - plen, suffix) == 0;
}

/* Check if line matches /^\w+\s*->$/ */
static int is_arrow_line(const char *s) {
    const char *p = s;
    if (!*p || !isalnum((unsigned char)*p)) return 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_')) p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p[0] == '-' && p[1] == '>' && p[2] == '\0') return 1;
    return 0;
}

/* Check if line matches /^\w+\s*=\s*.+$/ */
static int is_param_line(const char *s) {
    const char *p = s;
    if (!*p || !isalnum((unsigned char)*p)) return 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '.')) p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '=') return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    return *p != '\0';
}

/**
 * Extract a named parameter value from lines following a directive.
 * Scans forward from `start` until end/blank/structural line.
 * Returns pointer to a static buffer or NULL.
 */
static const char *extract_param(const line_array_t *la, size_t start, const char *key) {
    static char valbuf[512];
    size_t klen = strlen(key);
    for (size_t i = start; i < la->count; i++) {
        const char *l = la->lines[i];
        if (!l[0] || strcmp(l, "end") == 0 || str_ends_with(l, ":") || is_arrow_line(l)) break;
        /* Match: key = value */
        if (strncmp(l, key, klen) == 0) {
            const char *p = l + klen;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p != '=') continue;
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            /* Strip surrounding quotes */
            size_t vlen = strlen(p);
            if (vlen >= 2 && p[0] == '"' && p[vlen - 1] == '"') {
                snprintf(valbuf, sizeof(valbuf), "%.*s", (int)(vlen - 2), p + 1);
            } else {
                snprintf(valbuf, sizeof(valbuf), "%s", p);
            }
            return valbuf;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Push an instruction with argument payload                          */
/* ------------------------------------------------------------------ */

static uint32_t push_arg(instr_vec_t *instrs, byte_buf_t *args,
                         uint32_t arg_offset, uint8_t opcode,
                         const void *payload, size_t payload_len)
{
    instr_entry_t e;
    e.opcode     = opcode;
    e.arg_offset = arg_offset;
    e.arg_length = (uint32_t)payload_len;
    instr_vec_push(instrs, e);
    byte_buf_append(args, payload, payload_len);
    return arg_offset + (uint32_t)payload_len;
}

/* ------------------------------------------------------------------ */
/* Little-endian encoding helpers                                     */
/* ------------------------------------------------------------------ */

static void encode_u16(uint8_t *dst, uint16_t v) {
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void encode_u32(uint8_t *dst, uint32_t v) {
    dst[0] = (uint8_t)(v & 0xFF);
    dst[1] = (uint8_t)((v >> 8) & 0xFF);
    dst[2] = (uint8_t)((v >> 16) & 0xFF);
    dst[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* ------------------------------------------------------------------ */
/* Main compiler entry point                                          */
/* ------------------------------------------------------------------ */

fc_compile_result_t fc_compile(const char *source) {
    fc_compile_result_t result;
    memset(&result, 0, sizeof(result));

    /* Avoid unused-function warning; diag_vec_free is kept for symmetry. */
    (void)diag_vec_free;

    line_array_t la = split_lines(source);

    instr_vec_t  instrs;  instr_vec_init(&instrs);
    byte_buf_t   argbuf;  byte_buf_init(&argbuf);
    diag_vec_t   diags;   diag_vec_init(&diags);
    route_vec_t  routes;  route_vec_init(&routes);
    step_vec_t   steps;   step_vec_init(&steps);

    uint32_t arg_offset = 0;

    /* ---------- Collect step names for duplicate detection ---------- */
    for (size_t i = 0; i < la.count; i++) {
        const char *line = la.lines[i];
        if (!str_starts_with(line, "step ")) continue;
        /* Parse: step <name>: or step <name> */
        const char *p = line + 5;
        char name[128];
        size_t ni = 0;
        while (*p && *p != ':' && !isspace((unsigned char)*p) && ni < sizeof(name) - 1) {
            name[ni++] = *p++;
        }
        name[ni] = '\0';
        if (ni == 0) continue;

        int prev = step_vec_find(&steps, name);
        if (prev) {
            char msg[256];
            snprintf(msg, sizeof(msg), "duplicate step name \"%s\" (first defined at line %d)", name, prev);
            diag_vec_push(&diags, FC_DIAG_ERROR, (int)(i + 1), msg);
        } else {
            step_vec_push(&steps, name, (int)(i + 1));
        }
    }

    /* ---------- First pass: emit instructions ---------- */
    int after_stop = 0;
    for (size_t i = 0; i < la.count; i++) {
        const char *line = la.lines[i];

        /* Skip structural markers, blank lines, parameter lines */
        if (line[0] == '\0') continue;
        if (str_ends_with(line, ":") && !str_starts_with(line, "on_error")) continue;
        if (strcmp(line, "end") == 0) continue;
        if (is_arrow_line(line)) continue;
        if (is_param_line(line)) continue;

        /* Detect unreachable code after 'stop' */
        if (after_stop && strcmp(line, "stop") != 0) {
            if (str_starts_with(line, "step ")) {
                after_stop = 0;
            } else {
                char msg[256];
                snprintf(msg, sizeof(msg), "unreachable code after stop");
                diag_vec_push(&diags, FC_DIAG_WARNING, (int)(i + 1), msg);
                continue;
            }
        }

        if (str_starts_with(line, "emit")) {
            const char *value = extract_param(&la, i + 1, "value");
            if (!value) value = "complete";
            arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_EMIT, value, strlen(value));

        } else if (str_starts_with(line, "transform")) {
            /* transform <funcname> */
            const char *p = line + 9;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p) {
                arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_TRANSFORM, p, strlen(p));
            } else {
                instr_entry_t e = { OP_TRANSFORM, 0, 0 };
                instr_vec_push(&instrs, e);
            }

        } else if (str_starts_with(line, "store")) {
            const char *key = extract_param(&la, i + 1, "key");
            if (key) {
                arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_STORE, key, strlen(key));
            } else {
                instr_entry_t e = { OP_STORE, 0, 0 };
                instr_vec_push(&instrs, e);
            }

        } else if (str_starts_with(line, "loop")) {
            uint32_t target = (uint32_t)(instrs.count + 1);
            uint8_t tbuf[4];
            encode_u32(tbuf, target);
            route_target_t rt = { instrs.count, target, (int)(i + 1) };
            route_vec_push(&routes, rt);
            arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_LOOP, tbuf, 4);

        } else if (str_starts_with(line, "await")) {
            instr_entry_t e = { OP_AWAIT, 0, 0 };
            instr_vec_push(&instrs, e);

        } else if (str_starts_with(line, "match")) {
            uint32_t target = (uint32_t)(instrs.count + 1);
            uint8_t tbuf[4];
            encode_u32(tbuf, target);
            route_target_t rt = { instrs.count, target, (int)(i + 1) };
            route_vec_push(&routes, rt);
            arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_ROUTE, tbuf, 4);

        } else if (str_starts_with(line, "on_error")) {
            /* on_error <strategy> on same line */
            const char *p = line + 8;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p) {
                /* Inline strategy: on_error retry / on_error stop / etc. */
                char payload[256];
                snprintf(payload, sizeof(payload), "on_error:%s", p);
                arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_STORE, payload, strlen(payload));
            } else {
                /* Block form: look for retry/timeout/fallback params */
                const char *retry_v = extract_param(&la, i + 1, "retry");
                const char *timeout_v = extract_param(&la, i + 1, "timeout");
                const char *fallback_v = extract_param(&la, i + 1, "fallback");

                if (retry_v) {
                    const char *s = "__on_error.retry";
                    arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_STORE, s, strlen(s));
                }
                if (timeout_v) {
                    const char *s = "__on_error.timeout";
                    arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_STORE, s, strlen(s));
                }
                if (fallback_v) {
                    const char *s = "__on_error.fallback";
                    arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_STORE, s, strlen(s));
                }
                if (!retry_v && !timeout_v && !fallback_v) {
                    diag_vec_push(&diags, FC_DIAG_WARNING, (int)(i + 1),
                                  "on_error block with no strategy (retry, timeout, or fallback)");
                }
            }

        } else if (str_starts_with(line, "retry")) {
            diag_vec_push(&diags, FC_DIAG_WARNING, (int)(i + 1),
                          "standalone retry outside on_error block; wrap in on_error for proper handling");

        } else if (str_starts_with(line, "timeout")) {
            /* Recognized; consumed by extractParam from parent */

        } else if (str_starts_with(line, "compensate")) {
            /* Recognized structural marker */

        } else if (str_starts_with(line, "http.") || str_starts_with(line, "webhook")) {
            /* Plugin call */
            char name[128];
            size_t ni = 0;
            const char *p = line;
            while (*p && !isspace((unsigned char)*p) && ni < sizeof(name) - 1) name[ni++] = *p++;
            name[ni] = '\0';
            arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_CALL, name, ni);

        } else if (str_starts_with(line, "step ") ||
                   str_starts_with(line, "workflow") ||
                   str_starts_with(line, "trigger") ||
                   str_starts_with(line, "use ") ||
                   str_starts_with(line, "parallel") ||
                   strcmp(line, "stop") == 0 ||
                   str_starts_with(line, "email.") ||
                   str_starts_with(line, "crm.") ||
                   str_starts_with(line, "storage.")) {
            /* Plugin calls */
            if (str_starts_with(line, "email.") || str_starts_with(line, "crm.") || str_starts_with(line, "storage.")) {
                char name[128];
                size_t ni = 0;
                const char *p = line;
                while (*p && !isspace((unsigned char)*p) && ni < sizeof(name) - 1) name[ni++] = *p++;
                name[ni] = '\0';
                arg_offset = push_arg(&instrs, &argbuf, arg_offset, OP_CALL, name, ni);
            }
            if (strcmp(line, "stop") == 0) {
                after_stop = 1;
            }

        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "unrecognized line: %s", line);
            diag_vec_push(&diags, FC_DIAG_WARNING, (int)(i + 1), msg);
        }
    }

    /* ---------- Post-pass: validate route/loop targets ---------- */
    for (size_t i = 0; i < routes.count; i++) {
        route_target_t *rt = &routes.items[i];
        if (instrs.count > 0 && rt->target_instr >= (uint32_t)instrs.count) {
            if (rt->target_instr > (uint32_t)instrs.count) {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "route/loop target instruction %u exceeds program size %u",
                         rt->target_instr, (uint32_t)instrs.count);
                diag_vec_push(&diags, FC_DIAG_ERROR, rt->src_line, msg);
            }
        }
    }

    /* ---------- Encode bytecode ---------- */
    /* Header: 4 magic + 2 version + 2 reserved + 4 instr_count + 4 arg_size = 16 */
    size_t header_size = 16;
    size_t instr_blob_size = instrs.count * 9; /* packed: 1 opcode + 4 offset + 4 length */
    size_t total = header_size + instr_blob_size + argbuf.len;

    uint8_t *out = (uint8_t *)fc_alloc(total);
    if (!out) {
        /* Best effort: return empty */
        free_lines(&la);
        instr_vec_free(&instrs);
        byte_buf_free(&argbuf);
        route_vec_free(&routes);
        step_vec_free(&steps);
        result.diagnostics      = diags.items;
        result.diagnostic_count = diags.count;
        return result;
    }

    size_t pos = 0;

    /* Magic */
    memcpy(out + pos, "FCB1", 4); pos += 4;

    /* Version (u16 LE) */
    encode_u16(out + pos, 1); pos += 2;

    /* Reserved (u16 LE) */
    encode_u16(out + pos, 0); pos += 2;

    /* Instruction count (u32 LE) */
    encode_u32(out + pos, (uint32_t)instrs.count); pos += 4;

    /* Arg blob size (u32 LE) */
    encode_u32(out + pos, (uint32_t)argbuf.len); pos += 4;

    /* Instructions */
    for (size_t i = 0; i < instrs.count; i++) {
        out[pos] = instrs.items[i].opcode;
        pos++;
        encode_u32(out + pos, instrs.items[i].arg_offset);
        pos += 4;
        encode_u32(out + pos, instrs.items[i].arg_length);
        pos += 4;
    }

    /* Arg blob */
    if (argbuf.len > 0) {
        memcpy(out + pos, argbuf.data, argbuf.len);
        pos += argbuf.len;
    }

    result.bytecode      = out;
    result.bytecode_size = total;

    /* Check for errors in diagnostics */
    result.has_errors = 0;
    for (size_t i = 0; i < diags.count; i++) {
        if (diags.items[i].level == FC_DIAG_ERROR) {
            result.has_errors = 1;
            break;
        }
    }

    /* Transfer diagnostic ownership to result */
    result.diagnostics      = diags.items;
    result.diagnostic_count = diags.count;
    /* Don't free diags — ownership transferred */

    free_lines(&la);
    instr_vec_free(&instrs);
    byte_buf_free(&argbuf);
    route_vec_free(&routes);
    step_vec_free(&steps);

    return result;
}

void fc_compile_result_free(fc_compile_result_t *result) {
    if (!result) return;
    fc_free(result->bytecode);
    fc_free(result->diagnostics);
    memset(result, 0, sizeof(*result));
}

/* ------------------------------------------------------------------ */
/* CLI entry point: fcc <input.fc> <output.fcb>                       */
/* ------------------------------------------------------------------ */

#ifdef FC_COMPILER_MAIN

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "error: cannot open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) { fclose(f); return NULL; }
    char *buf = (char *)fc_alloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)len, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: fcc <input.fc> <output.fcb>\n");
        return 1;
    }

    char *source = read_file(argv[1]);
    if (!source) return 1;

    fc_compile_result_t result = fc_compile(source);
    fc_free(source);

    /* Print diagnostics to stderr */
    for (size_t i = 0; i < result.diagnostic_count; i++) {
        const fc_diagnostic_t *d = &result.diagnostics[i];
        fprintf(stderr, "%s: line %d: %s\n",
                d->level == FC_DIAG_ERROR ? "error" : "warning",
                d->line, d->message);
    }

    /* Always write bytecode (best-effort) */
    if (result.bytecode && result.bytecode_size > 0) {
        FILE *out = fopen(argv[2], "wb");
        if (!out) {
            fprintf(stderr, "error: cannot write %s\n", argv[2]);
            fc_compile_result_free(&result);
            return 1;
        }
        fwrite(result.bytecode, 1, result.bytecode_size, out);
        fclose(out);
    }

    if (result.has_errors) {
        fprintf(stderr, "compilation completed with errors\n");
        fc_compile_result_free(&result);
        return 1;
    }
    if (result.diagnostic_count > 0) {
        fprintf(stderr, "compilation completed with %zu diagnostic(s)\n", result.diagnostic_count);
    }

    fc_compile_result_free(&result);
    return 0;
}

#endif /* FC_COMPILER_MAIN */
