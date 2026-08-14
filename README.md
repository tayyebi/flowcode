# flowcode

Flowcode runtime and compiler for executing event-driven workflow programs:

- C runtime with bytecode loader, validator, and virtual machine
- C compiler: `fcc <input.fc> <output.fcb>`
- Deterministic FIFO scheduler
- In-memory state store with TTL expiry
- Plugin registry with dynamic loading (dlopen/LoadLibrary)
- Structured error handling with inspectable error codes and error-as-state
- Leveled logging (DEBUG/INFO/WARN/ERROR) to stderr
- CLI: `flowcode run file.fcb`

## User onboarding

If you are new to the repository, start here:

1. Build the runtime with `make`.
2. Run the existing test suite with `make test`.
3. Review the sample workflows under `samples/`.
4. Start with `samples/hello-world/` — two instructions, no plugins — then read `samples/customer-onboarding/` for a complete example that covers validation, human review, branching, loops, parallel work, state storage, and event emission.
5. Run compiled bytecode locally with `./flowcode run <file.fcb>`.

## Install a released binary

Every `v*` tag builds release archives for linux-x64, linux-arm64, darwin-x64,
and darwin-arm64 and attaches them to the GitHub Release. Each archive contains
`flowcode`, `fcc`, and a copy of `samples/`, so an unpacked release runs the
examples on its own:

```bash
tar -xzf flowcode-v0.1.0-linux-x64.tar.gz
cd flowcode-v0.1.0-linux-x64
./fcc samples/hello-world/hello.fc hello.fcb
./flowcode run hello.fcb
```

## Build

```bash
make
```

## Test

```bash
make test
```

## E2E Test

Runs end-to-end tests covering the full pipeline (compiler → bytecode → runtime), CLI error handling, VM execution with various instruction types, and compiler semantic validation:

```bash
make test-e2e
```

## Sample workflows

Compiles and runs every workflow under `samples/` with the built binaries — the
check that the shipped compiler and runtime execute the documented examples end
to end:

```bash
make test-samples
```

## Built-in plugins

The `flowcode` CLI registers no-I/O stubs for the standard integration names —
`http.*`, `email.*`, `crm.*`, `storage.*`, `form.*`, `ai.*`, `memory.*`,
`mqtt.*`, and `webhook`. Each logs the call and passes the token through, so a
workflow written against them runs to completion with no credentials, no
network, and no side effects. The CLI also seeds an empty default token so a
workflow that stores before it emits still runs.

Loading a plugin library that exports one of these names replaces the stub:

```c
fc_plugins_register_builtins(registry);   /* stubs first */
fc_plugins_load(registry, "./libhttp.so"); /* real http.* wins */
```

To turn the stubs off and have unresolved calls fail with
`FC_ERR_PLUGIN_NOT_FOUND`:

```bash
flowcode run workflow.fcb --strict
```

Embedders that build on `fc_vm_*` directly get neither stubs nor a default
token; both are CLI-level conveniences.

## Error handling

Flowcode treats errors as first-class citizens rather than opaque exit codes. The runtime provides:

### Structured error codes (`include/fc_error.h`)

Every subsystem returns a specific `fc_error_code_t` instead of a bare `-1`. Error codes include:

| Code | Meaning |
|------|---------|
| `FC_ERR_OK` | No error |
| `FC_ERR_PLUGIN_NOT_FOUND` | Named plugin not in registry |
| `FC_ERR_PLUGIN_CALL` | Plugin function returned `FC_PLUGIN_ERR` |
| `FC_ERR_ARENA_FULL` | Token arena exhausted |
| `FC_ERR_ROUTE_OOB` | Route/loop target out of bounds |
| `FC_ERR_MISSING_TOKEN` | Store requires a token but none exists |
| `FC_ERR_QUEUE_FULL` | Scheduler ring buffer at capacity |
| `FC_ERR_ALLOC` | Memory allocation failed |

See `include/fc_error.h` for the full list.

### Error context on the VM

After `fc_vm_run()` returns non-zero, call `fc_vm_last_error(vm)` to get an `fc_error_t` struct containing:
- `code` — the specific error code
- `message` — human-readable description
- `instruction_index` — the bytecode instruction that failed

### Error-as-state

When a step fails, the error is automatically recorded in the state store under the key `__error.<instruction_index>`. This allows subsequent workflow steps to inspect what failed and branch accordingly.

### Structured logging (`include/fc_log.h`)

All subsystems log diagnostic messages to stderr with level prefixes:
- `[flowcode:ERROR]` — failures that halt execution
- `[flowcode:WARN]` — degradation warnings (e.g., scheduler queue >75% full)
- `[flowcode:INFO]` — execution lifecycle events
- `[flowcode:DEBUG]` — verbose diagnostic output

Control the log level programmatically with `fc_log_set_level()`.

### Compiler diagnostics

The compiler performs semantic validation and reports structured diagnostics:
- **Duplicate step names** — error when the same step name is defined twice
- **Unreachable code** — warning for code after a `stop` directive
- **Route/loop target bounds** — error when a target exceeds program size
- **Error-handling constructs** — `on_error`, `retry`, `timeout`, and `compensate` are recognized keywords

## How to

### Explore a workflow example

- Read `samples/customer-onboarding/README.md` for the end-to-end onboarding flow.
- Open `samples/customer-onboarding/onboarding.fc` to see the source workflow definition.
- Browse the other sample directories in `samples/` for additional patterns.

### Run a compiled workflow

```bash
./fcc path/to/workflow.fc workflow.fcb
./flowcode run workflow.fcb
```

### Extend the compiler

The compiler entry point lives at `src/compiler.c` and handles bytecode generation for the Flowcode language. The public API is declared in `include/fc_compiler.h`.
