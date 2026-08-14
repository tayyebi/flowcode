CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS ?=
ifeq ($(OS),Windows_NT)
  LDLIBS :=
else
  LDLIBS := -ldl
endif

SRC = src/memory.c src/log.c src/error.c src/bytecode.c src/state.c src/scheduler.c src/plugin.c src/builtins.c src/vm.c
CLI = src/cli.c
COMPILER_SRC = src/memory.c src/log.c src/compiler.c

all: flowcode fcc

flowcode: $(SRC) $(CLI)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(CLI) $(LDFLAGS) $(LDLIBS)

fcc: $(COMPILER_SRC)
	$(CC) $(CFLAGS) -DFC_COMPILER_MAIN -o $@ $(COMPILER_SRC) $(LDFLAGS)

test: flowcode tests/test_bytecode tests/test_cli_run tests/test_error_handling
	./tests/test_bytecode
	./tests/test_cli_run
	./tests/test_error_handling

E2E_TESTS = tests/test_e2e_emit_store tests/test_e2e_transform tests/test_e2e_route tests/test_e2e_invalid_opcode

test-e2e: flowcode fcc $(E2E_TESTS)
	bash tests/test_e2e.sh

# Compile and run every workflow under samples/ with the built binaries.
test-samples: flowcode fcc
	bash tests/test_samples.sh

tests/test_bytecode: $(SRC) tests/test_bytecode.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_bytecode.c $(LDFLAGS) $(LDLIBS)

tests/test_cli_run: $(SRC) tests/test_cli_run.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_cli_run.c $(LDFLAGS) $(LDLIBS)

tests/test_error_handling: $(SRC) tests/test_error_handling.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_error_handling.c $(LDFLAGS) $(LDLIBS)

tests/test_e2e_emit_store: $(SRC) tests/test_e2e_emit_store.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_e2e_emit_store.c $(LDFLAGS) $(LDLIBS)

tests/test_e2e_transform: $(SRC) tests/test_e2e_transform.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_e2e_transform.c $(LDFLAGS) $(LDLIBS)

tests/test_e2e_route: $(SRC) tests/test_e2e_route.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_e2e_route.c $(LDFLAGS) $(LDLIBS)

tests/test_e2e_invalid_opcode: $(SRC) tests/test_e2e_invalid_opcode.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_e2e_invalid_opcode.c $(LDFLAGS) $(LDLIBS)

clean:
	rm -f flowcode fcc tests/test_bytecode tests/test_cli_run tests/test_error_handling $(E2E_TESTS) tests/*.fcb

.PHONY: all test test-e2e test-samples clean
