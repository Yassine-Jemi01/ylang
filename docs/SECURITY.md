# Security Model and Safe Defaults

**YLang is a compiled toolchain, not a sandbox.** A YLang executable runs with the privileges of the user who starts it. The compiler does not isolate file access, processes, network access, or imported native code.

## HTTP/HTTPS client

The experimental C-backend client uses libcurl and applies these defaults:

- Only \`http://\` and \`https://\` URLs are accepted. TLS certificate and hostname verification remain enabled; there is no insecure verification bypass.
- Connection attempts time out after 5 seconds and the entire request after 15 seconds.
- Response bodies are capped at 8 MiB, and request/response header text is bounded to 16 KiB.
- Automatic redirects are limited to five and allowed only for HTTP/HTTPS when no custom request headers are present. If a request includes headers, a 3xx response is returned directly rather than forwarding arbitrary headers such as \`X-API-Key\` to a new target.
- HTTP status codes such as 401, 404, and 500 are response statuses, not transport errors. Check \`http.status()\` and \`http.last_error()\` separately.
- At most 32 HTTP task handles may be in flight. \`http_task\` values are process-local, single-consumption handles; cancellation does not undo a request already received by the server.

These safeguards reduce common accidental leaks and resource exhaustion, but they do **not** make a caller-supplied URL safe in every deployment. In server-side applications, validate URLs against an application-specific hostname/IP allow-list and consider access to loopback, private, link-local, and metadata addresses before making a request.

## API keys and environment variables

Use \`env.get("SERVICE_API_KEY")\` to read a credential supplied by the parent process. Missing variables return an empty string. The helper does not invoke a shell and does not grant additional permissions.

Do not commit real API keys, print them in diagnostic output, or include them in generated source files. Use the credential mechanism supported by the host or CI environment, restrict keys to the minimum permissions required, and rotate leaked credentials.

## Native execution and debugging

- \`ylang build\`, \`ylang debug\`, and the native executable run code on the host with the invoking user's normal permissions.
- Debugging is not a privilege boundary or sandbox. GDB/LLDB can inspect process state according to the user's operating-system permissions.
- The experimental debugger is implemented for the C backend on Linux (GDB) and macOS (LLDB). Windows interactive debugging and the NASM source-debugging path are not implemented.
- Generated C and compiler diagnostics are developer tools, not a guarantee that arbitrary programs are memory-safe. YLang does not claim Rust-level memory safety; continue running regression tests, sanitizers, and fuzzing on supported environments.

For a security report, include the affected commit, OS/toolchain, exact compiler command, minimal reproducible source, and whether the issue reproduces with a non-privileged account. Do not post live credentials or private user data in an issue.
