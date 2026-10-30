[
  (line_comment)
  (block_comment)
] @comment

(function_declaration name: (identifier) @function)
(parameter name: (identifier) @variable.parameter)
(struct_declaration name: (type_identifier) @type)
(enum_declaration name: (type_identifier) @type)
(enum_variant) @constant
(field_declaration (identifier) @property)
(variable_binding name: (identifier) @variable)
(primitive_type) @type
(type_identifier) @type
(qualified_type) @type
(import_declaration) @keyword
(from_import_declaration) @keyword
[
  "fn" "function" "return" "let" "mut" "const" "if" "else" "loop" "while" "for"
  "break" "continue" "defer" "and" "or" "not" "await" "struct" "enum" "match"
  "from" "import"
] @keyword
(boolean) @boolean
(integer) @number
(float) @number
(string) @string
(format_string) @string
(character) @string
(escape_sequence) @string.escape
(call_expression function: (identifier) @function)
(member_expression property: (identifier) @property)
[
  "=" "+=" "-=" "*=" "/=" "+" "-" "*" "/" "%" "==" "!=" "<" "<=" ">" ">=" "++" "->" "!" ":"
] @operator
