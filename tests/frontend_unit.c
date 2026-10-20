#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(const char *message)
{
    fprintf(stderr, "frontend unit test failed: %s\n", message);
    return 1;
}

static int test_lexer(void)
{
    const char *source = "let count = 12 + 3;\n// ignored comment\ncount++ += count -> count == 3\n";
    const TokenType expected[] = {
        TOKEN_LET, TOKEN_IDENTIFIER, TOKEN_EQUAL, TOKEN_NUMBER,
        TOKEN_PLUS, TOKEN_NUMBER, TOKEN_SEMICOLON, TOKEN_NEWLINE,
        TOKEN_NEWLINE, TOKEN_IDENTIFIER, TOKEN_PLUS_PLUS, TOKEN_PLUS_EQUAL,
        TOKEN_IDENTIFIER, TOKEN_ARROW, TOKEN_IDENTIFIER, TOKEN_EQUAL_EQUAL,
        TOKEN_NUMBER, TOKEN_NEWLINE, TOKEN_EOF
    };
    Lexer lexer = lexer_init(source);
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        Token token = lexer_next(&lexer);
        if (token.type != expected[i]) {
            fprintf(stderr, "lexer token %zu: expected %s, got %s at %zu:%zu\n",
                    i, token_type_name(expected[i]), token_type_name(token.type),
                    token.line, token.column);
            return 1;
        }
    }

    Lexer bad_comment = lexer_init("/* never closed");
    Token error = lexer_next(&bad_comment);
    if (error.type != TOKEN_ERROR || !error.message ||
        strstr(error.message, "Unterminated block comment") == NULL) {
        return fail("unterminated block comment must return a lexer error token");
    }
    return 0;
}

static int test_gui_builtin_table(void)
{
    if (ylang_gui_builtin_count == 0U) {
        return fail("GUI builtin table must not be empty");
    }

    for (size_t i = 0; i < ylang_gui_builtin_count; i++) {
        const GuiBuiltinInfo *builtin = &ylang_gui_builtins[i];
        if (!builtin->name || strncmp(builtin->name, "gui.", 4U) != 0 ||
            !builtin->c_symbol || strncmp(builtin->c_symbol, "ylgui_api_", 10U) != 0) {
            return fail("each GUI builtin must define a GUI name and C ABI symbol");
        }
        if (builtin->arity > sizeof(builtin->args) / sizeof(builtin->args[0])) {
            return fail("GUI builtin arity must fit its argument type array");
        }
        if (find_gui_builtin_info(builtin->name) != builtin) {
            return fail("GUI builtin lookup must return the canonical table entry");
        }

        for (size_t j = i + 1U; j < ylang_gui_builtin_count; j++) {
            if (strcmp(builtin->name, ylang_gui_builtins[j].name) == 0) {
                return fail("GUI builtin names must be unique");
            }
            if (strcmp(builtin->c_symbol, ylang_gui_builtins[j].c_symbol) == 0) {
                return fail("GUI builtin C ABI symbols must be unique");
            }
        }
    }

    const GuiBuiltinInfo *create_window = find_gui_builtin_info("gui.create_window");
    if (!create_window || create_window->arity != 3U ||
        create_window->result != TYPE_BOOL ||
        create_window->args[0] != TYPE_STRING ||
        create_window->args[1] != TYPE_INT ||
        create_window->args[2] != TYPE_INT ||
        strcmp(create_window->c_symbol, "ylgui_api_create_window") != 0) {
        return fail("gui.create_window signature and C symbol must stay consistent");
    }

    const GuiBuiltinInfo *draw_rect = find_gui_builtin_info("gui.draw_rect");
    if (!draw_rect || draw_rect->arity != 7U ||
        draw_rect->result != TYPE_BOOL) {
        return fail("gui.draw_rect must expose seven integer arguments and return bool");
    }
    for (size_t i = 0; i < 7U; i++) {
        if (draw_rect->args[i] != TYPE_INT) {
            return fail("gui.draw_rect arguments must all be integers");
        }
    }

    const GuiBuiltinInfo *close = find_gui_builtin_info("gui.close");
    if (!close || close->arity != 0U || close->result != TYPE_VOID ||
        find_gui_builtin_info("gui.not_a_real_function") != NULL ||
        find_gui_builtin_info(NULL) != NULL) {
        return fail("GUI builtin lookup must handle void and unknown entries safely");
    }

    return 0;
}

static int test_parser(void)
{
    const char *source =
        "let int base = 2;\n"
        "enum Direction { North, East }\n"
        "struct Point { int x; int y; }\n"
        "function main() -> int {\n"
        "    let Point p = Point(base, 3);\n"
        "    return p.x;\n"
        "}\n";
    Compiler compiler = {0};
    compiler.filename = "frontend-unit.yl";
    compiler.source = (char *)source;
    compiler.source_length = strlen(source);

    Program *program = parse_program(&compiler);
    const int failed = compiler.had_error || !program ||
                       program->global_count != 1 ||
                       program->function_count != 1 ||
                       compiler.enum_count != 1 ||
                       compiler.struct_count != 1;
    arena_destroy(&compiler.arena);
    if (failed) return fail("valid global, enum, struct, and function declarations should parse");
    return 0;
}

int main(void)
{
    if (test_lexer() != 0) return EXIT_FAILURE;
    if (test_gui_builtin_table() != 0) return EXIT_FAILURE;
    if (test_parser() != 0) return EXIT_FAILURE;
    puts("Frontend unit tests passed.");
    return EXIT_SUCCESS;
}
