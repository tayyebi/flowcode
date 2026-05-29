# flowcode

Minimal Flowcode runtime/compiler scaffold implementing PRD v1.0 foundations:

- C runtime with bytecode loader/validator and VM skeleton
- Deterministic FIFO scheduler
- In-memory state store with TTL
- Plugin registry/ABI loader (dlopen/LoadLibrary)
- CLI MVP: `flowcode run file.fcb`
- TypeScript compiler skeleton: `compiler/index.ts`

## Build

```bash
make
```

## Test

```bash
make test
```

## Documentation

- [Flowcode documentation](./docs/README.md)
- [User manual](./docs/user-manual.md)
- [Tutorials](./docs/tutorials.md)
