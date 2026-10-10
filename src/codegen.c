#define _POSIX_C_SOURCE 200809L
#include "internal.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------- C code generation -------------------------- */
static const char *c_base_type(YType type)
{
    switch (type) {
        case TYPE_INT: return "int64_t";
        case TYPE_FLOAT: return "double";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_STRING: return "const char *";
        case TYPE_VOID: return "void";
        case TYPE_ERROR: return "int";
    }
    return "int";
}

static void append_c_quoted(StringBuilder *sb, const char *text)
{
    sb_append(sb, "\"");
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        switch (*p) {
            case '\\': sb_append(sb, "\\\\"); break;
            case '"': sb_append(sb, "\\\""); break;
            case '\n': sb_append(sb, "\\n"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\t': sb_append(sb, "\\t"); break;
            default:
                if (*p < 32 || *p == 127) sb_appendf(sb, "\\%03o", (unsigned)*p);
                else sb_append_n(sb, (const char *)p, 1);
        }
    }
    sb_append(sb, "\"");
}

static const char *format_for_type(YType type)
{
    switch (type) {
        case TYPE_INT: return "%lld";
        case TYPE_FLOAT: return "%.17g";
        case TYPE_BOOL: return "%s";
        case TYPE_CHAR: return "%c";
        case TYPE_STRING: return "%s";
        case TYPE_VOID: case TYPE_ERROR: return "%s";
    }
    return "%s";
}

static void emit_expr(StringBuilder *sb, Expr *expr);

static void append_fstring_format_text(StringBuilder *format, const char *text)
{
    for (const char *p = text; *p; p++) {
        if (*p == '%') sb_append(format, "%%");
        else sb_append_n(format, p, 1);
    }
}

static void emit_fstring_expr(StringBuilder *sb, Expr *expr)
{
    StringBuilder format, args;
    sb_init(&format);
    sb_init(&args);
    size_t arg_count = 0;
    for (size_t i = 0; i < expr->as.fstring.count; i++) {
        FPart *part = &expr->as.fstring.parts[i];
        if (part->text) {
            append_fstring_format_text(&format, part->text);
        } else if (part->expression) {
            Expr *embedded = part->expression;
            sb_append(&format, format_for_type(embedded->type));
            if (arg_count++) sb_append(&args, ", ");
            switch (embedded->type) {
                case TYPE_INT:
                    sb_append(&args, "(long long)("); emit_expr(&args, embedded); sb_append(&args, ")");
                    break;
                case TYPE_FLOAT:
                    sb_append(&args, "(double)("); emit_expr(&args, embedded); sb_append(&args, ")");
                    break;
                case TYPE_BOOL:
                    sb_append(&args, "yl_boolstr("); emit_expr(&args, embedded); sb_append(&args, ")");
                    break;
                case TYPE_CHAR:
                    sb_append(&args, "(int)("); emit_expr(&args, embedded); sb_append(&args, ")");
                    break;
                case TYPE_STRING:
                    sb_append(&args, "(const char *)("); emit_expr(&args, embedded); sb_append(&args, ")");
                    break;
                case TYPE_VOID: case TYPE_ERROR: break;
            }
        }
    }
    sb_append(sb, "yl_format(");
    append_c_quoted(sb, format.data ? format.data : "");
    if (arg_count) {
        sb_append(sb, ", ");
        sb_append_n(sb, args.data, args.length);
    }
    sb_append(sb, ")");
    sb_destroy(&format);
    sb_destroy(&args);
}

static const char *math_c_name(const char *name)
{
    if (strcmp(name, "math.abs") == 0) return "fabs";
    if (strcmp(name, "math.min") == 0) return "fmin";
    if (strcmp(name, "math.max") == 0) return "fmax";
    if (strcmp(name, "math.clamp") == 0) return "yl_math_clamp";
    if (strncmp(name, "math.", 5) == 0) return name + 5;
    return NULL;
}

static const char *builtin_c_name(const char *name)
{
    const char *math_name = math_c_name(name);
    if (math_name) return math_name;
    if (strcmp(name, "string.length") == 0) return "yl_string_length";
    if (strcmp(name, "string.contains") == 0) return "yl_string_contains";
    if (strcmp(name, "string.starts_with") == 0) return "yl_string_starts_with";
    if (strcmp(name, "string.ends_with") == 0) return "yl_string_ends_with";
    if (strcmp(name, "string.concat") == 0) return "yl_string_concat";
    if (strcmp(name, "io.read_line") == 0) return "yl_read_line";
    if (strcmp(name, "io.read_file") == 0) return "yl_read_file";
    if (strcmp(name, "io.write_file") == 0) return "yl_write_file";
    return NULL;
}

static void emit_expr(StringBuilder *sb, Expr *expr)
{
    if (!expr) { sb_append(sb, "0"); return; }
    switch (expr->kind) {
        case EXPR_ERROR: sb_append(sb, "0"); break;
        case EXPR_INT: {
            const char *digits = expr->token.start;
            size_t digits_len = expr->token.length;
            while (digits_len > 1 && *digits == '0') { digits++; digits_len--; }
            sb_append(sb, "INT64_C("); sb_append_n(sb, digits, digits_len); sb_append(sb, ")");
            break;
        }
        case EXPR_FLOAT:
            sb_append_n(sb, expr->token.start, expr->token.length);
            break;
        case EXPR_BOOL:
            sb_append(sb, expr->token.type == TOKEN_TRUE ? "true" : "false");
            break;
        case EXPR_CHAR:
        case EXPR_STRING:
            sb_append_n(sb, expr->token.start, expr->token.length);
            break;
        case EXPR_FSTRING:
            emit_fstring_expr(sb, expr);
            break;
        case EXPR_ARRAY:
            sb_append(sb, "{");
            for (size_t i = 0; i < expr->as.array.count; i++) {
                if (i) sb_append(sb, ", ");
                emit_expr(sb, expr->as.array.items[i]);
            }
            sb_append(sb, "}");
            break;
        case EXPR_INDEX:
            emit_expr(sb, expr->as.index.target);
            sb_append(sb, "[yl_bounds(");
            emit_expr(sb, expr->as.index.index);
            sb_appendf(sb, ", %zu)]", expr->as.index.variable ? expr->as.index.variable->array_length : 0U);
            break;
        case EXPR_NAME:
            sb_append(sb, expr->as.name.variable ? expr->as.name.variable->c_name : "yl_missing_variable");
            break;
        case EXPR_ASSIGN:
            sb_append(sb, "(");
            emit_expr(sb, expr->as.assign.target);
            sb_append(sb, " = "); emit_expr(sb, expr->as.assign.right); sb_append(sb, ")");
            break;
        case EXPR_CALL: {
            const char *builtin_name = builtin_c_name(expr->as.call.name);
            sb_append(sb, expr->as.call.function ? expr->as.call.function->c_name :
                         (builtin_name ? builtin_name : "yl_missing_function"));
            sb_append(sb, "(");
            for (size_t i = 0; i < expr->as.call.count; i++) {
                if (i) sb_append(sb, ", ");
                emit_expr(sb, expr->as.call.args[i]);
            }
            sb_append(sb, ")");
            break;
        }
        case EXPR_UNARY: {
            TokenType op = expr->as.unary.op.type;
            if (expr->is_min_int) {
                sb_append(sb, "INT64_MIN");
            } else if (op == TOKEN_MINUS && expr->type == TYPE_INT) {
                sb_append(sb, "yl_neg_i64("); emit_expr(sb, expr->as.unary.right); sb_append(sb, ")");
            } else {
                sb_append(sb, "(");
                if (op == TOKEN_MINUS) sb_append(sb, "-"); else sb_append(sb, "!");
                emit_expr(sb, expr->as.unary.right); sb_append(sb, ")");
            }
            break;
        }
        case EXPR_BINARY: {
            Expr *left = expr->as.binary.left;
            Expr *right = expr->as.binary.right;
            TokenType op = expr->as.binary.op.type;
            if (expr->type == TYPE_INT && op == TOKEN_PLUS) {
                sb_append(sb, "yl_add_i64("); emit_expr(sb, left); sb_append(sb, ", "); emit_expr(sb, right); sb_append(sb, ")");
            } else if (expr->type == TYPE_INT && op == TOKEN_MINUS) {
                sb_append(sb, "yl_sub_i64("); emit_expr(sb, left); sb_append(sb, ", "); emit_expr(sb, right); sb_append(sb, ")");
            } else if (expr->type == TYPE_INT && op == TOKEN_STAR) {
                sb_append(sb, "yl_mul_i64("); emit_expr(sb, left); sb_append(sb, ", "); emit_expr(sb, right); sb_append(sb, ")");
            } else if (expr->type == TYPE_INT && op == TOKEN_SLASH) {
                sb_append(sb, "yl_div_i64("); emit_expr(sb, left); sb_append(sb, ", "); emit_expr(sb, right); sb_append(sb, ")");
            } else if (expr->type == TYPE_INT && op == TOKEN_PERCENT) {
                sb_append(sb, "yl_mod_i64("); emit_expr(sb, left); sb_append(sb, ", "); emit_expr(sb, right); sb_append(sb, ")");
            } else if (left->type == TYPE_FLOAT && op == TOKEN_SLASH) {
                sb_append(sb, "yl_div_f64("); emit_expr(sb, left); sb_append(sb, ", "); emit_expr(sb, right); sb_append(sb, ")");
            } else if ((op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL) && left->type == TYPE_STRING) {
                sb_append(sb, "(strcmp("); emit_expr(sb, left); sb_append(sb, ", "); emit_expr(sb, right);
                sb_append(sb, ") "); sb_append(sb, op == TOKEN_EQUAL_EQUAL ? "== 0)" : "!= 0)");
            } else {
                sb_append(sb, "("); emit_expr(sb, left); sb_append(sb, " ");
                switch (op) {
                    case TOKEN_PLUS: sb_append(sb, "+"); break;
                    case TOKEN_MINUS: sb_append(sb, "-"); break;
                    case TOKEN_STAR: sb_append(sb, "*"); break;
                    case TOKEN_SLASH: sb_append(sb, "/"); break;
                    case TOKEN_PERCENT: sb_append(sb, "%"); break;
                    case TOKEN_EQUAL_EQUAL: sb_append(sb, "=="); break;
                    case TOKEN_BANG_EQUAL: sb_append(sb, "!="); break;
                    case TOKEN_LESS: sb_append(sb, "<"); break;
                    case TOKEN_LESS_EQUAL: sb_append(sb, "<="); break;
                    case TOKEN_GREATER: sb_append(sb, ">"); break;
                    case TOKEN_GREATER_EQUAL: sb_append(sb, ">="); break;
                    case TOKEN_AND: sb_append(sb, "&&"); break;
                    case TOKEN_OR: sb_append(sb, "||"); break;
                    default: sb_append(sb, "?"); break;
                }
                sb_append(sb, " "); emit_expr(sb, right); sb_append(sb, ")");
            }
            break;
        }
    }
}

static void emit_var_type(FILE *out, VarDecl *var, bool global)
{
    if (global) fputs("static ", out);
    if (var->is_const && var->type != TYPE_STRING) fputs("const ", out);
    fputs(c_base_type(var->type), out);
    if (var->is_const && var->type == TYPE_STRING) fputs(" const", out);
}

static void emit_expr_to_file(FILE *out, Expr *expr)
{
    StringBuilder sb;
    sb_init(&sb);
    emit_expr(&sb, expr);
    fputs(sb.data ? sb.data : "0", out);
    sb_destroy(&sb);
}

static void emit_indent(FILE *out, unsigned indent)
{
    for (unsigned i = 0; i < indent; i++) fputs("    ", out);
}

static void emit_print_value(FILE *out, Expr *expr, unsigned indent)
{
    emit_indent(out, indent);
    switch (expr->type) {
        case TYPE_STRING:
            fputs("fputs((const char *)(", out); emit_expr_to_file(out, expr);
            fputs("), stdout);\n", out); break;
        case TYPE_BOOL:
            fputs("fputs(yl_boolstr(", out); emit_expr_to_file(out, expr);
            fputs("), stdout);\n", out); break;
        case TYPE_CHAR:
            fputs("printf(\"%c\", (int)(", out); emit_expr_to_file(out, expr);
            fputs("));\n", out); break;
        case TYPE_FLOAT:
            fputs("printf(\"%.17g\", (double)(", out); emit_expr_to_file(out, expr);
            fputs("));\n", out); break;
        case TYPE_INT:
            fputs("printf(\"%lld\", (long long)(", out); emit_expr_to_file(out, expr);
            fputs("));\n", out); break;
        case TYPE_VOID: case TYPE_ERROR:
            fputs("(void)0;\n", out); break;
    }
}

static void emit_fstring_print(FILE *out, Expr *expr, unsigned indent)
{
    for (size_t i = 0; i < expr->as.fstring.count; i++) {
        FPart *part = &expr->as.fstring.parts[i];
        if (part->text && part->text[0]) {
            StringBuilder quoted;
            sb_init(&quoted);
            append_c_quoted(&quoted, part->text);
            emit_indent(out, indent);
            fprintf(out, "fputs(%s, stdout);\n", quoted.data ? quoted.data : "\"\"");
            sb_destroy(&quoted);
        } else if (part->expression) {
            emit_print_value(out, part->expression, indent);
        }
    }
}

static void emit_stmt(FILE *out, Stmt *stmt, unsigned indent)
{
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_BLOCK:
            emit_indent(out, indent); fputs("{\n", out);
            for (size_t i = 0; i < stmt->as.block.count; i++)
                emit_stmt(out, stmt->as.block.items[i], indent + 1);
            emit_indent(out, indent); fputs("}\n", out);
            break;
        case STMT_VAR: {
            VarDecl *var = stmt->as.variable;
            emit_indent(out, indent); emit_var_type(out, var, false);
            fprintf(out, " %s", var->c_name);
            if (var->is_array) fprintf(out, "[%zu]", var->array_length);
            if (var->initializer) {
                fputs(" = ", out); emit_expr_to_file(out, var->initializer);
            }
            fputs(";\n", out);
            break;
        }
        case STMT_PRINT:
            for (size_t i = 0; i < stmt->as.print.count; i++) {
                Expr *expr = stmt->as.print.args[i];
                if (i) {
                    emit_indent(out, indent);
                    fputs("fputs(\" \", stdout);\n", out);
                }
                if (expr->kind == EXPR_FSTRING) emit_fstring_print(out, expr, indent);
                else emit_print_value(out, expr, indent);
            }
            emit_indent(out, indent); fputs("putchar('\\n');\n", out);
            break;
        case STMT_IF:
            emit_indent(out, indent); fputs("if (", out); emit_expr_to_file(out, stmt->as.if_stmt.condition); fputs(") ", out);
            fputs("\n", out); emit_stmt(out, stmt->as.if_stmt.then_branch, indent);
            if (stmt->as.if_stmt.else_branch) {
                emit_indent(out, indent); fputs("else ", out); fputs("\n", out);
                emit_stmt(out, stmt->as.if_stmt.else_branch, indent);
            }
            break;
        case STMT_LOOP:
            emit_indent(out, indent); fputs("for (;;) ", out); fputs("\n", out);
            emit_stmt(out, stmt->as.loop_body, indent);
            break;
        case STMT_WHILE:
            emit_indent(out, indent); fputs("while (", out);
            emit_expr_to_file(out, stmt->as.while_stmt.condition);
            fputs(")\n", out);
            emit_stmt(out, stmt->as.while_stmt.body, indent);
            break;
        case STMT_FOR:
            emit_indent(out, indent); fputs("{\n", out);
            if (stmt->as.for_stmt.initializer)
                emit_stmt(out, stmt->as.for_stmt.initializer, indent + 1);
            emit_indent(out, indent + 1); fputs("for (; ", out);
            if (stmt->as.for_stmt.condition)
                emit_expr_to_file(out, stmt->as.for_stmt.condition);
            fputs("; ", out);
            if (stmt->as.for_stmt.increment)
                emit_expr_to_file(out, stmt->as.for_stmt.increment);
            fputs(")\n", out);
            emit_stmt(out, stmt->as.for_stmt.body, indent + 1);
            emit_indent(out, indent); fputs("}\n", out);
            break;
        case STMT_BREAK:
            emit_indent(out, indent); fputs("break;\n", out); break;
        case STMT_CONTINUE:
            emit_indent(out, indent); fputs("continue;\n", out); break;
        case STMT_RETURN:
            emit_indent(out, indent); fputs("return", out);
            if (stmt->as.return_value) { fputc(' ', out); emit_expr_to_file(out, stmt->as.return_value); }
            fputs(";\n", out); break;
        case STMT_EXPR:
            emit_indent(out, indent); emit_expr_to_file(out, stmt->as.expression); fputs(";\n", out); break;
    }
}

static void emit_runtime(FILE *out)
{
    fputs(
        "/* Generated by YLang v1.0.0. This file is compiler output. */\n"
        "#define _POSIX_C_SOURCE 200809L\n"
        "#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n"
        "#include <stdlib.h>\n#include <stdarg.h>\n#include <string.h>\n"
        "#include <limits.h>\n#include <stddef.h>\n#include <math.h>\n\n"
        "static void yl_runtime_error(const char *message) {\n"
        "    fprintf(stderr, \"YLang runtime error: %s\\n\", message);\n"
        "    exit(70);\n}\n"
        "typedef struct YLTracked { void *ptr; struct YLTracked *next; } YLTracked;\n"
        "static YLTracked *yl_tracked = NULL;\n"
        "static void yl_cleanup(void) {\n"
        "    while (yl_tracked) { YLTracked *next = yl_tracked->next; free(yl_tracked->ptr); free(yl_tracked); yl_tracked = next; }\n}\n"
        "static void *yl_track(void *ptr) {\n"
        "    YLTracked *node = malloc(sizeof(*node)); if (!node) yl_runtime_error(\"out of memory\");\n"
        "    node->ptr = ptr; node->next = yl_tracked; yl_tracked = node; return ptr;\n}\n"
        "static const char *yl_boolstr(bool value) { return value ? \"true\" : \"false\"; }\n"
        "static const char *yl_format(const char *format, ...) {\n"
        "    va_list args; va_start(args, format); va_list copy; va_copy(copy, args);\n"
        "    int needed = vsnprintf(NULL, 0, format, copy); va_end(copy);\n"
        "    if (needed < 0) { va_end(args); yl_runtime_error(\"formatting failed\"); }\n"
        "    char *buffer = malloc((size_t)needed + 1); if (!buffer) { va_end(args); yl_runtime_error(\"out of memory\"); }\n"
        "    (void)vsnprintf(buffer, (size_t)needed + 1, format, args); va_end(args);\n"
        "    return (const char *)yl_track(buffer);\n}\n"
        "static int64_t yl_add_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_add_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in addition\"); return r; }\n"
        "static int64_t yl_sub_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_sub_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in subtraction\"); return r; }\n"
        "static int64_t yl_mul_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_mul_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in multiplication\"); return r; }\n"
        "static int64_t yl_neg_i64(int64_t a) { if (a == INT64_MIN) yl_runtime_error(\"integer overflow in negation\"); return -a; }\n"
        "static int64_t yl_div_i64(int64_t a, int64_t b) { if (!b) yl_runtime_error(\"integer division by zero\"); if (a == INT64_MIN && b == -1) yl_runtime_error(\"integer overflow in division\"); return a / b; }\n"
        "static int64_t yl_mod_i64(int64_t a, int64_t b) { if (!b) yl_runtime_error(\"integer modulo by zero\"); if (a == INT64_MIN && b == -1) return 0; return a % b; }\n"
        "static double yl_div_f64(double a, double b) { if (b == 0.0) yl_runtime_error(\"floating-point division by zero\"); return a / b; }\n"
        "static size_t yl_bounds(int64_t index, size_t length) { if (index < 0 || (uint64_t)index >= (uint64_t)length) yl_runtime_error(\"array index out of bounds\"); return (size_t)index; }\n"
        "static double yl_math_clamp(double value, double low, double high) { if (low > high) yl_runtime_error(\"math.clamp lower bound exceeds upper bound\"); return fmin(fmax(value, low), high); }\n"
        "static int64_t yl_string_length(const char *value) { return (int64_t)strlen(value); }\n"
        "static bool yl_string_contains(const char *value, const char *needle) { return strstr(value, needle) != NULL; }\n"
        "static bool yl_string_starts_with(const char *value, const char *prefix) { size_t n = strlen(prefix); return strncmp(value, prefix, n) == 0; }\n"
        "static bool yl_string_ends_with(const char *value, const char *suffix) { size_t n = strlen(value), m = strlen(suffix); return m <= n && memcmp(value + n - m, suffix, m) == 0; }\n"
        "static const char *yl_string_concat(const char *left, const char *right) { return yl_format(\"%s%s\", left, right); }\n"
        "static const char *yl_read_line(void) { char *line = NULL; size_t capacity = 0; ssize_t got = getline(&line, &capacity, stdin); if (got < 0) { if (ferror(stdin)) { free(line); yl_runtime_error(\"failed to read standard input\"); } free(line); line = malloc(1); if (!line) yl_runtime_error(\"out of memory\"); line[0] = '\\0'; return (const char *)yl_track(line); } while (got > 0 && (line[got - 1] == '\\n' || line[got - 1] == '\\r')) line[--got] = '\\0'; return (const char *)yl_track(line); }\n"
        "static const char *yl_read_file(const char *path) { FILE *file = fopen(path, \"rb\"); if (!file) yl_runtime_error(\"cannot open file for reading\"); if (fseek(file, 0, SEEK_END) != 0) { fclose(file); yl_runtime_error(\"cannot seek input file\"); } long end = ftell(file); if (end < 0 || (uintmax_t)end >= (uintmax_t)SIZE_MAX) { fclose(file); yl_runtime_error(\"input file is too large\"); } rewind(file); size_t size = (size_t)end; char *data = malloc(size + 1); if (!data) { fclose(file); yl_runtime_error(\"out of memory\"); } size_t got = fread(data, 1, size, file); bool failed = ferror(file) != 0 || got != size; fclose(file); if (failed) { free(data); yl_runtime_error(\"failed to read input file\"); } if (memchr(data, '\\0', size)) { free(data); yl_runtime_error(\"binary files are not supported by string I/O\"); } data[size] = '\\0'; return (const char *)yl_track(data); }\n"
        "static bool yl_write_file(const char *path, const char *content) { FILE *file = fopen(path, \"wb\"); if (!file) return false; size_t size = strlen(content); bool ok = fwrite(content, 1, size, file) == size; if (fclose(file) != 0) ok = false; return ok; }\n\n",
        out);
}

bool generate_c(Compiler *c, const char *path)
{
    FILE *out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "ylang: cannot write generated C file '%s': %s\n", path, strerror(errno));
        return false;
    }
    emit_runtime(out);

    for (size_t i = 0; i < c->program->global_count; i++) {
        VarDecl *var = c->program->globals[i];
        emit_var_type(out, var, true);
        fprintf(out, " %s", var->c_name);
        if (var->is_array) fprintf(out, "[%zu]", var->array_length);
        if (var->initializer) {
            fputs(" = ", out); emit_expr_to_file(out, var->initializer);
        } else if (var->type == TYPE_STRING) {
            fputs(" = \"\"", out);
        } else if (var->type == TYPE_FLOAT) {
            fputs(" = 0.0", out);
        } else if (var->type == TYPE_BOOL) {
            fputs(" = false", out);
        } else if (var->type == TYPE_CHAR) {
            fputs(" = '\\0'", out);
        } else if (var->type == TYPE_INT) {
            fputs(" = INT64_C(0)", out);
        }
        fputs(";\n", out);
    }
    if (c->program->global_count) fputc('\n', out);

    for (size_t i = 0; i < c->program->function_count; i++) {
        Function *fn = c->program->functions[i];
        fprintf(out, "%s %s(", c_base_type(fn->return_type), fn->c_name);
        if (fn->param_count == 0) fputs("void", out);
        for (size_t j = 0; j < fn->param_count; j++) {
            if (j) fputs(", ", out);
            fprintf(out, "%s %s", c_base_type(fn->params[j]->type), fn->params[j]->c_name);
        }
        fputs(");\n", out);
    }
    fputc('\n', out);

    for (size_t i = 0; i < c->program->function_count; i++) {
        Function *fn = c->program->functions[i];
        fprintf(out, "%s %s(", c_base_type(fn->return_type), fn->c_name);
        if (fn->param_count == 0) fputs("void", out);
        for (size_t j = 0; j < fn->param_count; j++) {
            if (j) fputs(", ", out);
            fprintf(out, "%s %s", c_base_type(fn->params[j]->type), fn->params[j]->c_name);
        }
        fputs(") ", out); fputc('\n', out);
        emit_stmt(out, fn->body, 0);
        fputc('\n', out);
    }

    Function *main_fn = find_function(c, "main");
    if (main_fn && main_fn->return_type == TYPE_VOID) {
        fprintf(out, "int main(void) { %s(); yl_cleanup(); return 0; }\n", main_fn->c_name);
    } else if (main_fn) {
        fprintf(out, "int main(void) { int64_t result = %s(); yl_cleanup(); return (int)result; }\n", main_fn->c_name);
    }

    bool okay = !ferror(out);
    if (fclose(out) != 0) okay = false;
    if (!okay) fprintf(stderr, "ylang: failed while writing '%s'\n", path);
    return okay;
}

