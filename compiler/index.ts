import * as fs from 'fs';

type Instruction = { opcode: number; argOffset: number; argLength: number };

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

export function compileFlowcode(source: string): Buffer {
  const lines = source.split(/\r?\n/).map((l) => l.trim());
  const instructions: Instruction[] = [];
  const args: Buffer[] = [];
  let argOffset = 0;

  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];

    // Skip structural markers, blank lines, and parameter lines (key = value).
    if (!line || line.endsWith(':') || line === 'end' || /^\w+\s*->$/.test(line)) continue;
    if (/^\w+\s*=\s*.+$/.test(line)) continue;

    if (line.startsWith('emit')) {
      // Parse the value parameter from subsequent lines; fall back to "complete".
      const value = extractParam(lines, i + 1, 'value') ?? 'complete';
      const payload = Buffer.from(value, 'utf8');
      argOffset = pushArg(instructions, args, argOffset, OPCODES.emit, payload);
    } else if (line.startsWith('transform')) {
      // The transform function name follows the keyword on the same line.
      const funcName = line.replace(/^transform\s*/, '');
      if (funcName) {
        const payload = Buffer.from(funcName, 'utf8');
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
      const loopArg = encodeU32(instructions.length + 1);
      argOffset = pushArg(instructions, args, argOffset, OPCODES.loop, loopArg);
    } else if (line.startsWith('await')) {
      instructions.push({ opcode: OPCODES.await, argOffset: 0, argLength: 0 });
    } else if (line.startsWith('match')) {
      // Route target: next sequential instruction after this route opcode.
      const routeArg = encodeU32(instructions.length + 1);
      argOffset = pushArg(instructions, args, argOffset, OPCODES.route, routeArg);
    } else if (line.startsWith('http.') || line.startsWith('webhook')) {
      const targetName = Buffer.from(line.split(/\s+/)[0], 'utf8');
      argOffset = pushArg(instructions, args, argOffset, OPCODES.call, targetName);
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
      }
    } else {
      process.stderr.write(`warning: unrecognized line ${i + 1}: ${line}\n`);
    }
  }

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

  return Buffer.concat([header, insBuf, ...args]);
}

if (require.main === module) {
  const [, , inputPath, outputPath] = process.argv;
  if (!inputPath || !outputPath) {
    process.stderr.write('usage: node compiler/index.ts <input.fc> <output.fcb>\n');
    process.exit(1);
  }
  const source = fs.readFileSync(inputPath, 'utf8');
  fs.writeFileSync(outputPath, compileFlowcode(source));
}
