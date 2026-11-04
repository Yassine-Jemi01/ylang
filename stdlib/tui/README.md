# std.tui — Terminal UI runtime

`std.tui` provides synchronous raw-key terminal input and ANSI terminal control for interactive command-line applications. Its functions are compiler-recognized builtins; no external TUI dependency is required.

## API

| Function | Return | Purpose |
|---|---|---|
| `tui.init()` | `bool` | Enable raw input and disable terminal echo |
| `tui.shutdown()` | `void` | Restore the terminal's saved input mode |
| `tui.read_key()` | `int` | Block until a key or supported escape sequence is read |
| `tui.poll_key()` | `int` | Return a key immediately, or `-1` when no key is available |
| `tui.width()` / `tui.height()` | `int` | Query current terminal dimensions |
| `tui.clear()` | `bool` | Clear the screen and move the cursor home |
| `tui.write(text)` | `bool` | Buffer text/ANSI sequences for efficient frame output |
| `tui.flush()` | `bool` | Flush buffered terminal output immediately |
| `tui.set_cursor(column, row)` | `bool` | Move the cursor using 1-based coordinates |
| `tui.hide_cursor()` / `tui.show_cursor()` | `bool` | Control cursor visibility |
| `tui.sleep_ms(milliseconds)` | `bool` | Sleep without busy-waiting |
| `tui.last_error()` | `string` | Return the most recent TUI error |

## Key codes

Printable keys return their byte value. Common controls return Enter as 13, Escape as 27, and Backspace as the terminal's byte value. Special navigation keys use stable values:

- Up 1001, Down 1002, Right 1003, Left 1004
- Home 1005, End 1006, Delete 1007
- Page Up 1008, Page Down 1009

Unknown escape sequences return Escape (27). The current implementation does not provide key-release events, mouse input, or a terminal-resize event queue; query dimensions during the application loop.

## Resource safety

Call `tui.init()` before entering the UI loop and always call `tui.shutdown()` before normal exit. The runtime also registers a process-exit restoration handler, so a normal process exit restores the saved terminal mode. Applications should still use a cleanup path for errors and user-requested exits.

The API is synchronous. `tui.read_key()` blocks until input arrives; `tui.poll_key()` is nonblocking and returns `-1` when idle. Pending output is flushed before either key-reading function waits or polls, so applications can compose a frame from many `tui.write()` calls without forcing a terminal flush for every fragment. Call `tui.flush()` when output must appear before the next input operation. Escape-prefixed navigation sequences are given a short completion window so split terminal writes are not misread as ordinary characters. On POSIX, `std.tui` temporarily disables terminal signal handling so Ctrl+C is delivered as key code 3; the application can then shut down the UI and restore terminal state cleanly. Use `tui.sleep_ms()` between event-loop polls to avoid busy loops.

The C backend is supported. The experimental NASM backend rejects raw TUI calls. POSIX raw-key behavior has a pseudo-terminal integration test; Windows console input is implemented but still requires a dedicated interactive Windows test before claiming parity.
