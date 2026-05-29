# flowcode

Minimal Flowcode runtime/compiler scaffold implementing PRD v1.0 foundations:

- C runtime with bytecode loader/validator and VM skeleton
- Deterministic FIFO scheduler
- In-memory state store with TTL
- Plugin registry/ABI loader (dlopen/LoadLibrary)
- CLI MVP: `flowcode run file.fcb`
- TypeScript compiler skeleton: `compiler/index.ts`

## User onboarding

If you are new to the repository, start here:

1. Build the runtime with `make`.
2. Run the existing test suite with `make test`.
3. Review the sample workflows under `/samples`.
4. Start with `/samples/customer-onboarding` for a complete example that covers validation, human review, branching, loops, parallel work, state storage, and event emission.
5. Run compiled bytecode locally with `./flowcode run <file.fcb>`.

## Build

```bash
make
```

## Test

```bash
make test
```

## How to

### Explore a workflow example

- Read `/samples/customer-onboarding/README.md` for the end-to-end onboarding flow.
- Open `/samples/customer-onboarding/onboarding.fc` to see the source workflow definition.
- Browse the other sample directories in `/samples` for additional patterns.

### Run a compiled workflow

```bash
./flowcode run path/to/workflow.fcb
```

### Extend the compiler

The compiler entry point lives at `/compiler/index.ts` and currently provides the MVP bytecode generation scaffold.
