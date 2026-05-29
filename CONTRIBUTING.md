# Contributing to Flowcode

Thank you for your interest in contributing to Flowcode! This guide covers the conventions, workflow, and standards expected for all contributions.

## Project Structure

```
compiler/       TypeScript compiler (.fc → .fcb bytecode)
include/        C header files (public API)
src/            C runtime source (VM, scheduler, state, plugins, CLI)
tests/          C test files
samples/        Example .fc workflow programs
```

## Build and Test

Build the project:

```bash
make
```

Run all tests:

```bash
make test
```

Clean build artifacts:

```bash
make clean
```

Always ensure `make test` passes before submitting a pull request.

## C Runtime Guidelines

- Use C11 (`-std=c11`) with strict warnings (`-Wall -Wextra -Werror -pedantic`).
- Allocate memory through the `fc_alloc` / `fc_calloc` / `fc_realloc` / `fc_free` wrappers in `fc_memory.h`; never call `malloc` or `free` directly in runtime code.
- Use the `FC_MAX_NAME_LENGTH` constant for fixed-size name buffers and always validate lengths before copying.
- All public API functions are declared in `include/flowcode.h`. Keep internal helpers `static` within their source files.
- Check every allocation and I/O call for failure; return `-1` on error.
- Opaque struct typedefs (e.g., `fc_vm_t`, `fc_state_store_t`) keep internals private; define the full struct only in the corresponding `.c` file.

## Compiler Guidelines

- The compiler is a single TypeScript file at `compiler/index.ts`.
- Validate opcode arguments with explicit patterns (e.g., regex for identifiers) rather than accepting arbitrary input.
- Emit warnings to `stderr` for unrecognized source lines; do not silently discard input.

## Commit and Pull Request Conventions

- Write clear, concise commit messages in imperative mood (e.g., "Add loop opcode support").
- Keep pull requests focused on a single change or closely related set of changes.
- Include or update tests for any new or changed functionality.

## Adding a New Opcode

1. Add the opcode constant to `fc_opcode_t` in `include/flowcode.h` and the `OPCODES` map in `compiler/index.ts`.
2. Implement the `exec_<opcode>` handler in `src/vm.c` and add its case to the switch in `fc_vm_run`.
3. Add compiler parsing logic in `compiler/index.ts`.
4. Add a test in `tests/` that exercises the new instruction end-to-end.
5. Update `valid_opcode()` in `src/bytecode.c` if the opcode range changes.

## Reporting Issues

Open a GitHub issue with a clear description, steps to reproduce, and the expected vs. actual behavior.
