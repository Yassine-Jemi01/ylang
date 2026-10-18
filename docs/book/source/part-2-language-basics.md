# Part II — Language Foundations

# Chapter 5. Variables and Constants

A variable is a named value that your program can use and update. A constant is a name whose value cannot be reassigned after initialization.

The basic declaration form is `let type name;` or `let type name = expression;`. The type comes before the variable name.

```ylang
let int score = 100;
let string player = "Ada";
let bool game_running = true;

score = 125;
print(player, score, game_running);
```

YLang declarations make the type explicit. When you initialize a variable, the compiler checks that the expression has the declared type.

## Initialize before reading

A local variable can be declared before you assign to it:

```ylang
let int result;
result = 20 + 22;
print(result);
```

The compiler rejects a read that it cannot prove is initialized on every relevant control-flow path. This catches mistakes such as printing a local variable before assigning it.

Prefer initializing in the declaration when that makes the code easier to follow.

## Constants

Use `let const` to declare a constant:

```ylang
let const int MAX_USERS = 100;
let const string APP_NAME = "YLang Demo";

print(APP_NAME, MAX_USERS);
```

A constant needs an initial value and cannot be assigned again. Constants are useful for limits, labels, and values that should not accidentally change in a long function.

## Names, scope, and globals

Names are case-sensitive: `score` and `Score` are different. Duplicate declarations in the same scope are errors. A declaration inside an inner block can shadow a name from an outer block, but too much shadowing makes a program difficult to read.

Globals are declared outside functions. A global with no explicit initializer receives a type-appropriate zero or empty default. Explicit initialization is usually easier to understand:

```ylang
let int total = 0;

function add_point() -> int {
    total += 1;
    return total;
}

function main() -> int {
    print(add_point());
    return 0;
}
```

This shares state between functions. In larger programs, consider passing values as parameters and returning results so that the data a function uses is visible in its signature.

---

# Chapter 6. Types

A type tells the compiler what kind of value an expression represents and which operations are allowed.

| Type | Meaning | Example |
| --- | --- | --- |
| `int` | Signed 64-bit integer | `let int count = 12;` |
| `float` | 64-bit floating-point value | `let float ratio = 0.5;` |
| `bool` | Boolean value | `let bool ready = true;` |
| `char` | One byte, ASCII-oriented | `let char initial = 'Y';` |
| `string` | Immutable string data | `let string title = "Hello";` |
| `void` | Function return type only | `function log() -> void { ... }` |

`void` cannot be used as a variable type. It describes a function that does not return a value.

## Integer and float literals

An integer such as `42` has integer type. A decimal such as `4.5` has float type. YLang also supports hexadecimal integers and scientific floating-point notation:

```ylang
let int mask = 0xFF;
let float large = 1e3;
let float small = 2.5e-4;
```

`10` is an integer; `10.0` is a float. The difference matters when calling a math function or combining operands in an expression.

## Booleans, characters, and strings

A boolean has the value `true` or `false`. Conditions require a boolean; YLang does not treat an arbitrary number as true or false.

A `char` is one byte, not a full Unicode character. YLang's string helpers are byte-oriented: the reported string length is a count of bytes, and case conversion is not Unicode-aware. A user-visible symbol in UTF-8 can occupy multiple bytes.

A `string` is immutable. String functions produce new values rather than modifying the existing value in place.

## No implicit conversion

YLang does not silently convert values between types. This is invalid because the expression combines an integer and a float:

```ylang
let int count = 5;
let float ratio = 2.0;

// Invalid: incompatible operand types.
// let float result = count + ratio;
```

Convert explicitly when that is what you intend. The next chapters introduce the conversion functions and safe string-parsing pattern.

---

# Chapter 7. Printing and F-Strings

The `print` statement writes one or more values to standard output. It separates multiple arguments with one space and appends a newline.

```ylang
print("Hello");
print("The answer is", 42);
```

Output:

```text
Hello
The answer is 42
```

Supported printable values are `int`, `float`, `bool`, `char`, and `string`. `print` is a statement, not an expression that returns a new string.

## Formatted strings

An f-string inserts values into a text template. Write the letter `f` directly before the opening quote and put an expression in braces.

```ylang
let string name = "Mina";
let int score = 87;

print(f"Player: {name}");
print(f"Score: {score}");
```

F-strings make labels and values easier to read. Supported interpolations include variables and operators. Function calls and assignments are not supported inside interpolation expressions, so calculate complex values first:

```ylang
let int first = 20;
let int second = 22;
let int total = first + second;

print(f"Total: {total}");
```

Use the documented escape forms `{{`, `}}`, `\\{`, and `\\}` when a literal brace must appear in an f-string instead of starting or ending an interpolation.

## Debugging output

Print labels alongside values:

```ylang
let int width = 640;
let int height = 400;
print("Width:", width);
print("Height:", height);
```

This is more informative than printing two numbers without identifying what they mean.

---

# Chapter 8. Operators and Expressions

Operators calculate values and compare them. The type of each operand matters because YLang checks expressions explicitly.

## Arithmetic

| Operator | Meaning | Example |
| --- | --- | --- |
| `+` | Addition | `a + b` |
| `-` | Subtraction | `a - b` |
| `*` | Multiplication | `a * b` |
| `/` | Division | `a / b` |
| `%` | Integer remainder | `a % b` |

The `%` operator is for integers. Integer division does not produce a fractional result; use floating-point values or an explicit conversion when you need a decimal answer.

```ylang
let int a = 17;
let int b = 5;

print(a + b);
print(a - b);
print(a * b);
print(a / b);
print(a % b);
```

## Comparisons

Comparison operators produce a boolean:

- `==` means equal.
- `!=` means not equal.
- `<` means less than.
- `<=` means less than or equal.
- `>` means greater than.
- `>=` means greater than or equal.

Assignment is different from comparison: `score = 10;` stores a value, whereas `score == 10` tests it.

## Boolean operators

Use `and` when both conditions must be true, `or` when at least one must be true, and `not` to negate a boolean.

```ylang
let int age = 20;
let bool has_ticket = true;

if (age >= 18 and has_ticket) {
    print("Access granted.");
}
if (not has_ticket) {
    print("Buy a ticket first.");
}
```

Parentheses help explain complex logic:

```ylang
let bool is_admin = false;\n\nif ((age >= 18 and has_ticket) or is_admin) {
    print("Continue.");
}
```

The stable core does not promise short-circuit behavior for `and` and `or`. Do not rely on the second operand being skipped to protect a side effect or invalid operation; evaluate such operations separately and check their result explicitly.

## Assignment shortcuts and precedence

`=` assigns a value. `+=` adds to the current value, and postfix `++` increments a mutable numeric variable by one:

```ylang
let int count = 0;
count += 1;
count++;
print(count);
```

These are standalone update statements, not general nested expressions.

Precedence follows conventional rules: unary operators first, multiplication/division/remainder, addition/subtraction, comparisons/equality, `and`, then `or`. When an expression is difficult to scan, use parentheses even if the compiler could infer the order.

---

# Chapter 9. Making Decisions with if

The `if` statement chooses which block to run based on a boolean condition.

```ylang
let int temperature = 28;

if (temperature > 30) {
    print("Hot");
} else {
    print("Not hotter than 30");
}
```

The condition appears inside parentheses. Braces mark the statements controlled by that condition.

## Several possible cases

Use `else if` for more than two cases:

```ylang
let int score = 76;

if (score >= 90) {
    print("Excellent");
} else if (score >= 70) {
    print("Good");
} else if (score >= 50) {
    print("Passed");
} else {
    print("Try again");
}
```

Conditions are checked from top to bottom. When one is true, its body executes and the rest of the chain is skipped. Put narrower or higher thresholds first, otherwise an earlier broad condition may hide a later case.

## Combine conditions

```ylang
let int age = 19;
let bool accepted_rules = true;

if (age >= 18 and accepted_rules) {
    print("Registration allowed");
}
```

The full condition must be boolean. If a compiler diagnostic says an `if` condition has the wrong type, check each operand and ensure you did not mix strings, integers, floats, and booleans accidentally.

A common mistake is using `=` where `==` was intended. Remember that assignment changes a variable, while comparison answers a yes-or-no question.

---

# Chapter 10. Repeating Things: loop, while, and for

A loop repeats statements. YLang provides `loop()`, `while`, and `for`; choose the form that makes the stopping condition easy to see.

## loop()

`loop()` repeats until the program leaves it with `break` or terminates:

```ylang
let int count = 0;

loop() {
    print(count);
    count += 1;

    if (count >= 3) {
        break;
    }
}
```

The values printed are `0`, `1`, and `2`. If you forget a path to exit an unconditional loop, it can run forever.

## while

A `while` loop repeats as long as its boolean condition remains true:

```ylang
let int count = 1;

while (count <= 3) {
    print(count);
    count += 1;
}
```

Pay attention to where the loop variable changes. If it never changes in a way that makes the condition false, the loop will not end on its own.

## for

A `for` loop groups an initializer, condition, and increment:

```ylang
for (let int i = 0; i < 5; i = i + 1) {
    print(i);
}
```

The initializer runs once. The condition is checked before each iteration. After the body completes, the increment runs before the next check. A variable declared in the initializer is scoped to the loop.

## break and continue

`break;` exits the innermost loop. `continue;` skips the rest of the current iteration and begins the next one.

```ylang
for (let int i = 0; i < 6; i = i + 1) {
    if (i == 2) {
        continue;
    }
    if (i == 5) {
        break;
    }
    print(i);
}
```

This prints `0`, `1`, `3`, and `4`. The value two is skipped, and the loop exits before printing five.

Use `for` for familiar counting patterns, `while` when repetition depends on a condition, and `loop()` when you want an explicit exit inside the body. Before running a loop, be able to explain what changes each iteration and why the loop eventually stops.
