# std.terminal — Terminal input and output

YLang provides a small synchronous terminal API through the built-in `terminal.*` namespace. It is intended for command-line applications and does not require an external package.

## Example

```yl
function main() -> int {
    let owned_string name = terminal.read_line("Name: ");
    terminal.write_line(f"Hello, {name}");

    let int age = terminal.read_int("Age: ");
    let float score = terminal.read_float("Score: ");
    let bool confirmed = terminal.read_bool("Continue? (yes/no): ");

    print(age, score, confirmed);
    return 0;
}
```

## API

| Function | Return | Behavior |
|---|---|---|
| `terminal.read_line(prompt)` | `owned_string` | Print an optional prompt, flush stdout, and read one line from stdin |
| `terminal.read_int(prompt)` | `int` | Read and parse a signed 64-bit decimal integer |
| `terminal.read_float(prompt)` | `float` | Read and parse a finite floating-point value |
| `terminal.read_bool(prompt)` | `bool` | Accept `true`, `false`, `yes`, `no`, `1`, or `0` |
| `terminal.write(text)` | `bool` | Write text without a newline and flush stdout |
| `terminal.write_line(text)` | `bool` | Write text followed by a newline and flush stdout |
| `terminal.clear()` | `bool` | Emit the ANSI clear-screen and home-cursor sequence |
| `terminal.is_interactive()` | `bool` | Check whether stdin is attached to a terminal |

## Behavior and limits

- Input is line-oriented and synchronous. `read_line` removes the line ending and supports arbitrarily long lines subject to available memory.
- EOF in `read_line` returns an empty string, matching the existing `io.read_line` behavior.
- Invalid numeric or boolean input terminates the program with a descriptive runtime error (status 70). Integer overflow and non-finite floats are rejected.
- Prompts are written to stdout and flushed before waiting for input.
- `is_interactive` checks stdin; redirected input and pipes return false.
- `clear` emits ANSI escape sequences. It does not attempt terminal capability detection.
- The `terminal.*` API is line-oriented. For raw key events, terminal dimensions, cursor movement, ANSI output, and interactive screen control, use the separate [std.tui runtime](../stdlib/tui/README.md).
- The C backend is supported. The experimental NASM backend does not implement standard-library terminal calls.
