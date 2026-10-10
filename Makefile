.RECIPEPREFIX := >

VERSION := $(shell cat VERSION)
CC = gcc
CPPFLAGS += -Iinclude -DYLANG_VERSION=\"$(VERSION)\"
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O0
LDFLAGS ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
BIN := build/ylang

.PHONY: all clean test sanitize install uninstall print-version gui gui-example install-gui check-sdl2

all: $(BIN)

$(BIN): $(OBJ)
>$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

build/%.o: src/%.c
>@mkdir -p build
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

print-version:
>@printf '%s\n' '$(VERSION)'

test: $(BIN)
>sh tests/run.sh

sanitize:
>$(MAKE) clean
>$(MAKE) CFLAGS='-std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test

GUI_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
GUI_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)

check-sdl2:
>@pkg-config --exists sdl2 || { echo 'SDL2 development files and pkg-config are required (install libsdl2-dev or brew install sdl2 pkg-config).' >&2; exit 1; }

gui: check-sdl2 build/libylang_gui.a

gui-example: check-sdl2 build/gui-example

build/ylang_gui.o: stdlib/gui/gui.c include/ylang/gui.h
>@mkdir -p build
>$(CC) $(CPPFLAGS) $(CFLAGS) $(GUI_CFLAGS) -c stdlib/gui/gui.c -o $@

build/libylang_gui.a: build/ylang_gui.o
>$(AR) rcs $@ $<

build/gui-example: examples/gui.c build/libylang_gui.a
>$(CC) $(CPPFLAGS) $(CFLAGS) $(GUI_CFLAGS) $< build/libylang_gui.a $(GUI_LIBS) -o $@

install-gui: gui
>install -d '$(DESTDIR)$(PREFIX)/include/ylang' '$(DESTDIR)$(PREFIX)/lib'
>install -m 644 include/ylang/gui.h '$(DESTDIR)$(PREFIX)/include/ylang/gui.h'
>install -m 644 build/libylang_gui.a '$(DESTDIR)$(PREFIX)/lib/libylang_gui.a'

install: $(BIN)
>install -d '$(DESTDIR)$(BINDIR)'
>install -m 755 $(BIN) '$(DESTDIR)$(BINDIR)/ylang'

uninstall:
>rm -f '$(DESTDIR)$(BINDIR)/ylang' '$(DESTDIR)$(PREFIX)/include/ylang/gui.h' '$(DESTDIR)$(PREFIX)/lib/libylang_gui.a'

clean:
>rm -rf build
