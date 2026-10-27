# YLang 2.2.0 Roadmap

**Status: planning document only. This is not a release announcement or a promise that any listed feature is already implemented.**

YLang 2.1.x should focus on fixes, correctness, compatibility, tests, and completion of the current release candidate. The 2.2 line is intended to expand what developers can build with YLang and improve the developer experience.

## Scope: five feature areas

### 1. Asynchronous programming

Provide a deliberate async model for work that waits on I/O, especially HTTP requests.

Acceptance goals:
- Document the execution model before freezing syntax: scheduling, suspension/resumption, cancellation, and shutdown behavior must be explicit.
- Define how errors and return values move through asynchronous calls.
- Ensure blocking operations are not advertised as non-blocking.
- Add tests for successful completion, failures, timeouts, cancellation, and repeated use.
- Preserve existing synchronous programs and the default build path.

A first async I/O slice now exists on this development branch: `http.request_async(...)` returns an `http_task`, `await task` consumes it to return the response body, and `http.cancel(task)` requests cancellation. It uses libcurl's multi interface so queued transfers progress concurrently while a task is awaited. General-purpose async functions, a background scheduler, and GUI/event-loop integration remain unimplemented; the broader acceptance goal is only partially complete.

### 2. HTTP and API requests

Provide a standard, typed way to call HTTP APIs from YLang. A first useful milestone should cover:
- GET and POST, with a clear path to other common methods.
- Request headers, request bodies, response status, response headers, and response bodies.
- JSON request/response interoperability.
- Connection and overall timeouts, bounded response sizes, and useful error categories.
- HTTPS certificate verification enabled by default.
- Documented redirect behavior.
- Examples that read credentials from environment/configuration rather than hard-coding secrets.

The API may work synchronously first if needed, with async integration designed as part of feature 1. Never embed API credentials in the compiler or examples.

### 3. YLang Debugger

Build source-level debugging around YLang files rather than asking users to debug generated C manually.

Initial capability goals:
- Debug build mode with symbols and settings suitable for stepping.
- Breakpoints, continue, step over, step into, and step out.
- Show locations in the original `.yl` source and display a call stack.
- Inspect local variables where generated-code mapping supports it.
- Report a clear diagnostic when a value or source mapping cannot be inspected.
- Integrate debugging into the YLang VS Code extension.
- Start with the established C backend; do not imply NASM support until debug information and behavior are tested.

The implementation may reuse GDB/LLDB as the native debugging engine, but should hide generated-C details as much as practical. Source mapping, generated C naming, launch behavior, supported platforms, and debugger dependencies need a separate design and acceptance tests before a stable interface is promised.

### 4. Performance and security improvements

Treat speed and security as measurable engineering work, not an unqualified marketing guarantee.

Acceptance goals:
- Keep representative benchmarks for compiler time, executable runtime, and binary size.
- Compare changes against a recorded baseline on the same machine and toolchain.
- Add regression tests for arithmetic overflow, array bounds, error paths, ownership cleanup, and backend parity as appropriate.
- Run the existing test suite and sanitizers; extend front-end fuzzing where practical.
- Review process invocation, file handling, network behavior, and secret handling for unsafe defaults.
- Publish only reproducible results and document remaining memory-safety limitations.

### 5. VS Code onboarding and developer experience

Make it straightforward to discover, install, and use the official YLang extension.

Acceptance goals:
- Publish and maintain the extension on the Visual Studio Marketplace with the `.yl` language contribution and clear installation instructions.
- Verify that VS Code can recommend the Marketplace extension when opening an unrecognized `.yl` file. Dynamic file-type recommendations depend on the extension being available from the Marketplace.
- Keep the extension ID stable and document manual installation for offline users.
- Provide clear compiler-path setup, diagnostics, run/check commands, and debugger integration.
- Test the extension package and supported VS Code versions before release.

A workspace recommendation helps people who open the YLang repository, but does not replace Marketplace discovery when someone opens an arbitrary `.yl` file outside that repository.

## Brand and website refresh

The new YLang identity and redesigned official website are companion launch work, not a sixth language feature. Keep the language name and `.yl` identity recognizable while introducing consistent logo, typography, colors, icon assets, installation entry points, and documentation navigation.

Website acceptance goals:
- The home page, book, editor extension, and repository use consistent approved brand assets.
- The getting-started path makes compiler and editor setup easy to find.
- Existing technical guidance remains accurate; styling changes must not silently remove or rewrite documentation.
- Links, navigation, mobile layouts, and code examples are checked before launch.

## Suggested order

1. Finish the 2.1.x correctness and release acceptance work.
2. Design async execution and HTTP APIs together, then implement the smallest coherent vertical slice.
3. Design source mapping and the debugger interface before implementing the VS Code debugger flow.
4. Run performance/security work continuously, not only at the end.
5. Complete Marketplace onboarding, identity, website, compatibility checks, and release notes before publishing 2.2.0.

## Release gates

Do not label 2.2.0 stable until:
- Required test, sanitizer, and relevant platform CI jobs pass on the exact release commit.
- New syntax and standard-library APIs have reference documentation and runnable examples.
- Existing programs covered by the compatibility policy continue to build and behave as documented.
- HTTP security defaults and timeout/error behavior are tested.
- Debugger limitations and supported platforms are explicit.
- VS Code packaging and Marketplace instructions are verified.
- Website links, assets, and installation guidance are checked.

The feature list and order may change based on implementation findings. The HTTP task API is only the first async slice; do not mark broader asynchronous programming complete until general async functions, cancellation/error behavior, and event-loop integration have been separately designed and tested. Do not describe a future feature as implemented until its acceptance checks pass.