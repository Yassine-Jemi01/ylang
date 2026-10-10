# YLang compiler error codes

YLang diagnostics include stable codes to help users search documentation and report bugs. Wording can improve between releases; documented meanings should remain stable.

## Lexing and parsing

| Code | Meaning | What to do |
|---|---|---|
| E1003 | An expression was expected but the current token cannot start one. | Check the preceding operator and use a supported expression. |
| E1016 | Unknown standard-library namespace. | Use a documented namespace such as `math`, `string`, `io`, `path`, or `image`. |

## Types, names, and expressions

| Code | Meaning | What to do |
|---|---|---|
| E2002 | Unknown type name. | Use a type listed in the language specification. |
| E2004 | Duplicate name in one scope. | Rename or remove the duplicate declaration. |
| E2010 | Integer literal is outside the signed 64-bit range. | Use a representable integer. |
| E2011 | Floating-point literal is invalid or outside the finite range. | Use a finite float literal. |
| E2012 | Character literal is not one byte. | Use one ASCII byte or an escape. |
| E2020 | Unknown variable name. | Declare the variable or correct its spelling. |
| E2027 | Invalid array operation or indexing target. | Index a supported array with an integer index. |
| E2029 | Unsupported array use in a function call or other context. | See array limitations in the language specification. |
| E2040 | Unknown function name. | Check spelling or define the function. |
| E2041 | Wrong number of function or math arguments. | Match the function's documented arity. |
| E2042 | Math function argument is not a float. | Use a float or explicitly call `to_float(int_value)`. |
| E2043 | Wrong number of standard-library or conversion arguments. | Match the function's documented arity. |
| E2044 | A string standard-library function received a non-string argument. | Pass strings. |
| E2045 | An explicit conversion received an unsupported source type. | Check the conversion function's accepted input type. |

Runtime arithmetic and bounds failures currently use descriptive text rather than compiler error codes. Runtime source line and function attribution are not yet implemented.
