# Binary I/O

YLang's experimental Binary I/O API operates on raw bytes without passing binary data through text strings. It includes fixed byte arrays, block transfers, endian-aware scalar access, and synchronous streaming handles.

## Byte values and arrays

The `byte` type stores an unsigned 8-bit value (0–255). Use `byte(value)` for a checked conversion from `int`; values outside that range cause a runtime error.

```ylang
let byte header[] = [byte(0x59), byte(0x4c), byte(0x41), byte(0x4e)];
print(header[0]);
```

Byte buffers use YLang's existing array syntax. Buffer APIs accept a named `byte[]` local or array parameter; parameter lengths are passed automatically.

## File API

| Function | Return | Behavior |
|---|---|---|
| `io.binary_size(path)` | `int` | File size in bytes, or `-1` on failure. |
| `io.read_u8(path, offset)` | `int` | Read one byte (0–255), or `-1` on failure/EOF. |
| `io.write_u8(path, offset, value)` | `bool` | Write one byte at an absolute offset. |
| `io.read_bytes(path, offset, buffer)` | `int` | Fill a byte array and return bytes read; `-1` on error. |
| `io.write_bytes(path, offset, buffer)` | `int` | Write a byte array and return bytes written; `-1` on error. |
| `io.read_binary_hex(path, offset, length)` | `owned_string` | Read bytes and return lowercase hexadecimal text. |
| `io.write_binary_hex(path, offset, hex)` | `bool` | Decode valid even-length hex and write bytes. |

Offsets are zero-based byte counts. Block reads return fewer bytes at EOF. Writes overwrite existing bytes and do not truncate trailing contents. A block write creates a file when it does not exist. Hex reads and writes are limited to 16 MiB per call; use byte buffers or streams for larger data.

## Dynamic byte buffers

Use the opaque integer handle API when the amount of data is not known in advance. Buffers grow automatically and are limited to 256 MiB each; at most 64 may be open at once.

| Function | Return | Behavior |
|---|---|---|
| `io.buffer_new(initial_capacity)` | `int` | Create an empty growable buffer; returns a handle or `-1`. |
| `io.buffer_slice(handle, offset, length)` | `int` | Create a non-owning view over a range of an existing buffer; returns a handle or `-1`. |
| `io.buffer_length(handle)` | `int` | Current number of bytes, or `-1` for an invalid handle. |
| `io.buffer_capacity(handle)` | `int` | Allocated capacity (slice extent for a view), or `-1`. |
| `io.buffer_get(handle, index)` | `int` | Read a byte (0–255), or `-1` for an invalid index/handle. |
| `io.buffer_set(handle, index, value)` | `bool` | Replace an existing byte; value must be 0–255. |
| `io.buffer_push(handle, value)` | `bool` | Append a byte and grow capacity when required. |
| `io.buffer_resize(handle, length)` | `bool` | Change length; newly added bytes are zero-filled. |
| `io.buffer_clear(handle)` | `bool` | Set length to zero while retaining capacity. |
| `io.buffer_free(handle)` | `bool` | Release the buffer and invalidate its handle. |
| `io.buffer_load(path, offset, length)` | `int` | Load up to length bytes into a new buffer; returns its handle or `-1`. |
| `io.buffer_save(path, offset, handle)` | `int` | Write buffer contents at a file offset and return bytes written, or `-1`. |
| `io.buffer_append_file(path, handle)` | `int` | Append raw buffer bytes to a file and return bytes written, or `-1`. |

Example:

```ylang
let int packet = io.buffer_new(16);
io.buffer_push(packet, 0x59);
io.buffer_push(packet, 0x4c);
io.buffer_push(packet, 0xff);
print(io.buffer_length(packet));
io.buffer_save("packet.bin", 0, packet);
io.buffer_free(packet);
```

A slice shares its parent's storage: changing a byte through either handle is visible through the other. Slices may be nested. A slice cannot be resized, cleared, or appended to; it can be read, modified, and saved. Freeing the parent invalidates its slices, and shrinking a parent makes out-of-range slices temporarily inaccessible. Free slice handles explicitly. The runtime releases any buffers not explicitly freed during normal program cleanup. Handles are process-local and must not be persisted.

## Endian-aware integers

The API provides `io.read_u16_le/be(path, offset)`, `io.read_u32_le/be(path, offset)`, and `io.read_u64_le/be(path, offset)`, plus matching `io.write_u16_le/be(path, offset, value)`, `io.write_u32_le/be(...)`, and `io.write_u64_le/be(...)` functions.

- `le` means little-endian; `be` means big-endian.
- 16-bit and 32-bit reads return nonnegative values in `int`.
- 64-bit reads return the raw 64-bit pattern represented by the signed `int` type. Values with the high bit set therefore appear negative.
- Scalar reads return `-1` on failure; for 64-bit values, `-1` can also be the valid all-bits-set pattern. Check `io.binary_size` when distinguishing that case matters.
- Writes accept an `int` and write its low 16, 32, or 64 bits.

## Streaming large files

Streaming avoids loading an entire file into memory. Handles are integer IDs managed by the runtime.

```ylang
function main() -> int {
    let int input = io.stream_open_read("large.dat");
    if (input < 0) return 1;

    let byte chunk[] = [byte(0), byte(0), byte(0), byte(0)];
    let int count = io.stream_read(input, chunk);
    while (count > 0) {
        // Process chunk[0] through chunk[count - 1].
        count = io.stream_read(input, chunk);
    }

    if (!io.stream_close(input)) return 1;
    return 0;
}
```

- `io.stream_open_read(path)` opens a file for reading.
- `io.stream_open_write(path)` creates or truncates a file for writing.
- `io.stream_read(handle, buffer)` returns the number of bytes read, zero at EOF, or `-1` on error.
- `io.stream_write(handle, buffer)` returns the number of bytes written, or `-1` on error.
- `io.stream_close(handle)` closes a handle. The runtime also closes any still-open streams during normal program cleanup.
- At most 64 streams may be open simultaneously. Invalid or already-closed handles fail.

## Complete example

```ylang
function main() -> int {
    let string path = "header.bin";
    let byte data[] = [byte(0x59), byte(0x4c), byte(0x41), byte(0x4e)];

    if (io.write_bytes(path, 0, data) != 4) return 1;
    print(io.binary_size(path));
    print(io.read_u32_be(path, 0));
    return 0;
}
```

## Limitations

- Byte arrays are fixed-size. Use the dynamic `io.buffer_*` API when a growable byte buffer or a non-owning slice is required.
- Streaming is synchronous and does not yet expose seek, flush, append, or async operations.
- Runtime file offsets use portable C stdio seek operations and must fit the platform's `long` range.
- Binary I/O is experimental and currently supported by the C backend. The NASM backend reports a clear unsupported-type/API diagnostic.
- The existing `io.read_file` and `io.write_file` functions remain text APIs.
