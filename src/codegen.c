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
        case TYPE_INT_ARRAY: return "YlArrayInt";
        case TYPE_FLOAT_ARRAY: return "YlArrayFloat";
        case TYPE_BOOL_ARRAY: return "YlArrayBool";
        case TYPE_CHAR_ARRAY: return "YlArrayChar";
        case TYPE_STRING_ARRAY: return "YlArrayString";
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
        case TYPE_INT_ARRAY: case TYPE_FLOAT_ARRAY: case TYPE_BOOL_ARRAY:
        case TYPE_CHAR_ARRAY: case TYPE_STRING_ARRAY: return "%s";
        case TYPE_VOID: case TYPE_ERROR: return "%s";
    }
    return "%s";
}

static void emit_expr(StringBuilder *sb, Expr *expr);

static const char *array_suffix(YType array_type)
{
    switch (array_type) {
        case TYPE_INT_ARRAY: return "int";
        case TYPE_FLOAT_ARRAY: return "float";
        case TYPE_BOOL_ARRAY: return "bool";
        case TYPE_CHAR_ARRAY: return "char";
        case TYPE_STRING_ARRAY: return "string";
        default: return "invalid";
    }
}

static void emit_move_value(StringBuilder *sb, Expr *expr)
{
    if (expr && expr->kind == EXPR_NAME && expr->as.name.variable &&
        ylang_type_is_owned(expr->type)) {
        VarDecl *var = expr->as.name.variable;
        if (ylang_type_is_array(expr->type)) {
            sb_appendf(sb, "yl_array_%s_move(&%s)",
                       array_suffix(expr->type), var->c_name);
        } else {
            sb_appendf(sb, "yl_string_move(&%s)", var->c_name);
        }
    } else {
        emit_expr(sb, expr);
    }
}

static void emit_array_literal(StringBuilder *sb, Expr *expr)
{
    YType element_type = ylang_array_element_type(expr->type);
    const char *suffix = array_suffix(expr->type);
    const char *element_ctype = c_base_type(element_type);
    sb_appendf(sb, "yl_array_%s_make(", suffix);
    if (expr->as.array_literal.count == 0) {
        sb_append(sb, "NULL, 0");
    } else {
        sb_appendf(sb, "(%s[]){", element_ctype);
        for (size_t i = 0; i < expr->as.array_literal.count; i++) {
            if (i) sb_append(sb, ", ");
            if (element_type == TYPE_STRING) emit_move_value(sb, expr->as.array_literal.items[i]);
            else emit_expr(sb, expr->as.array_literal.items[i]);
        }
        sb_appendf(sb, "}, %zu", expr->as.array_literal.count);
    }
    sb_append(sb, ")");
}

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
                case TYPE_INT_ARRAY: case TYPE_FLOAT_ARRAY: case TYPE_BOOL_ARRAY:
                case TYPE_CHAR_ARRAY: case TYPE_STRING_ARRAY:
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
        case EXPR_ARRAY_LITERAL:
            emit_array_literal(sb, expr);
            break;
        case EXPR_INDEX: {
            Expr *array_expr = expr->as.index.array;
            VarDecl *array_var = expr->as.index.variable;
            const char *suffix = array_suffix(array_var ? array_var->type : TYPE_ERROR);
            sb_appendf(sb, "(*yl_array_%s_at(&%s, ", suffix,
                       array_var ? array_var->c_name : "yl_missing_array");
            emit_expr(sb, expr->as.index.index);
            sb_append(sb, "))");
            (void)array_expr;
            break;
        }
        case EXPR_ASSIGN:
            if (expr->as.assign.target && expr->as.assign.target->kind == EXPR_NAME &&
                ylang_type_is_array(expr->type) && expr->as.assign.variable) {
                sb_appendf(sb, "yl_array_%s_replace(&%s, ",
                           array_suffix(expr->type), expr->as.assign.variable->c_name);
                emit_move_value(sb, expr->as.assign.right);
                sb_append(sb, ")");
            } else {
                sb_append(sb, "(");
                emit_expr(sb, expr->as.assign.target);
                sb_append(sb, " = ");
                if (expr->type == TYPE_STRING) emit_move_value(sb, expr->as.assign.right);
                else emit_expr(sb, expr->as.assign.right);
                sb_append(sb, ")");
            }
            break;
        case EXPR_CALL: {
            const char *name = expr->as.call.name;
            if (strcmp(name, "input") == 0) sb_append(sb, "yl_input_string()");
            else if (strcmp(name, "input_int") == 0) sb_append(sb, "yl_input_int()");
            else if (strcmp(name, "input_float") == 0) sb_append(sb, "yl_input_float()");
            else if (strcmp(name, "len") == 0 && expr->as.call.count == 1) {
                Expr *arg = expr->as.call.args[0];
                if (arg->type == TYPE_STRING) {
                    sb_append(sb, "yl_string_len("); emit_expr(sb, arg); sb_append(sb, ")");
                } else {
                    sb_append(sb, "(int64_t)("); emit_expr(sb, arg); sb_append(sb, ").len");
                }
            } else if (strcmp(name, "clone") == 0 && expr->as.call.count == 1) {
                Expr *arg = expr->as.call.args[0];
                if (arg->type == TYPE_STRING) {
                    sb_append(sb, "yl_clone_string("); emit_expr(sb, arg); sb_append(sb, ")");
                } else {
                    sb_appendf(sb, "yl_array_%s_clone(", array_suffix(arg->type));
                    emit_expr(sb, arg); sb_append(sb, ")");
                }
            } else if (strcmp(name, "append") == 0 && expr->as.call.count == 2) {
                Expr *arg = expr->as.call.args[0];
                sb_appendf(sb, "yl_array_%s_append(", array_suffix(arg->type));
                emit_move_value(sb, arg);
                sb_append(sb, ", "); emit_expr(sb, expr->as.call.args[1]); sb_append(sb, ")");
            } else if (expr->as.call.function) {
                sb_append(sb, expr->as.call.function->c_name);
                sb_append(sb, "(");
                for (size_t i = 0; i < expr->as.call.count; i++) {
                    if (i) sb_append(sb, ", ");
                    if (i < expr->as.call.function->param_count &&
                        !expr->as.call.function->params[i]->is_borrowed &&
                        ylang_type_is_owned(expr->as.call.function->params[i]->type))
                        emit_move_value(sb, expr->as.call.args[i]);
                    else emit_expr(sb, expr->as.call.args[i]);
                }
                sb_append(sb, ")");
            } else {
                sb_append(sb, "yl_missing_function(");
                for (size_t i = 0; i < expr->as.call.count; i++) {
                    if (i) sb_append(sb, ", ");
                    emit_expr(sb, expr->as.call.args[i]);
                }
                sb_append(sb, ")");
            }
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
    if (var->is_const && var->type != TYPE_STRING && !ylang_type_is_array(var->type)) fputs("const ", out);
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
        case TYPE_INT_ARRAY: case TYPE_FLOAT_ARRAY: case TYPE_BOOL_ARRAY:
        case TYPE_CHAR_ARRAY: case TYPE_STRING_ARRAY:
            fputs("(void)0;\n", out); break;
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
            for (size_t i = stmt->as.block.count; i > 0; i--) {
                Stmt *item = stmt->as.block.items[i - 1];
                if (item && item->kind == STMT_VAR) {
                    VarDecl *var = item->as.variable;
                    if (ylang_type_is_array(var->type)) {
                        emit_indent(out, indent + 1);
                        fprintf(out, "yl_array_%s_drop(&%s);\n",
                                array_suffix(var->type), var->c_name);
                    } else if (var->type == TYPE_STRING) {
                        emit_indent(out, indent + 1);
                        fprintf(out, "yl_string_drop(&%s);\n", var->c_name);
                    }
                }
            }
            emit_indent(out, indent); fputs("}\n", out);
            break;
        case STMT_VAR: {
            VarDecl *var = stmt->as.variable;
            emit_indent(out, indent); emit_var_type(out, var, false);
            fprintf(out, " %s", var->c_name);
            if (var->initializer) {
                fputs(" = ", out);
                if (ylang_type_is_owned(var->type) && var->initializer->kind == EXPR_NAME) {
                    StringBuilder moved;
                    sb_init(&moved);
                    emit_move_value(&moved, var->initializer);
                    fputs(moved.data ? moved.data : "0", out);
                    sb_destroy(&moved);
                } else emit_expr_to_file(out, var->initializer);
            } else if (ylang_type_is_array(var->type)) {
                fputs(" = {0}", out);
            } else if (var->type == TYPE_STRING) {
                fputs(" = NULL", out);
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
        case STMT_FOR:
            emit_indent(out, indent);
            fputs("for (", out);
            if (stmt->as.for_stmt.initializer &&
                stmt->as.for_stmt.initializer->kind == STMT_VAR) {
                VarDecl *var = stmt->as.for_stmt.initializer->as.variable;
                emit_var_type(out, var, false);
                fprintf(out, " %s", var->c_name);
                if (var->initializer) {
                    fputs(" = ", out);
                    emit_expr_to_file(out, var->initializer);
                }
            }
            fputs("; ", out);
            if (stmt->as.for_stmt.condition) emit_expr_to_file(out, stmt->as.for_stmt.condition);
            else fputs("true", out);
            fputs("; ", out);
            if (stmt->as.for_stmt.increment) emit_expr_to_file(out, stmt->as.for_stmt.increment);
            fputs(")\n", out);
            emit_stmt(out, stmt->as.for_stmt.body, indent);
            break;
        case STMT_BREAK:
            emit_indent(out, indent); fputs("break;\n", out); break;
        case STMT_CONTINUE:
            emit_indent(out, indent); fputs("continue;\n", out); break;
        case STMT_RETURN:
            emit_indent(out, indent); fputs("return", out);
            if (stmt->as.return_value) {
                fputc(' ', out);
                if (ylang_type_is_owned(stmt->as.return_value->type)) {
                    StringBuilder moved;
                    sb_init(&moved);
                    emit_move_value(&moved, stmt->as.return_value);
                    fputs(moved.data ? moved.data : "0", out);
                    sb_destroy(&moved);
                } else emit_expr_to_file(out, stmt->as.return_value);
            }
            fputs(";\n", out); break;
        case STMT_EXPR:
            emit_indent(out, indent); emit_expr_to_file(out, stmt->as.expression); fputs(";\n", out); break;
    }
}

#ifndef YLANG_VERSION
#define YLANG_VERSION "2.0.0-dev"
#endif

static void emit_runtime(FILE *out)
{
    fprintf(out, "/* Generated by YLang v%s. This file is compiler output. */\n", YLANG_VERSION);
    fputs("#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdarg.h>\n#include <string.h>\n#include <limits.h>\n#include <stddef.h>\n#include <errno.h>\n#include <ctype.h>\n#include <math.h>\n\nstatic void yl_runtime_error(const char *message) {\n    fprintf(stderr, \"YLang runtime error: %s\\n\", message);\n    exit(70);\n}\n\ntypedef struct YLTracked { void *ptr; struct YLTracked *next; } YLTracked;\nstatic YLTracked *yl_tracked = NULL;\n\nstatic void *yl_track(void *ptr) {\n    if (!ptr) return NULL;\n    YLTracked *node = (YLTracked *)malloc(sizeof(*node));\n    if (!node) yl_runtime_error(\"out of memory while tracking an allocation\");\n    node->ptr = ptr;\n    node->next = yl_tracked;\n    yl_tracked = node;\n    return ptr;\n}\n\nstatic void *yl_realloc_tracked(void *ptr, size_t bytes) {\n    if (bytes == 0) bytes = 1;\n    if (!ptr) {\n        void *created = malloc(bytes);\n        if (!created) yl_runtime_error(\"out of memory\");\n        return yl_track(created);\n    }\n    YLTracked *node = yl_tracked;\n    while (node && node->ptr != ptr) node = node->next;\n    if (!node) yl_runtime_error(\"internal allocation registry mismatch\");\n    void *grown = realloc(ptr, bytes);\n    if (!grown) yl_runtime_error(\"out of memory while growing a buffer\");\n    node->ptr = grown;\n    return grown;\n}\n\nstatic void yl_untrack_free(void *ptr) {\n    if (!ptr) return;\n    YLTracked **link = &yl_tracked;\n    while (*link && (*link)->ptr != ptr) link = &(*link)->next;\n    if (!*link) return; /* String literals are static and are never freed. */\n    YLTracked *node = *link;\n    *link = node->next;\n    free(node->ptr);\n    free(node);\n}\n\nstatic void yl_cleanup(void) {\n    while (yl_tracked) {\n", out);
    fputs("        YLTracked *next = yl_tracked->next;\n        free(yl_tracked->ptr);\n        free(yl_tracked);\n        yl_tracked = next;\n    }\n}\n\nstatic const char *yl_string_move(const char **source) {\n    const char *value = *source;\n    *source = NULL;\n    return value;\n}\n\nstatic void yl_string_drop(const char **value) {\n    if (value && *value) yl_untrack_free((void *)*value);\n    if (value) *value = NULL;\n}\n\nstatic const char *yl_string_replace(const char **target, const char *value) {\n    yl_string_drop(target);\n    *target = value;\n    return value;\n}\n\nstatic const char *yl_clone_string(const char *value) {\n    if (!value) yl_runtime_error(\"attempted to clone an invalid string\");\n    size_t length = strlen(value);\n    if (length == SIZE_MAX) yl_runtime_error(\"string length overflow\");\n    char *copy = (char *)malloc(length + 1U);\n    if (!copy) yl_runtime_error(\"out of memory while cloning a string\");\n    memcpy(copy, value, length + 1U);\n    return (const char *)yl_track(copy);\n}\n\nstatic int64_t yl_string_len(const char *value) {\n    if (!value) yl_runtime_error(\"attempted to get the length of an invalid string\");\n    size_t length = strlen(value);\n    if (length > (size_t)INT64_MAX) yl_runtime_error(\"string is too large\");\n    return (int64_t)length; /* Byte length, not Unicode character count. */\n}\n\nstatic const char *yl_input_string(void) {\n    size_t capacity = 128U;\n    size_t length = 0;\n    char *buffer = (char *)malloc(capacity);\n    if (!buffer) yl_runtime_error(\"out of memory while reading input\");\n    int ch = EOF;\n    while ((ch = getchar()) != EOF && ch != '\\n') {\n        if (ch == '\\r') continue;\n        if (length >= 1048576U) {\n            free(buffer);\n            yl_runtime_error(\"input line exceeds the 1 MiB limit\");\n        }\n", out);
    fputs("        if (length + 1U >= capacity) {\n            size_t next = capacity > 524288U ? 1048577U : capacity * 2U;\n            char *grown = (char *)realloc(buffer, next);\n            if (!grown) {\n                free(buffer);\n                yl_runtime_error(\"out of memory while growing input\");\n            }\n            buffer = grown;\n            capacity = next;\n        }\n        buffer[length++] = (char)ch;\n    }\n    if (ch == EOF && ferror(stdin)) {\n        free(buffer);\n        yl_runtime_error(\"failed to read standard input\");\n    }\n    if (ch == EOF && length == 0) {\n        free(buffer);\n        yl_runtime_error(\"unexpected end of input\");\n    }\n    buffer[length] = '\\0';\n    return (const char *)yl_track(buffer);\n}\n\nstatic int64_t yl_input_int(void) {\n    const char *line = yl_input_string();\n    errno = 0;\n    char *end = NULL;\n    long long value = strtoll(line, &end, 10);\n    while (end && isspace((unsigned char)*end)) end++;\n    if (errno == ERANGE || end == line || !end || *end != '\\0')\n        yl_runtime_error(\"input_int() expected a valid signed integer\");\n    return (int64_t)value;\n}\n\nstatic double yl_input_float(void) {\n    const char *line = yl_input_string();\n    errno = 0;\n    char *end = NULL;\n    double value = strtod(line, &end);\n    while (end && isspace((unsigned char)*end)) end++;\n    if (errno == ERANGE || end == line || !end || *end != '\\0' || !isfinite(value))\n        yl_runtime_error(\"input_float() expected a finite floating-point number\");\n    return value;\n}\n\nstatic const char *yl_boolstr(bool value) { return value ? \"true\" : \"false\"; }\n\nstatic const char *yl_format(const char *format, ...) {\n    va_list args;\n    va_start(args, format);\n    va_list copy;\n    va_copy(copy, args);\n    int needed = vsnprintf(NULL, 0, format, copy);\n", out);
    fputs("    va_end(copy);\n    if (needed < 0) { va_end(args); yl_runtime_error(\"formatting failed\"); }\n    size_t bytes = (size_t)needed + 1U;\n    char *buffer = (char *)malloc(bytes);\n    if (!buffer) { va_end(args); yl_runtime_error(\"out of memory\"); }\n    (void)vsnprintf(buffer, bytes, format, args);\n    va_end(args);\n    return (const char *)yl_track(buffer);\n}\n\ntypedef struct { int64_t *data; size_t len; size_t cap; } YlArrayInt;\ntypedef struct { double *data; size_t len; size_t cap; } YlArrayFloat;\ntypedef struct { bool *data; size_t len; size_t cap; } YlArrayBool;\ntypedef struct { char *data; size_t len; size_t cap; } YlArrayChar;\ntypedef struct { const char **data; size_t len; size_t cap; } YlArrayString;\n\n#define YL_DEFINE_ARRAY(lower, Upper, ElementType) \\\nstatic YlArray##Upper yl_array_##lower##_make(const ElementType *items, size_t count) { \\\n    YlArray##Upper result = {0}; \\\n    if (count > SIZE_MAX / sizeof(*result.data)) yl_runtime_error(\"array size overflow\"); \\\n    if (count) { \\\n        result.data = (ElementType *)malloc(count * sizeof(*result.data)); \\\n        if (!result.data) yl_runtime_error(\"out of memory while creating an array\"); \\\n        memcpy(result.data, items, count * sizeof(*result.data)); \\\n        yl_track(result.data); \\\n    } \\\n    result.len = count; result.cap = count; \\\n    return result; \\\n} \\\nstatic YlArray##Upper yl_array_##lower##_move(YlArray##Upper *source) { \\\n    YlArray##Upper value = *source; *source = (YlArray##Upper){0}; return value; \\\n} \\\nstatic YlArray##Upper yl_array_##lower##_clone(YlArray##Upper source) { \\\n    return yl_array_##lower##_make(source.data, source.len); \\\n} \\\nstatic void yl_array_##lower##_drop(YlArray##Upper *value) { \\\n    if (!value) return; \\\n    yl_untrack_free(value->data); \\\n", out);
    fputs("    *value = (YlArray##Upper){0}; \\\n} \\\nstatic void yl_array_##lower##_replace(YlArray##Upper *target, YlArray##Upper value) { \\\n    yl_array_##lower##_drop(target); *target = value; \\\n} \\\nstatic ElementType *yl_array_##lower##_at(YlArray##Upper *value, int64_t index) { \\\n    if (index < 0 || (uint64_t)index >= (uint64_t)value->len) \\\n        yl_runtime_error(\"array index out of bounds\"); \\\n    return &value->data[(size_t)index]; \\\n} \\\nstatic YlArray##Upper yl_array_##lower##_append(YlArray##Upper value, ElementType element) { \\\n    if (value.len == SIZE_MAX) yl_runtime_error(\"array length overflow\"); \\\n    if (value.len == value.cap) { \\\n        size_t capacity = value.cap ? value.cap * 2U : 4U; \\\n        if (capacity < value.len + 1U || capacity > SIZE_MAX / sizeof(*value.data)) \\\n            yl_runtime_error(\"array capacity overflow\"); \\\n        value.data = (ElementType *)yl_realloc_tracked(value.data, capacity * sizeof(*value.data)); \\\n        value.cap = capacity; \\\n    } \\\n    value.data[value.len++] = element; \\\n    return value; \\\n}\n\nYL_DEFINE_ARRAY(int, Int, int64_t)\nYL_DEFINE_ARRAY(float, Float, double)\nYL_DEFINE_ARRAY(bool, Bool, bool)\nYL_DEFINE_ARRAY(char, Char, char)\nYL_DEFINE_ARRAY(string, String, const char *)\n\nstatic int64_t yl_add_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_add_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in addition\"); return r; }\nstatic int64_t yl_sub_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_sub_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in subtraction\"); return r; }\nstatic int64_t yl_mul_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_mul_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in multiplication\"); return r; }\n", out);
    fputs("static int64_t yl_neg_i64(int64_t a) { if (a == INT64_MIN) yl_runtime_error(\"integer overflow in negation\"); return -a; }\nstatic int64_t yl_div_i64(int64_t a, int64_t b) { if (!b) yl_runtime_error(\"integer division by zero\"); if (a == INT64_MIN && b == -1) yl_runtime_error(\"integer overflow in division\"); return a / b; }\nstatic int64_t yl_mod_i64(int64_t a, int64_t b) { if (!b) yl_runtime_error(\"integer modulo by zero\"); if (a == INT64_MIN && b == -1) return 0; return a % b; }\nstatic double yl_div_f64(double a, double b) { if (b == 0.0) yl_runtime_error(\"floating-point division by zero\"); return a / b; }\n\n", out);
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

