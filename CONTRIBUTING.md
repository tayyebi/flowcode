# Contributing to Flowcode

Thank you for your interest in contributing to Flowcode.
This document establishes **mandatory** development rules designed for pessimistic, safe, guarded, self-healing, hardened, military-grade code.
Every contributor **must** follow these rules without exception.

> **Philosophy:** Assume every input is hostile, every allocation will fail, every pointer is `NULL`, every buffer is too small, and every syscall will return an error.
> Write code as if the next person to maintain it is an adversary who knows where you live.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Build & Test](#build--test)
3. [Memory Safety](#memory-safety)
4. [Input Validation & Trust Boundaries](#input-validation--trust-boundaries)
5. [Integer Safety](#integer-safety)
6. [String Handling](#string-handling)
7. [Error Handling & Defensive Returns](#error-handling--defensive-returns)
8. [Pointer Discipline](#pointer-discipline)
9. [Concurrency & Reentrancy](#concurrency--reentrancy)
10. [Self-Healing & Graceful Degradation](#self-healing--graceful-degradation)
11. [Static Analysis & Compiler Hardening](#static-analysis--compiler-hardening)
12. [Plugin / Dynamic Loading Safety](#plugin--dynamic-loading-safety)
13. [File & I/O Safety](#file--io-safety)
14. [Coding Style & Hygiene](#coding-style--hygiene)
15. [Testing Requirements](#testing-requirements)
16. [Pull Request Checklist](#pull-request-checklist)

---

## Getting Started

1. Fork the repository and create a feature branch from `main`.
2. Keep commits small, atomic, and descriptive.
3. Read this entire document before writing any code.

---

## Build & Test

```bash
make          # Build the runtime
make test     # Run unit tests
make test-e2e # Run end-to-end tests (compiler → bytecode → runtime)
```

All compiler warnings are errors (`-Werror`). A build with warnings is a **failed** build.

---

## Memory Safety

These rules exist because C provides **zero** safety nets. Every byte you allocate is your responsibility.

### Allocation

- **Always** check the return value of `fc_alloc`, `fc_calloc`, `fc_realloc`, and any system allocator. Never assume success.
  ```c
  void *p = fc_alloc(size);
  if (!p) return -1; /* MANDATORY */
  ```
- **Prefer `fc_calloc`** over `fc_alloc` to guarantee zero-initialized memory. Uninitialized memory is a vulnerability.
- **Never use raw `malloc`/`calloc`/`realloc`/`free`.** All allocations must go through `fc_alloc`/`fc_calloc`/`fc_realloc`/`fc_free` in `fc_memory.h`. This enables future instrumentation, leak tracking, and hardened allocation.
- **Limit allocation sizes.** Enforce maximum bounds before allocating. Never trust external input to dictate unbounded allocation sizes.
  ```c
  if (requested_size > FC_MAX_ALLOC) return -1;
  ```

### Deallocation

- **Set pointers to `NULL` after `fc_free`.** Use-after-free is a critical vulnerability.
  ```c
  fc_free(ptr);
  ptr = NULL;
  ```
- **Free in reverse order of allocation.** This prevents dangling references.
- **Never double-free.** The `NULL`-after-free rule naturally guards against this.
- **On error paths, free all resources acquired before the failure point.** No partial cleanup.

### Stack Safety

- **Never allocate large buffers on the stack.** Stack overflows are silent and deadly. Use heap allocation for anything over 256 bytes.
- **Never use variable-length arrays (VLAs).** They are unbounded and can cause stack overflow.
- **Never use `alloca`.** Same reason as VLAs.

---

## Input Validation & Trust Boundaries

Every function is a trust boundary. Every parameter is suspect.

### Null Guards

- **Every public function must validate all pointer parameters at entry.** Return an error code immediately if any are `NULL`.
  ```c
  int fc_state_set(fc_state_store_t *store, const char *key, ...) {
      if (!store || !key) return -1;
      /* ... */
  }
  ```
- Internal/static functions should also validate pointers when they accept external-origin data.

### Bounds Checking

- **Validate all array indices against bounds before access.** Never trust computed indices.
  ```c
  if (index >= array_count) return -1;
  ```
- **Validate all offsets and lengths in bytecode against blob boundaries.**
  ```c
  if (ins->arg_offset > program->arg_blob_size) return -1;
  if (ins->arg_length > program->arg_blob_size - ins->arg_offset) return -1;
  ```
  Note the subtraction order — this prevents unsigned underflow.

### File Input

- **Treat all file input as hostile.** Validate magic numbers, version fields, counts, and sizes before using them.
- **Reject files with instruction counts or argument sizes exceeding hard-coded maximums.**
- **Never trust `fread` return values without checking** that the exact expected number of elements were read.

---

## Integer Safety

Integer overflow and underflow in C are silent killers.

- **Use exact-width types** (`uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`) from `<stdint.h>`. Never use `int` for sizes or counts.
- **Check for overflow before arithmetic**, not after.
  ```c
  /* WRONG: checking after (too late, UB already happened) */
  uint32_t result = a + b;
  if (result < a) { /* ... */ }

  /* RIGHT: checking before */
  if (a > UINT32_MAX - b) return -1;
  uint32_t result = a + b;
  ```
- **Use `size_t` for memory sizes** and `uint32_t` for counts/indices within Flowcode structures.
- **Never cast between signed and unsigned without validating the value is non-negative.**
- **Multiplication overflow checks are mandatory** when computing allocation sizes.
  ```c
  if (count != 0 && size > SIZE_MAX / count) return -1;
  ```
- **Use `u` suffix on unsigned literals** to prevent implicit sign conversion: `128u`, `0u`, `1u`.

---

## String Handling

Strings in C are the #1 source of exploitable bugs. Treat them with extreme suspicion.

- **Never use `strcpy`.** Use `memcpy` with a pre-validated length, or `strncpy` with explicit null-termination.
  ```c
  /* Preferred pattern */
  if (len >= sizeof(buf)) return -1;
  memcpy(buf, src, len);
  buf[len] = '\0';
  ```
- **Never use `sprintf`.** Use `snprintf` with buffer size.
- **Never use `gets`.** This function is so dangerous it was removed from C11.
- **Never use `strcat` or `strncat`.** Compute the final length, allocate, and `memcpy`.
- **Always explicitly null-terminate** buffers after copying data into them.
- **Validate string lengths before use.** Never assume a string from external input is null-terminated.

---

## Error Handling & Defensive Returns

In Flowcode, errors are **not** optional. Every operation can fail, and every failure must be handled.

### Return Conventions

- Return `0` for success, `-1` (or negative) for failure.
- Return `NULL` for pointer-returning functions on failure.
- Never return a partial result. Either fully succeed or fully fail.

### Fail-Fast, Fail-Loud

- **On unrecoverable errors, fail immediately.** Do not try to continue with corrupted state.
- **Log diagnostic info on failure** when a logging subsystem is available.
- **Prefer early returns** to deeply nested if-else chains. Each guard clause is a checkpoint.
  ```c
  if (!ptr) return -1;
  if (size == 0) return -1;
  if (size > MAX) return -1;
  /* proceed with confidence */
  ```

### Cleanup on Error

- Use a single cleanup label (e.g., `goto cleanup;`) for functions with multiple resources, to ensure deterministic cleanup.
  ```c
  int func(void) {
      int rc = -1;
      void *a = fc_alloc(100);
      if (!a) goto cleanup;
      void *b = fc_alloc(200);
      if (!b) goto cleanup;

      /* work */
      rc = 0;

  cleanup:
      fc_free(b);
      fc_free(a);
      return rc;
  }
  ```
- Ensure the cleanup section handles partially initialized resources (e.g., `fc_free(NULL)` must be safe — and it is, since `free(NULL)` is a no-op).

---

## Pointer Discipline

- **Never dereference a pointer without a prior `NULL` check** in the same scope.
- **Never return a pointer to stack-allocated memory.**
- **Never store a pointer to a local variable** beyond the variable's lifetime.
- **Use `const` aggressively.** If a function does not modify a pointed-to value, the parameter must be `const`.
  ```c
  int fc_program_validate(const fc_program_t *program);
  ```
- **Cast function pointers through a `union`**, never through direct casts, to avoid undefined behavior (see `plugin.c` for the pattern).
- **Mark intentionally unused parameters** with `(void)param;` to silence warnings and signal intent.

---

## Concurrency & Reentrancy

Although Flowcode is currently single-threaded, code must be **concurrency-ready**.

- **No global mutable state.** All state must live in explicitly passed context structures (`fc_vm_t`, `fc_state_store_t`, etc.).
- **No `static` mutable variables** inside functions (hidden global state).
- **Functions must be reentrant** unless explicitly documented otherwise.
- **Never use `strtok`** — it uses internal static state. Use `strtok_r` or manual parsing.
- **Never use `ctime`, `asctime`, `gmtime`, `localtime`** — use their `_r` variants.

---

## Self-Healing & Graceful Degradation

Code should **recover** when possible and **degrade gracefully** when recovery is impossible.

- **Validate internal invariants** with assertions in debug builds. Use `assert()` only for "this should never happen" programmer errors, not for runtime input validation.
- **Zero-out sensitive structures after use** with `memset` to prevent data leakage (see `fc_program_free`).
- **Idempotent destroy functions.** Every `_destroy` function must accept `NULL` safely and be callable multiple times without harm.
  ```c
  void fc_vm_destroy(fc_vm_t *vm) {
      if (!vm) return;
      /* ... */
  }
  ```
- **Bounded loops.** Never write a loop that can run unbounded. Every loop must have a provable upper bound.
  ```c
  for (i = 0; i < store->capacity; ++i) { /* bounded by capacity */ }
  ```
- **Limit resource consumption.** Enforce hard caps on instruction counts, token arena sizes, scheduler queue depths, and state store entries.

---

## Static Analysis & Compiler Hardening

- **All code must compile cleanly** with `-std=c11 -Wall -Wextra -Werror -pedantic`.
- **Add additional warning flags** when practical:
  - `-Wconversion` — catch implicit narrowing conversions.
  - `-Wshadow` — catch variable shadowing.
  - `-Wformat=2` — catch format string vulnerabilities.
  - `-Wstrict-prototypes` — enforce proper function declarations.
  - `-Wstack-protector` — warn about unprotectable functions.
- **Enable stack canaries** in production builds with `-fstack-protector-strong`.
- **Enable position-independent code** with `-fPIE -pie` for ASLR support.
- **Enable FORTIFY_SOURCE** with `-D_FORTIFY_SOURCE=2` to catch buffer overflows in standard library calls at runtime.
- **Run static analysis** (e.g., `clang-tidy`, `cppcheck`, Coverity) before submitting a pull request. Zero warnings policy.
- **Enable AddressSanitizer** (`-fsanitize=address`) and **UndefinedBehaviorSanitizer** (`-fsanitize=undefined`) in test/debug builds.

---

## Plugin / Dynamic Loading Safety

The plugin system (`dlopen`/`LoadLibrary`) is a **high-risk attack surface**.

- **Validate the plugin descriptor** returned by `fc_plugin_init`. Check for `NULL` descriptor, `NULL` exports array, zero export count, and `NULL` function pointers.
- **Validate plugin names** for reasonable length and content before copying.
- **Never trust function pointers from plugins** without verifying the symbol was resolved successfully.
- **Limit the number of loaded plugins** to a hard maximum.
- **Consider restricting plugin paths** to a known directory to prevent path traversal attacks.

---

## File & I/O Safety

- **Always check `fopen` return values.** A failed open is a `NULL` pointer.
- **Always close files** in every code path, including error paths. Use the `goto cleanup` pattern.
- **Check every `fread`/`fwrite` return value** and compare against the expected count.
- **Never use `tmpnam` or `mktemp`.** Use `mkstemp` for temporary files.
- **Validate file sizes** before reading entire files into memory to prevent denial-of-service.

---

## Coding Style & Hygiene

- **C11 standard only.** No compiler-specific extensions unless guarded by `#ifdef`.
- **All headers must use include guards** (`#ifndef HEADER_H` / `#define HEADER_H` / `#endif`).
- **Declare variables at the top of the scope** for clarity. One declaration per line.
- **Use `/* */` comments only.** No `//` comments (C99-style may not be portable in all C11 configurations).
- **Prefer `static` for internal functions** — minimize the public API surface. Everything that can be `static`, must be.
- **No magic numbers.** Use `#define` or `enum` constants with descriptive names.
- **Keep functions short.** A function over 60 lines is a candidate for splitting.
- **No dead code.** Remove unused functions, variables, and `#include` directives.
- **Use `sizeof(variable)` not `sizeof(type)`** to prevent type mismatch bugs:
  ```c
  memset(&frame, 0, sizeof(frame));     /* GOOD */
  memset(&frame, 0, sizeof(fc_frame_t)); /* FRAGILE */
  ```

---

## Testing Requirements

- **Every new feature must include tests.** No exceptions.
- **Every bug fix must include a regression test** that reproduces the original bug.
- **Test failure paths, not just success paths.** At minimum, test:
  - `NULL` inputs
  - Zero-length inputs
  - Maximum-length inputs
  - Allocation failure (if mocking is available)
  - Malformed/corrupt input files
  - Integer boundary values (`0`, `1`, `UINT32_MAX`)
- **Tests must be deterministic.** No reliance on timing, randomness, or external network state.
- **Run `make test` and `make test-e2e` locally** before submitting. All tests must pass.
- **Tests are code.** They follow the same quality rules as production code.

---

## Pull Request Checklist

Before submitting a PR, verify **all** of the following:

- [ ] Code compiles with zero warnings under `-Wall -Wextra -Werror -pedantic`.
- [ ] All existing tests pass (`make test && make test-e2e`).
- [ ] New tests are included for new functionality or bug fixes.
- [ ] All pointer parameters are validated for `NULL` at function entry.
- [ ] All allocations are checked for failure.
- [ ] All allocated memory is freed on every code path (including error paths).
- [ ] No use of banned functions (`strcpy`, `sprintf`, `gets`, `strcat`, `strtok`, `alloca`).
- [ ] No variable-length arrays (VLAs).
- [ ] No integer overflow in size calculations.
- [ ] All array/buffer accesses are bounds-checked.
- [ ] All files are closed on every code path.
- [ ] No new global mutable state.
- [ ] `const` used wherever a function does not modify pointed-to data.
- [ ] No compiler-specific extensions used without `#ifdef` guards.
- [ ] Commit messages are clear and descriptive.

---

## Banned Functions Quick Reference

| Banned | Replacement |
|---|---|
| `malloc`, `calloc`, `realloc`, `free` | `fc_alloc`, `fc_calloc`, `fc_realloc`, `fc_free` |
| `strcpy` | `memcpy` with length validation |
| `strncpy` (without manual null-term) | `memcpy` + explicit `\0` |
| `sprintf` | `snprintf` |
| `vsprintf` | `vsnprintf` |
| `gets` | Removed in C11 — never use |
| `strcat`, `strncat` | Compute length, allocate, `memcpy` |
| `strtok` | `strtok_r` or manual parse |
| `alloca` | Heap allocation via `fc_alloc` |
| `tmpnam`, `mktemp` | `mkstemp` |
| `ctime`, `gmtime`, `localtime` | `ctime_r`, `gmtime_r`, `localtime_r` |

---

> **Remember:** In C, the compiler will not save you. The runtime will not save you. Only discipline, paranoia, and rigorous code review will save you. Write every line as if it will be executed with root privileges on a hostile network processing untrusted input — because one day, it might be.
