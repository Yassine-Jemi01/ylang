
VERSION := $(shell cat VERSION)
CC = gcc
AR ?= ar
PYTHON ?= python3
CPPFLAGS += -Iinclude -Ibuild -DYLANG_VERSION=\"$(VERSION)\" -DYLANG_SHARE_DIR=\"$(PREFIX)/share/ylang\" -DYLANG_INCLUDE_DIR=\"$(PREFIX)/include\"
CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -g3 -O0
LDFLAGS ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))

ifeq ($(OS),Windows_NT)
BIN := build/ylang.exe
GAME2D_OUTPUT ?= build/dodge.exe
else
BIN := build/ylang
GAME2D_OUTPUT ?= build/dodge
endif

.PHONY: all clean test ylanglings-test ylanglings-verify runtime-feature-test sanitize install uninstall install-ylanglings uninstall-ylanglings print-version tui-example gui gui-example game2d-example install-gui voice voice-example voice-test install-voice check-sdl2 check-voice check-runtime-template regen-runtime-template frontend-unit-test fuzz-frontend fuzz-smoke

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/runtime_template.h: runtime/runtime_template.c tools/embed_text.py
	@mkdir -p build
	@$(PYTHON) tools/embed_text.py $< $@

build/codegen.o: build/runtime_template.h

check-runtime-template: build/runtime_template.h
	@$(PYTHON) tools/embed_text.py runtime/runtime_template.c build/runtime_template.h --check

regen-runtime-template:
	@mkdir -p build
	@$(PYTHON) tools/embed_text.py runtime/runtime_template.c build/runtime_template.h

print-version:
	@printf '%s\n' '$(VERSION)'

build/frontend-unit: tests/frontend_unit.c src/parser.c src/lexer.c src/arena.c src/strbuf.c src/diagnostics.c src/string_utils.c src/internal.h include/ylang/compiler.h include/ylang/lexer.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) -Isrc $(CFLAGS) tests/frontend_unit.c src/parser.c src/lexer.c src/arena.c src/strbuf.c src/diagnostics.c src/string_utils.c $(LDFLAGS) -o $@

frontend-unit-test: build/frontend-unit
	./build/frontend-unit

ylanglings-test:
	$(PYTHON) -m unittest discover -s tools/ylanglings/tests -v

ylanglings-verify: $(BIN)
	$(PYTHON) tools/ylanglings/ylanglings.py --compiler ./$(BIN) verify-all

runtime-feature-test: $(BIN)
	$(PYTHON) tests/check_runtime_feature_gates.py --cc "$(CC)"

test: $(BIN) frontend-unit-test ylanglings-test ylanglings-verify runtime-feature-test
	sh tests/run.sh

FUZZ_CC ?= clang
FUZZ_CFLAGS ?= -std=c17 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer

fuzz-frontend:
	@mkdir -p build
	$(FUZZ_CC) $(CPPFLAGS) -Isrc $(FUZZ_CFLAGS) tests/fuzz_frontend.c src/parser.c src/lexer.c src/arena.c src/strbuf.c src/diagnostics.c src/string_utils.c src/sema.c -o build/fuzz-frontend

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

tui-example: $(BIN)
	@mkdir -p build
	./build/ylang build examples/tui-menu.yl -o build/tui-menu

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

VOICE_CFLAGS := $(shell pkg-config --cflags SDL2_mixer 2>/dev/null)
VOICE_LIBS := $(shell pkg-config --libs SDL2_mixer 2>/dev/null)

check-voice:
	@pkg-config --exists SDL2_mixer || { echo 'SDL2_mixer development files and pkg-config are required (Fedora: SDL2_mixer-devel; Debian/Ubuntu: libsdl2-mixer-dev; macOS: brew install sdl2_mixer).' >&2; exit 1; }

voice: check-voice build/libylang_voice.a

voice-example: check-voice $(BIN)
	@mkdir -p build
	./build/ylang build examples/voice.yl -o build/voice-example

build/voice-runtime-test: tests/voice_runtime.c stdlib/voice/voice.c include/ylang/voice.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VOICE_CFLAGS) -Umain tests/voice_runtime.c stdlib/voice/voice.c $(VOICE_LIBS) -o $@

voice-test: check-voice build/voice-runtime-test
	SDL_AUDIODRIVER=dummy ./build/voice-runtime-test

build/ylang_voice.o: stdlib/voice/voice.c include/ylang/voice.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(VOICE_CFLAGS) -c stdlib/voice/voice.c -o $@

build/libylang_voice.a: build/ylang_voice.o
	$(AR) rcs $@ $<

install-voice: voice
	install -d '$(DESTDIR)$(PREFIX)/include/ylang' '$(DESTDIR)$(PREFIX)/lib' '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/voice'
	install -m 644 include/ylang/voice.h '$(DESTDIR)$(PREFIX)/include/ylang/voice.h'
	install -m 644 stdlib/voice/voice.c '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/voice/voice.c'
	install -m 644 stdlib/voice/README.md '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/voice/README.md'
	install -m 644 build/libylang_voice.a '$(DESTDIR)$(PREFIX)/lib/libylang_voice.a'

install-gui: gui
	install -d '$(DESTDIR)$(PREFIX)/include/ylang' '$(DESTDIR)$(PREFIX)/lib' '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/gui' '$(DESTDIR)$(PREFIX)/share/ylang/stdlib'
	install -m 644 include/ylang/gui.h '$(DESTDIR)$(PREFIX)/include/ylang/gui.h'
	install -m 644 stdlib/game2d.yl '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/game2d.yl'
	install -m 644 stdlib/gui/gui.c '$(DESTDIR)$(PREFIX)/share/ylang/stdlib/gui/gui.c'
	install -m 644 build/libylang_gui.a '$(DESTDIR)$(PREFIX)/lib/libylang_gui.a'

install: $(BIN)
	install -d '$(DESTDIR)$(BINDIR)'
	install -m 755 $(BIN) '$(DESTDIR)$(BINDIR)/ylang'

install-ylanglings:
	$(MAKE) -C tools/ylanglings install PREFIX="$(PREFIX)" BINDIR="$(BINDIR)" DATADIR="$(PREFIX)/share/ylanglings" PYTHON="$(PYTHON)"

uninstall-ylanglings:
	$(MAKE) -C tools/ylanglings uninstall PREFIX="$(PREFIX)" BINDIR="$(BINDIR)" DATADIR="$(PREFIX)/share/ylanglings"

uninstall:
	rm -f '$(DESTDIR)$(BINDIR)/ylang'

clean:
	rm -rf build
