# YLang Language Specification — 1.0.0

This document defines the supported language subset for the YLang 1.0.0 stable release. Syntax not listed here is not part of the v1.0 language contract.

## 1. Compilation model

YLang is a compiled language. `ylang check` parses and performs semantic checks. `ylang build` generates C and invokes GCC or Clang to produce a native executable. `ylang emit-c` writes C for inspection. There is no interpreter in this release.

## 2. Source form and statements

Statements end with `;`, blocks use `{` and `}`, and comments use `//` through the end of a line. Keywords are lowercase and case-sensitive. Source files are text and embedded NUL bytes are rejected before lexing. Each source file passed to the CLI is compiled as one program. A program must define `function main() -> int` or `function main() -> void`, with no parameters.

## 3. Variables and constants

```ylang
let int age;
let int score = 100;
let string name = "YLang";
let bool enabled = true;
let const int LIMIT = 100;
```

- Mutable declaration: `let type name;` or `let type name = expression;`.
- Constant declaration: `let const type name = expression;`. Constants must be initialized and cannot be reassigned.
- A local variable must be assigned on every path before it is read. The checker rejects a read it cannot prove initialized.
- Global variables without explicit initializers receive a type-appropriate zero/empty default. Explicit initialization is preferred.
- A declaration can shadow a name in an outer block, but duplicate names within one scope are errors.
- Assignment is a statement, not a general nested expression.

## 4. Types

| Type | Meaning |
| --- | --- |
| `int` | Signed 64-bit integer |
| `float` | 64-bit floating-point value |
| `bool` | `true` or `false` |
| `char` | One byte; ASCII-oriented |
| `string` | Immutable string data represented as a C string by the current backend |
| `void` | Function return type only; cannot be used as a variable type |

No implicit numeric conversions are performed. Arithmetic operands must have compatible matching types. `%` is available only for `int`. Conditions must have type `bool`. Strings can be compared for equality/inequality. String values are immutable.

## 5. Literals and strings

```ylang
let int n = 42;
let float ratio = 1.25;
let bool ready = true;
let char initial = 'Y';
let string message = "Hello\n";
let string greeting = f"Hello {message}";
```

`char` is one byte, not a Unicode scalar. A string literal must be terminated. F-strings begin with `f"` and interpolate expressions inside `{}`. Supported interpolations include variables and operators. Function calls and assignments are not allowed inside interpolation expressions; compute the value in a preceding statement. Literal braces can be escaped as documented by the lexer (`{{`, `}}`, `\{`, and `\}`).

## 6. Output

`print(expression, ...)` writes values separated by one space and appends a newline:

```ylang
print("answer:", 42);
print(f"Count: {n}");
```

Supported printable values are `int`, `float`, `bool`, `char`, and `string`. `print` is a statement in this release.

## 7. Expressions and operators

- Unary: `-`, `not`
- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Boolean: `and`, `or`
- Assignment: `=` as a statement
- Grouping: `(expression)`
- Calls: `name(argument, ...)`

Operator precedence is conventional: unary operators, multiplication/division/modulo, addition/subtraction, comparisons/equality, `and`, then `or`. Parentheses may be used to make grouping explicit.

Signed integer overflow, integer division/modulo by zero, and floating-point division by zero are handled by generated runtime helpers and terminate the program with a runtime-error message and status 70. This does not mean every possible C-level issue is guarded or that the language is memory-safe.

## Loops

Three loop forms are supported:

```ylang
loop() {
    // Repeat until break.
    break;
}

while (count < limit) {
    count = count + 1;
}

for (let int i = 0; i < limit; i = i + 1) {
    print(i);
}
```

- `loop()` is an unconditional loop.
- `while (condition)` repeats while a boolean condition is true.
- `for (initializer; condition; increment)` uses C-style clauses. The initializer may be a `let` declaration or expression; condition and increment may be omitted.
- `break;` exits the innermost loop and `continue;` starts its next iteration.
- A `for` initializer declaration is scoped to the loop and its body.

## Fixed-size arrays

YLang supports fixed-size arrays with an explicit element type and an initializer. The length is inferred from the initializer. Arrays may be local or global, must contain at least one element, and all elements must have the same type.

```ylang
let int scores[] = [10, 20, 30];

function main() -> int {
    scores[1] = 42;
    print(scores[0], scores[1], scores[2]);
    return 0;
}
```

- Indexing starts at zero.
- Array indices must be `int`; every access performs a runtime bounds check and exits with status 70 on an invalid index.
- Element assignment is supported, such as `scores[1] = 42;`.
- Whole-array assignment, passing arrays to functions, returning arrays, nested arrays, and `const` arrays are not supported yet.
- Arrays must be initialized at declaration; their size cannot change at runtime.

## Standard string and input library

YLang includes byte-oriented string helpers and a line reader for simple command-line programs:

```ylang
let string name = io.read_line();
let string greeting = string.concat("Hello, ", name);
print(greeting);
print(string.length(name));
print(string.contains(name, "lang"));
print(string.starts_with(name, "Y"));
print(string.ends_with(name, "n"));
```

- `string.length(value)` returns the number of bytes, not Unicode characters.
- `string.contains(value, needle)`, `string.starts_with(value, prefix)`, and `string.ends_with(value, suffix)` return `bool`.
- `string.concat(left, right)` returns a new string. The generated runtime tracks allocated strings until process exit, so repeated concatenation in a long-running loop can increase memory use.
- `io.read_line()` reads one line from standard input and removes its trailing newline. At end-of-file it returns an empty string.
- `io.read_file(path)` reads an entire UTF-8/ASCII-compatible text file into a string. A file-open/read failure or embedded NUL byte terminates the program with a runtime error; binary files are not supported by the string API.
- `io.write_file(path, content)` overwrites or creates a text file and returns `true` on success or `false` if it cannot open/write the file. It does not create missing parent directories.
- File paths are interpreted relative to the process working directory. File I/O is synchronous and currently has no structured error/exception type.

## Standard math library

YLang provides a built-in `math` namespace for common floating-point operations. These calls are checked by the compiler and emitted as native C math-library calls.

```ylang
let float root = math.sqrt(81.0);
let float power = math.pow(2.0, 8.0);
let float angle = math.sin(0.0);
let float bounded = math.clamp(12.0, 0.0, 10.0);
```

Available functions (all arguments must be `float`, and all return `float`):

- Unary: `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `exp`, `log`, `log10`, `floor`, `ceil`, `round`, `abs`.
- Binary: `pow`, `atan2`, `min`, `max`, `hypot`.
- Three arguments: `clamp(value, low, high)`; a lower bound greater than the upper bound triggers a runtime error.

YLang does not implicitly convert integers to floats, so use float literals such as `9.0`. Domain errors and non-finite results follow the platform C math library behavior.

## 8. Conditions and loops

```ylang
if (age >= 18) {
    print("Adult");
} else if (age >= 13) {
    print("Teenager");
} else {
    print("Child");
}

loop() {
    if (done) {
        break;
    }
    continue;
}
```

Conditions must be boolean. `loop()` repeats indefinitely until `break` executes. `continue` skips to the next iteration. Both are only valid inside a loop. `for` and `while` are not supported in v1.0.0.

## 9. Functions

```ylang
function add(int left, int right) -> int {
    return left + right;
}

function main() -> int {
    print(add(20, 22));
    return 0;
}
```

Parameter and return types are explicit. Function overloading is not supported. A non-void function must return a value on supported control-flow paths. `main` has no parameters and returns `int` or `void`.

## 10. Diagnostics and tooling

`check` reports parse and semantic/type diagnostics without invoking a native compiler. Diagnostics include an error code, file/line/column, source excerpt, caret, and a hint when one is available. `build` does not produce a successful executable if YLang diagnostics contain errors.

`fix input.yl -o output.yl` currently implements one high-confidence source transformation: changing a `pritn(...)` call typo to `print(...)` when no function named `pritn` is declared. It requires an output path and does not intentionally modify comment/string text. It is not a general repair engine. Always run `check` after applying a fix.

## 11. Unsupported features and implementation limits

The following are not supported by the current language subset: classes/OOP, exception syntax (`try`/`catch`), modules, generics, raw pointers/references, passing or returning arrays, nested/const arrays, and a dedicated LLVM/native backend. `class`, `try`, and `catch` may be tokenized as reserved words but are not valid executable constructs.

The compiler generates C, so native code inherits ordinary process privileges and is not sandboxed. Some formatted string values are stored until process exit; creating many such values in a long-running loop can increase memory consumption. The language does not define ownership/borrowing or a garbage collector in this release. Do not assume Rust-like memory-safety guarantees.
