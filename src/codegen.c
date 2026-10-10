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
        case TYPE_INT_ARRAY: case TYPE_FLOAT_ARRAY: case TYPE_BOOL_ARRAY:
        case TYPE_CHAR_ARRAY: case TYPE_STRING_ARRAY: return "YLArray";
        case TYPE_ERROR: return "int";
    }
    return "int";
}

static bool type_is_array(YType type)
{
    return type == TYPE_INT_ARRAY || type == TYPE_FLOAT_ARRAY ||
           type == TYPE_BOOL_ARRAY || type == TYPE_CHAR_ARRAY ||
           type == TYPE_STRING_ARRAY;
}

static YType array_element_type(YType type)
{
    switch (type) {
        case TYPE_INT_ARRAY: return TYPE_INT;
        case TYPE_FLOAT_ARRAY: return TYPE_FLOAT;
        case TYPE_BOOL_ARRAY: return TYPE_BOOL;
        case TYPE_CHAR_ARRAY: return TYPE_CHAR;
        case TYPE_STRING_ARRAY: return TYPE_STRING;
        default: return TYPE_ERROR;
    }
}

static int array_kind(YType type)
{
    switch (type) {
        case TYPE_INT_ARRAY: return 1;
        case TYPE_FLOAT_ARRAY: return 2;
        case TYPE_BOOL_ARRAY: return 3;
        case TYPE_CHAR_ARRAY: return 4;
        case TYPE_STRING_ARRAY: return 5;
        default: return 0;
    }
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
        case TYPE_INT_ARRAY: case TYPE_FLOAT_ARRAY: case TYPE_BOOL_ARRAY:
        case TYPE_CHAR_ARRAY: case TYPE_STRING_ARRAY:
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
        case EXPR_NAME:
            if (expr->as.name.variable && expr->as.name.variable->is_borrowed) {
                sb_append(sb, "(*");
                sb_append(sb, expr->as.name.variable->c_name);
                sb_append(sb, ")");
            } else {
                sb_append(sb, expr->as.name.variable ? expr->as.name.variable->c_name : "yl_missing_variable");
            }
            break;
        case EXPR_BORROW:
            sb_append(sb, "&(");
            emit_expr(sb, expr->as.borrow.target);
            sb_append(sb, ")");
            break;
        case EXPR_ASSIGN:
            sb_append(sb, "(");
            emit_expr(sb, expr->as.assign.target);
            sb_append(sb, " = "); emit_expr(sb, expr->as.assign.right); sb_append(sb, ")");
            break;
        case EXPR_ARRAY: {
            YType element = expr->as.array.element_type;
            if (expr->as.array.count == 0) {
                sb_appendf(sb, "yl_array_make(sizeof(%s), NULL, 0, %d)",
                           c_base_type(element), array_kind(expr->type));
                break;
            }
            sb_appendf(sb, "yl_array_make(sizeof(%s), (%s[]){",
                       c_base_type(element), c_base_type(element));
            for (size_t i = 0; i < expr->as.array.count; i++) {
                if (i) sb_append(sb, ", ");
                emit_expr(sb, expr->as.array.items[i]);
            }
            sb_appendf(sb, "}, %zu, %d)", expr->as.array.count, array_kind(expr->type));
            break;
        }
        case EXPR_INDEX: {
            YType element = expr->as.index.element_type;
            sb_appendf(sb, "(*((%s *)yl_array_at(", c_base_type(element));
            emit_expr(sb, expr->as.index.array);
            sb_append(sb, ", ");
            emit_expr(sb, expr->as.index.index);
            sb_appendf(sb, ", %d)))", array_kind(expr->as.index.array->type));
            break;
        }
        case EXPR_CALL:
            if (!expr->as.call.function && strcmp(expr->as.call.name, "append") == 0 &&
                expr->as.call.count == 2 && expr->as.call.args[0]->kind == EXPR_NAME) {
                Expr *array_arg = expr->as.call.args[0];
                Expr *value_arg = expr->as.call.args[1];
                YType element = array_element_type(array_arg->type);
                sb_append(sb, "yl_array_append(&");
                emit_expr(sb, array_arg);
                sb_appendf(sb, ", &(%s){", c_base_type(element));
                emit_expr(sb, value_arg);
                sb_appendf(sb, "}, sizeof(%s), %d)", c_base_type(element), array_kind(array_arg->type));
                break;
            }
            if (expr->as.call.function) {
                sb_append(sb, expr->as.call.function->c_name);
            } else if (strcmp(expr->as.call.name, "read_file") == 0) {
                sb_append(sb, "yl_read_file");
            } else if (strcmp(expr->as.call.name, "write_file") == 0) {
                sb_append(sb, "yl_write_file");
            } else if (strcmp(expr->as.call.name, "read_line") == 0) {
                sb_append(sb, "yl_read_line");
            } else if (strcmp(expr->as.call.name, "parse_int") == 0) {
                sb_append(sb, "yl_parse_int");
            } else if (strcmp(expr->as.call.name, "parse_float") == 0) {
                sb_append(sb, "yl_parse_float");
            } else if (strcmp(expr->as.call.name, "len") == 0) {
                if (expr->as.call.count == 1 && type_is_array(expr->as.call.args[0]->type))
                    sb_append(sb, "yl_array_len");
                else
                    sb_append(sb, "yl_len_string");
            } else if (strcmp(expr->as.call.name, "clone") == 0) {
                if (expr->as.call.count == 1 && type_is_array(expr->as.call.args[0]->type))
                    sb_append(sb, "yl_array_clone");
                else
                    sb_append(sb, "yl_clone_string");
            } else {
                sb_append(sb, "yl_missing_function");
            }
            sb_append(sb, "(");
            for (size_t i = 0; i < expr->as.call.count; i++) {
                if (i) sb_append(sb, ", ");
                emit_expr(sb, expr->as.call.args[i]);
            }
            sb_append(sb, ")");
            break;
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

static void emit_param_type(FILE *out, VarDecl *param)
{
    if (!param->is_borrowed) {
        fputs(c_base_type(param->type), out);
        return;
    }
    if (param->type == TYPE_STRING) {
        fputs(param->is_mut_borrow ? "const char **" : "const char * const *", out);
        return;
    }
    if (!param->is_mut_borrow) fputs("const ", out);
    fputs(c_base_type(param->type), out);
    fputs(" *", out);
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
        case TYPE_INT_ARRAY: case TYPE_FLOAT_ARRAY: case TYPE_BOOL_ARRAY:
        case TYPE_CHAR_ARRAY: case TYPE_STRING_ARRAY:
            fputs("printf(\"[array len=%lld]\", (long long)yl_array_len(", out);
            emit_expr_to_file(out, expr); fputs("));\n", out); break;
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
        case STMT_FOR_EACH: {
            VarDecl *variable = stmt->as.for_each.variable;
            Expr *array = stmt->as.for_each.array;
            YType element = array_element_type(array->type);
            char index_name[128], end_name[128];
            (void)snprintf(index_name, sizeof(index_name), "yl_idx_%s", variable->c_name);
            (void)snprintf(end_name, sizeof(end_name), "yl_end_%s", variable->c_name);
            emit_indent(out, indent);
            fprintf(out, "for (size_t %s = 0, %s = (", index_name, end_name);
            emit_expr_to_file(out, array);
            fprintf(out, ").len; %s < %s; %s++) {\n", index_name, end_name, index_name);
            emit_indent(out, indent + 1);
            emit_var_type(out, variable, false);
            fprintf(out, " %s = *((%s *)yl_array_at(", variable->c_name, c_base_type(element));
            emit_expr_to_file(out, array);
            fprintf(out, ", (int64_t)%s, %d));\n", index_name, array_kind(array->type));
            emit_stmt(out, stmt->as.for_each.body, indent + 1);
            emit_indent(out, indent);
            fputs("}\n", out);
            break;
        }
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
        "/* Generated by YLang development compiler. */\n"
        "#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n"
        "#include <stdlib.h>\n#include <stdarg.h>\n#include <string.h>\n#include <errno.h>\n#include <math.h>\n"
        "#include <limits.h>\n\n"
        "typedef struct { void *data; size_t len; size_t cap; int kind; } YLArray;\n"
        "typedef struct YLTracked { void *ptr; struct YLTracked *next; } YLTracked;\n"
        "typedef struct YLTrackedArray { void *ptr; struct YLTrackedArray *next; } YLTrackedArray;\n"
        "static YLTracked *yl_tracked = NULL;\nstatic YLTrackedArray *yl_arrays = NULL;\n"
        "static void yl_runtime_error(const char *message) { fprintf(stderr, \"YLang runtime error: %s\\n\", message); exit(70); }\n"
        "static void yl_cleanup(void) {\n"
        "    while (yl_tracked) { YLTracked *next = yl_tracked->next; free(yl_tracked->ptr); free(yl_tracked); yl_tracked = next; }\n"
        "    while (yl_arrays) { YLTrackedArray *next = yl_arrays->next; free(yl_arrays->ptr); free(yl_arrays); yl_arrays = next; }\n"
        "}\n"
        "static void *yl_track(void *ptr) { YLTracked *node = malloc(sizeof(*node)); if (!node) yl_runtime_error(\"out of memory\"); node->ptr = ptr; node->next = yl_tracked; yl_tracked = node; return ptr; }\n"
        "static void yl_track_array_buffer(void *ptr) { YLTrackedArray *node = malloc(sizeof(*node)); if (!node) yl_runtime_error(\"out of memory\"); node->ptr = ptr; node->next = yl_arrays; yl_arrays = node; }\n"
        "static void yl_replace_array_buffer(void *old_ptr, void *new_ptr) { for (YLTrackedArray *node = yl_arrays; node; node = node->next) if (node->ptr == old_ptr) { node->ptr = new_ptr; return; } yl_runtime_error(\"array allocation tracking failed\"); }\n"
        "static const char *yl_boolstr(bool value) { return value ? \"true\" : \"false\"; }\n"
        "static const char *yl_format(const char *format, ...) {\n"
        "    va_list args; va_start(args, format); va_list copy; va_copy(copy, args);\n"
        "    int needed = vsnprintf(NULL, 0, format, copy); va_end(copy);\n"
        "    if (needed < 0) { va_end(args); yl_runtime_error(\"formatting failed\"); }\n"
        "    char *buffer = malloc((size_t)needed + 1); if (!buffer) { va_end(args); yl_runtime_error(\"out of memory\"); }\n"
        "    (void)vsnprintf(buffer, (size_t)needed + 1, format, args); va_end(args);\n"
        "    return (const char *)yl_track(buffer);\n}\n"
        "static const char *yl_read_line(void) {\n"
        "    size_t capacity = 128, length = 0; char *buffer = malloc(capacity);\n"
        "    if (!buffer) yl_runtime_error(\"out of memory while reading input\");\n"
        "    int ch = EOF;\n"
        "    while ((ch = fgetc(stdin)) != EOF && ch != '\\n') {\n"
        "        if (length + 1 >= capacity) {\n"
        "            if (capacity > SIZE_MAX / 2) { free(buffer); yl_runtime_error(\"input line is too large\"); }\n"
        "            size_t next = capacity * 2; char *grown = realloc(buffer, next);\n"
        "            if (!grown) { free(buffer); yl_runtime_error(\"out of memory while reading input\"); }\n"
        "            buffer = grown; capacity = next;\n"
        "        }\n"
        "        buffer[length++] = (char)ch;\n"
        "    }\n"
        "    if (ch == EOF && ferror(stdin)) { free(buffer); yl_runtime_error(\"input failed\"); }\n"
        "    if (ch == EOF && length == 0) { free(buffer); yl_runtime_error(\"end of input\"); }\n"
        "    if (length > 0 && buffer[length - 1] == '\\r') length--;\n"
        "    buffer[length] = '\\0'; return (const char *)yl_track(buffer);\n"
        "}\n"
        "static const char *yl_read_file(const char *path) {\n"
        "    if (!path) yl_runtime_error(\"read_file() received a null path\");\n"
        "    FILE *file = fopen(path, \"rb\"); if (!file) yl_runtime_error(\"cannot open file for reading\");\n"
        "    size_t capacity = 4096, length = 0; char *buffer = malloc(capacity);\n"
        "    if (!buffer) { fclose(file); yl_runtime_error(\"out of memory while reading file\"); }\n"
        "    for (;;) {\n"
        "        if (length + 1 >= capacity) { if (capacity > SIZE_MAX / 2) { free(buffer); fclose(file); yl_runtime_error(\"file is too large\"); } size_t next = capacity * 2; char *grown = realloc(buffer, next); if (!grown) { free(buffer); fclose(file); yl_runtime_error(\"out of memory while reading file\"); } buffer = grown; capacity = next; }\n"
        "        size_t got = fread(buffer + length, 1, capacity - length - 1, file); length += got;\n"
        "        if (got == 0) { if (ferror(file)) { free(buffer); fclose(file); yl_runtime_error(\"failed while reading file\"); } break; }\n"
        "    }\n"
        "    if (memchr(buffer, '\\0', length)) { free(buffer); fclose(file); yl_runtime_error(\"read_file() only supports text without NUL bytes\"); }\n"
        "    if (fclose(file) != 0) { free(buffer); yl_runtime_error(\"failed to close file after reading\"); }\n"
        "    buffer[length] = '\\0'; return (const char *)yl_track(buffer);\n"
        "}\n"
        "static void yl_write_file(const char *path, const char *content) {\n"
        "    if (!path || !content) yl_runtime_error(\"write_file() received a null string\");\n"
        "    FILE *file = fopen(path, \"wb\"); if (!file) yl_runtime_error(\"cannot open file for writing\");\n"
        "    size_t length = strlen(content); bool ok = fwrite(content, 1, length, file) == length;\n"
        "    if (fclose(file) != 0) ok = false; if (!ok) yl_runtime_error(\"failed while writing file\");\n"
        "}\n"
        "static int64_t yl_len_string(const char *value) { if (!value) yl_runtime_error(\"len() received a null string\"); size_t n = strlen(value); if (n > (size_t)INT64_MAX) yl_runtime_error(\"string is too large for len()\"); return (int64_t)n; }\n"
        "static const char *yl_clone_string(const char *value) { if (!value) yl_runtime_error(\"clone() received a null string\"); size_t n = strlen(value); char *copy = malloc(n + 1); if (!copy) yl_runtime_error(\"out of memory while cloning string\"); memcpy(copy, value, n + 1); return (const char *)yl_track(copy); }\n"
        "static int64_t yl_parse_int(const char *text) {\n"
        "    if (!text) yl_runtime_error(\"parse_int() received a null string\");\n"
        "    errno = 0; char *end = NULL; long long value = strtoll(text, &end, 10);\n"
        "    if (errno == ERANGE || end == text) yl_runtime_error(\"invalid integer input\");\n"
        "    while (*end == ' ' || *end == '\\t' || *end == '\\r' || *end == '\\n') end++;\n"
        "    if (*end != '\\0') yl_runtime_error(\"invalid integer input\");\n"
        "    return (int64_t)value;\n"
        "}\n"
        "static double yl_parse_float(const char *text) {\n"
        "    if (!text) yl_runtime_error(\"parse_float() received a null string\");\n"
        "    errno = 0; char *end = NULL; double value = strtod(text, &end);\n"
        "    if (errno == ERANGE || end == text || !isfinite(value)) yl_runtime_error(\"invalid floating-point input\");\n"
        "    while (*end == ' ' || *end == '\\t' || *end == '\\r' || *end == '\\n') end++;\n"
        "    if (*end != '\\0') yl_runtime_error(\"invalid floating-point input\");\n"
        "    return value;\n"
        "}\n"
        "static size_t yl_array_element_size(int kind) { switch (kind) { case 1: return sizeof(int64_t); case 2: return sizeof(double); case 3: return sizeof(bool); case 4: return sizeof(char); case 5: return sizeof(const char *); default: yl_runtime_error(\"invalid array element type\"); } return 0; }\n"
        "static YLArray yl_array_make(size_t element_size, const void *values, size_t count, int kind) { if (!element_size || count > SIZE_MAX / element_size) yl_runtime_error(\"array size overflow\"); size_t bytes = count * element_size; void *data = bytes ? malloc(bytes) : NULL; if (bytes && !data) yl_runtime_error(\"out of memory while creating array\"); if (bytes) memcpy(data, values, bytes); if (data) yl_track_array_buffer(data); return (YLArray){data, count, count, kind}; }\n"
        "static void *yl_array_at(YLArray array, int64_t index, int kind) { if (array.kind != kind || !array.data) yl_runtime_error(\"invalid or uninitialized array\"); if (index < 0 || (uint64_t)index >= array.len) yl_runtime_error(\"array index out of bounds\"); return (char *)array.data + (size_t)index * yl_array_element_size(kind); }\n"
        "static int64_t yl_array_len(YLArray array) { if (array.len > (size_t)INT64_MAX) yl_runtime_error(\"array is too large for len()\"); return (int64_t)array.len; }\n"
        "static void yl_array_append(YLArray *array, const void *value, size_t element_size, int kind) { if (!array || array->kind != kind || !element_size) yl_runtime_error(\"append() type mismatch or uninitialized array\"); if (array->len == array->cap) { size_t next = array->cap ? array->cap * 2 : 4; if (next < array->cap || next > SIZE_MAX / element_size) yl_runtime_error(\"array capacity overflow\"); void *old = array->data; void *grown = realloc(old, next * element_size); if (!grown) yl_runtime_error(\"out of memory while growing array\"); if (old) yl_replace_array_buffer(old, grown); else yl_track_array_buffer(grown); array->data = grown; array->cap = next; } memcpy((char *)array->data + array->len * element_size, value, element_size); array->len++; }\n"
        "static YLArray yl_array_clone(YLArray array) { size_t size = yl_array_element_size(array.kind); YLArray copy = yl_array_make(size, array.data, array.len, array.kind); if (array.kind == 5) { const char **source = (const char **)array.data; const char **target = (const char **)copy.data; for (size_t i = 0; i < array.len; i++) target[i] = yl_clone_string(source[i]); } return copy; }\n"
        "static int64_t yl_add_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_add_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in addition\"); return r; }\n"
        "static int64_t yl_sub_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_sub_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in subtraction\"); return r; }\n"
        "static int64_t yl_mul_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_mul_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in multiplication\"); return r; }\n"
        "static int64_t yl_neg_i64(int64_t a) { if (a == INT64_MIN) yl_runtime_error(\"integer overflow in negation\"); return -a; }\n"
        "static int64_t yl_div_i64(int64_t a, int64_t b) { if (!b) yl_runtime_error(\"integer division by zero\"); if (a == INT64_MIN && b == -1) yl_runtime_error(\"integer overflow in division\"); return a / b; }\n"
        "static int64_t yl_mod_i64(int64_t a, int64_t b) { if (!b) yl_runtime_error(\"integer modulo by zero\"); if (a == INT64_MIN && b == -1) return 0; return a % b; }\n"
        "static double yl_div_f64(double a, double b) { if (b == 0.0) yl_runtime_error(\"floating-point division by zero\"); return a / b; }\n\n",
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
            { emit_param_type(out, fn->params[j]); fprintf(out, " %s", fn->params[j]->c_name); }
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
            { emit_param_type(out, fn->params[j]); fprintf(out, " %s", fn->params[j]->c_name); }
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

