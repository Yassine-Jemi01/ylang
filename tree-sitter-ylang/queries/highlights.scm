; Comments
(comment) @comment

; Keywords
[
  "function"
  "let"
  "const"
  "if"
  "else"
  "loop"
  "break"
  "continue"
  "return"
  "mut"
] @keyword

; Borrow operator
"&" @operator

; Keywords @keyword

; Types
(type) @type

; Built-in output statement
"print" @function.builtin

; Function names, parameters, and calls
(function_definition name: (identifier) @function)
(parameter name: (identifier) @variable.parameter)
(call_expression function: (identifier) @function.call)

; Variable declarations, assignments, and variable references
(variable_declaration name: (identifier) @variable)
(assignment_statement left: (identifier) @variable)
(expression (identifier) @variable)

; Literals
(boolean_literal) @boolean
(integer_literal) @number
(float_literal) @number.float
(string_literal) @string
(string_content) @string
(f_string) @string
(f_string_text) @string
(character_literal) @character
(escape_sequence) @string.escape
(f_string_escape) @string.escape
(interpolation) @embedded

; Operators
[
  "and"
  "or"
  "not"
  "+"
  "-"
  "*"
  "/"
  "%"
  "="
  "=="
  "!="
  "<"
  "<="
  ">"
  ">="
  "->"
] @operator

; Punctuation
[
  ";"
  ","
] @punctuation.delimiter

[
  "("
  ")"
  "{"
  "}"
] @punctuation.bracket
