# std.tui — Terminal UI

std.tui is a built-in, dependency-free terminal UI namespace for programs compiled with YLang's C backend. It uses ANSI/VT terminal sequences and native console input; it does not require SDL2, ncurses, or a separate package.

## Minimal example

```ylang
function main() -> int {
    if (not tui.init()) {
        print("Could not initialize terminal mode");
        return 1;
    }

    tui.clear();
    tui.set_color(39, 16);
    tui.draw_box(2, 2, 42, 9, "YLang TUI");
    tui.move_to(4, 4);
    tui.println("Welcome to std.tui");
    tui.move_to(4, 6);
    tui.write("Loading ");
    tui.progress_bar(13, 6, 24, 7, 10);
    tui.reset_style();

    while (not tui.key_available()) {
        tui.sleep_ms(10);
    }
    let int key = tui.read_key();
    tui.shutdown();
    return 0;
}
```

Always call tui.shutdown() before leaving the application so the terminal's cursor, screen buffer, and input mode are restored.

## YTilawa starter

A keyboard-driven player shell is available at `examples/tilawa-player-tui.yl`. It demonstrates a now-playing panel, playback progress, a selectable menu, Up/Down navigation, Enter actions, and Q/Escape to quit. It is a UI shell only; connect its actions to YLang's audio engine and Quran/reciter data layer.

## API

| Function | Return | Description |
| --- | --- | --- |
| tui.init() | bool | Enter alternate screen, hide cursor, and enable raw keyboard input. |
| tui.shutdown() | void | Restore terminal state. Safe to call more than once. |
| tui.clear() | void | Clear the screen and move the cursor to the top-left. |
| tui.width(), tui.height() | int | Current terminal dimensions; fallback is 80×24 if unavailable. |
| tui.move_to(x, y) | void | Move to a 1-based column and row. |
| tui.write(text) | void | Write text without a newline. |
| tui.println(text) | void | Write text followed by a newline. |
| tui.key_available() | bool | Check whether a key can be read without blocking. |
| tui.read_line(prompt, maximum) | string | Read an ASCII line into a bounded internal buffer. Enter accepts; Escape cancels and returns an empty string. |
| tui.read_key() | int | Read one key byte, or return -1 when no key is waiting. |
| tui.set_color(fg, bg) | bool | Set 256-color foreground and background (0–255). |
| tui.reset_style() | void | Reset ANSI text styling. |
| tui.hide_cursor(), tui.show_cursor() | void | Control cursor visibility. |
| tui.draw_box(x, y, width, height, title) | bool | Draw an ASCII bordered box. Coordinates are 1-based. |
| tui.progress_bar(x, y, width, value, maximum) | bool | Draw a bounded ASCII progress bar. maximum must be positive. |
| tui.sleep_ms(milliseconds) | void | Pause the current thread for a non-negative duration. |

### Keyboard values

tui.read_key() returns ASCII values for ordinary keys. Enter is 13, Escape is 27, and -1 means no key is ready. On Windows, arrow keys map to 1001 (up), 1002 (down), 1003 (left), and 1004 (right). POSIX terminals currently return the first byte of an escape sequence, so full arrow-key event decoding is not yet available there.

## Backend and platform notes

- The C backend is the supported backend for tui.*.
- The experimental NASM backend rejects TUI calls with an explicit diagnostic.
- No SDL2, ncurses, or pkg-config dependency is needed.
- Linux and macOS use POSIX terminal APIs. Windows uses the native console APIs and enables virtual-terminal output when the console supports it.
- When stdin is not a terminal, initialization still allows non-interactive output, but raw key input is not available.
- tui.read_key() is non-blocking. Use tui.key_available() before reading, and tui.sleep_ms() in polling loops to avoid busy-waiting.
- Always shut down on normal exit. A forced process termination cannot guarantee terminal restoration.

## Current limits

This is a terminal rendering and input foundation, not yet a retained widget framework. Applications can implement menus, playback controls, status panels, progress indicators, and keyboard navigation using the primitives above. Automatic layout containers, built-in focus management, mouse events, Unicode-aware cell-width measurement, and a managed text-input widget are not included yet. For basic ASCII text entry, use read_line(prompt, maximum). It supports Backspace, Enter, and Escape; the returned string uses an internal reusable buffer and must be copied if it needs to outlive the next read_line() call. Unicode editing and cursor movement inside the line are not implemented yet. The library currently treats strings as terminal output: sanitize untrusted text before passing it to tui.write(), tui.println(), or draw_box() because embedded terminal escape sequences can affect terminal state.
