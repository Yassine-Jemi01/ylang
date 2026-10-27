module.exports = grammar({
  name: "ylang",

  extras: $ => [/\s/, $.line_comment, $.block_comment],

  word: $ => $.identifier,

  conflicts: $ => [
    [$._type, $.identifier],
    [$.call_expression, $.struct_initializer],
    [$.array_type, $.array_expression],
  ],

  rules: {
    source_file: $ => repeat($._statement),

    _statement: $ => choice(
      $.function_declaration,
      $.struct_declaration,
      $.enum_declaration,
      $.variable_declaration,
      $.import_declaration,
      $.from_import_declaration,
      $.if_statement,
      $.while_statement,
      $.for_statement,
      $.loop_statement,
      $.match_statement,
      $.return_statement,
      $.break_statement,
      $.continue_statement,
      $.expression_statement,
      $.block
    ),

    function_declaration: $ => seq(
      "function", field("name", $.identifier),
      "(", optional($.parameter_list), ")",
      optional(seq("->", field("return_type", $._type))),
      $.block
    ),
    parameter_list: $ => commaSep1($.parameter),
    parameter: $ => seq(field("type", $._type), field("name", $.identifier)),
    struct_declaration: $ => seq(
      "struct", field("name", $.type_identifier), "{",
      repeat($.field_declaration), "}"
    ),
    field_declaration: $ => seq($._type, $.identifier, optional("[]"), ";"),
    enum_declaration: $ => seq(
      "enum", field("name", $.type_identifier), "{",
      commaSep1($.enum_variant), optional(","), "}"
    ),
    enum_variant: $ => $.identifier,
    variable_declaration: $ => seq(
      "let", optional("const"), field("type", $._type),
      field("name", $.identifier), optional("[]"),
      optional(seq("=", $._expression)), ";"
    ),
    import_declaration: $ => seq("import", $.string, ";"),
    from_import_declaration: $ => seq(
      "from", choice($.identifier, $.string),
      "import", commaSep1($.identifier), ";"
    ),
    if_statement: $ => seq("if", "(", $._expression, ")", $.block,
      optional(seq("else", choice($.block, $.if_statement)))),
    while_statement: $ => seq("while", "(", $._expression, ")", $.block),
    for_statement: $ => seq("for", "(", optional($._expression), ";",
      optional($._expression), ";", optional($._expression), ")", $.block),
    loop_statement: $ => seq("loop", $.block),
    match_statement: $ => seq("match", "(", $._expression, ")", "{",
      repeat($.match_arm), "}"),
    match_arm: $ => seq(
      field("pattern", choice($.identifier, $.qualified_identifier, $.integer, $.string, "true", "false", "_")),
      "->", choice($.block, $._expression), optional(",")
    ),
    return_statement: $ => seq("return", optional($._expression), ";"),
    break_statement: $ => seq("break", ";"),
    continue_statement: $ => seq("continue", ";"),
    block: $ => seq("{", repeat($._statement), "}"),
    expression_statement: $ => seq($._expression, ";"),

    _type: $ => choice(
      $.primitive_type, $.type_identifier,
      $.array_type, $.qualified_type
    ),
    primitive_type: $ => choice("int", "float", "bool", "char", "string", "void", "http_task"),
    type_identifier: $ => /[A-Z][A-Za-z0-9_]*/,
    qualified_type: $ => seq($.type_identifier, ".", $.type_identifier),
    array_type: $ => seq(choice($.primitive_type, $.type_identifier), "[]"),

    _expression: $ => choice(
      $.assignment_expression,
      $.binary_expression,
      $.unary_expression,
      $.await_expression,
      $.call_expression,
      $.member_expression,
      $.index_expression,
      $.struct_initializer,
      $.array_expression,
      $.parenthesized_expression,
      $.identifier,
      $.qualified_identifier,
      $.integer,
      $.float,
      $.string,
      $.character,
      $.format_string,
      $.boolean
    ),
    assignment_expression: $ => prec.right(1, seq(
      field("left", choice($.identifier, $.member_expression, $.index_expression)),
      field("operator", choice("=", "+=", "-=", "*=", "/=")),
      field("right", $._expression)
    )),
    binary_expression: $ => {
      const table = [
        [prec.left, 7, choice("*", "/", "%")],
        [prec.left, 6, choice("+", "-")],
        [prec.left, 5, choice("<", "<=", ">", ">=")],
        [prec.left, 4, choice("==", "!=")],
        [prec.left, 3, "and"],
        [prec.left, 2, "or"],
      ];
      return choice(...table.map(([fn, precedence, operator]) =>
        fn(precedence, seq(
          field("left", $._expression),
          field("operator", operator),
          field("right", $._expression)
        ))
      ));
    },
    unary_expression: $ => prec(8, seq(choice("!", "-", "not"), $._expression)),
    await_expression: $ => prec(8, seq("await", $._expression)),
    call_expression: $ => prec(9, seq(
      field("function", choice($.identifier, $.member_expression, $.qualified_identifier)),
      "(", optional($.argument_list), ")"
    )),
    argument_list: $ => commaSep1($._expression),
    member_expression: $ => prec.left(10, seq($._expression, ".", field("property", $.identifier))),
    qualified_identifier: $ => prec(11, seq($.identifier, ".", $.identifier)),
    index_expression: $ => prec.left(10, seq($._expression, "[", $._expression, "]")),
    struct_initializer: $ => prec(2, seq($.type_identifier, "(", optional($.argument_list), ")")),
    array_expression: $ => seq("[", optional(commaSep1($._expression)), "]"),
    parenthesized_expression: $ => seq("(", $._expression, ")"),
    integer: $ => /(?:0[xX][0-9a-fA-F]+|[0-9]+)/,
    float: $ => /[0-9]+\.[0-9]+(?:[eE][+-]?[0-9]+)?/,
    string: $ => seq('"', repeat(choice($.escape_sequence, /[^"\\\n]/)), '"'),
    format_string: $ => seq('f"', repeat(choice($.interpolation, $.escape_sequence, /[^"\\\n{}]/)), '"'),
    interpolation: $ => seq("{", $._expression, "}"),
    character: $ => seq("'", choice($.escape_sequence, /[^'\\\n]/), "'"),
    escape_sequence: $ => /\\(?:[\\'"nrt0]|x[0-9a-fA-F]{2})/,
    boolean: $ => choice("true", "false"),
    identifier: $ => /[A-Za-z_][A-Za-z0-9_]*/,
    line_comment: $ => token(seq("//", /.*/)),
    block_comment: $ => token(seq("/*", /[^*]*\*+([^/*][^*]*\*+)*/, "/")),
  }
});

function commaSep1(rule) {
  return seq(rule, repeat(seq(",", rule)), optional(","));
}
