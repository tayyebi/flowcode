/** Diagnostic message emitted during compilation. */
type Diagnostic = {
    level: 'error' | 'warning';
    line: number;
    message: string;
};
/**
 * Compile Flowcode source to bytecode.
 *
 * Returns `{ bytecode, diagnostics }`. The `diagnostics` array contains
 * all warnings and errors found during compilation.  Compilation always
 * produces bytecode (best-effort) so that partial programs can be
 * inspected, but callers should check diagnostics for errors.
 */
export declare function compileFlowcode(source: string): {
    bytecode: Buffer;
    diagnostics: Diagnostic[];
};
export {};
