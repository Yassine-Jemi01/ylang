# YLang Standard Library Plan

**Status:** Initial `read_line()`, `parse_int()`, `parse_float()`, `len()`, `clone()`, and array built-ins are implemented on `dev/lsp-foundation`. The complete module API below remains a proposal; each API becomes supported only after tests exist on Linux and Windows.

## Compatibility rule

Portable APIs have the same behavior and error semantics on supported platforms. Platform-specific behavior is explicit in module names or documented platform conditions. No module should expose C pointers or platform handles in safe user code.

## Proposed modules

| Module | Initial responsibilities | Important contract |
|---|---|---|
| `std.io` | `print` and `read_line()` built-ins (dev branch) | `read_line()` removes the line ending; EOF before any bytes and I/O errors are runtime errors in this initial API |
| `std.string` | `len(string)`, `clone(string)`, `parse_int(string)`, and `parse_float(string)` built-ins (dev branch) | `len` counts UTF-8 bytes; numeric parse failures terminate with a runtime error; other operations remain planned |
| `std.array` | typed one-dimensional arrays, checked indexing, `len`, `append`, `clone` (dev branch); capacity API planned | Bounds checks, allocation failure, and ownership are defined |
| `std.math` | common math operations/constants | Domain errors and floating-point behavior are documented |
| `std.fs` | read/write files, metadata, directory iteration | Handles close deterministically; permissions and errors are preserved |
| `std.path` | join, normalize, extension, filename | Uses platform-neutral path values and correct Windows path semantics |
| `std.process` | args, environment, exit, spawn | Argument passing avoids shell-string concatenation by default |
| `std.time` | monotonic clock, durations | Monotonic time is used for elapsed durations |
| `std.test` | assertions, test discovery, reports | Stable nonzero exit status on failure and machine-readable output |
| `std.net` | TCP/UDP and address parsing | Deferred until resource ownership and typed I/O errors are ready |

## Third-party dependency policy

The core compiler should remain small. Reuse mature open-source projects for components where doing so reduces risk, but verify actual build compatibility and licensing before adding them. Candidate categories:

- LLVM: optional native code-generation/optimization backend after a Windows/Linux prototype.
- Tree-sitter: editor parsing/highlighting; not a replacement for the compiler parser.
- A Unicode library only if built-in UTF-8 routines prove insufficient; choose a maintained library with an acceptable license.
- A tested TLS/networking library only when network modules are approved; do not implement cryptography from scratch.
- Existing test/fuzz tooling from the host toolchain rather than embedding a new framework without need.

For each dependency, record the upstream URL, pinned version, license, platforms tested, update owner/process, and known security advisories. A library being open source does not by itself prove it is secure or compatible.
