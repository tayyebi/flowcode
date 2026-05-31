#include "fc_log.h"

#include <stdarg.h>
#include <stdio.h>

static fc_log_level_t g_min_level = FC_LOG_WARN;

void fc_log_set_level(fc_log_level_t level) {
    g_min_level = level;
}

fc_log_level_t fc_log_get_level(void) {
    return g_min_level;
}

void fc_log(fc_log_level_t level, const char *fmt, ...) {
    static const char *prefixes[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    va_list ap;
    if (level < g_min_level) return;
    fprintf(stderr, "[flowcode:%s] ", prefixes[level]);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
