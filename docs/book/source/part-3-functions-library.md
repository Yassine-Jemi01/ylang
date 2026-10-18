# Part III — Functions and Standard Library

# Chapter 11. Functions

A function gives a name to a sequence of statements. It lets you reuse logic and divide a larger program into smaller pieces that each have a recognizable job.

```ylang
function add(int left, int right) -> int {
    return left + right;
}

function main() -> int {
    let int answer = add(20, 22);
    print(answer);
    return 0;
}
```

The `add` function receives two integer parameters and returns an integer. Calling `add(20, 22)` supplies the arguments and produces the value returned by the function.

## Return types

The declared return type must match the returned value. A non-void function must return a value on the supported control-flow paths.

```ylang
function is_even(int value) -> bool {
    return value % 2 == 0;
}
```

A function that does not return a value can use `void`:

```ylang
function show_message() -> void {
    print("Welcome");
}

function main() -> int {
    show_message();
    return 0;
}
```

## Parameters have explicit types

Each parameter declares a type. The compiler rejects calls with a mismatched number or type of arguments. Since YLang does not perform implicit conversions, convert a value before passing it to a function that expects another type.

Function overloading is not part of the stable core. Do not define multiple functions with the same name and expect the compiler to select one based on the argument types.

## Keep functions focused

Names such as `read_score`, `is_even`, and `calculate_total` tell a reader what the function is responsible for. Prefer passing values as parameters and returning results over using shared global state, unless shared state is genuinely part of the design.

---

# Chapter 12. Arrays

An array stores multiple values of one element type under a single name. YLang's stable core provides fixed-size arrays whose length is inferred from the initializer.

```ylang
let int scores[] = [10, 20, 30];

function main() -> int {
    print(scores[0]);
    print(scores[1]);
    print(scores[2]);
    return 0;
}
```

Array indexing starts at zero. In a three-element array, valid indices are zero, one, and two.

## Read and update elements

```ylang
let int scores[] = [10, 20, 30];

scores[1] = 42;
print(scores[0], scores[1], scores[2]);
```

The assignment changes only the selected element. Every access is bounds-checked at runtime; an invalid index terminates the program with a runtime error rather than silently reading arbitrary memory.

All elements must have the same type:

```ylang
let string names[] = ["Mina", "Omar", "Lea"];
let bool flags[] = [true, false, true];
```

## Arrays and functions

Array parameters use a declaration such as `int values[]`. The native interface passes a pointer plus a separate length value. Consult the current core specification and examples for the supported parameter forms.

## Limits to remember

The stable core does not support returning arrays from functions, nested arrays, whole-array assignment, or constant arrays. An array's length cannot change at runtime. Initialize an array in its declaration and update individual elements.

If an index comes from user input or a calculation, verify that it is within the valid range before using it. A valid array declaration cannot guarantee that every future index is valid.

---

# Chapter 13. Converting Between Types

YLang requires explicit conversions. Writing the conversion in your code makes the intended change visible to both the compiler and the reader.

## Integer to float

```ylang
let int count = 5;
let float precise_count = to_float(count);
let float half = precise_count / 2.0;

print(half);
```

Use a float literal such as `2.0` when the expression requires floating-point operands. Do not expect YLang to convert `count` automatically.

## Float to integer

```ylang
let float measurement = 12.8;
let int whole = to_int(measurement);
print(whole);
```

`to_int` truncates toward zero; it does not round to the nearest integer. Thus `12.8` becomes `12`, and `-12.8` becomes `-12`. If the float is not finite or is outside the signed 64-bit integer range, the runtime reports an error.

## Values to strings

```ylang
let int score = 42;
let string message = "Score: " + to_string(score);
print(message);
```

`to_string` supports `int`, `float`, `bool`, `char`, and `string`. For a string argument it returns that string unchanged.

## Parse text carefully

Terminal input and file contents are text. Validate a numeric string before converting it:

```ylang
let string raw = "123";

if (string.is_int(raw)) {
    let int value = string.parse_int(raw);
    print(value + 1);
} else {
    print("That is not an integer.");
}
```

`string.is_int` checks whether the entire string is a valid base-10 signed 64-bit integer. `string.parse_int` converts a valid value. Invalid or out-of-range text causes a runtime error, so check untrusted input first.

Use the same stored string for the check and the conversion, as in the example. If a value can change between validation and parsing, the second operation may fail even though the earlier check passed.

---

# Chapter 14. The Math Library

The built-in `math` namespace provides common floating-point functions. Calls are checked by the compiler and translated into native C math-library calls by the default backend.

```ylang
let float root = math.sqrt(81.0);
let float power = math.pow(2.0, 8.0);
let float angle = math.sin(0.0);

print(root);
print(power);
print(angle);
```

All arguments to the current math API must be floats, and the functions return floats. Write `math.sqrt(9.0)` rather than `math.sqrt(9)`. If the value you have is an integer, convert it with `to_float`.

## Function groups

Unary functions include `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `exp`, `log`, `log10`, `floor`, `ceil`, `round`, and `abs`.

Binary functions include `pow`, `atan2`, `min`, `max`, and `hypot`. The `clamp(value, low, high)` function accepts three floats and limits the value to the provided interval. A lower bound greater than the upper bound causes a runtime error.

```ylang
let float power = math.pow(2.0, 8.0);
let float distance = math.hypot(3.0, 4.0);
let float bounded = math.clamp(12.0, 0.0, 10.0);

print(power);
print(distance);
print(bounded);
```

The domain rules and non-finite results follow the platform C math library. Validate values before converting a math result into an integer or using it as an array index.

---

# Chapter 15. Strings, Paths, and Input/Output

YLang provides basic string manipulation, line input, text-file I/O, and path utilities. These functions are useful for small command-line tools, but they are not a complete Unicode or binary-file framework.

## Read input

```ylang
print("What is your name?");
let string name = io.read_line();
print(f"Hello, {name}!");
```

`io.read_line()` reads a line from standard input and removes its trailing newline. At end-of-file it returns an empty string. The input is still a string, even if the user types digits.

## String operations

```ylang
let string name = "YLang";

print(string.length(name));
print(string.contains(name, "Lan"));
print(string.starts_with(name, "Y"));
print(string.ends_with(name, "g"));
print(string.upper(name));
print(string.lower(name));
```

The available helpers include:

- `string.length(value)` returns bytes, not Unicode characters.
- `string.contains(value, needle)` checks for a substring.
- `string.starts_with(value, prefix)` and `string.ends_with(value, suffix)` check the ends of the string.
- `string.concat(left, right)` returns a new concatenated string.
- `string.replace(value, needle, replacement)` replaces non-overlapping matches; an empty search string leaves the original unchanged.
- `string.upper(value)` and `string.lower(value)` return copies using byte-oriented case mapping from the C locale.

String equality is case-sensitive and ordering is bytewise lexicographic. Case conversion is not Unicode-aware. Repeated concatenation or case conversion in a long-running loop can increase memory use because generated strings are tracked until process exit.

## Read and write text files

```ylang
let string contents = io.read_file("notes.txt");
print(contents);
let bool saved = io.write_file("copy.txt", contents);

if (not saved) {
    print("Could not write the file.");
}
```

`io.read_file(path)` reads the entire text file into a string. An open/read failure or an embedded NUL byte terminates the program with a runtime error. Binary files are not supported by this string interface.

`io.write_file(path, content)` creates or overwrites a text file and returns `true` on success or `false` on failure. It does not create missing parent directories. Paths are interpreted relative to the process working directory, not automatically relative to the source file.

## Path helpers

```ylang
let string file = "assets/logo.png";

if (path.exists(file)) {
    print(path.basename(file));
    print(path.extension(file));
}
```

`path.exists(path)` tests whether the path can be inspected with `stat`. It may be true for a file, directory, or another filesystem object. `path.basename(path)` returns the final component. `path.extension(path)` returns the suffix after the final dot without the dot; a leading dot alone is not an extension. These helpers use POSIX path separators.

## Open a file in a desktop application

`image.open(path)` launches the operating system's default opener. On Linux this is typically `xdg-open`; on macOS it uses `open`. The function does not decode an image or draw it inside a YLang GUI window. It needs a desktop session and a registered handler.

```ylang
let string image_path = "picture.png";
let bool opened = image.open(image_path);

if (not opened) {
    print("Could not open the file.");
}
```

Treat filesystem failures as normal possibilities. Check a path when useful and always inspect return values from functions that report success or failure.
