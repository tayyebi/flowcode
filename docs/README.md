# Flowcode Documentation

This directory contains end-user documentation for writing Flowcode workflows.

## Contents

- [User Manual](./user-manual.md) — concepts, syntax, and authoring patterns.
- [Tutorials](./tutorials.md) — guided walkthroughs based on the sample workflows in this repository.

## Quick Start

1. Write a workflow in a `.fc` file.
2. Compile it into bytecode:

   ```bash
   node compiler/index.ts path/to/workflow.fc path/to/workflow.fcb
   ```

3. Run the compiled workflow:

   ```bash
   ./flowcode run path/to/workflow.fcb
   ```

If you are new to Flowcode, start with the [User Manual](./user-manual.md) and then work through the [Tutorials](./tutorials.md).
