# The Standard Library

YLang's standard library provides small built-in namespaces for common tasks. The available surface depends on the current documented language subset; use the reference pages rather than assuming that a similarly named function exists in another language.

The current documentation covers namespaces such as:

- `math` for supported mathematical operations.
- `string` for string inspection and explicit parsing/conversion helpers.
- `io` for console and file-related operations.
- `path` for path-related checks.
- `image` for supported image-related operations.
- `env.get(name)` for reading a process environment variable without launching a shell.
- `http` for optional synchronous GET/POST requests through libcurl; see [HTTP and API Requests](reference/HTTP.md) for dependency requirements and security defaults.

The [Language Specification](reference/language-spec.md) documents the stable language subset and conversion rules. HTTP and environment helpers are new experimental additions and are outside the 2.0.0 stable-core compatibility guarantee. The [Compiler Error Codes](reference/error-codes.md) page includes diagnostics for unknown standard-library namespaces. GUI and game helpers are optional and have their own SDL2 requirements; start with [GUI and Game2D](gui-game2d.md).

Do not assume every standard-library call is supported by the experimental NASM backend. The C backend remains the broad-coverage default.
