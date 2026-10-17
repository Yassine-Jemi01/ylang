# Lexer and parser fuzzing

The default make test command runs deterministic lexer and parser unit tests.
Optional libFuzzer targets exercise malformed input with AddressSanitizer and UndefinedBehaviorSanitizer.
They require a Clang build that supports -fsanitize=fuzzer; fuzzing is not required for ordinary builds.

Build the lexer harness: make fuzz-lexer FUZZ_CC=clang
Run it: ./build/fuzz-lexer -max_total_time=60 tests/fuzz/corpus/lexer

Build the parser harness: make fuzz-parser FUZZ_CC=clang
Run it: ./build/fuzz-parser -max_total_time=60 tests/fuzz/corpus/parser

Keep minimized, useful reproducers in the appropriate corpus directory and add a deterministic regression test when a crash or incorrect parse is found.
The parser harness calls the parser directly; it does not run code generation or execute generated programs.
