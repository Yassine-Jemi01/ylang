# std.tui — Terminal UI

YLang includes a small synchronous TUI runtime for interactive terminal programs. It provides raw key input, dimensions, cursor movement, screen clearing, ANSI output, and a portable sleep function.

## Example

```yl
function main() -> int {
    if (!tui.init()) {
        print(tui.last_error());
        return 1;
    }

    tui.hide_cursor();
    tui.clear();
    tui.write("YTilawa\n");
    tui.write("Use the arrow keys; press Enter to exit.\n");

    let bool running = true;
    while (running) {
        let int key = tui.read_key();
        if (key == 13 || key == 27) {
            running = false;
        } else if (key == 1001) {
            tui.write("Up\n");
        } else if (key == 1002) {
            tui.write("Down\n");
        }
    }

    tui.show_cursor();
    tui.shutdown();
    return 0;
}
```

## API

See [the std.tui reference](../../../stdlib/tui/README.md) for all functions and key codes.

| Function | Return | Behavior |
|---|---|---|
| `tui.init()` | `bool` | Enable raw input and disable echo |
| `tui.shutdown()` | `void` | Restore terminal input mode |
| `tui.read_key()` | `int` | Read one key or supported navigation key |
| `tui.width()` / `tui.height()` | `int` | Read current terminal dimensions |
| `tui.clear()` | `bool` | Clear and home |
| `tui.write(text)` | `bool` | Write text or ANSI escape sequences |
| `tui.set_cursor(column, row)` | `bool` | Move cursor (1-based) |
| `tui.hide_cursor()` / `tui.show_cursor()` | `bool` | Set cursor visibility |
| `tui.sleep_ms(ms)` | `bool` | Sleep without busy-waiting |
| `tui.last_error()` | `string` | Last terminal error |

Navigation key codes are Up 1001, Down 1002, Right 1003, Left 1004, Home 1005, End 1006, Delete 1007, Page Up 1008, and Page Down 1009. Printable keys return their byte value; Enter is 13 and Escape is 27.

## Limitations

The first version is synchronous and single-key oriented. It does not yet provide mouse events, key-release events, Unicode grapheme editing, terminal resize events, or a full widget/layout system. Re-query dimensions during the event loop. The C backend is supported; the experimental NASM backend rejects TUI calls. POSIX raw-key behavior is covered by a pseudo-terminal integration test. Windows raw input still needs dedicated interactive CI coverage.
