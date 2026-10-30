# Program Structure

A YLang source file uses the \`.yl\` extension. The language makes types visible in declarations and function signatures, and most command-line programs define a \`main\` function that returns an integer status.

This chapter introduces the pieces that appear in many programs: functions, local variables, constants, expressions, control flow, and user-defined data. For exact grammar and compiler-specific restrictions, use the [Complete Language Specification](reference/language-spec.md).

## A complete small program

\`\`\`ylang
fn add(int left, int right) -> int {
    return left + right;
}

fn main() -> int {
    let total: int = add(20, 22);
    print(total);
    return 0;
}
\`\`\`

The first function accepts two integer parameters and returns their sum. The \`main\` function calls it, stores the returned value, prints it, and returns a status code. Breaking behavior into small functions makes it easier to name the purpose of each operation and test it separately.

## Functions and signatures

A function declaration includes its name, parameter list, return type, and body:

\`\`\`ylang
fn is_positive(int value) -> bool {
    return value > 0;
}
\`\`\`

The parameter's type describes what the function expects. The return type after \`->\` describes the value callers receive. When changing a function signature, check every call site: callers must pass compatible arguments and use the returned value correctly.

Use the compiler to confirm the exact syntax accepted by your version. Do not assume that a feature from C, Python, Rust, or another language is available merely because its syntax looks familiar.

## Local variables

A mutable local uses a declaration such as:

\`\`\`ylang
let mut score: int = 10;
score = score + 1;
\`\`\`

The declared type is \`int\`; the next statement reads the current value, adds one, and assigns the result back to the same binding.

A constant declaration uses \`const\`:

\`\`\`ylang
let language: string = "YLang";
\`\`\`

Initialize a local before reading it. An immutable binding cannot be reassigned after its declaration. Prefer immutable bindings when a value should not change, because this communicates intent to readers and prevents accidental reassignment.

## Expressions and statements

An expression computes a value. Examples include an integer literal such as \`42\`, a variable name such as \`score\`, and an arithmetic expression such as \`score + 1\`. A statement performs an action, such as declaring a variable, assigning a value, calling a function, or returning from a function.

Keep expressions small while learning. If a diagnostic points to a long expression, split the calculation into named intermediate values; that makes the types and the source of a mismatch easier to see.

## Control flow

Conditional statements and loops let a function choose and repeat work. A condition should have the type required by the language, and expressions in the body must obey the same type and initialization rules as expressions elsewhere.

When learning a control-flow construct:

1. Start with the smallest program that uses it.
2. Check the program before adding more behavior.
3. Try a boundary case, such as a condition that is false immediately.
4. Inspect the output and any diagnostic rather than guessing what the construct means.

Consult the [Complete Language Specification](reference/language-spec.md) for the supported grammar and exact restrictions.

## User-defined data

YLang 2.x includes experimental structs and unit enums. Structs group named fields into a single value; enums represent one of a declared set of variants. These features are useful for modeling small domains, but their limits and backend support are not the same as Rust's.

Continue to [Structs](reference/STRUCTS.md) and [Enums and Matching](reference/ENUMS.md) for the actual contracts. In particular, verify support for a feature in the backend and platform you plan to use instead of assuming every accepted construct is portable.

## Common mistakes

- **Missing a declaration:** declare a local before using its name.
- **Wrong type:** compare the declared type with the expression's type and use only documented explicit conversions.
- **Reassigning a constant:** use a mutable declaration if the value genuinely needs to change.
- **Assuming familiar syntax:** check the language specification for the exact form accepted by YLang.
- **Treating a design proposal as shipped:** proposals and roadmaps describe intent; the implementation and release documentation define current support.

## Practice

1. Change the example so that \`add\` adds different integer values, then check and run it.
2. Add a constant string describing the result and print it alongside the total.
3. Deliberately assign a value of the wrong type, read the complete compiler diagnostic, and then correct the declaration or expression.

After these exercises, continue to [Types and Explicit Conversions](types.md).