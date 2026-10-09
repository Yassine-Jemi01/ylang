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
] @keyword

; Types
(type) @type

; Built-in output statement
"print" @function.builtin

; Function names and parameters
(function_definition name: (identifier) @function)
(parameter name: (identifier) @variable.parameter)
(call_expression function: (identifier) @function.call)

; Variables
(variable_declaration name: (identifier) @variable)
(assignment_statement left: (identifier) @variable)
(identifier) @variable

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
  "."
] @punctuation.delimiter

[
  "("
  ")"
  "{"
  "}"
] @punctuation.bracket
