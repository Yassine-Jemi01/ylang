# YLang 2.1.0 — Release Candidate Notes

**Status: candidate preparation only. This is not a published release.**

YLang 2.1.0 is intended to introduce an experimental ownership milestone while preserving the existing legacy `string` behavior and the documented 2.0 stable-core contract.

## Experimental ownership additions

- `owned_string` represents a tracked, owned string allocation in the experimental C backend.
- `string.own(text)` creates an owned copy.
- `move(name)` explicitly transfers ownership and makes the source binding unavailable until it is reinitialized.
- Owned-string helpers include `concat`, `replace`, `upper`, and `lower`; owned input helpers include `io.read_line_owned` and `io.read_file_owned`.
- Generated cleanup is emitted for owned locals at normal scope exit and before `return`, `break`, and `continue`.
- `borrowed_string` is available as a restricted immutable function-parameter view.

## Important limitations

This milestone is **not** a complete Rust-like ownership or borrowing system.

- These ownership features currently require the C backend.
- Owned globals, arrays of owned strings, and owned-string struct fields remain unsupported.
- `borrowed_string` is restricted to immutable parameters; borrowed returns and local/global/field storage are not supported.
- General `&T` and `&mut T` syntax, full lifetime analysis, and general alias/conflicting-borrow analysis are not implemented.
- The existing `string` type retains its legacy allocation behavior and is not automatically made memory-safe by this change.
- YLang does not claim Rust-level memory safety or provide a security sandbox.

## Validation required before publication

Do not create the `v2.1.0` tag or publish a GitHub Release until the candidate commit has passed the repository's required release checks, including clean GCC/Clang builds, regression tests, sanitizers, native Windows smoke tests, editor diagnostics, website/book integrity, and YSPM package validation. Review the actual workflow results for the exact commit; a successful earlier PR run is not a substitute for testing this release candidate.

## Upgrade guidance

Existing programs using `string` do not need to migrate to `owned_string`. Treat the new ownership APIs as experimental and opt in only where the documented restrictions are satisfied. Do not assume that adding `owned_string` makes all allocations in a program automatically reclaimed.
