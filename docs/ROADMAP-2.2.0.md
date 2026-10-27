# YLang 2.2.0 Release Scope and Follow-up Roadmap

**Status: YLang 2.2.0 is released. This document records the shipped scope and follow-up work; incomplete goals below are not claims that those features shipped in 2.2.0.**

YLang 2.2.0 expands the toolchain with an initial HTTP/async-task API, environment-variable access, source-aware command-line debugging, and developer-tooling improvements. The documented core language and default C-backend workflow remain the compatibility foundation; the new APIs and integrations retain their stated experimental limits.

## Scope: five feature areas

### 1. Asynchronous programming

Provide a deliberate async model for work that waits on I/O, especially HTTP requests.

Acceptance goals:
- Document the execution model before freezing syntax: scheduling, suspension/resumption, cancellation, and shutdown behavior must be explicit.
- Define how errors and return values move through asynchronous calls.
- Ensure blocking operations are not advertised as non-blocking.
- Add tests for successful completion, failures, timeouts, cancellation, and repeated use.
- Preserve existing synchronous programs and the default build path.

YLang 2.2.0 ships a first async I/O slice: `http.request_async(...)` returns an `http_task`, `await task` consumes it to return the response body, and `http.cancel(task)` requests cancellation. It uses libcurl's multi interface so queued transfers progress concurrently while a task is awaited. General-purpose async functions, a background scheduler, and GUI/event-loop integration remain unimplemented; the broader acceptance goal is only partially complete.

### 2. HTTP and API requests

Provide a standard, typed way to call HTTP APIs from YLang. A first useful milestone should cover:
- GET and POST, with a clear path to other common methods.
- Request headers, request bodies, response status, response headers, and response bodies.
- JSON request/response interoperability.
- Connection and overall timeouts, bounded response sizes, and useful error categories.
- HTTPS certificate verification enabled by default.
- Documented redirect behavior.
- Examples that read credentials from environment/configuration rather than hard-coding secrets.

YLang 2.2.0 implementation status: the C backend has synchronous HTTP helpers and a generic method API; `http.request_async`, `await http_task`, and `http.cancel` use libcurl's multi interface. Response bodies remain strings and there is no built-in JSON parser/serializer. The NASM backend does not support HTTP; the native Windows HTTP path is covered by CI, but interactive debugging is not implemented there. Never embed API credentials in the compiler or examples.

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

YLang 2.2.0 implementation status: `ylang debug` creates an unoptimized debug build with symbols and source-line mapping, then launches GDB on Linux or LLDB on macOS. VS Code exposes a terminal-based command for it. There is no dedicated Debug Adapter Protocol adapter, so IDE breakpoints, Variables/Call Stack panes, generated local names, and scope mapping are still limited. The NASM backend and Windows interactive debugging are not supported.

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

## Follow-up priorities

1. Design and implement general-purpose async functions, scheduler behavior, and cancellation/error propagation.
2. Add a documented JSON parser/serializer and improve HTTP API ergonomics.
3. Evaluate a dedicated VS Code Debug Adapter Protocol integration, local-variable inspection, and expanded platform support.
4. Continue performance and security testing with reproducible baselines and explicit compatibility checks.
5. Complete Marketplace publication and future editor-discovery work separately from the compiler release.

## Future release gates

For future releases that expand these areas:
- Require test, sanitizer, and relevant platform CI jobs on the exact release commit.
- Add reference documentation and runnable examples for new syntax and standard-library APIs.
- Preserve the documented core compatibility policy.
- Test HTTP security defaults, timeout/error behavior, and debugger platform boundaries.
- Verify editor packaging, Marketplace instructions, website links, assets, and installation guidance before claiming those tasks complete.

Current editor/website status: VS Code is version 0.6.0 in this release, the repository recommends it via `.vscode/extensions.json`, syntax highlighting includes `http` and `env`, and the debug command is wired to the native compiler. Marketplace publication is still a manual release task; a workspace recommendation does not discover the extension for arbitrary `.yl` files. The homepage, reference landing page, and online book share the refreshed dark-teal/rust design, while the original static home is preserved as `classic.html` in the deployment artifact.

The feature list and order may change based on implementation findings. The HTTP task API is only the first async slice; do not mark broader asynchronous programming complete until general async functions, cancellation/error behavior, and event-loop integration have been separately designed and tested. Do not describe a future feature as implemented until its acceptance checks pass.