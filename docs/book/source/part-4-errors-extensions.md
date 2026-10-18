# Part IV — Errors and Extensions

# Chapter 16. Reading Compiler Errors and Using fix

A diagnostic is the compiler's explanation of why it cannot accept a program. YLang errors contain a code such as `E2001`, the source path, a line and column, a source excerpt, a caret, and a hint when available.

Read the first error carefully. Later messages may be consequences of the first mistake. Fix a single issue, run `ylang check` again, and repeat until the compiler accepts the source.

## Type mismatches

Consider this function:

```ylang
function double_value(int value) -> int {
    return value * 2;
}

function main() -> int {
    let float amount = 2.5;
    // double_value requires int, not float.
    print(double_value(to_int(amount)));
    return 0;
}
```

The conversion makes the types match. Remember that `to_int(2.5)` truncates to `2`, so the result is `4`. If rounding is what you intend, choose and implement the rounding step deliberately rather than assuming `to_int` rounds.

## The fix command

```sh
ylang fix broken.yl -o fixed.yl
ylang check fixed.yl
```

The current fix operation is deliberately narrow: it corrects `pritn(...)` to `print(...)`. It is not a general automatic repair engine. Review the output and run `check` on it before building.

## A reliable debugging routine

1. Run `ylang check` to get fresh diagnostics.
2. Find the first reported location and inspect the surrounding statement.
3. Verify spelling, declaration order, types, initialization, and argument counts.
4. Change one thing and check again.
5. When the source checks, build and run it.
6. If the failure appears only while running, investigate runtime conditions such as division by zero, invalid array indices, or file errors.

When asking for help, include the exact version, operating system, compiler, command, a minimal source file, expected behavior, and actual behavior.

---

# Chapter 17. Runtime Errors

A program can pass compiler checks and still fail at runtime. Some operations depend on values supplied by the user, filesystem state, or calculations that cannot always be known statically.

## Arithmetic failures

Signed integer overflow, integer division or remainder by zero, and floating-point division by zero are handled by generated runtime helpers. These failures terminate the program with an error message and exit status `70`.

If a divisor comes from input, check it before dividing. A successful type check does not tell you that a value is non-zero.

## Array bounds failures

A three-element array has valid indices `0`, `1`, and `2`. Accessing index `3` is invalid. YLang performs a runtime bounds check and terminates with a runtime error instead of silently reading arbitrary memory.

Validate indices derived from calculations or external input before accessing the array.

## Conversion and input failures

`to_int` can fail if a float is non-finite or outside the signed 64-bit integer range. `string.parse_int` fails for invalid or out-of-range text. Validate untrusted text with `string.is_int` before parsing.

## File failures

`io.read_file` terminates the program if it cannot open/read the file or if the contents include an embedded NUL byte. `io.write_file` returns a boolean that indicates whether the write succeeded. Check the return type of the particular function you use instead of assuming all file operations fail in the same way.

YLang does not currently provide structured exception handling or implemented `Result<T, E>` values. The repository's Option and Result document is a design proposal, not an implemented core feature.

## What runtime checks do not mean

These guards cover specific documented operations. They do not make YLang memory-safe at Rust's level, and they do not provide a security sandbox. Successful compilation must not be treated as proof that a program can safely process hostile data.

---

# Chapter 18. A Complete Program

This example combines input, integer validation, a loop, a helper function, arithmetic, an explicit conversion, and formatted output. It asks for three integer scores and displays their sum and average.

```ylang
function read_score(int position) -> int {
    loop() {
        print(f"Enter score {position}:");
        let string raw = io.read_line();

        if (string.is_int(raw)) {
            return string.parse_int(raw);
        }

        print("Please enter a whole integer.");
    }
}

function main() -> int {
    let int first = read_score(1);
    let int second = read_score(2);
    let int third = read_score(3);
    let int total = first + second + third;
    let float average = to_float(total) / 3.0;

    print(f"Total: {total}");
    print(f"Average: {average}");
    return 0;
}
```

Build and run:

```sh
ylang check scores.yl
ylang build scores.yl -o scores
./scores
```

## Follow the data

`read_score` receives the number of the score to ask for. The loop reads a line of text. If the string is a valid signed 64-bit integer, it parses and returns the number. If the user enters invalid text, the function reports the problem and repeats the prompt.

`main` calls the helper three times and stores each result in an integer variable. The total is the sum of those values. To calculate a fractional average, the program converts the total to a float and divides by `3.0`. It does not rely on implicit conversion.

## Improve it

Try rejecting negative values. Add a fourth score. Create a separate function that calculates a total from a fixed-size array. For each extension, decide what the function receives, what type it returns, and what data needs validation.

The general habit is to make each step explicit: validate external input, use matching types, split work into functions, and test with both valid and invalid values.

---

# Chapter 19. Multi-File Imports (Experimental)

Named imports allow a file to use selected functions and global variables defined in another `.yl` file. This extension is experimental and is not included in the stable core's compatibility guarantee.

```ylang
from "./math_tools.yl" import double_value;

function main() -> int {
    print(double_value(21));
    return 0;
}
```

The file `math_tools.yl` must define a top-level function named `double_value`. Import statements name a symbol, not a call, so omit the parentheses after the imported name.

Paths are resolved relative to the importing file first. On Linux and macOS, `YLANG_PATH` may hold a colon-separated list of library directories:

```sh
YLANG_PATH="$HOME/ylang-libs" ylang build main.yl -o main
```

The current implementation supports nested imports and reuses repeated references to the same canonical file. Circular imports, missing names, duplicate imports, and imports of `main()` are rejected.

Import aliases, qualified module namespaces, package manifests, dependency solving, and a package registry are not provided. The default C backend supports module graphs, but the NASM backend accepts imports only when every imported feature is in its supported subset. Read `docs/MODULE-SYSTEM-SPEC.md` before relying on imports in a large project.

---

# Chapter 20. Structs and Enums (Experimental)

Structs and enums are experimental extensions. They are useful to learn about but should not be presented as stable core features.

## Structs

A struct groups named fields into a custom type:

```ylang
struct Point {
    int x;
    int y;
}

function shift(Point point, int dx) -> Point {
    return Point(point.x + dx, point.y);
}

function main() -> int {
    let Point origin = Point(2, 7);
    let Point moved = shift(origin, 3);
    print(moved.x, moved.y);
    return 0;
}
```

The current subset supports named fields with explicit types, positional construction in declaration order, field reads, passing and returning structs, and assignment between values of the same struct type. Fields of a mutable struct variable can be assigned values of the declared type, including a field reached through nested structs. For example, if `Hero` contains a `Position pos` field, you can write `hero.pos.x = 9;` directly. The compiler checks the final field's type and requires the root variable to be mutable; you do not need to copy the nested value and rebuild the outer struct just to update one of its fields.

Current limits include recursive by-value fields, methods, inheritance, generics, field defaults, and named constructor arguments. The C backend supports the documented struct subset; the NASM backend does not promise parity.

## Enums and exhaustive matching

An enum defines a set of named alternatives:

```ylang
enum Direction { North, East, South, West }

function main() -> int {
    let Direction facing = Direction.North;

    match (facing) {
        Direction.North -> { print("north"); }
        Direction.East -> { print("east"); }
        Direction.South -> { print("south"); }
        Direction.West -> { print("west"); }
    }
    return 0;
}
```

Each enum is a distinct type. Its unit variants can be stored in variables, passed to and returned from functions, and compared with variants of that same enum. Variants do not carry payload values in this milestone.

The `match` statement must include every variant exactly once. Missing or duplicated arms are errors. Store a value in a variable before matching if the expression would contain a call or assignment.

Payload variants, generic enums, matching bindings, and a guarantee about numeric tag values are not provided. See `docs/ENUMS.md` and `docs/STRUCTS.md` for the current reference and limits.
