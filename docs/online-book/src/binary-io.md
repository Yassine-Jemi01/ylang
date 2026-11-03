# Binary I/O

YLang's experimental binary I/O helpers work with raw file bytes without passing binary data through the text-string reader. The current API is deliberately small and portable.

## API

| Function | Return | Behavior |
|---|---|---|
| `io.binary_size(path)` | `int` | File size in bytes, or `-1` on failure. |
| `io.read_u8(path, offset)` | `int` | Read one unsigned byte (0–255), or `-1` on failure/EOF. |
| `io.write_u8(path, offset, value)` | `bool` | Write one byte at an absolute offset; value must be 0–255. |
| `io.read_binary_hex(path, offset, length)` | `owned_string` | Read up to `length` bytes and return lowercase hexadecimal text. |
| `io.write_binary_hex(path, offset, hex)` | `bool` | Decode an even-length hexadecimal string and write those bytes at an absolute offset. |

Offsets and lengths are zero-based byte counts. A write at an offset beyond the current end may create a gap according to the host filesystem. Writes overwrite existing bytes but do not truncate trailing bytes. The hex read is capped at 16 MiB per call; a read that reaches EOF returns the bytes that were available. Invalid paths, offsets, byte values, and malformed hex payloads fail without being treated as text.

## Example

```ylang
function main() -> int {
    let string path = "header.bin";

    if (!io.write_binary_hex(path, 0, "594c414e4700ff")) {
        print("Could not write binary header.");
        return 1;
    }

    print("Size:", io.binary_size(path));
    print("Magic bytes:", io.read_binary_hex(path, 0, 4));
    print("First byte:", io.read_u8(path, 0));

    if (!io.write_u8(path, 4, 50)) {
        print("Could not update byte.");
        return 1;
    }

    print("Updated:", io.read_binary_hex(path, 0, 7));
    return 0;
}
```

## Important limitations

- Hex encoding is an interoperability convenience, not a zero-copy byte buffer. It uses twice as much text storage as the underlying byte count.
- This initial API does not expose byte arrays, memory-mapped files, streaming handles, or typed endian-aware integer readers/writers.
- The runtime uses portable C stdio seek operations, so offsets must fit the platform's `long` file-offset range.
- Binary I/O is synchronous and currently belongs to the experimental standard-library surface. Check every boolean or sentinel result.
- The existing `io.read_file` and `io.write_file` APIs remain text APIs; `io.read_file` still rejects embedded NUL bytes.
