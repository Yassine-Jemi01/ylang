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

This first milestone provides low-level terminal primitives and a small set of drawing helpers. It is not yet a retained widget framework: there are no automatic layout containers, focus management, mouse events, text-input widgets, or full key-event objects. Those can be layered on after the runtime and input contracts are stable.
