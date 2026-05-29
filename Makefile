CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude
LDFLAGS ?=
ifeq ($(OS),Windows_NT)
  LDLIBS :=
else
  LDLIBS := -ldl
endif

SRC = src/memory.c src/bytecode.c src/state.c src/scheduler.c src/plugin.c src/vm.c
CLI = src/cli.c

all: flowcode

flowcode: $(SRC) $(CLI)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(CLI) $(LDFLAGS) $(LDLIBS)

test: flowcode tests/test_bytecode tests/test_cli_run
	./tests/test_bytecode
	./tests/test_cli_run

E2E_TESTS = tests/test_e2e_emit_store tests/test_e2e_transform tests/test_e2e_route tests/test_e2e_invalid_opcode

test-e2e: flowcode $(E2E_TESTS) compiler
	bash tests/test_e2e.sh

compiler:
	npx tsc

tests/test_bytecode: $(SRC) tests/test_bytecode.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_bytecode.c $(LDFLAGS) $(LDLIBS)

tests/test_cli_run: $(SRC) tests/test_cli_run.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_cli_run.c $(LDFLAGS) $(LDLIBS)

tests/test_e2e_emit_store: $(SRC) tests/test_e2e_emit_store.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_e2e_emit_store.c $(LDFLAGS) $(LDLIBS)

tests/test_e2e_transform: $(SRC) tests/test_e2e_transform.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_e2e_transform.c $(LDFLAGS) $(LDLIBS)

tests/test_e2e_route: $(SRC) tests/test_e2e_route.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_e2e_route.c $(LDFLAGS) $(LDLIBS)

tests/test_e2e_invalid_opcode: $(SRC) tests/test_e2e_invalid_opcode.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_e2e_invalid_opcode.c $(LDFLAGS) $(LDLIBS)

clean:
	rm -f flowcode tests/test_bytecode tests/test_cli_run $(E2E_TESTS) tests/*.fcb
	rm -rf dist

.PHONY: all test test-e2e compiler clean
