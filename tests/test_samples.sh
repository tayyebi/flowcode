#!/bin/bash
# Compiles and runs every workflow under samples/ with the built binaries.
# This is the check that the shipped `fcc` and `flowcode` actually execute the
# documented examples end to end.
set -uo pipefail

PASS=0
FAIL=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FLOWCODE="${FLOWCODE:-$ROOT_DIR/flowcode}"
COMPILER="${FCC:-$ROOT_DIR/fcc}"
TMP_DIR="$(mktemp -d)"

cleanup() { rm -rf "$TMP_DIR"; }
trap cleanup EXIT

if [ ! -x "$FLOWCODE" ] || [ ! -x "$COMPILER" ]; then
    printf "error: missing binaries (%s, %s); run 'make' first\n" "$FLOWCODE" "$COMPILER" >&2
    exit 1
fi

printf "=== Flowcode Sample Workflows ===\n\n"
printf "flowcode: %s\n" "$FLOWCODE"
printf "fcc:      %s\n\n" "$COMPILER"

for src in "$ROOT_DIR"/samples/*/*.fc; do
    name="$(basename "$(dirname "$src")")/$(basename "$src")"
    out="$TMP_DIR/$(basename "$(dirname "$src")").fcb"

    COMPILE_LOG="$("$COMPILER" "$src" "$out" 2>&1)"
    if [ $? -ne 0 ]; then
        printf "  FAIL: %s did not compile\n" "$name"
        printf "%s\n" "$COMPILE_LOG" | sed 's/^/        /'
        FAIL=$((FAIL + 1))
        continue
    fi

    # Warnings are acceptable; errors are not (they make fcc exit non-zero above).
    if printf "%s\n" "$COMPILE_LOG" | grep -q "unrecognized line"; then
        printf "  FAIL: %s has unrecognized source lines\n" "$name"
        printf "%s\n" "$COMPILE_LOG" | grep "unrecognized line" | sed 's/^/        /'
        FAIL=$((FAIL + 1))
        continue
    fi

    RUN_LOG="$("$FLOWCODE" run "$out" 2>&1)"
    if [ $? -ne 0 ]; then
        printf "  FAIL: %s compiled but did not run\n" "$name"
        printf "%s\n" "$RUN_LOG" | sed 's/^/        /'
        FAIL=$((FAIL + 1))
        continue
    fi

    printf "  PASS: %s compiles and runs\n" "$name"
    PASS=$((PASS + 1))
done

printf "\n=== Results: %d/%d samples passed ===\n" "$PASS" "$((PASS + FAIL))"
[ "$FAIL" -eq 0 ]
