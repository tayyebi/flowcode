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

tests/test_bytecode: $(SRC) tests/test_bytecode.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_bytecode.c $(LDFLAGS) $(LDLIBS)

tests/test_cli_run: $(SRC) tests/test_cli_run.c
	$(CC) $(CFLAGS) -o $@ $(SRC) tests/test_cli_run.c $(LDFLAGS) $(LDLIBS)

clean:
	rm -f flowcode tests/test_bytecode tests/test_cli_run tests/*.fcb

.PHONY: all test clean
