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
            } else if (expr->type == TYPE_STRING &&
                       expr->as.assign.target &&
                       expr->as.assign.target->kind == EXPR_NAME &&
                       expr->as.assign.variable) {
                sb_appendf(sb, "yl_string_replace(&%s, ",
                           expr->as.assign.variable->c_name);
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
            if (strcmp(name, "json_get_raw") == 0 ||
                strcmp(name, "json_get_string") == 0 ||
                strcmp(name, "json_get_int") == 0 ||
                strcmp(name, "json_get_float") == 0 ||
                strcmp(name, "json_get_bool") == 0 ||
                strcmp(name, "json_has") == 0) {
                const char *runtime_name =
                    strcmp(name, "json_get_raw") == 0 ? "yl_json_get_raw" :
                    strcmp(name, "json_get_string") == 0 ? "yl_json_get_string" :
                    strcmp(name, "json_get_int") == 0 ? "yl_json_get_int" :
                    strcmp(name, "json_get_float") == 0 ? "yl_json_get_float" :
                    strcmp(name, "json_get_bool") == 0 ? "yl_json_get_bool" :
                    "yl_json_has";
                sb_appendf(sb, "%s(", runtime_name);
                emit_expr(sb, expr->as.call.args[0]);
                sb_append(sb, ", ");
                emit_expr(sb, expr->as.call.args[1]);
                sb_append(sb, ")");
            } else if (strcmp(name, "json_array_length") == 0) {
                sb_append(sb, "yl_json_array_length(");
                emit_expr(sb, expr->as.call.args[0]);
                sb_append(sb, ")");
            } else if (strcmp(name, "json_array_item") == 0) {
                sb_append(sb, "yl_json_array_item(");
                emit_expr(sb, expr->as.call.args[0]);
                sb_append(sb, ", ");
                emit_expr(sb, expr->as.call.args[1]);
                sb_append(sb, ")");
            } else if (strcmp(name, "json_quote") == 0) {
                sb_append(sb, "yl_json_quote(");
                emit_expr(sb, expr->as.call.args[0]);
                sb_append(sb, ")");
            } else if (strcmp(name, "json_last_error") == 0) {
                sb_append(sb, "yl_clone_string(yl_json_last_error())");
            } else if (strcmp(name, "http_get") == 0) {
                sb_append(sb, "yl_http_request(\"GET\", ");
                emit_expr(sb, expr->as.call.args[0]);
                sb_append(sb, ", \"\", \"\")");
            } else if (strcmp(name, "http_get_with_headers") == 0) {
                sb_append(sb, "yl_http_request(\"GET\", ");
                emit_expr(sb, expr->as.call.args[0]);
                sb_append(sb, ", \"\", ");
                emit_expr(sb, expr->as.call.args[1]);
                sb_append(sb, ")");
            } else if (strcmp(name, "http_post") == 0) {
                sb_append(sb, "yl_http_request(\"POST\", ");
                emit_expr(sb, expr->as.call.args[0]);
                sb_append(sb, ", ");
                emit_expr(sb, expr->as.call.args[1]);
                sb_append(sb, ", \"Content-Type: application/json\")");
            } else if (strcmp(name, "http_post_with_headers") == 0) {
                sb_append(sb, "yl_http_request(\"POST\", ");
                emit_expr(sb, expr->as.call.args[0]);
                sb_append(sb, ", ");
                emit_expr(sb, expr->as.call.args[1]);
                sb_append(sb, ", ");
                emit_expr(sb, expr->as.call.args[2]);
                sb_append(sb, ")");
            } else if (strcmp(name, "http_request") == 0) {
                sb_append(sb, "yl_http_request(");
                for (size_t i = 0; i < expr->as.call.count; i++) {
                    if (i) sb_append(sb, ", ");
                    emit_expr(sb, expr->as.call.args[i]);
                }
                sb_append(sb, ")");
            } else if (strcmp(name, "http_status_code") == 0) {
                sb_append(sb, "yl_http_status_code()");
            } else if (strcmp(name, "http_last_error") == 0) {
                sb_append(sb, "yl_clone_string(yl_http_last_error())");
            } else if (strcmp(name, "input") == 0) sb_append(sb, "yl_input_string()");
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


typedef struct EmitLocal {
    VarDecl *variable;
    struct EmitLocal *previous;
} EmitLocal;

typedef struct EmitLoop {
    EmitLocal *locals_at_entry;
    struct EmitLoop *previous;
} EmitLoop;

typedef struct {
    Compiler *compiler;
    EmitLocal *locals;
    EmitLoop *loop;
    unsigned return_temp_counter;
    bool function_body;
} EmitContext;

static void emit_drop_var(FILE *out, VarDecl *var, unsigned indent)
{
    if (!var || !ylang_type_is_owned(var->type)) return;
    emit_indent(out, indent);
    if (ylang_type_is_array(var->type)) {
        fprintf(out, "yl_array_%s_drop(&%s);\n",
                array_suffix(var->type), var->c_name);
    } else if (var->type == TYPE_STRING) {
        fprintf(out, "yl_string_drop(&%s);\n", var->c_name);
    }
}

static void emit_cleanup_until(FILE *out, EmitContext *context,
                               EmitLocal *stop, unsigned indent)
{
    for (EmitLocal *local = context->locals;
         local && local != stop; local = local->previous) {
        emit_drop_var(out, local->variable, indent);
    }
}

static void emit_track_local(EmitContext *context, VarDecl *variable)
{
    EmitLocal *local = arena_alloc(&context->compiler->arena, sizeof(*local));
    local->variable = variable;
    local->previous = context->locals;
    context->locals = local;
}


static void emit_stmt(FILE *out, Stmt *stmt, unsigned indent,
                      EmitContext *context)
{
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_BLOCK: {
            EmitLocal *scope_mark = context->function_body ? NULL : context->locals;
            context->function_body = false;
            emit_indent(out, indent);
            fputs("{\n", out);
            for (size_t i = 0; i < stmt->as.block.count; i++) {
                emit_stmt(out, stmt->as.block.items[i], indent + 1, context);
            }
            emit_cleanup_until(out, context, scope_mark, indent + 1);
            context->locals = scope_mark;
            emit_indent(out, indent);
            fputs("}\n", out);
            break;
        }
        case STMT_VAR: {
            VarDecl *var = stmt->as.variable;
            emit_indent(out, indent);
            emit_var_type(out, var, false);
            fprintf(out, " %s", var->c_name);
            if (var->initializer) {
                fputs(" = ", out);
                if (ylang_type_is_owned(var->type) &&
                    var->initializer->kind == EXPR_NAME) {
                    StringBuilder moved;
                    sb_init(&moved);
                    emit_move_value(&moved, var->initializer);
                    fputs(moved.data ? moved.data : "0", out);
                    sb_destroy(&moved);
                } else {
                    emit_expr_to_file(out, var->initializer);
                }
            } else if (ylang_type_is_array(var->type)) {
                fputs(" = {0}", out);
            } else if (var->type == TYPE_STRING) {
                fputs(" = NULL", out);
            }
            fputs(";\n", out);
            emit_track_local(context, var);
            break;
        }
        case STMT_PRINT:
            for (size_t i = 0; i < stmt->as.print.count; i++) {
                Expr *expr = stmt->as.print.args[i];
                if (i) {
                    emit_indent(out, indent);
                    fputs("fputs(\" \", stdout);\n", out);
                }
                if (expr->kind == EXPR_FSTRING) {
                    emit_fstring_print(out, expr, indent);
                } else {
                    emit_print_value(out, expr, indent);
                }
            }
            emit_indent(out, indent);
            fputs("putchar('\\n');\n", out);
            break;
        case STMT_IF:
            emit_indent(out, indent);
            fputs("if (", out);
            emit_expr_to_file(out, stmt->as.if_stmt.condition);
            fputs(") \n", out);
            emit_stmt(out, stmt->as.if_stmt.then_branch, indent, context);
            if (stmt->as.if_stmt.else_branch) {
                emit_indent(out, indent);
                fputs("else \n", out);
                emit_stmt(out, stmt->as.if_stmt.else_branch, indent, context);
            }
            break;
        case STMT_LOOP: {
            EmitLoop loop_frame = { context->locals, context->loop };
            context->loop = &loop_frame;
            emit_indent(out, indent);
            fputs("for (;;) \n", out);
            emit_stmt(out, stmt->as.loop_body, indent, context);
            context->loop = loop_frame.previous;
            break;
        }
        case STMT_FOR: {
            EmitLoop loop_frame = { context->locals, context->loop };
            context->loop = &loop_frame;
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
            if (stmt->as.for_stmt.condition) {
                emit_expr_to_file(out, stmt->as.for_stmt.condition);
            } else {
                fputs("true", out);
            }
            fputs("; ", out);
            if (stmt->as.for_stmt.increment) {
                emit_expr_to_file(out, stmt->as.for_stmt.increment);
            }
            fputs(")\n", out);
            emit_stmt(out, stmt->as.for_stmt.body, indent, context);
            context->loop = loop_frame.previous;
            break;
        }
        case STMT_BREAK:
            if (context->loop) {
                emit_cleanup_until(out, context,
                                   context->loop->locals_at_entry, indent);
            }
            emit_indent(out, indent);
            fputs("break;\n", out);
            break;
        case STMT_CONTINUE:
            if (context->loop) {
                emit_cleanup_until(out, context,
                                   context->loop->locals_at_entry, indent);
            }
            emit_indent(out, indent);
            fputs("continue;\n", out);
            break;
        case STMT_RETURN:
            if (stmt->as.return_value) {
                char return_name[64];
                (void)snprintf(return_name, sizeof(return_name),
                               "yl_return_temp_%u",
                               context->return_temp_counter++);
                emit_indent(out, indent);
                fprintf(out, "%s %s = ",
                        c_base_type(stmt->as.return_value->type), return_name);
                if (ylang_type_is_owned(stmt->as.return_value->type)) {
                    StringBuilder moved;
                    sb_init(&moved);
                    emit_move_value(&moved, stmt->as.return_value);
                    fputs(moved.data ? moved.data : "0", out);
                    sb_destroy(&moved);
                } else {
                    emit_expr_to_file(out, stmt->as.return_value);
                }
                fputs(";\n", out);
                emit_cleanup_until(out, context, NULL, indent);
                emit_indent(out, indent);
                fprintf(out, "return %s;\n", return_name);
            } else {
                emit_cleanup_until(out, context, NULL, indent);
                emit_indent(out, indent);
                fputs("return;\n", out);
            }
            break;
        case STMT_EXPR:
            emit_indent(out, indent);
            emit_expr_to_file(out, stmt->as.expression);
            fputs(";\n", out);
            break;
    }
}


#ifndef YLANG_VERSION
#define YLANG_VERSION "2.0.0-dev"
#endif

static void emit_runtime(FILE *out, const Compiler *compiler)
{
    fprintf(out, "/* Generated by YLang v%s. This file is compiler output. */\n", YLANG_VERSION);
    if (compiler->uses_http) fputs("#include <curl/curl.h>\n", out);
    fputs("#include <stdbool.h>\n#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <stdarg.h>\n#include <string.h>\n#include <limits.h>\n#include <stddef.h>\n#include <errno.h>\n#include <ctype.h>\n#include <math.h>\n\nstatic void yl_runtime_error(const char *message) {\n    fprintf(stderr, \"YLang runtime error: %s\\n\", message);\n    exit(70);\n}\n\ntypedef struct YLTracked { void *ptr; struct YLTracked *next; } YLTracked;\nstatic YLTracked *yl_tracked = NULL;\n\nstatic void *yl_track(void *ptr) {\n    if (!ptr) return NULL;\n    YLTracked *node = (YLTracked *)malloc(sizeof(*node));\n    if (!node) yl_runtime_error(\"out of memory while tracking an allocation\");\n    node->ptr = ptr;\n    node->next = yl_tracked;\n    yl_tracked = node;\n    return ptr;\n}\n\nstatic void *yl_realloc_tracked(void *ptr, size_t bytes) {\n    if (bytes == 0) bytes = 1;\n    if (!ptr) {\n        void *created = malloc(bytes);\n        if (!created) yl_runtime_error(\"out of memory\");\n        return yl_track(created);\n    }\n    YLTracked *node = yl_tracked;\n    while (node && node->ptr != ptr) node = node->next;\n    if (!node) yl_runtime_error(\"internal allocation registry mismatch\");\n    void *grown = realloc(ptr, bytes);\n    if (!grown) yl_runtime_error(\"out of memory while growing a buffer\");\n    node->ptr = grown;\n    return grown;\n}\n\nstatic void yl_untrack_free(void *ptr) {\n    if (!ptr) return;\n    YLTracked **link = &yl_tracked;\n    while (*link && (*link)->ptr != ptr) link = &(*link)->next;\n    if (!*link) return; /* String literals are static and are never freed. */\n    YLTracked *node = *link;\n    *link = node->next;\n    free(node->ptr);\n    free(node);\n}\n\nstatic void yl_cleanup(void) {\n    while (yl_tracked) {\n", out);
    fputs("        YLTracked *next = yl_tracked->next;\n        free(yl_tracked->ptr);\n        free(yl_tracked);\n        yl_tracked = next;\n    }\n}\n\nstatic const char *yl_string_move(const char **source) {\n    const char *value = *source;\n    *source = NULL;\n    return value;\n}\n\nstatic void yl_string_drop(const char **value) {\n    if (value && *value) yl_untrack_free((void *)*value);\n    if (value) *value = NULL;\n}\n\nstatic const char *yl_string_replace(const char **target, const char *value) {\n    yl_string_drop(target);\n    *target = value;\n    return value;\n}\n\nstatic const char *yl_clone_string(const char *value) {\n    if (!value) yl_runtime_error(\"attempted to clone an invalid string\");\n    size_t length = strlen(value);\n    if (length == SIZE_MAX) yl_runtime_error(\"string length overflow\");\n    char *copy = (char *)malloc(length + 1U);\n    if (!copy) yl_runtime_error(\"out of memory while cloning a string\");\n    memcpy(copy, value, length + 1U);\n    return (const char *)yl_track(copy);\n}\n\nstatic int64_t yl_string_len(const char *value) {\n    if (!value) yl_runtime_error(\"attempted to get the length of an invalid string\");\n    size_t length = strlen(value);\n    if (length > (size_t)INT64_MAX) yl_runtime_error(\"string is too large\");\n    return (int64_t)length; /* Byte length, not Unicode character count. */\n}\n\nstatic const char *yl_input_string(void) {\n    size_t capacity = 128U;\n    size_t length = 0;\n    char *buffer = (char *)malloc(capacity);\n    if (!buffer) yl_runtime_error(\"out of memory while reading input\");\n    int ch = EOF;\n    while ((ch = getchar()) != EOF && ch != '\\n') {\n        if (ch == '\\r') continue;\n        if (length >= 1048576U) {\n            free(buffer);\n            yl_runtime_error(\"input line exceeds the 1 MiB limit\");\n        }\n", out);
    fputs("        if (length + 1U >= capacity) {\n            size_t next = capacity > 524288U ? 1048577U : capacity * 2U;\n            char *grown = (char *)realloc(buffer, next);\n            if (!grown) {\n                free(buffer);\n                yl_runtime_error(\"out of memory while growing input\");\n            }\n            buffer = grown;\n            capacity = next;\n        }\n        buffer[length++] = (char)ch;\n    }\n    if (ch == EOF && ferror(stdin)) {\n        free(buffer);\n        yl_runtime_error(\"failed to read standard input\");\n    }\n    if (ch == EOF && length == 0) {\n        free(buffer);\n        yl_runtime_error(\"unexpected end of input\");\n    }\n    buffer[length] = '\\0';\n    return (const char *)yl_track(buffer);\n}\n\nstatic int64_t yl_input_int(void) {\n    const char *line = yl_input_string();\n    errno = 0;\n    char *end = NULL;\n    long long value = strtoll(line, &end, 10);\n    while (end && isspace((unsigned char)*end)) end++;\n    if (errno == ERANGE || end == line || !end || *end != '\\0')\n        yl_runtime_error(\"input_int() expected a valid signed integer\");\n    return (int64_t)value;\n}\n\nstatic double yl_input_float(void) {\n    const char *line = yl_input_string();\n    errno = 0;\n    char *end = NULL;\n    double value = strtod(line, &end);\n    while (end && isspace((unsigned char)*end)) end++;\n    if (errno == ERANGE || end == line || !end || *end != '\\0' || !isfinite(value))\n        yl_runtime_error(\"input_float() expected a finite floating-point number\");\n    return value;\n}\n\nstatic const char *yl_boolstr(bool value) { return value ? \"true\" : \"false\"; }\n\nstatic const char *yl_format(const char *format, ...) {\n    va_list args;\n    va_start(args, format);\n    va_list copy;\n    va_copy(copy, args);\n    int needed = vsnprintf(NULL, 0, format, copy);\n", out);
    fputs("    va_end(copy);\n    if (needed < 0) { va_end(args); yl_runtime_error(\"formatting failed\"); }\n    size_t bytes = (size_t)needed + 1U;\n    char *buffer = (char *)malloc(bytes);\n    if (!buffer) { va_end(args); yl_runtime_error(\"out of memory\"); }\n    (void)vsnprintf(buffer, bytes, format, args);\n    va_end(args);\n    return (const char *)yl_track(buffer);\n}\n\ntypedef struct { int64_t *data; size_t len; size_t cap; } YlArrayInt;\ntypedef struct { double *data; size_t len; size_t cap; } YlArrayFloat;\ntypedef struct { bool *data; size_t len; size_t cap; } YlArrayBool;\ntypedef struct { char *data; size_t len; size_t cap; } YlArrayChar;\ntypedef struct { const char **data; size_t len; size_t cap; } YlArrayString;\n\n#define YL_DEFINE_ARRAY(lower, Upper, ElementType) \\\nstatic YlArray##Upper yl_array_##lower##_make(const ElementType *items, size_t count) { \\\n    YlArray##Upper result = {0}; \\\n    if (count > SIZE_MAX / sizeof(*result.data)) yl_runtime_error(\"array size overflow\"); \\\n    if (count) { \\\n        result.data = (ElementType *)malloc(count * sizeof(*result.data)); \\\n        if (!result.data) yl_runtime_error(\"out of memory while creating an array\"); \\\n        memcpy(result.data, items, count * sizeof(*result.data)); \\\n        yl_track(result.data); \\\n    } \\\n    result.len = count; result.cap = count; \\\n    return result; \\\n} \\\nstatic YlArray##Upper yl_array_##lower##_move(YlArray##Upper *source) { \\\n    YlArray##Upper value = *source; *source = (YlArray##Upper){0}; return value; \\\n} \\\nstatic YlArray##Upper yl_array_##lower##_clone(YlArray##Upper source) { \\\n    return yl_array_##lower##_make(source.data, source.len); \\\n} \\\nstatic void yl_array_##lower##_drop(YlArray##Upper *value) { \\\n    if (!value) return; \\\n    yl_untrack_free(value->data); \\\n", out);
    fputs("    *value = (YlArray##Upper){0}; \\\n} \\\nstatic void yl_array_##lower##_replace(YlArray##Upper *target, YlArray##Upper value) { \\\n    yl_array_##lower##_drop(target); *target = value; \\\n} \\\nstatic ElementType *yl_array_##lower##_at(YlArray##Upper *value, int64_t index) { \\\n    if (index < 0 || (uint64_t)index >= (uint64_t)value->len) \\\n        yl_runtime_error(\"array index out of bounds\"); \\\n    return &value->data[(size_t)index]; \\\n} \\\nstatic YlArray##Upper yl_array_##lower##_append(YlArray##Upper value, ElementType element) { \\\n    if (value.len == SIZE_MAX) yl_runtime_error(\"array length overflow\"); \\\n    if (value.len == value.cap) { \\\n        size_t capacity = value.cap ? value.cap * 2U : 4U; \\\n        if (capacity < value.len + 1U || capacity > SIZE_MAX / sizeof(*value.data)) \\\n            yl_runtime_error(\"array capacity overflow\"); \\\n        value.data = (ElementType *)yl_realloc_tracked(value.data, capacity * sizeof(*value.data)); \\\n        value.cap = capacity; \\\n    } \\\n    value.data[value.len++] = element; \\\n    return value; \\\n}\n\nYL_DEFINE_ARRAY(int, Int, int64_t)\nYL_DEFINE_ARRAY(float, Float, double)\nYL_DEFINE_ARRAY(bool, Bool, bool)\nYL_DEFINE_ARRAY(char, Char, char)\nYL_DEFINE_ARRAY(string, String, const char *)\n\nstatic int64_t yl_add_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_add_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in addition\"); return r; }\nstatic int64_t yl_sub_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_sub_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in subtraction\"); return r; }\nstatic int64_t yl_mul_i64(int64_t a, int64_t b) { int64_t r; if (__builtin_mul_overflow(a,b,&r)) yl_runtime_error(\"integer overflow in multiplication\"); return r; }\n", out);
    fputs("static int64_t yl_neg_i64(int64_t a) { if (a == INT64_MIN) yl_runtime_error(\"integer overflow in negation\"); return -a; }\nstatic int64_t yl_div_i64(int64_t a, int64_t b) { if (!b) yl_runtime_error(\"integer division by zero\"); if (a == INT64_MIN && b == -1) yl_runtime_error(\"integer overflow in division\"); return a / b; }\nstatic int64_t yl_mod_i64(int64_t a, int64_t b) { if (!b) yl_runtime_error(\"integer modulo by zero\"); if (a == INT64_MIN && b == -1) return 0; return a % b; }\nstatic double yl_div_f64(double a, double b) { if (b == 0.0) yl_runtime_error(\"floating-point division by zero\"); return a / b; }\n\n", out);
    if (compiler->uses_http) {
        fputs("typedef struct {\n    char *data;\n    size_t length;\n    bool too_large;\n} YlHttpBuffer;\n\nstatic int64_t yl_http_status_value = 0;\nstatic char yl_http_error_value[512] = \"\";\nstatic bool yl_http_curl_initialized = false;\n\nstatic void yl_http_set_error(const char *message) {\n    (void)snprintf(yl_http_error_value, sizeof(yl_http_error_value), \"%s\",\n                   message ? message : \"unknown HTTP error\");\n}\n\nstatic const char *yl_http_empty_response(void) {\n    char *empty = (char *)yl_realloc_tracked(NULL, 1U);\n    empty[0] = '\\0';\n    return (const char *)empty;\n}\n\nstatic size_t yl_http_write_callback(char *contents, size_t size,\n                                     size_t count, void *opaque) {\n    YlHttpBuffer *buffer = (YlHttpBuffer *)opaque;\n    if (size != 0 && count > SIZE_MAX / size) return 0;\n    size_t bytes = size * count;\n    if (buffer->length > 10485760U ||\n        bytes > 10485760U - buffer->length) {\n        buffer->too_large = true;\n        yl_http_set_error(\"HTTP response exceeded the 10 MiB limit\");\n        return 0;\n    }\n    if (bytes > SIZE_MAX - buffer->length - 1U) return 0;\n    char *grown = (char *)yl_realloc_tracked(buffer->data,\n                                             buffer->length + bytes + 1U);\n    if (bytes != 0) memcpy(grown + buffer->length, contents, bytes);\n    buffer->length += bytes;\n    grown[buffer->length] = '\\0';\n    buffer->data = grown;\n    return bytes;\n}\n\nstatic bool yl_http_name_equal(const char *name, size_t length,\n                               const char *expected) {\n    if (strlen(expected) != length) return false;\n    for (size_t i = 0; i < length; i++) {\n        if (tolower((unsigned char)name[i]) !=\n            tolower((unsigned char)expected[i])) return false;\n    }\n    return true;\n}\n\nstatic bool yl_http_valid_header_name(const char *name, size_t length) {\n    if (length == 0) return false;\n    const char *punctuation = \"!#$%&'*+-.^_|~\";\n    for (size_t i = 0; i < length; i++) {\n        unsigned char ch = (unsigned char)name[i];\n        if (!((ch >= 'A' && ch <= 'Z') ||\n              (ch >= 'a' && ch <= 'z') ||\n              (ch >= '0' && ch <= '9') ||\n              ch == 96U || strchr(punctuation, (int)ch) != NULL)) return false;\n    }\n    return true;\n}\n\nstatic const char *yl_http_request(const char *method, const char *url,\n                                   const char *body, const char *raw_headers) {\n    yl_http_status_value = 0;\n    yl_http_error_value[0] = '\\0';\n    if (!method || !url || !body || !raw_headers) {\n        yl_http_set_error(\"HTTP request arguments cannot be null\");\n        return yl_http_empty_response();\n    }\n\n    bool method_ok =\n        strcmp(method, \"GET\") == 0 || strcmp(method, \"POST\") == 0 ||\n        strcmp(method, \"PUT\") == 0 || strcmp(method, \"PATCH\") == 0 ||\n        strcmp(method, \"DELETE\") == 0 || strcmp(method, \"HEAD\") == 0;\n    if (!method_ok) {\n        yl_http_set_error(\"Unsupported HTTP method\");\n        return yl_http_empty_response();\n    }\n    size_t url_length = strlen(url);\n    size_t body_length = strlen(body);\n    if (url_length == 0 || url_length > 8192U ||\n        body_length > 10485760U || strlen(raw_headers) > 32768U) {\n        yl_http_set_error(\"URL, body, or headers exceed the HTTP runtime limit\");\n        return yl_http_empty_response();\n    }\n\n    if (!yl_http_curl_initialized) {\n        CURLcode init_code = curl_global_init(CURL_GLOBAL_DEFAULT);\n        if (init_code != CURLE_OK) {\n            yl_http_set_error(curl_easy_strerror(init_code));\n            return yl_http_empty_response();\n        }\n        yl_http_curl_initialized = true;\n    }\n    CURL *curl = curl_easy_init();\n    if (!curl) {\n        yl_http_set_error(\"libcurl could not create a request\");\n        return yl_http_empty_response();\n    }\n\n    YlHttpBuffer buffer = {0};\n    struct curl_slist *headers = NULL;\n    const char *cursor = raw_headers;\n    const char *end = raw_headers + strlen(raw_headers);\n    bool headers_ok = true;\n    bool has_content_type = false;\n    bool has_accept = false;\n    while (cursor < end) {\n        const char *line_end = memchr(cursor, '\\n', (size_t)(end - cursor));\n        size_t line_length = line_end ? (size_t)(line_end - cursor)\n                                      : (size_t)(end - cursor);\n        if (line_length != 0) {\n            const char *colon = memchr(cursor, ':', line_length);\n            size_t name_length = colon ? (size_t)(colon - cursor) : 0U;\n            if (!colon || !yl_http_valid_header_name(cursor, name_length)) {\n                yl_http_set_error(\"Invalid HTTP header; expected Name: value\");\n                headers_ok = false;\n                break;\n            }\n            for (size_t i = 0; i < line_length; i++) {\n                unsigned char ch = (unsigned char)cursor[i];\n                if (ch == '\\r' || ch == 127U || (ch < 32U && ch != '\\t')) {\n                    yl_http_set_error(\"HTTP headers contain a forbidden control character\");\n                    headers_ok = false;\n                    break;\n                }\n            }\n            if (!headers_ok) break;\n            if (yl_http_name_equal(cursor, name_length, \"Host\") ||\n                yl_http_name_equal(cursor, name_length, \"Content-Length\") ||\n                yl_http_name_equal(cursor, name_length, \"Transfer-Encoding\")) {\n                yl_http_set_error(\"Host, Content-Length, and Transfer-Encoding are managed by the runtime\");\n                headers_ok = false;\n                break;\n            }\n            if (yl_http_name_equal(cursor, name_length, \"Content-Type\"))\n                has_content_type = true;\n            if (yl_http_name_equal(cursor, name_length, \"Accept\"))\n                has_accept = true;\n            char *line = (char *)malloc(line_length + 1U);\n            if (!line) {\n                yl_http_set_error(\"Out of memory while preparing HTTP headers\");\n                headers_ok = false;\n                break;\n            }\n            memcpy(line, cursor, line_length);\n            line[line_length] = '\\0';\n            struct curl_slist *grown = curl_slist_append(headers, line);\n            free(line);\n            if (!grown) {\n                yl_http_set_error(\"Out of memory while storing HTTP headers\");\n                headers_ok = false;\n                break;\n            }\n            headers = grown;\n        }\n        cursor = line_end ? line_end + 1 : end;\n    }\n    if (!headers_ok) goto cleanup;\n\n    if (!has_content_type && body_length != 0 &&\n        strcmp(method, \"GET\") != 0 && strcmp(method, \"HEAD\") != 0) {\n        struct curl_slist *grown =\n            curl_slist_append(headers, \"Content-Type: application/json\");\n        if (!grown) {\n            yl_http_set_error(\"Out of memory while setting Content-Type\");\n            goto cleanup;\n        }\n        headers = grown;\n    }\n\n    if (!has_accept) {\n        struct curl_slist *grown = curl_slist_append(headers, \"Accept: application/json\");\n        if (!grown) {\n            yl_http_set_error(\"Out of memory while setting Accept\");\n            goto cleanup;\n        }\n        headers = grown;\n    }\n\n    CURLcode code = CURLE_OK;\n#define YL_HTTP_SETOPT(option, value) do { \\\n    code = curl_easy_setopt(curl, option, value); \\\n    if (code != CURLE_OK) { \\\n        yl_http_set_error(curl_easy_strerror(code)); \\\n        goto cleanup; \\\n    } \\\n} while (0)\n    YL_HTTP_SETOPT(CURLOPT_URL, url);\n    YL_HTTP_SETOPT(CURLOPT_WRITEFUNCTION, yl_http_write_callback);\n    YL_HTTP_SETOPT(CURLOPT_WRITEDATA, &buffer);\n    YL_HTTP_SETOPT(CURLOPT_NOSIGNAL, 1L);\n    YL_HTTP_SETOPT(CURLOPT_CONNECTTIMEOUT, 5L);\n    YL_HTTP_SETOPT(CURLOPT_TIMEOUT, 15L);\n    YL_HTTP_SETOPT(CURLOPT_FOLLOWLOCATION, 1L);\n    YL_HTTP_SETOPT(CURLOPT_MAXREDIRS, 5L);\n    YL_HTTP_SETOPT(CURLOPT_SSL_VERIFYPEER, 1L);\n    YL_HTTP_SETOPT(CURLOPT_SSL_VERIFYHOST, 2L);\n    YL_HTTP_SETOPT(CURLOPT_USERAGENT, \"YLang/2.0\");\n    YL_HTTP_SETOPT(CURLOPT_HTTPHEADER, headers);\n#if LIBCURL_VERSION_NUM >= 0x075500\n    YL_HTTP_SETOPT(CURLOPT_PROTOCOLS_STR, \"http,https\");\n    YL_HTTP_SETOPT(CURLOPT_REDIR_PROTOCOLS_STR, \"http,https\");\n#else\n    YL_HTTP_SETOPT(CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));\n    YL_HTTP_SETOPT(CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));\n#endif\n    if (body_length != 0) {\n        YL_HTTP_SETOPT(CURLOPT_POSTFIELDS, body);\n        YL_HTTP_SETOPT(CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body_length);\n    }\n    if (strcmp(method, \"HEAD\") == 0)\n        YL_HTTP_SETOPT(CURLOPT_NOBODY, 1L);\n    YL_HTTP_SETOPT(CURLOPT_CUSTOMREQUEST, method);\n#undef YL_HTTP_SETOPT\n\n    code = curl_easy_perform(curl);\n    long response_code = 0;\n    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);\n    yl_http_status_value = (int64_t)response_code;\n    if (code != CURLE_OK && !buffer.too_large)\n        yl_http_set_error(curl_easy_strerror(code));\n\ncleanup:\n    curl_easy_cleanup(curl);\n    curl_slist_free_all(headers);\n    if (!buffer.data) return yl_http_empty_response();\n    return (const char *)buffer.data;\n}\n\nstatic int64_t yl_http_status_code(void) {\n    return yl_http_status_value;\n}\n\nstatic const char *yl_http_last_error(void) {\n    return yl_http_error_value;\n}\n", out);
    }
    if (compiler->uses_json) fputs("typedef struct {\n    const char *start;\n    size_t length;\n    char kind;\n} YlJsonSlice;\n\nstatic char yl_json_error_value[512] = \"\";\n\nstatic const char *yl_json_empty_string(void) {\n    char *empty = (char *)yl_realloc_tracked(NULL, 1U);\n    empty[0] = '\\0';\n    return (const char *)empty;\n}\n\nstatic void yl_json_set_error(const char *message) {\n    (void)snprintf(yl_json_error_value, sizeof(yl_json_error_value), \"%s\",\n                   message ? message : \"invalid JSON\");\n}\n\nstatic void yl_json_skip_space(const char **cursor) {\n    while (**cursor == ' ' || **cursor == '\\t' ||\n           **cursor == '\\r' || **cursor == '\\n') {\n        (*cursor)++;\n    }\n}\n\nstatic int yl_json_hex_value(char ch) {\n    if (ch >= '0' && ch <= '9') return ch - '0';\n    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;\n    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;\n    return -1;\n}\n\nstatic bool yl_json_parse_string(const char **cursor) {\n    if (**cursor != '\"') {\n        yl_json_set_error(\"expected a JSON string\");\n        return false;\n    }\n    (*cursor)++;\n    while (**cursor) {\n        unsigned char ch = (unsigned char)**cursor;\n        if (ch == '\"') {\n            (*cursor)++;\n            return true;\n        }\n        if (ch < 0x20U) {\n            yl_json_set_error(\"unescaped control character in JSON string\");\n            return false;\n        }\n        if (ch != '\\\\') {\n            (*cursor)++;\n            continue;\n        }\n        (*cursor)++;\n        char escaped = **cursor;\n        if (escaped == '\\0') {\n            yl_json_set_error(\"unfinished escape in JSON string\");\n            return false;\n        }\n        if (escaped == '\"' || escaped == '\\\\' || escaped == '/' ||\n            escaped == 'b' || escaped == 'f' || escaped == 'n' ||\n            escaped == 'r' || escaped == 't') {\n            (*cursor)++;\n            continue;\n        }\n        if (escaped == 'u') {\n            (*cursor)++;\n            for (unsigned i = 0; i < 4; i++) {\n                if (yl_json_hex_value((*cursor)[i]) < 0) {\n                    yl_json_set_error(\"invalid Unicode escape in JSON string\");\n                    return false;\n                }\n            }\n            (*cursor) += 4;\n            continue;\n        }\n        yl_json_set_error(\"invalid escape in JSON string\");\n        return false;\n    }\n    yl_json_set_error(\"unterminated JSON string\");\n    return false;\n}\n\nstatic bool yl_json_parse_value(const char **cursor, unsigned depth,\n                                YlJsonSlice *slice);\n\nstatic bool yl_json_parse_number(const char **cursor) {\n    const char *p = *cursor;\n    if (*p == '-') p++;\n    if (*p == '0') {\n        p++;\n        if (*p >= '0' && *p <= '9') {\n            yl_json_set_error(\"leading zero in JSON number\");\n            return false;\n        }\n    } else if (*p >= '1' && *p <= '9') {\n        do { p++; } while (*p >= '0' && *p <= '9');\n    } else {\n        yl_json_set_error(\"invalid JSON value\");\n        return false;\n    }\n    if (*p == '.') {\n        p++;\n        if (*p < '0' || *p > '9') {\n            yl_json_set_error(\"fraction requires digits in JSON number\");\n            return false;\n        }\n        do { p++; } while (*p >= '0' && *p <= '9');\n    }\n    if (*p == 'e' || *p == 'E') {\n        p++;\n        if (*p == '+' || *p == '-') p++;\n        if (*p < '0' || *p > '9') {\n            yl_json_set_error(\"exponent requires digits in JSON number\");\n            return false;\n        }\n        do { p++; } while (*p >= '0' && *p <= '9');\n    }\n    *cursor = p;\n    return true;\n}\n\nstatic bool yl_json_parse_value(const char **cursor, unsigned depth,\n                                YlJsonSlice *slice) {\n    if (depth > 64U) {\n        yl_json_set_error(\"JSON nesting exceeds the 64-level limit\");\n        return false;\n    }\n    yl_json_skip_space(cursor);\n    const char *start = *cursor;\n    char kind = '\\0';\n    if (**cursor == '\"') {\n        kind = 's';\n        if (!yl_json_parse_string(cursor)) return false;\n    } else if (**cursor == '{') {\n        kind = 'o';\n        (*cursor)++;\n        yl_json_skip_space(cursor);\n        if (**cursor != '}') {\n            for (;;) {\n                if (**cursor != '\"' || !yl_json_parse_string(cursor)) {\n                    yl_json_set_error(\"expected a string key in JSON object\");\n                    return false;\n                }\n                yl_json_skip_space(cursor);\n                if (**cursor != ':') {\n                    yl_json_set_error(\"expected ':' after JSON object key\");\n                    return false;\n                }\n                (*cursor)++;\n                YlJsonSlice child;\n                if (!yl_json_parse_value(cursor, depth + 1U, &child))\n                    return false;\n                yl_json_skip_space(cursor);\n                if (**cursor == '}') break;\n                if (**cursor != ',') {\n                    yl_json_set_error(\"expected ',' or '}' in JSON object\");\n                    return false;\n                }\n                (*cursor)++;\n                yl_json_skip_space(cursor);\n                if (**cursor == '}') {\n                    yl_json_set_error(\"trailing comma in JSON object\");\n                    return false;\n                }\n            }\n        }\n        if (**cursor != '}') {\n            yl_json_set_error(\"unterminated JSON object\");\n            return false;\n        }\n        (*cursor)++;\n    } else if (**cursor == '[') {\n        kind = 'a';\n        (*cursor)++;\n        yl_json_skip_space(cursor);\n        if (**cursor != ']') {\n            for (;;) {\n                YlJsonSlice child;\n                if (!yl_json_parse_value(cursor, depth + 1U, &child))\n                    return false;\n                yl_json_skip_space(cursor);\n                if (**cursor == ']') break;\n                if (**cursor != ',') {\n                    yl_json_set_error(\"expected ',' or ']' in JSON array\");\n                    return false;\n                }\n                (*cursor)++;\n                yl_json_skip_space(cursor);\n                if (**cursor == ']') {\n                    yl_json_set_error(\"trailing comma in JSON array\");\n                    return false;\n                }\n            }\n        }\n        if (**cursor != ']') {\n            yl_json_set_error(\"unterminated JSON array\");\n            return false;\n        }\n        (*cursor)++;\n    } else if (strncmp(*cursor, \"true\", 4) == 0) {\n        kind = 't';\n        *cursor += 4;\n    } else if (strncmp(*cursor, \"false\", 5) == 0) {\n        kind = 'f';\n        *cursor += 5;\n    } else if (strncmp(*cursor, \"null\", 4) == 0) {\n        kind = 'z';\n        *cursor += 4;\n    } else {\n        kind = 'n';\n        if (!yl_json_parse_number(cursor)) return false;\n    }\n    slice->start = start;\n    slice->length = (size_t)(*cursor - start);\n    slice->kind = kind;\n    return true;\n}\n\nstatic bool yl_json_parse_document(const char *json, YlJsonSlice *root) {\n    if (!json) {\n        yl_json_set_error(\"JSON text cannot be null\");\n        return false;\n    }\n    if (strlen(json) > 10485760U) {\n        yl_json_set_error(\"JSON text exceeded the 10 MiB limit\");\n        return false;\n    }\n    const char *cursor = json;\n    if (!yl_json_parse_value(&cursor, 0U, root)) return false;\n    yl_json_skip_space(&cursor);\n    if (*cursor != '\\0') {\n        yl_json_set_error(\"unexpected trailing characters after JSON value\");\n        return false;\n    }\n    return true;\n}\n\nstatic bool yl_json_decode_hex_quad(const char *text, uint32_t *value) {\n    uint32_t result = 0;\n    for (unsigned i = 0; i < 4; i++) {\n        int digit = yl_json_hex_value(text[i]);\n        if (digit < 0) return false;\n        result = (result << 4U) | (uint32_t)digit;\n    }\n    *value = result;\n    return true;\n}\n\nstatic size_t yl_json_write_utf8(char *output, uint32_t codepoint) {\n    if (codepoint <= 0x7FU) {\n        output[0] = (char)codepoint;\n        return 1U;\n    }\n    if (codepoint <= 0x7FFU) {\n        output[0] = (char)(0xC0U | (codepoint >> 6U));\n        output[1] = (char)(0x80U | (codepoint & 0x3FU));\n        return 2U;\n    }\n    if (codepoint <= 0xFFFFU) {\n        output[0] = (char)(0xE0U | (codepoint >> 12U));\n        output[1] = (char)(0x80U | ((codepoint >> 6U) & 0x3FU));\n        output[2] = (char)(0x80U | (codepoint & 0x3FU));\n        return 3U;\n    }\n    output[0] = (char)(0xF0U | (codepoint >> 18U));\n    output[1] = (char)(0x80U | ((codepoint >> 12U) & 0x3FU));\n    output[2] = (char)(0x80U | ((codepoint >> 6U) & 0x3FU));\n    output[3] = (char)(0x80U | (codepoint & 0x3FU));\n    return 4U;\n}\n\nstatic char *yl_json_decode_string(const YlJsonSlice *slice) {\n    if (!slice || slice->kind != 's' || slice->length < 2U ||\n        slice->start[0] != '\"' || slice->start[slice->length - 1U] != '\"') {\n        yl_json_set_error(\"expected a JSON string value\");\n        return NULL;\n    }\n    char *out = (char *)malloc(slice->length + 1U);\n    if (!out) {\n        yl_json_set_error(\"out of memory decoding JSON string\");\n        return NULL;\n    }\n    size_t write_at = 0;\n    size_t i = 1U;\n    size_t content_end = slice->length - 1U;\n    while (i < content_end) {\n        unsigned char ch = (unsigned char)slice->start[i++];\n        if (ch != '\\\\') {\n            out[write_at++] = (char)ch;\n            continue;\n        }\n        char escape = slice->start[i++];\n        switch (escape) {\n            case '\"': out[write_at++] = '\"'; break;\n            case '\\\\': out[write_at++] = '\\\\'; break;\n            case '/': out[write_at++] = '/'; break;\n            case 'b': out[write_at++] = '\\b'; break;\n            case 'f': out[write_at++] = '\\f'; break;\n            case 'n': out[write_at++] = '\\n'; break;\n            case 'r': out[write_at++] = '\\r'; break;\n            case 't': out[write_at++] = '\\t'; break;\n            case 'u': {\n                uint32_t codepoint = 0;\n                if (!yl_json_decode_hex_quad(slice->start + i, &codepoint)) {\n                    free(out);\n                    yl_json_set_error(\"invalid Unicode escape in JSON string\");\n                    return NULL;\n                }\n                i += 4U;\n                if (codepoint >= 0xD800U && codepoint <= 0xDBFFU) {\n                    if (i + 6U > content_end ||\n                        slice->start[i] != '\\\\' || slice->start[i + 1U] != 'u') {\n                        free(out);\n                        yl_json_set_error(\"unpaired high surrogate in JSON string\");\n                        return NULL;\n                    }\n                    uint32_t low = 0;\n                    if (!yl_json_decode_hex_quad(slice->start + i + 2U, &low) ||\n                        low < 0xDC00U || low > 0xDFFFU) {\n                        free(out);\n                        yl_json_set_error(\"invalid low surrogate in JSON string\");\n                        return NULL;\n                    }\n                    codepoint = 0x10000U +\n                        ((codepoint - 0xD800U) << 10U) + (low - 0xDC00U);\n                    i += 6U;\n                } else if (codepoint >= 0xDC00U && codepoint <= 0xDFFFU) {\n                    free(out);\n                    yl_json_set_error(\"unpaired low surrogate in JSON string\");\n                    return NULL;\n                }\n                if (codepoint == 0U) {\n                    free(out);\n                    yl_json_set_error(\"JSON strings containing U+0000 cannot be represented by YLang strings\");\n                    return NULL;\n                }\n                write_at += yl_json_write_utf8(out + write_at, codepoint);\n                break;\n            }\n            default:\n                free(out);\n                yl_json_set_error(\"invalid escape in JSON string\");\n                return NULL;\n        }\n    }\n    out[write_at] = '\\0';\n    return out;\n}\n\nstatic bool yl_json_string_equals(const YlJsonSlice *slice,\n                                  const char *expected) {\n    char *decoded = yl_json_decode_string(slice);\n    if (!decoded) return false;\n    bool matches = strcmp(decoded, expected) == 0;\n    free(decoded);\n    return matches;\n}\n\nstatic bool yl_json_find_member(const char *json, const char *key,\n                               YlJsonSlice *value, bool *found) {\n    *found = false;\n    YlJsonSlice root;\n    if (!yl_json_parse_document(json, &root)) return false;\n    if (root.kind != 'o') {\n        yl_json_set_error(\"expected a JSON object\");\n        return false;\n    }\n    const char *cursor = root.start + 1U;\n    for (;;) {\n        yl_json_skip_space(&cursor);\n        if (*cursor == '}') return true;\n        YlJsonSlice key_slice = {0};\n        key_slice.start = cursor;\n        if (!yl_json_parse_string(&cursor)) return false;\n        key_slice.length = (size_t)(cursor - key_slice.start);\n        key_slice.kind = 's';\n        yl_json_skip_space(&cursor);\n        if (*cursor != ':') {\n            yl_json_set_error(\"expected ':' after JSON object key\");\n            return false;\n        }\n        cursor++;\n        YlJsonSlice candidate;\n        if (!yl_json_parse_value(&cursor, 1U, &candidate)) return false;\n        if (yl_json_string_equals(&key_slice, key)) {\n            *value = candidate;\n            *found = true;\n            return true;\n        }\n        yl_json_skip_space(&cursor);\n        if (*cursor == '}') return true;\n        if (*cursor != ',') {\n            yl_json_set_error(\"expected ',' or '}' in JSON object\");\n            return false;\n        }\n        cursor++;\n    }\n}\n\nstatic const char *yl_json_copy_slice(const YlJsonSlice *slice) {\n    if (slice->length == SIZE_MAX) {\n        yl_json_set_error(\"JSON value size overflow\");\n        return (const char *)yl_json_empty_string();\n    }\n    char *copy = (char *)yl_realloc_tracked(NULL, slice->length + 1U);\n    memcpy(copy, slice->start, slice->length);\n    copy[slice->length] = '\\0';\n    return (const char *)copy;\n}\n\nstatic const char *yl_json_get_raw(const char *json, const char *key) {\n    yl_json_error_value[0] = '\\0';\n    YlJsonSlice value;\n    bool found = false;\n    if (!yl_json_find_member(json, key, &value, &found)) {\n        return yl_json_empty_string();\n    }\n    if (!found) {\n        yl_json_set_error(\"JSON object key was not found\");\n        return yl_json_empty_string();\n    }\n    return yl_json_copy_slice(&value);\n}\n\nstatic const char *yl_json_get_string(const char *json, const char *key) {\n    yl_json_error_value[0] = '\\0';\n    YlJsonSlice value;\n    bool found = false;\n    if (!yl_json_find_member(json, key, &value, &found)) {\n        return yl_json_empty_string();\n    }\n    if (!found) {\n        yl_json_set_error(\"JSON object key was not found\");\n        return yl_json_empty_string();\n    }\n    char *decoded = yl_json_decode_string(&value);\n    if (!decoded) return yl_json_empty_string();\n    return (const char *)yl_track(decoded);\n}\n\nstatic bool yl_json_has(const char *json, const char *key) {\n    yl_json_error_value[0] = '\\0';\n    YlJsonSlice value;\n    bool found = false;\n    if (!yl_json_find_member(json, key, &value, &found)) return false;\n    return found;\n}\n\nstatic char *yl_json_number_text(const YlJsonSlice *value) {\n    char *text = (char *)malloc(value->length + 1U);\n    if (!text) {\n        yl_json_set_error(\"out of memory parsing JSON number\");\n        return NULL;\n    }\n    memcpy(text, value->start, value->length);\n    text[value->length] = '\\0';\n    return text;\n}\n\nstatic int64_t yl_json_get_int(const char *json, const char *key) {\n    yl_json_error_value[0] = '\\0';\n    YlJsonSlice value;\n    bool found = false;\n    if (!yl_json_find_member(json, key, &value, &found)) return 0;\n    if (!found) {\n        yl_json_set_error(\"JSON object key was not found\");\n        return 0;\n    }\n    if (value.kind != 'n') {\n        yl_json_set_error(\"JSON value is not a number\");\n        return 0;\n    }\n    char *text = yl_json_number_text(&value);\n    if (!text) return 0;\n    errno = 0;\n    char *end = NULL;\n    long long result = strtoll(text, &end, 10);\n    bool valid = errno != ERANGE && end == text + value.length;\n    free(text);\n    if (!valid) {\n        yl_json_set_error(\"JSON value is not an in-range integer\");\n        return 0;\n    }\n    return (int64_t)result;\n}\n\nstatic double yl_json_get_float(const char *json, const char *key) {\n    yl_json_error_value[0] = '\\0';\n    YlJsonSlice value;\n    bool found = false;\n    if (!yl_json_find_member(json, key, &value, &found)) return 0.0;\n    if (!found) {\n        yl_json_set_error(\"JSON object key was not found\");\n        return 0.0;\n    }\n    if (value.kind != 'n') {\n        yl_json_set_error(\"JSON value is not a number\");\n        return 0.0;\n    }\n    char *text = yl_json_number_text(&value);\n    if (!text) return 0.0;\n    errno = 0;\n    char *end = NULL;\n    double result = strtod(text, &end);\n    bool valid = errno != ERANGE && end == text + value.length && isfinite(result);\n    free(text);\n    if (!valid) {\n        yl_json_set_error(\"JSON number is invalid or out of range\");\n        return 0.0;\n    }\n    return result;\n}\n\nstatic bool yl_json_get_bool(const char *json, const char *key) {\n    yl_json_error_value[0] = '\\0';\n    YlJsonSlice value;\n    bool found = false;\n    if (!yl_json_find_member(json, key, &value, &found)) return false;\n    if (!found) {\n        yl_json_set_error(\"JSON object key was not found\");\n        return false;\n    }\n    if (value.kind == 't') return true;\n    if (value.kind == 'f') return false;\n    yl_json_set_error(\"JSON value is not a boolean\");\n    return false;\n}\n\nstatic int64_t yl_json_array_length(const char *json) {\n    yl_json_error_value[0] = '\\0';\n    YlJsonSlice root;\n    if (!yl_json_parse_document(json, &root)) return 0;\n    if (root.kind != 'a') {\n        yl_json_set_error(\"expected a JSON array\");\n        return 0;\n    }\n    const char *cursor = root.start + 1U;\n    int64_t count = 0;\n    for (;;) {\n        yl_json_skip_space(&cursor);\n        if (*cursor == ']') return count;\n        YlJsonSlice item;\n        if (!yl_json_parse_value(&cursor, 1U, &item)) return 0;\n        if (count == INT64_MAX) {\n            yl_json_set_error(\"JSON array length exceeds the integer range\");\n            return 0;\n        }\n        count++;\n        yl_json_skip_space(&cursor);\n        if (*cursor == ']') return count;\n        if (*cursor != ',') {\n            yl_json_set_error(\"expected ',' or ']' in JSON array\");\n            return 0;\n        }\n        cursor++;\n    }\n}\n\nstatic const char *yl_json_array_item(const char *json, int64_t wanted) {\n    yl_json_error_value[0] = '\\0';\n    if (wanted < 0) {\n        yl_json_set_error(\"JSON array index cannot be negative\");\n        return yl_json_empty_string();\n    }\n    YlJsonSlice root;\n    if (!yl_json_parse_document(json, &root)) return yl_json_empty_string();\n    if (root.kind != 'a') {\n        yl_json_set_error(\"expected a JSON array\");\n        return yl_json_empty_string();\n    }\n    const char *cursor = root.start + 1U;\n    int64_t index = 0;\n    for (;;) {\n        yl_json_skip_space(&cursor);\n        if (*cursor == ']') {\n            yl_json_set_error(\"JSON array index is out of range\");\n            return yl_json_empty_string();\n        }\n        YlJsonSlice item;\n        if (!yl_json_parse_value(&cursor, 1U, &item))\n            return yl_json_empty_string();\n        if (index == wanted) return yl_json_copy_slice(&item);\n        index++;\n        yl_json_skip_space(&cursor);\n        if (*cursor == ']') {\n            yl_json_set_error(\"JSON array index is out of range\");\n            return yl_json_empty_string();\n        }\n        if (*cursor != ',') {\n            yl_json_set_error(\"expected ',' or ']' in JSON array\");\n            return yl_json_empty_string();\n        }\n        cursor++;\n    }\n}\n\nstatic const char *yl_json_quote(const char *text) {\n    yl_json_error_value[0] = '\\0';\n    if (!text) {\n        yl_json_set_error(\"json_quote() received a null string\");\n        return yl_json_empty_string();\n    }\n    size_t length = strlen(text);\n    if (length > (SIZE_MAX - 3U) / 6U) {\n        yl_json_set_error(\"JSON string encoding size overflow\");\n        return yl_json_empty_string();\n    }\n    size_t capacity = length * 6U + 3U;\n    char *out = (char *)malloc(capacity);\n    if (!out) {\n        yl_json_set_error(\"out of memory while encoding JSON string\");\n        return yl_json_empty_string();\n    }\n    static const char hex[] = \"0123456789abcdef\";\n    size_t j = 0;\n    out[j++] = '\"';\n    for (size_t i = 0; i < length; i++) {\n        unsigned char ch = (unsigned char)text[i];\n        switch (ch) {\n            case '\"': out[j++] = '\\\\'; out[j++] = '\"'; break;\n            case '\\\\': out[j++] = '\\\\'; out[j++] = '\\\\'; break;\n            case '\\b': out[j++] = '\\\\'; out[j++] = 'b'; break;\n            case '\\f': out[j++] = '\\\\'; out[j++] = 'f'; break;\n            case '\\n': out[j++] = '\\\\'; out[j++] = 'n'; break;\n            case '\\r': out[j++] = '\\\\'; out[j++] = 'r'; break;\n            case '\\t': out[j++] = '\\\\'; out[j++] = 't'; break;\n            default:\n                if (ch < 0x20U) {\n                    out[j++] = '\\\\'; out[j++] = 'u';\n                    out[j++] = '0'; out[j++] = '0';\n                    out[j++] = hex[ch >> 4U];\n                    out[j++] = hex[ch & 0x0FU];\n                } else out[j++] = (char)ch;\n                break;\n        }\n    }\n    out[j++] = '\"';\n    out[j] = '\\0';\n    return (const char *)yl_track(out);\n}\n\nstatic const char *yl_json_last_error(void) {\n    return yl_json_error_value;\n}\n", out);
}

bool generate_c(Compiler *c, const char *path)
{
    FILE *out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "ylang: cannot write generated C file '%s': %s\n", path, strerror(errno));
        return false;
    }
    emit_runtime(out, c);

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
        fprintf(out, "/* YLang function: %s */\n", fn->name);
        fprintf(out, "%s %s(", c_base_type(fn->return_type), fn->c_name);
        if (fn->param_count == 0) fputs("void", out);
        for (size_t j = 0; j < fn->param_count; j++) {
            if (j) fputs(", ", out);
            { emit_param_type(out, fn->params[j]); fprintf(out, " %s", fn->params[j]->c_name); }
        }
        fputs(") ", out); fputc('\n', out);
        EmitContext context = { .compiler = c, .function_body = true };
        for (size_t j = 0; j < fn->param_count; j++) {
            emit_track_local(&context, fn->params[j]);
        }
        emit_stmt(out, fn->body, 0, &context);
        context.locals = NULL;
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

