.RECIPEPREFIX := >

VERSION := $(shell cat VERSION)
CC = gcc
CPPFLAGS += -Iinclude -DYLANG_VERSION=\"$(VERSION)\"
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O0
LDFLAGS ?=
UNAME_S := $(shell uname -s 2>/dev/null)
EXEEXT ?=
ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(UNAME_S)))
EXEEXT := .exe
endif

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
BIN := build/ylang$(EXEEXT)

.PHONY: all clean test sanitize install uninstall print-version

all: $(BIN)

$(BIN): $(OBJ)
>$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

build/%.o: src/%.c
>@mkdir -p build
>$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

print-version:
>@printf '%s\n' '$(VERSION)'

test: $(BIN)
>YLANG_EXEEXT='$(EXEEXT)' sh tests/run.sh

sanitize:
>$(MAKE) clean
>$(MAKE) CFLAGS='-std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test

install: $(BIN)
>install -d '$(DESTDIR)$(BINDIR)'
>install -m 755 $(BIN) '$(DESTDIR)$(BINDIR)/ylang$(EXEEXT)'

uninstall:
>rm -f '$(DESTDIR)$(BINDIR)/ylang$(EXEEXT)'

clean:
>rm -rf build
