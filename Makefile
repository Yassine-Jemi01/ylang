
VERSION := $(shell cat VERSION)
CC = gcc
AR ?= ar
CPPFLAGS += -Iinclude -DYLANG_VERSION=\"$(VERSION)\" -DYLANG_SHARE_DIR=\"$(PREFIX)/share/ylang\" -DYLANG_INCLUDE_DIR=\"$(PREFIX)/include\"
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O0
LDFLAGS ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
BIN := build/ylang

ifeq ($(OS),Windows_NT)
GAME2D_OUTPUT ?= build/dodge.exe
else
GAME2D_OUTPUT ?= build/dodge
endif

.PHONY: all clean test sanitize install uninstall print-version gui gui-example game2d-example install-gui check-sdl2 check-runtime-template regen-runtime-template frontend-unit-test fuzz-frontend fuzz-smoke

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

check-runtime-template:
	@python3 tools/embed_text.py runtime/runtime_template.c src/runtime_template.h --check

regen-runtime-template:
	@python3 tools/embed_text.py runtime/runtime_template.c src/runtime_template.h

print-version:
	@printf '%s\n' '$(VERSION)'

build/frontend-unit: tests/frontend_unit.c src/parser.c src/lexer.c src/internal.h include/ylang/compiler.h include/ylang/lexer.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) tests/frontend_unit.c src/parser.c src/lexer.c $(LDFLAGS) -o $@

frontend-unit-test: build/frontend-unit
	./build/frontend-unit

test: $(BIN) frontend-unit-test
	sh tests/run.sh

FUZZ_CC ?= clang
FUZZ_CFLAGS ?= -std=c17 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer

fuzz-frontend:
	@mkdir -p build
	$(FUZZ_CC) $(CPPFLAGS) -Isrc $(FUZZ_CFLAGS) tests/fuzz_frontend.c src/parser.c src/lexer.c -o build/fuzz-frontend

fuzz-smoke: fuzz-frontend
	@mkdir -p build/fuzz-corpus
	cp tests/fuzz-corpus/* build/fuzz-corpus/
	./build/fuzz-frontend -runs=1000 -max_len=4096 build/fuzz-corpus

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test

GUI_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
GUI_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

check-sdl2:
	@pkg-config --exists sdl2 || { echo 'SDL2 development files and pkg-config are required (install libsdl2-dev or brew install sdl2 pkg-config).' >&2; exit 1; }

gui: check-sdl2 build/libylang_gui.a

gui-example: check-sdl2 build/gui-example

game2d-example: check-sdl2 $(BIN)
	@mkdir -p build
	YLANG_PATH=stdlib ./build/ylang build examples/games/dodge.yl -o $(GAME2D_OUTPUT)

build/ylang_gui.o: stdlib/gui/gui.c include/ylang/gui.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GUI_CFLAGS) -c stdlib/gui/gui.c -o $@

build/libylang_gui.a: build/ylang_gui.o
	$(AR) rcs $@ $<

build/gui-example: examples/gui.c build/libylang_gui.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GUI_CFLAGS) $< build/libylang_gui.a $(GUI_LIBS) -o $@

install-gui: gui
	install -d '$(DESTDIR)$(PREFIX)/include/ylang' '$(DESTDIR)$(PREFIX)/lib' '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/gui' '$(DESTDIR)$(PREFIX)/share/ylang/stdlib'
	install -m 644 include/ylang/gui.h '$(DESTDIR)$(PREFIX)/include/ylang/gui.h'
	install -m 644 stdlib/game2d.yl '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/game2d.yl'
	install -m 644 stdlib/gui/gui.c '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/gui/gui.c'
	install -m 644 build/libylang_gui.a '$(DESTDIR)$(PREFIX)/lib/libylang_gui.a'

install: $(BIN)
	install -d '$(DESTDIR)$(BINDIR)'
	install -m 755 $(BIN) '$(DESTDIR)$(BINDIR)/ylang'

uninstall:
	rm -f '$(DESTDIR)$(BINDIR)/ylang'

clean:
	rm -rf build
