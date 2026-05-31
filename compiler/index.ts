import * as fs from 'fs';

type Instruction = { opcode: number; argOffset: number; argLength: number };

/** Diagnostic message emitted during compilation. */
type Diagnostic = { level: 'error' | 'warning'; line: number; message: string };

const OPCODES: Record<string, number> = {
  emit: 0x01,
  await: 0x02,
  call: 0x03,
  transform: 0x04,
  route: 0x05,
  loop: 0x06,
  store: 0x07,
};

function encodeU16(v: number): Buffer {
  const b = Buffer.alloc(2);
  b.writeUInt16LE(v, 0);
  return b;
}

function encodeU32(v: number): Buffer {
  const b = Buffer.alloc(4);
  b.writeUInt32LE(v, 0);
  return b;
}

/**
 * Extract a named parameter value from the lines following a directive.
 * Scans forward from `start` until `end` or a blank/structural line,
 * looking for `key = <value>`.  Returns the raw value string or null.
 */
function extractParam(lines: string[], start: number, key: string): string | null {
  for (let i = start; i < lines.length; i++) {
    const l = lines[i];
    if (!l || l === 'end' || l.endsWith(':') || /^\w+\s*->$/.test(l)) break;
    const m = l.match(new RegExp(`^${key}\\s*=\\s*(.+)$`));
    if (m) return m[1].replace(/^"(.*)"$/, '$1');
  }
  return null;
}

function pushArg(
  instructions: Instruction[],
  args: Buffer[],
  argOffset: number,
  opcode: number,
  payload: Buffer,
): number {
  instructions.push({ opcode, argOffset, argLength: payload.length });
  args.push(payload);
  return argOffset + payload.length;
}

/**
 * Compile Flowcode source to bytecode.
 *
 * Returns `{ bytecode, diagnostics }`. The `diagnostics` array contains
 * all warnings and errors found during compilation.  Compilation always
 * produces bytecode (best-effort) so that partial programs can be
 * inspected, but callers should check diagnostics for errors.
 */
export function compileFlowcode(source: string): {
  bytecode: Buffer;
  diagnostics: Diagnostic[];
} {
  const lines = source.split(/\r?\n/).map((l) => l.trim());
  const instructions: Instruction[] = [];
  const args: Buffer[] = [];
  const diagnostics: Diagnostic[] = [];
  let argOffset = 0;

  /* ---------- Collect step names for semantic validation ---------- */
  const stepNames = new Map<string, number>(); // name → first source line
  for (let i = 0; i < lines.length; i++) {
    const stepMatch = lines[i].match(/^step\s+([\w.]+)\s*:?$/);
    if (stepMatch) {
      const name = stepMatch[1];
      if (stepNames.has(name)) {
        diagnostics.push({
          level: 'error',
          line: i + 1,
          message: `duplicate step name "${name}" (first defined at line ${stepNames.get(name)})`,
        });
      } else {
        stepNames.set(name, i + 1);
      }
    }
  }

  /* ---------- Track route/loop targets for bounds validation ---------- */
  const routeTargets: { instrIndex: number; targetInstr: number; srcLine: number }[] = [];

  /* ---------- Identify plugin-calling steps for on_error warnings ---------- */
  const pluginCallLines: number[] = [];

  /* ---------- First pass: emit instructions ---------- */
  let afterStop = false;
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];

    // Skip structural markers, blank lines, and parameter lines (key = value).
    if (!line || line.endsWith(':') || line === 'end' || /^\w+\s*->$/.test(line)) continue;
    if (/^\w+\s*=\s*.+$/.test(line)) continue;

    // Detect unreachable code after 'stop'
    if (afterStop && line !== 'stop') {
      // Reset afterStop on new step boundary
      if (line.startsWith('step ')) {
        afterStop = false;
      } else {
        diagnostics.push({
          level: 'warning',
          line: i + 1,
          message: 'unreachable code after stop',
        });
        continue;
      }
    }

    if (line.startsWith('emit')) {
      // Parse the value parameter from subsequent lines; fall back to "complete".
      const value = extractParam(lines, i + 1, 'value') ?? 'complete';
      const payload = Buffer.from(value, 'utf8');
      argOffset = pushArg(instructions, args, argOffset, OPCODES.emit, payload);
    } else if (line.startsWith('transform')) {
      // The transform function name follows the keyword on the same line.
      const match = line.match(/^transform\s+([\w.]+)$/);
      if (match) {
        const payload = Buffer.from(match[1], 'utf8');
        argOffset = pushArg(instructions, args, argOffset, OPCODES.transform, payload);
      } else {
        instructions.push({ opcode: OPCODES.transform, argOffset: 0, argLength: 0 });
      }
    } else if (line.startsWith('store')) {
      // Parse the key parameter from subsequent lines.
      const key = extractParam(lines, i + 1, 'key');
      if (key) {
        const payload = Buffer.from(key, 'utf8');
        argOffset = pushArg(instructions, args, argOffset, OPCODES.store, payload);
      } else {
        instructions.push({ opcode: OPCODES.store, argOffset: 0, argLength: 0 });
      }
    } else if (line.startsWith('loop')) {
      // Encode loop target as the next sequential instruction index.
      const targetInstr = instructions.length + 1;
      const loopArg = encodeU32(targetInstr);
      routeTargets.push({ instrIndex: instructions.length, targetInstr, srcLine: i + 1 });
      argOffset = pushArg(instructions, args, argOffset, OPCODES.loop, loopArg);
    } else if (line.startsWith('await')) {
      instructions.push({ opcode: OPCODES.await, argOffset: 0, argLength: 0 });
    } else if (line.startsWith('match')) {
      // Route target: next sequential instruction after this route opcode.
      const targetInstr = instructions.length + 1;
      const routeArg = encodeU32(targetInstr);
      routeTargets.push({ instrIndex: instructions.length, targetInstr, srcLine: i + 1 });
      argOffset = pushArg(instructions, args, argOffset, OPCODES.route, routeArg);
    } else if (line.startsWith('on_error')) {
      // on_error is a recognized error-handling directive.
      // Extract the strategy from the same line or subsequent params.
      const strategyMatch = line.match(/^on_error\s+(retry|stop|skip|goto\s+\S+)$/);
      if (strategyMatch) {
        // Emit an emit instruction with the error strategy as metadata.
        // The runtime can read __on_error state to decide behavior.
        const strategy = strategyMatch[1];
        const payload = Buffer.from(`on_error:${strategy}`, 'utf8');
        argOffset = pushArg(instructions, args, argOffset, OPCODES.store, payload);
      } else {
        // Parse strategy from subsequent params
        const retryCount = extractParam(lines, i + 1, 'retry');
        const timeout = extractParam(lines, i + 1, 'timeout');
        const fallback = extractParam(lines, i + 1, 'fallback');

        // Store the on_error configuration in state for runtime inspection
        if (retryCount) {
          const payload = Buffer.from(`__on_error.retry`, 'utf8');
          argOffset = pushArg(instructions, args, argOffset, OPCODES.store, payload);
        }
        if (timeout) {
          const payload = Buffer.from(`__on_error.timeout`, 'utf8');
          argOffset = pushArg(instructions, args, argOffset, OPCODES.store, payload);
        }
        if (fallback) {
          const payload = Buffer.from(`__on_error.fallback`, 'utf8');
          argOffset = pushArg(instructions, args, argOffset, OPCODES.store, payload);
        }
        if (!retryCount && !timeout && !fallback) {
          diagnostics.push({
            level: 'warning',
            line: i + 1,
            message: 'on_error block with no strategy (retry, timeout, or fallback)',
          });
        }
      }
    } else if (line.startsWith('retry')) {
      // retry <count> — recognized error-handling directive
      diagnostics.push({
        level: 'warning',
        line: i + 1,
        message: 'standalone retry outside on_error block; wrap in on_error for proper handling',
      });
    } else if (line.startsWith('timeout')) {
      // timeout <duration> — recognized as a step attribute
      // Silently skip; the value is consumed by extractParam from parent step
    } else if (line.startsWith('compensate')) {
      // compensate block — recognized error-handling directive
      // Silently skip structural marker; compensation steps within are compiled normally
    } else if (line.startsWith('http.') || line.startsWith('webhook')) {
      const targetName = Buffer.from(line.split(/\s+/)[0], 'utf8');
      argOffset = pushArg(instructions, args, argOffset, OPCODES.call, targetName);
      pluginCallLines.push(i + 1);
    } else if (
      line.startsWith('step ') ||
      line.startsWith('workflow') ||
      line.startsWith('trigger') ||
      line.startsWith('use ') ||
      line.startsWith('parallel') ||
      line === 'stop' ||
      line.startsWith('email.') ||
      line.startsWith('crm.') ||
      line.startsWith('storage.')
    ) {
      // Known structural or plugin-call keywords handled elsewhere; skip silently.
      if (line.startsWith('email.') || line.startsWith('crm.') || line.startsWith('storage.')) {
        const targetName = Buffer.from(line.split(/\s+/)[0], 'utf8');
        argOffset = pushArg(instructions, args, argOffset, OPCODES.call, targetName);
        pluginCallLines.push(i + 1);
      }
      if (line === 'stop') {
        afterStop = true;
      }
    } else {
      diagnostics.push({
        level: 'warning',
        line: i + 1,
        message: `unrecognized line: ${line}`,
      });
    }
  }

  /* ---------- Post-pass semantic validation ---------- */

  // Validate route/loop targets are within instruction bounds
  for (const rt of routeTargets) {
    if (rt.targetInstr >= instructions.length && instructions.length > 0) {
      // Target points beyond the last instruction — this is only an error
      // if it's strictly beyond count (== count means "past end" which the
      // VM will handle as normal exit).
      if (rt.targetInstr > instructions.length) {
        diagnostics.push({
          level: 'error',
          line: rt.srcLine,
          message: `route/loop target instruction ${rt.targetInstr} exceeds program size ${instructions.length}`,
        });
      }
    }
  }

  /* ---------- Encode bytecode ---------- */

  const header = Buffer.concat([
    Buffer.from('FCB1', 'ascii'),
    encodeU16(1),
    encodeU16(0),
    encodeU32(instructions.length),
    encodeU32(argOffset),
  ]);

  const insBuf = Buffer.alloc(instructions.length * 9);
  instructions.forEach((ins, idx) => {
    const off = idx * 9;
    insBuf.writeUInt8(ins.opcode, off);
    insBuf.writeUInt32LE(ins.argOffset, off + 1);
    insBuf.writeUInt32LE(ins.argLength, off + 5);
  });

  return {
    bytecode: Buffer.concat([header, insBuf, ...args]),
    diagnostics,
  };
}

if (require.main === module) {
  const [, , inputPath, outputPath] = process.argv;
  if (!inputPath || !outputPath) {
    process.stderr.write('usage: node compiler/index.ts <input.fc> <output.fcb>\n');
    process.exit(1);
  }
  const source = fs.readFileSync(inputPath, 'utf8');
  const { bytecode, diagnostics } = compileFlowcode(source);

  // Print diagnostics to stderr
  let hasErrors = false;
  for (const d of diagnostics) {
    process.stderr.write(`${d.level}: line ${d.line}: ${d.message}\n`);
    if (d.level === 'error') hasErrors = true;
  }

  // Always write bytecode (best-effort) but exit 1 if there were errors
  fs.writeFileSync(outputPath, bytecode);

  if (hasErrors) {
    process.stderr.write(`compilation completed with errors\n`);
    process.exit(1);
  }
  if (diagnostics.length > 0) {
    process.stderr.write(
      `compilation completed with ${diagnostics.length} diagnostic(s)\n`,
    );
  }
}
