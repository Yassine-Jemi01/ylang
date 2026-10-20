#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include "runtime_template.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef YLANG_VERSION
#define YLANG_VERSION "dev"
#endif

/* --------------------------- C code generation -------------------------- */
static const char *c_base_type(Compiler *compiler, YType type)
{
    if (type >= TYPE_STRUCT_BASE) {
        for (size_t i = 0; i < compiler->struct_count; i++) {
            if (compiler->structs[i]->type == type) return compiler->structs[i]->c_name;
        }
        return "int";
    }
    if (type >= TYPE_ENUM_BASE) return "int64_t";
    switch (type) {
        case TYPE_INT: return "int64_t";
        case TYPE_FLOAT: return "double";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_STRING: return "const char *";
        case TYPE_VOID: return "void";
        case TYPE_ERROR: return "int";
        default: return "int64_t";
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
    if (strcmp(name, "string.replace") == 0) return "yl_string_replace";
    if (strcmp(name, "string.upper") == 0) return "yl_string_upper";
    if (strcmp(name, "string.lower") == 0) return "yl_string_lower";
    if (strcmp(name, "string.parse_int") == 0) return "yl_string_parse_int";
    if (strcmp(name, "string.is_int") == 0) return "yl_string_is_int";
    if (strcmp(name, "io.read_line") == 0) return "yl_read_line";
    if (strcmp(name, "io.read_file") == 0) return "yl_read_file";
    if (strcmp(name, "io.write_file") == 0) return "yl_write_file";
    if (strcmp(name, "io.file_exists") == 0) return "yl_path_exists";
    if (strcmp(name, "path.exists") == 0) return "yl_path_exists";
    if (strcmp(name, "path.basename") == 0) return "yl_path_basename";
    if (strcmp(name, "path.extension") == 0) return "yl_path_extension";
    if (strcmp(name, "image.open") == 0) return "yl_image_open";
    const GuiBuiltinInfo *gui_builtin = find_gui_builtin_info(name);
    if (gui_builtin) return gui_builtin->c_symbol;
    return NULL;
}

static void emit_expr(StringBuilder *sb, Expr *expr)
{
    if (!expr) { sb_append(sb, "0"); return; }
    switch (expr->kind) {
        case EXPR_ERROR: sb_append(sb, "0"); break;
        case EXPR_ENUM_VALUE:
            sb_appendf(sb, "INT64_C(%lld)",
                       (long long)expr->as.enum_value.variant->value);
            break;
        case EXPR_STRUCT_INIT: {
            StructDecl *structure = expr->as.struct_init.structure;
            sb_append(sb, "(");
            sb_append(sb, structure ? structure->c_name : "int");
            sb_append(sb, "){");
            if (structure) {
                size_t count = expr->as.struct_init.count < structure->field_count
                    ? expr->as.struct_init.count : structure->field_count;
                for (size_t i = 0; i < count; i++) {
                    if (i) sb_append(sb, ", ");
                    sb_append(sb, ".");
                    sb_append(sb, structure->fields[i]->c_name);
                    sb_append(sb, " = ");
                    emit_expr(sb, expr->as.struct_init.args[i]);
                }
            }
            sb_append(sb, "}");
            break;
        }
        case EXPR_MEMBER:
            emit_expr(sb, expr->as.member.target);
            sb_append(sb, ".");
            sb_append(sb, expr->as.member.field ? expr->as.member.field->c_name : expr->as.member.name);
            break;
        case EXPR_INT: {
            const char *digits = expr->token.start;
            size_t digits_len = expr->token.length;
            bool is_hex = digits_len > 2 && digits[0] == '0' &&
                          (digits[1] == 'x' || digits[1] == 'X');
            if (!is_hex) {
                while (digits_len > 1 && *digits == '0') { digits++; digits_len--; }
            }
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
            if (expr->as.index.variable && expr->as.index.variable->is_array &&
                expr->as.index.variable->array_length == 0) {
                sb_appendf(sb, ", %s_len)]", expr->as.index.variable->c_name);
            } else {
                sb_appendf(sb, ", %zu)]",
                    expr->as.index.variable ? expr->as.index.variable->array_length : 0U);
            }
            break;
        case EXPR_NAME:
            sb_append(sb, expr->as.name.variable ? expr->as.name.variable->c_name : "yl_missing_variable");
            break;
        case EXPR_ASSIGN:
            if (expr->as.assign.is_compound &&
                expr->as.assign.variable &&
                expr->as.assign.target && expr->as.assign.target->type == TYPE_INT) {
                sb_append(sb, "yl_add_assign_i64(&(");
                emit_expr(sb, expr->as.assign.target);
                sb_append(sb, "), ");
                emit_expr(sb, expr->as.assign.right);
                sb_append(sb, ")");
            } else if (expr->as.assign.is_compound &&
                       expr->as.assign.variable &&
                       expr->as.assign.target && expr->as.assign.target->type == TYPE_FLOAT) {
                sb_append(sb, "yl_add_assign_f64(&(");
                emit_expr(sb, expr->as.assign.target);
                sb_append(sb, "), ");
                emit_expr(sb, expr->as.assign.right);
                sb_append(sb, ")");
            } else {
                sb_append(sb, "(");
                emit_expr(sb, expr->as.assign.target);
                sb_append(sb, " = ");
                emit_expr(sb, expr->as.assign.right);
                sb_append(sb, ")");
            }
            break;
        case EXPR_CALL: {
            if (strcmp(expr->as.call.name, "length") == 0 && expr->as.call.count == 1) {
                Expr *arg = expr->as.call.args[0];
                VarDecl *var = arg->kind == EXPR_NAME ? arg->as.name.variable : NULL;
                if (var && var->is_array) {
                    if (var->array_length == 0) sb_appendf(sb, "((int64_t)%s_len)", var->c_name);
                    else sb_appendf(sb, "INT64_C(%zu)", var->array_length);
                } else sb_append(sb, "INT64_C(0)");
                break;
            }
            if (strcmp(expr->as.call.name, "to_string") == 0 && expr->as.call.count == 1) {
                Expr *arg = expr->as.call.args[0];
                switch (arg->type) {
                    case TYPE_STRING: emit_expr(sb, arg); break;
                    case TYPE_INT: sb_append(sb, "yl_to_string_int("); emit_expr(sb, arg); sb_append(sb, ")"); break;
                    case TYPE_FLOAT: sb_append(sb, "yl_to_string_float("); emit_expr(sb, arg); sb_append(sb, ")"); break;
                    case TYPE_BOOL: sb_append(sb, "yl_boolstr("); emit_expr(sb, arg); sb_append(sb, ")"); break;
                    case TYPE_CHAR: sb_append(sb, "yl_to_string_char("); emit_expr(sb, arg); sb_append(sb, ")"); break;
                    default: sb_append(sb, "\"\""); break;
                }
                break;
            }
            if (strcmp(expr->as.call.name, "to_float") == 0 && expr->as.call.count == 1) {
                sb_append(sb, "((double)("); emit_expr(sb, expr->as.call.args[0]); sb_append(sb, "))");
                break;
            }
            if (strcmp(expr->as.call.name, "to_int") == 0 && expr->as.call.count == 1) {
                sb_append(sb, "yl_to_int("); emit_expr(sb, expr->as.call.args[0]); sb_append(sb, ")");
                break;
            }
            const char *builtin_name = builtin_c_name(expr->as.call.name);
            sb_append(sb, expr->as.call.function ? expr->as.call.function->c_name :
                         (builtin_name ? builtin_name : "yl_missing_function"));
            sb_append(sb, "(");
            for (size_t i = 0; i < expr->as.call.count; i++) {
                if (i) sb_append(sb, ", ");
                Expr *arg = expr->as.call.args[i];
                bool array_param = expr->as.call.function &&
                    i < expr->as.call.function->param_count &&
                    expr->as.call.function->params[i]->is_array;
                emit_expr(sb, arg);
                if (array_param) {
                    VarDecl *var = arg->kind == EXPR_NAME ? arg->as.name.variable : NULL;
                    sb_append(sb, ", ");
                    if (var && var->is_array && var->array_length == 0)
                        sb_appendf(sb, "%s_len", var->c_name);
                    else if (var && var->is_array)
                        sb_appendf(sb, "((size_t)%zu)", var->array_length);
                    else
                        sb_append(sb, "((size_t)0)");
                }
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
            } else if (left->type == TYPE_STRING &&
                       (op == TOKEN_LESS || op == TOKEN_LESS_EQUAL ||
                        op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL)) {
                sb_append(sb, "(strcmp("); emit_expr(sb, left); sb_append(sb, ", "); emit_expr(sb, right);
                sb_append(sb, ") ");
                switch (op) {
                    case TOKEN_LESS: sb_append(sb, "< 0)"); break;
                    case TOKEN_LESS_EQUAL: sb_append(sb, "<= 0)"); break;
                    case TOKEN_GREATER: sb_append(sb, "> 0)"); break;
                    case TOKEN_GREATER_EQUAL: sb_append(sb, ">= 0)"); break;
                    default: sb_append(sb, "0)"); break;
                }
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

static void emit_var_type(Compiler *compiler, FILE *out, VarDecl *var, bool global)
{
    if (global) fputs("static ", out);
    if (var->is_const && var->type != TYPE_STRING) fputs("const ", out);
    fputs(c_base_type(compiler, var->type), out);
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

/* Global initializers must stay within C's constant-expression rules.
   Runtime checked arithmetic helpers such as yl_neg_i64() are not valid here. */
static void emit_constant_initializer(StringBuilder *sb, Expr *expr)
{
    if (!expr) {
        sb_append(sb, "0");
        return;
    }
    if (expr->kind == EXPR_UNARY && expr->as.unary.op.type == TOKEN_MINUS) {
        if (expr->is_min_int) {
            sb_append(sb, "INT64_MIN");
        } else {
            sb_append(sb, "(-");
            emit_constant_initializer(sb, expr->as.unary.right);
            sb_append(sb, ")");
        }
        return;
    }
    if (expr->kind == EXPR_ARRAY) {
        sb_append(sb, "{");
        for (size_t i = 0; i < expr->as.array.count; i++) {
            if (i) sb_append(sb, ", ");
            emit_constant_initializer(sb, expr->as.array.items[i]);
        }
        sb_append(sb, "}");
        return;
    }
    emit_expr(sb, expr);
}

static void emit_global_initializer_to_file(FILE *out, Expr *expr)
{
    StringBuilder sb;
    sb_init(&sb);
    emit_constant_initializer(&sb, expr);
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

static void emit_stmt(Compiler *c, FILE *out, Stmt *stmt, unsigned indent)
{
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_BLOCK:
            emit_indent(out, indent); fputs("{\n", out);
            for (size_t i = 0; i < stmt->as.block.count; i++)
                emit_stmt(c, out, stmt->as.block.items[i], indent + 1);
            emit_indent(out, indent); fputs("}\n", out);
            break;
        case STMT_VAR: {
            VarDecl *var = stmt->as.variable;
            emit_indent(out, indent); emit_var_type(c, out, var, false);
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
            fputs("\n", out); emit_stmt(c, out, stmt->as.if_stmt.then_branch, indent);
            if (stmt->as.if_stmt.else_branch) {
                emit_indent(out, indent); fputs("else ", out); fputs("\n", out);
                emit_stmt(c, out, stmt->as.if_stmt.else_branch, indent);
            }
            break;
        case STMT_LOOP:
            emit_indent(out, indent); fputs("for (;;) ", out); fputs("\n", out);
            emit_stmt(c, out, stmt->as.loop_body, indent);
            break;
        case STMT_WHILE:
            emit_indent(out, indent); fputs("while (", out);
            emit_expr_to_file(out, stmt->as.while_stmt.condition);
            fputs(")\n", out);
            emit_stmt(c, out, stmt->as.while_stmt.body, indent);
            break;
        case STMT_FOR:
            emit_indent(out, indent); fputs("{\n", out);
            if (stmt->as.for_stmt.initializer)
                emit_stmt(c, out, stmt->as.for_stmt.initializer, indent + 1);
            emit_indent(out, indent + 1); fputs("for (; ", out);
            if (stmt->as.for_stmt.condition)
                emit_expr_to_file(out, stmt->as.for_stmt.condition);
            fputs("; ", out);
            if (stmt->as.for_stmt.increment)
                emit_expr_to_file(out, stmt->as.for_stmt.increment);
            fputs(")\n", out);
            emit_stmt(c, out, stmt->as.for_stmt.body, indent + 1);
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
        case STMT_MATCH: {
            char match_name[96];
            (void)snprintf(match_name, sizeof(match_name), "yl_match_%zu_%zu",
                           stmt->token.line, stmt->token.column);
            emit_indent(out, indent); fputs("{\n", out);
            emit_indent(out, indent + 1);
            fprintf(out, "int64_t %s = ", match_name);
            emit_expr_to_file(out, stmt->as.match_stmt.value);
            fputs(";\n", out);
            for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++) {
                MatchArm *arm = stmt->as.match_stmt.arms[i];
                emit_indent(out, indent + 1);
                fputs(i == 0 ? "if (" : "else if (", out);
                fprintf(out, "%s == INT64_C(%lld))\n", match_name,
                        (long long)(arm->variant ? arm->variant->value : 0));
                emit_stmt(c, out, arm->body, indent + 1);
            }
            emit_indent(out, indent); fputs("}\n", out);
            break;
        }
        case STMT_EXPR:
            emit_indent(out, indent); emit_expr_to_file(out, stmt->as.expression); fputs(";\n", out); break;
    }
}

static bool emit_runtime(FILE *out)
{
    if (fprintf(out, "/* Generated by YLang v%s. This file is compiler output. */\n",
                 YLANG_VERSION) < 0) {
        return false;
    }
    return fwrite(ylang_runtime_template, 1, sizeof(ylang_runtime_template), out) ==
           sizeof(ylang_runtime_template);
}

bool generate_c(Compiler *c, const char *path)
{
    FILE *out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "ylang: cannot write generated C file '%s': %s\n", path, strerror(errno));
        return false;
    }
    if (!emit_runtime(out)) {
        fprintf(stderr, "ylang: failed to emit the generated-C runtime\\n");
        (void)fclose(out);
        (void)remove(path);
        return false;
    }

    for (size_t i = 0; i < c->struct_count; i++) {
        StructDecl *structure = c->structs[i];
        fputs("typedef struct {\n", out);
        for (size_t j = 0; j < structure->field_count; j++) {
            StructField *field = structure->fields[j];
            fprintf(out, "    %s %s;\n", c_base_type(c, field->type), field->c_name);
        }
        fprintf(out, "} %s;\n\n", structure->c_name);
    }

    for (size_t i = 0; i < c->program->global_count; i++) {
        VarDecl *var = c->program->globals[i];
        emit_var_type(c, out, var, true);
        fprintf(out, " %s", var->c_name);
        if (var->is_array) fprintf(out, "[%zu]", var->array_length);
        if (var->initializer) {
            fputs(" = ", out); emit_global_initializer_to_file(out, var->initializer);
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
        fprintf(out, "%s %s(", c_base_type(c, fn->return_type), fn->c_name);
        if (fn->param_count == 0) fputs("void", out);
        bool first_param = true;
        for (size_t j = 0; j < fn->param_count; j++) {
            VarDecl *param = fn->params[j];
            if (!first_param) fputs(", ", out);
            if (param->is_array) {
                fprintf(out, "%s *%s, size_t %s_len",
                        c_base_type(c, param->type), param->c_name, param->c_name);
            } else {
                fprintf(out, "%s %s", c_base_type(c, param->type), param->c_name);
            }
            first_param = false;
        }
        fputs(");\n", out);
    }
    fputc('\n', out);

    for (size_t i = 0; i < c->program->function_count; i++) {
        Function *fn = c->program->functions[i];
        fprintf(out, "%s %s(", c_base_type(c, fn->return_type), fn->c_name);
        if (fn->param_count == 0) fputs("void", out);
        bool first_param = true;
        for (size_t j = 0; j < fn->param_count; j++) {
            VarDecl *param = fn->params[j];
            if (!first_param) fputs(", ", out);
            if (param->is_array) {
                fprintf(out, "%s *%s, size_t %s_len",
                        c_base_type(c, param->type), param->c_name, param->c_name);
            } else {
                fprintf(out, "%s %s", c_base_type(c, param->type), param->c_name);
            }
            first_param = false;
        }
        fputs(") ", out); fputc('\n', out);
        emit_stmt(c, out, fn->body, 0);
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

