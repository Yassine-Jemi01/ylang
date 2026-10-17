
VERSION := $(shell cat VERSION)
CC = gcc
AR ?= ar
CPPFLAGS += -Iinclude -DYLANG_VERSION=\"$(VERSION)\" -DYLANG_SHARE_DIR=\"$(PREFIX)/share/ylang\" -DYLANG_INCLUDE_DIR=\"$(PREFIX)/include\" -DYLANG_RUNTIME_SOURCE_PATH=\"$(CURDIR)/runtime/runtime-template.c\"
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O0
LDFLAGS ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
BIN := build/ylang

.PHONY: all clean test sanitize install uninstall print-version gui gui-example install-gui check-sdl2 fuzz-lexer fuzz-parser

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

print-version:
	@printf '%s\n' '$(VERSION)'

UNIT_OBJ := $(filter-out build/main.o build/driver.o,$(OBJ))
FUZZ_CC ?= clang
FUZZ_FLAGS := -std=c17 -g -O1 -Wall -Wextra -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -Iinclude -Isrc

build/test-lexer: tests/unit/lexer_test.c src/lexer.c include/ylang/lexer.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $< src/lexer.c -o $@

build/test-parser: tests/unit/parser_test.c $(UNIT_OBJ)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -Isrc $< $(UNIT_OBJ) $(LDFLAGS) -lm -o $@

test: $(BIN) build/test-lexer build/test-parser
	./build/test-lexer
	./build/test-parser
	sh tests/run.sh

fuzz-lexer: tests/fuzz/fuzz_lexer.c src/lexer.c
	@mkdir -p build
	$(FUZZ_CC) $(CPPFLAGS) $(FUZZ_FLAGS) $^ -o build/fuzz-lexer

fuzz-parser: tests/fuzz/fuzz_parser.c $(filter-out src/main.c src/driver.c,$(SRC))
	@mkdir -p build
	$(FUZZ_CC) $(CPPFLAGS) $(FUZZ_FLAGS) $^ -lm -o build/fuzz-parser

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test

GUI_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
GUI_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

check-sdl2:
	@pkg-config --exists sdl2 || { echo 'SDL2 development files and pkg-config are required (install libsdl2-dev or brew install sdl2 pkg-config).' >&2; exit 1; }

gui: check-sdl2 build/libylang_gui.a

gui-example: check-sdl2 build/gui-example

build/ylang_gui.o: stdlib/gui/gui.c include/ylang/gui.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GUI_CFLAGS) -c stdlib/gui/gui.c -o $@

build/libylang_gui.a: build/ylang_gui.o
	$(AR) rcs $@ $<

build/gui-example: examples/gui.c build/libylang_gui.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GUI_CFLAGS) $< build/libylang_gui.a $(GUI_LIBS) -o $@

install-gui: gui
	install -d '$(DESTDIR)$(PREFIX)/include/ylang' '$(DESTDIR)$(PREFIX)/lib' '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/gui'
	install -m 644 include/ylang/gui.h '$(DESTDIR)$(PREFIX)/include/ylang/gui.h'
	install -m 644 stdlib/gui/gui.c '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/gui/gui.c'
	install -m 644 build/libylang_gui.a '$(DESTDIR)$(PREFIX)/lib/libylang_gui.a'

install: $(BIN)
	install -d '$(DESTDIR)$(BINDIR)' '$(DESTDIR)$(PREFIX)/share/ylang'
	install -m 755 $(BIN) '$(DESTDIR)$(BINDIR)/ylang'
	install -m 644 runtime/runtime-template.c '$(DESTDIR)$(PREFIX)/share/ylang/runtime-template.c'

uninstall:
	rm -f '$(DESTDIR)$(BINDIR)/ylang'

clean:
	rm -rf build
