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

export function compileFlowcode(source: string): Buffer {
  const lines = source.split(/\r?\n/).map((l) => l.trim());
  const instructions: Instruction[] = [];
  const args: Buffer[] = [];
  let argOffset = 0;

  for (const line of lines) {
    if (!line || line.endsWith(':') || line === 'end' || line.includes('->')) continue;

    if (line.startsWith('emit')) {
      const payload = Buffer.from('complete', 'utf8');
      instructions.push({ opcode: OPCODES.emit, argOffset, argLength: payload.length });
      args.push(payload);
      argOffset += payload.length;
    } else if (line.startsWith('transform')) {
      instructions.push({ opcode: OPCODES.transform, argOffset: 0, argLength: 0 });
    } else if (line.startsWith('store')) {
      instructions.push({ opcode: OPCODES.store, argOffset: 0, argLength: 0 });
    } else if (line.startsWith('match')) {
      const routeArg = encodeU32(instructions.length + 1);
      instructions.push({ opcode: OPCODES.route, argOffset, argLength: routeArg.length });
      args.push(routeArg);
      argOffset += routeArg.length;
    } else if (line.startsWith('http.') || line.startsWith('webhook')) {
      instructions.push({ opcode: OPCODES.call, argOffset: 0, argLength: 0 });
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
