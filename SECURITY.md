# Security policy

## Supported version

Security reports should target the latest released version where possible.

## Reporting a vulnerability

Please do not publish exploit details in a public issue. Use GitHub's private vulnerability reporting feature for this repository if enabled. If private reporting is unavailable, contact the maintainers privately before sharing a proof of concept publicly.

## Scope and limitations

YLang 1.0.0 is a small compiler that generates C and invokes a native compiler. It is not a sandbox. Compiled programs run with the permissions of the invoking user. The language does not currently provide Rust-like memory-safety guarantees, and the compiler should not be used as a security boundary for hostile source code. Reports concerning memory corruption, arbitrary code generation, unsafe generated C, or unintended command execution are especially relevant.
