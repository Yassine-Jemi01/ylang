# YLang compiler error codes

YLang diagnostics include stable codes to help users search documentation and report bugs. Wording can improve between releases; documented meanings should remain stable.

## Lexing and parsing

| Code | Meaning | What to do |
|---|---|---|
| E1003 | An expression was expected but the current token cannot start one. | Check the preceding operator and use a supported expression. |
| E1015 | `print()` has no arguments. | Pass at least one value, such as `print("");`. |
| E1016 | Unknown standard-library namespace. | Use a documented namespace such as `math`, `string`, `io`, `path`, or `image`. |
| E1017 | Expression nesting exceeds the parser's supported depth. | Simplify deeply nested parentheses, unary operators, or call/index expressions. |
| E1020 | Malformed import declaration or path. | Use `from "./module.yl" import name;`. |
| E1022 | Imported source file could not be resolved or read. | Check the path relative to the importing file. |
| E1023 | Circular module dependency. | Break the import cycle. |
| E1024 | Imported function or global does not exist. | Check the target module declarations and spelling. |
| E1025 | Imported name is duplicated, ambiguous, or conflicts with a local declaration. | Import each name once and avoid local name conflicts. |
| E1026 | An imported module defines or imports the program entry point. | Keep `main()` in the root program file. |

## Types, names, and expressions

| Code | Meaning | What to do |
|---|---|---|
| E2001 | Assignment or initializer type mismatch. | Match the declared type or use an explicit conversion. |
| E2002 | Unknown type name. | Use a declared or built-in type. |
| E2004 | Duplicate name in one scope. | Rename or remove the duplicate declaration. |
| E2005 | `void` used in a value position. | Use a value type, or remove the value where appropriate. |
| E2010 | Integer literal is outside the signed 64-bit range. | Use a representable integer. |
| E2011 | Floating-point literal is invalid or out of range. | Use a finite float literal. |
| E2012 | Character literal is not one byte. | Use one ASCII byte or an escape. |
| E2014 | Function calls or assignments appear in an f-string interpolation. | Compute the value before interpolating it. |
| E2020 | Unknown variable name. | Declare the variable or correct its spelling. |
| E2021 | Variable may be read before initialization. | Assign it on every control-flow path before reading it. |
| E2022 | Assignment to a `const` variable. | Keep constants immutable. |
| E2023 | Assignment used as a nested value expression. | Keep assignment as a statement. |
| E2027 | Invalid array operation or indexing target. | Index a supported array with an integer index. |
| E2029 | An array is used as a value without an index. | Index the array or pass it to a function parameter that explicitly accepts an array. |
| E2030 | Unary minus used with a non-numeric value. | Negate an `int` or `float`. |
| E2031 | Logical negation used with a non-boolean value. | Use a `bool` expression. |
| E2032 | Logical operators require boolean operands. | Use boolean expressions on both sides. |
| E2034 | Numeric operation uses incompatible operand types. | Convert explicitly so the numeric types match. |
| E2035 | Ordered comparison uses incompatible types. | Compare matching numbers, chars, or strings. |
| E2036 | Equality operands are incompatible or are unsupported struct values. | Compare matching comparable values; struct equality is not implemented. |
| E2040 | Unknown function name. | Check spelling or define the function. |
| E2041 | Wrong number of user-defined function arguments. | Match the function's declared parameter count. |
| E2042 | Function argument type mismatch. | Match each argument to the expected type. |
| E2043 | Duplicate function definition in one module. | Keep one definition per function name in a module. |
| E2044 | A string standard-library function received a non-string argument. | Pass strings. |
| E2045 | An explicit conversion received an unsupported source type. | Check the conversion function's accepted input type. |
| E2050 | `print` received a `void` expression. | Print a value, or change the function to return one. |
| E2051 | An `if`, `while`, or `for` condition is not boolean. | Use a boolean expression. |
| E2052 | `break` or `continue` appears outside a loop. | Place it inside a loop. |
| E2053 | A non-void function return omits a value. | Return a value of the declared type. |
| E2054 | A void function returns a value. | Use `return;` or change the return type. |
| E2055 | Function return type mismatch. | Return the declared type. |
| E2056 | Global initializer is not a constant expression. | Use a constant initializer or initialize inside a function. |
| E2057 | `match` does not receive a user-defined enum. | Match a declared enum value. |
| E2058 | A `match` arm uses a different enum type. | Use variants from the matched enum. |
| E2059 | Duplicate enum variant in a `match`. | Keep one arm per enum variant. |
| E2060 | Program has no `main()` function. | Define the program entry point. |
| E2061 | Invalid `main()` signature. | Use no parameters and return `int` or `void`. |
| E2062 | A non-void function path can end without returning. | Return a value on every supported path. |
| E2063 | Enum `match` is not exhaustive. | Add an arm for every variant. |
| E2064 | `match` input contains a call or assignment. | Store the value in a variable before matching. |
| E2070 | A struct declaration conflicts with an existing struct or enum name. | Choose a unique name for the struct. |
| E2071 | Duplicate struct field name. | Use a unique field name. |
| E2072 | Unsupported struct field type. | Use a field type supported by the experimental struct feature. |
| E2073 | Struct has no supported fields. | Add at least one supported field. |
| E2074 | Wrong number of struct constructor arguments. | Pass one value per field. |
| E2075 | Struct field initializer type mismatch. | Match arguments to declared field types. |
| E2076 | Member access target is not a struct. | Access a field on a value of a declared struct type. |
| E2077 | Unknown struct field. | Correct the field name or declaration. |
| E2078 | Unsupported struct assignment target. | Assign to a supported named struct variable's field. |
| E2079 | Direct printing of struct values is unsupported. | Print individual fields. |
| E2080 | Unknown variant for an enum type. | Use a variant declared by that enum. |
| E2081 | Duplicate enum type name in a module. | Use a unique enum name. |
| E2082 | Duplicate variant in an enum declaration. | Declare each variant only once. |
| E2083 | Empty enum declaration. | Declare at least one variant. |
| E2084 | A `match` arm does not name an enum variant. | Write a valid enum variant in every arm. |
| E2085 | Wrong number of GUI function arguments. | Follow the GUI function's documented signature. |
| E2086 | Conversion function received the wrong number of arguments. | Pass exactly one argument. |
| E2087 | `length` received the wrong number of arguments. | Pass one array argument. |
| E2088 | Standard-library function received the wrong number of arguments. | Follow the function's documented signature. |
| E2089 | Unknown GUI function. | Check the GUI namespace API documentation. |
| E2090 | GUI function argument type mismatch. | Pass values of the types required by the GUI API. |
| E2091 | Direct printing of enum values is unsupported. | Compare variants and print a string label. |
| E2092 | Assignment target is unknown or unsupported. | Assign to a declared mutable variable, array element, or supported struct field. |
| E2093 | Increment operator has an invalid target. | Use `++` on a mutable numeric variable or supported struct field. |
| E2094 | Function parameter expects an array argument. | Pass a declared array variable of the expected element type. |
| E2095 | `+=` target is not an `int` or `float`. | Use `+=` only with numeric variables or fields. |
| E2096 | `length` received a non-array value. | Pass a declared array to `length`. |
| E2097 | Wrong number of math function arguments. | Match the math function's documented arity. |
| E2098 | Math function argument is not a `float`. | Use a float literal or explicitly convert an integer with `to_float`. |
| E2099 | Function array argument has the wrong element type. | Pass an array whose element type matches the parameter. |

Runtime arithmetic and bounds failures currently use descriptive text rather than compiler error codes. Runtime source line and function attribution are not yet implemented.
