/**
 * Tree-sitter grammar for YLang 1.0.
 * https://github.com/Yassine-Jemi01/ylang
 * SPDX-License-Identifier: MIT
 */

const PREC = {
  OR: 1,
  AND: 2,
  EQUALITY: 3,
  COMPARISON: 4,
  ADD: 5,
  MULTIPLY: 6,
  UNARY: 7,
  CALL: 8
};

module.exports = grammar({
  name: "ylang",

  extras: $ => [
    /\s/,
    $.comment
  ],

  word: $ => $.identifier,

  rules: {
    source_file: $ => repeat(choice(
      $.function_definition,
      $.variable_declaration
    )),

    comment: $ => token(seq("//", /[^\r\n]*/)),

    function_definition: $ => seq(
      "function",
      field("name", $.identifier),
      "(",
      optional($.parameter_list),
      ")",
      "->",
      field("return_type", $.type),
      $.block
    ),

    parameter_list: $ => commaSep1($.parameter),

    parameter: $ => seq(
      optional(seq("&", optional("mut"))),
      field("type", $.type),
      field("name", $.identifier)
    ),

    type: $ => choice(
      seq(choice("int", "float", "bool", "char", "string"), "[", "]"),
      "int",
      "float",
      "bool",
      "char",
      "string",
      "void"
    ),

    variable_declaration: $ => seq(
      "let",
      optional("const"),
      field("type", $.type),
      field("name", $.identifier),
      optional(seq("=", field("value", $.expression))),
      ";"
    ),

    block: $ => seq(
      "{",
      repeat($.statement),
      "}"
    ),

    statement: $ => choice(
      $.variable_declaration,
      $.print_statement,
      $.assignment_statement,
      $.expression_statement,
      $.if_statement,
      $.loop_statement,
      $.break_statement,
      $.continue_statement,
      $.return_statement
    ),

    print_statement: $ => seq(
      "print",
      "(",
      field("argument", $.expression),
      repeat(seq(",", field("argument", $.expression))),
      ")",
      ";"
    ),

    assignment_statement: $ => seq(
      field("left", $.identifier),
      "=",
      field("right", $.expression),
      ";"
    ),

    expression_statement: $ => seq(
      $.expression,
      ";"
    ),

    if_statement: $ => seq(
      "if",
      "(",
      field("condition", $.expression),
      ")",
      $.block,
      optional($.else_clause)
    ),

    else_clause: $ => choice(
      seq(
        "else",
        "if",
        "(",
        field("condition", $.expression),
        ")",
        $.block,
        optional($.else_clause)
      ),
      seq("else", $.block)
    ),

    loop_statement: $ => seq(
      "loop",
      "(",
      ")",
      $.block
    ),

    break_statement: $ => seq("break", ";"),

    continue_statement: $ => seq("continue", ";"),

    return_statement: $ => seq(
      "return",
      optional(field("value", $.expression)),
      ";"
    ),

    expression: $ => choice(
      $.binary_expression,
      $.unary_expression,
      $.call_expression,
      $.borrow_expression,
      $.array_literal,
      $.index_expression,
      $.parenthesized_expression,
      $.identifier,
      $.integer_literal,
      $.float_literal,
      $.boolean_literal,
      $.character_literal,
      $.string_literal,
      $.f_string
    ),

    binary_expression: $ => choice(
      prec.left(PREC.OR, seq(
        field("left", $.expression),
        field("operator", "or"),
        field("right", $.expression)
      )),
      prec.left(PREC.AND, seq(
        field("left", $.expression),
        field("operator", "and"),
        field("right", $.expression)
      )),
      prec.left(PREC.EQUALITY, seq(
        field("left", $.expression),
        field("operator", choice("==", "!=")),
        field("right", $.expression)
      )),
      prec.left(PREC.COMPARISON, seq(
        field("left", $.expression),
        field("operator", choice("<", "<=", ">", ">=")),
        field("right", $.expression)
      )),
      prec.left(PREC.ADD, seq(
        field("left", $.expression),
        field("operator", choice("+", "-")),
        field("right", $.expression)
      )),
      prec.left(PREC.MULTIPLY, seq(
        field("left", $.expression),
        field("operator", choice("*", "/", "%")),
        field("right", $.expression)
      ))
    ),

    unary_expression: $ => prec(PREC.UNARY, seq(
      field("operator", choice("-", "not")),
      field("argument", $.expression)
    )),

    call_expression: $ => prec(PREC.CALL, seq(
      field("function", $.identifier),
      field("arguments", $.argument_list)
    )),

    array_literal: $ => seq(
      "[",
      commaSep1($.expression),
      "]"
    ),

    index_expression: $ => prec(PREC.CALL, seq(
      field("array", $.expression),
      "[",
      field("index", $.expression),
      "]"
    )),

    borrow_expression: $ => choice(
      prec(PREC.UNARY, seq(
        "&",
        token(prec(2, "mut")),
        field("target", $.identifier)
      )),
      prec(PREC.UNARY, seq(
        "&",
        field("target", $.identifier)
      ))
    ),

    argument_list: $ => seq(
      "(",
      optional(commaSep1($.expression)),
      ")"
    ),

    parenthesized_expression: $ => seq(
      "(",
      $.expression,
      ")"
    ),

    boolean_literal: $ => choice("true", "false"),

    integer_literal: $ => token(/[0-9]+/),

    float_literal: $ => token(prec(2, /[0-9]+\.[0-9]+/)),

    character_literal: $ => token(seq(
      "'",
      choice(/[^'\\\r\n]/, /\\[^\r\n]/),
      "'"
    )),

    string_literal: $ => seq(
      '"',
      repeat(choice($.string_content, $.escape_sequence)),
      '"'
    ),

    string_content: $ => token(prec(1, /[^"\\\r\n]+/)),

    f_string: $ => seq(
      'f"',
      repeat(choice(
        $.interpolation,
        $.f_string_escape,
        $.f_string_text,
        $.escape_sequence
      )),
      '"'
    ),

    f_string_text: $ => token(prec(1, /[^"{}\\\r\n]+/)),

    f_string_escape: $ => token(prec(2, choice(
      "{{",
      "}}",
      "\\{",
      "\\}"
    ))),

    interpolation: $ => seq(
      "{",
      $.expression,
      "}"
    ),

    escape_sequence: $ => token(/\\[^\r\n]/),

    identifier: $ => /[A-Za-z_][A-Za-z0-9_]*/
  }
});

function commaSep1(rule) {
  return seq(rule, repeat(seq(",", rule)));
}
