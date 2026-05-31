#ifndef FC_LOG_H
#define FC_LOG_H

/**
 * Minimal structured logging for the Flowcode runtime.
 *
 * Usage:
 *   fc_log(FC_LOG_ERROR, "plugin %s not found", name);
 *   fc_log(FC_LOG_WARN,  "scheduler at %u/%u capacity", count, cap);
 */

typedef enum {
    FC_LOG_DEBUG = 0,
    FC_LOG_INFO  = 1,
    FC_LOG_WARN  = 2,
    FC_LOG_ERROR = 3
} fc_log_level_t;

/** Set the minimum log level; messages below this are suppressed. */
void fc_log_set_level(fc_log_level_t level);

/** Get the current minimum log level. */
fc_log_level_t fc_log_get_level(void);

/**
 * Log a message at the given level.
 * Messages are written to stderr with a level prefix.
 */
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
void fc_log(fc_log_level_t level, const char *fmt, ...);

#endif
