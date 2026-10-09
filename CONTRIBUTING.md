# Contributing to YLang

Thanks for considering a contribution.

## Before opening a change

1. Search existing issues so duplicate work can be avoided.
2. For language changes, discuss the syntax and semantics before implementing them.
3. Keep changes focused and explain user-visible behavior in the pull request.

## Development setup

Requirements: a C17 compiler, `make`, and GCC or Clang for compiling generated C.

```sh
make clean
make
make test
```

Optional extra checks:

```sh
make CC=clang clean all test
make sanitize
```

## Pull request checklist

- [ ] The change has a focused explanation and appropriate tests.
- [ ] `make clean && make && make test` passes.
- [ ] Any syntax/semantic behavior change updates `docs/language-spec.md`.
- [ ] User-facing diagnostic changes include expected-message regression tests.
- [ ] Security and memory-management implications are documented.
- [ ] No generated binaries or unrelated build outputs are committed.

## Language compatibility

YLang 1.0.0 defines the supported syntax documented in `docs/language-spec.md`. Changes that break this behavior require an explicit compatibility discussion and should not be merged as a silent patch-level change.
