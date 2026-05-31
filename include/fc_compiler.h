#ifndef FC_COMPILER_H
#define FC_COMPILER_H

#include <stddef.h>
#include <stdint.h>

/** Diagnostic severity. */
typedef enum {
    FC_DIAG_WARNING = 0,
    FC_DIAG_ERROR   = 1
} fc_diag_level_t;

/** A single diagnostic message emitted during compilation. */
typedef struct {
    fc_diag_level_t level;
    int             line;
    char            message[256];
} fc_diagnostic_t;

/** Result of a compilation. */
typedef struct {
    uint8_t        *bytecode;
    size_t          bytecode_size;
    fc_diagnostic_t *diagnostics;
    size_t          diagnostic_count;
    int             has_errors;
} fc_compile_result_t;

/**
 * Compile Flowcode source text to bytecode.
 *
 * Returns a result struct containing the bytecode buffer and diagnostics.
 * The caller must free the result with fc_compile_result_free().
 */
fc_compile_result_t fc_compile(const char *source);

/** Free all memory owned by a compile result. */
void fc_compile_result_free(fc_compile_result_t *result);

#endif
