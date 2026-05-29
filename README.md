# flowcode

Flowcode runtime and compiler for executing event-driven workflow programs:

- C runtime with bytecode loader, validator, and virtual machine
- Deterministic FIFO scheduler
- In-memory state store with TTL expiry
- Plugin registry with dynamic loading (dlopen/LoadLibrary)
- CLI: `flowcode run file.fcb`
- TypeScript compiler: `compiler/index.ts`

## Build

```bash
make
```

## Test

```bash
make test
```
