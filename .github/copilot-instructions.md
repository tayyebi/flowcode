## Copilot Instructions for Flowcode

Before making any changes to this repository, read and follow the guidelines in [CONTRIBUTING.md](../CONTRIBUTING.md). It covers the project structure, build/test commands, coding standards for both the C runtime and TypeScript compiler, and the process for adding new opcodes.

Key points:

- Build with `make` and run tests with `make test`. All tests must pass before submitting changes.
- C code must compile cleanly under `-std=c11 -Wall -Wextra -Werror -pedantic`.
- Use the `fc_alloc`/`fc_calloc`/`fc_realloc`/`fc_free` memory wrappers; do not use `malloc`/`free` directly.
- Use the `FC_MAX_NAME_LENGTH` constant for fixed-size name buffers and always validate lengths.
- Keep internal struct definitions and helper functions `static` in their `.c` files; only expose the public API declared in `include/flowcode.h`.
- In the compiler, validate opcode arguments with explicit patterns and emit warnings for unrecognized lines.
