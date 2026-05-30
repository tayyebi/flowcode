## Copilot Instructions for Flowcode

Before making any changes to this repository, read and follow the **mandatory** guidelines in [CONTRIBUTING.md](../CONTRIBUTING.md). It enforces pessimistic, hardened, military-grade coding standards. Every rule is non-negotiable.

Key points to internalize before writing any code:

- **Assume hostility.** Every input is hostile, every allocation will fail, every pointer is `NULL`, every buffer is too small.
- Build with `make`, run unit tests with `make test`, and run end-to-end tests with `make test-e2e`. All must pass with zero warnings.
- C code must compile cleanly under `-std=c11 -Wall -Wextra -Werror -pedantic`.
- Use the `fc_alloc`/`fc_calloc`/`fc_realloc`/`fc_free` memory wrappers; never use `malloc`/`free` directly.
- **Set pointers to `NULL` after `fc_free`.** Use-after-free is a critical vulnerability.
- Use `FC_MAX_NAME_LENGTH` for fixed-size name buffers and always validate lengths before copying.
- **Never use banned functions:** `strcpy`, `sprintf`, `gets`, `strcat`, `strtok`, `alloca`, VLAs. See the Banned Functions table in CONTRIBUTING.md.
- Check **every** allocation and I/O call for failure; return `-1` on error.
- Validate **all** pointer parameters for `NULL` at every public function entry.
- Keep internal struct definitions and helper functions `static` in their `.c` files; only expose the public API declared in `include/flowcode.h`.
- Use `/* */` comments only; no `//` C99-style comments.
- Use `const` aggressively on parameters not modified by the function.
- In the compiler, validate opcode arguments with explicit patterns and emit warnings for unrecognized lines.
- Every new feature must include tests. Every bug fix must include a regression test.
