#define _POSIX_C_SOURCE 200809L
#include "internal.h"
#include <runtime_template.h>

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef YLANG_VERSION
#define YLANG_VERSION "dev"
#endif

static void emit_indent(FILE *out, unsigned indent);

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
        case TYPE_STRING:
        case TYPE_BORROWED_STRING:
        case TYPE_OWNED_STRING: return "const char *";
        case TYPE_VOID: return "void";
        case TYPE_ERROR: return "int";
        default: return "int64_t";
    }
}

static void append_c_quoted_bytes(StringBuilder *sb,
                                  const unsigned char *text, size_t length)
{
    sb_append(sb, "\"");
    for (size_t i = 0; i < length; i++) {
        unsigned char ch = text[i];
        switch (ch) {
            case '\\': sb_append(sb, "\\\\"); break;
            case '"': sb_append(sb, "\\\""); break;
            /* Avoid C trigraph translation in generated string literals. */
            case '?': sb_append(sb, "\\?"); break;
            case '\n': sb_append(sb, "\\n"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\t': sb_append(sb, "\\t"); break;
            default:
                if (ch < 32U || ch >= 127U) {
                    sb_appendf(sb, "\\%03o", (unsigned)ch);
                } else {
                    sb_append_n(sb, (const char *)&text[i], 1U);
                }
                break;
        }
    }
    sb_append(sb, "\"");
}

static void append_c_quoted(StringBuilder *sb, const char *text)
{
    append_c_quoted_bytes(sb, (const unsigned char *)text, strlen(text));
}

static bool is_c_string_type(YType type)
{
    return type == TYPE_STRING || type == TYPE_BORROWED_STRING ||
           type == TYPE_OWNED_STRING;
}

static const char *format_for_type(YType type)
{
    switch (type) {
        case TYPE_INT: return "%lld";
        case TYPE_FLOAT: return "%.17g";
        case TYPE_BOOL: return "%s";
        case TYPE_CHAR: return "%c";
        case TYPE_STRING:
        case TYPE_BORROWED_STRING:
        case TYPE_OWNED_STRING: return "%s";
        case TYPE_VOID: case TYPE_ERROR: return "%s";
    }
    return "%s";
}

static void emit_expr(StringBuilder *sb, Expr *expr);

static bool expr_has_side_effects(Expr *expr)
{
    if (!expr) return false;
    switch (expr->kind) {
        case EXPR_CALL:
        case EXPR_ASSIGN:
            return true;
        case EXPR_UNARY:
            return expr_has_side_effects(expr->as.unary.right);
        case EXPR_BINARY:
            return expr_has_side_effects(expr->as.binary.left) ||
                   expr_has_side_effects(expr->as.binary.right);
        case EXPR_STRUCT_INIT:
            for (size_t i = 0; i < expr->as.struct_init.count; i++)
                if (expr_has_side_effects(expr->as.struct_init.args[i])) return true;
            return false;
        case EXPR_MEMBER:
            return expr_has_side_effects(expr->as.member.target);
        case EXPR_ARRAY:
            for (size_t i = 0; i < expr->as.array.count; i++)
                if (expr_has_side_effects(expr->as.array.items[i])) return true;
            return false;
        case EXPR_INDEX:
            return expr_has_side_effects(expr->as.index.target) ||
                   expr_has_side_effects(expr->as.index.index);
        case EXPR_FSTRING:
            for (size_t i = 0; i < expr->as.fstring.count; i++)
                if (expr_has_side_effects(expr->as.fstring.parts[i].expression)) return true;
            return false;
        case EXPR_ERROR:
        case EXPR_ENUM_VALUE:
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_BOOL:
        case EXPR_CHAR:
        case EXPR_STRING:
        case EXPR_NAME:
            return false;
    }
    return false;
}

static bool binary_needs_sequence(Expr *expr)
{
    if (!expr || expr->kind != EXPR_BINARY) return false;
    TokenType op = expr->as.binary.op.type;
    if (op == TOKEN_AND || op == TOKEN_OR) return false;
    return expr_has_side_effects(expr->as.binary.left) ||
           expr_has_side_effects(expr->as.binary.right);
}

static bool call_argument_is_array(const Expr *call, size_t index)
{
    return call && call->kind == EXPR_CALL && call->as.call.function &&
           index < call->as.call.function->param_count &&
           call->as.call.function->params[index]->is_array;
}

static bool call_needs_sequence(Expr *expr)
{
    if (!expr || expr->kind != EXPR_CALL || expr->as.call.count < 2U) return false;
    for (size_t i = 0; i < expr->as.call.count; i++)
        if (expr_has_side_effects(expr->as.call.args[i])) return true;
    return false;
}

static bool struct_init_needs_sequence(Expr *expr)
{
    return expr && expr->kind == EXPR_STRUCT_INIT &&
           expr->as.struct_init.count > 1U && expr_has_side_effects(expr);
}

static bool array_init_needs_sequence(Expr *expr)
{
    return expr && expr->kind == EXPR_ARRAY &&
           expr->as.array.count > 1U && expr_has_side_effects(expr);
}

static bool assignment_needs_sequence(Expr *expr)
{
    return expr && expr->kind == EXPR_ASSIGN &&
           (expr_has_side_effects(expr->as.assign.target) ||
            expr_has_side_effects(expr->as.assign.right));
}

static void assign_expr_codegen_ids(Expr *expr, size_t *next_id)
{
    if (!expr || expr->codegen_id != 0U) return;
    expr->codegen_id = (*next_id)++;
    switch (expr->kind) {
        case EXPR_UNARY:
            assign_expr_codegen_ids(expr->as.unary.right, next_id);
            break;
        case EXPR_BINARY:
            assign_expr_codegen_ids(expr->as.binary.left, next_id);
            assign_expr_codegen_ids(expr->as.binary.right, next_id);
            break;
        case EXPR_ASSIGN:
            assign_expr_codegen_ids(expr->as.assign.target, next_id);
            assign_expr_codegen_ids(expr->as.assign.right, next_id);
            break;
        case EXPR_CALL:
            for (size_t i = 0; i < expr->as.call.count; i++)
                assign_expr_codegen_ids(expr->as.call.args[i], next_id);
            break;
        case EXPR_STRUCT_INIT:
            for (size_t i = 0; i < expr->as.struct_init.count; i++)
                assign_expr_codegen_ids(expr->as.struct_init.args[i], next_id);
            break;
        case EXPR_MEMBER:
            assign_expr_codegen_ids(expr->as.member.target, next_id);
            break;
        case EXPR_ARRAY:
            for (size_t i = 0; i < expr->as.array.count; i++)
                assign_expr_codegen_ids(expr->as.array.items[i], next_id);
            break;
        case EXPR_INDEX:
            assign_expr_codegen_ids(expr->as.index.target, next_id);
            assign_expr_codegen_ids(expr->as.index.index, next_id);
            break;
        case EXPR_FSTRING:
            for (size_t i = 0; i < expr->as.fstring.count; i++)
                assign_expr_codegen_ids(expr->as.fstring.parts[i].expression, next_id);
            break;
        case EXPR_ERROR:
        case EXPR_ENUM_VALUE:
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_BOOL:
        case EXPR_CHAR:
        case EXPR_STRING:
        case EXPR_NAME:
            break;
    }
}

static void assign_stmt_codegen_ids(Stmt *stmt, size_t *next_id)
{
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_BLOCK:
            for (size_t i = 0; i < stmt->as.block.count; i++)
                assign_stmt_codegen_ids(stmt->as.block.items[i], next_id);
            break;
        case STMT_VAR:
            assign_expr_codegen_ids(stmt->as.variable->initializer, next_id);
            break;
        case STMT_PRINT:
            for (size_t i = 0; i < stmt->as.print.count; i++)
                assign_expr_codegen_ids(stmt->as.print.args[i], next_id);
            break;
        case STMT_IF:
            assign_expr_codegen_ids(stmt->as.if_stmt.condition, next_id);
            assign_stmt_codegen_ids(stmt->as.if_stmt.then_branch, next_id);
            assign_stmt_codegen_ids(stmt->as.if_stmt.else_branch, next_id);
            break;
        case STMT_LOOP:
            assign_stmt_codegen_ids(stmt->as.loop_body, next_id);
            break;
        case STMT_WHILE:
            assign_expr_codegen_ids(stmt->as.while_stmt.condition, next_id);
            assign_stmt_codegen_ids(stmt->as.while_stmt.body, next_id);
            break;
        case STMT_FOR:
            assign_stmt_codegen_ids(stmt->as.for_stmt.initializer, next_id);
            assign_expr_codegen_ids(stmt->as.for_stmt.condition, next_id);
            assign_expr_codegen_ids(stmt->as.for_stmt.increment, next_id);
            assign_stmt_codegen_ids(stmt->as.for_stmt.body, next_id);
            break;
        case STMT_RETURN:
            assign_expr_codegen_ids(stmt->as.return_value, next_id);
            break;
        case STMT_MATCH:
            assign_expr_codegen_ids(stmt->as.match_stmt.value, next_id);
            for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++)
                assign_stmt_codegen_ids(stmt->as.match_stmt.arms[i]->body, next_id);
            break;
        case STMT_EXPR:
            assign_expr_codegen_ids(stmt->as.expression, next_id);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
    }
}

static void emit_temp_name(StringBuilder *sb, Expr *owner, const char *suffix)
{
    sb_appendf(sb, "yl_codegen_tmp_%zu_%s", owner->codegen_id, suffix);
}

static void emit_temp_declaration(FILE *out, Compiler *compiler, Expr *owner,
                                  const char *suffix, YType type,
                                  bool pointer, unsigned indent)
{
    if (type == TYPE_ERROR || type == TYPE_VOID) return;
    for (unsigned i = 0; i < indent; i++) fputs("    ", out);
    fprintf(out, "%s%s yl_codegen_tmp_%zu_%s;\n",
            c_base_type(compiler, type), pointer ? " *" : "",
            owner->codegen_id, suffix);
}

static void emit_temp_declarations_expr(Compiler *compiler, FILE *out,
                                        Expr *expr, unsigned indent)
{
    if (!expr) return;
    if (binary_needs_sequence(expr)) {
        emit_temp_declaration(out, compiler, expr, "left",
                              expr->as.binary.left->type, false, indent);
        emit_temp_declaration(out, compiler, expr, "right",
                              expr->as.binary.right->type, false, indent);
    }
    if (call_needs_sequence(expr)) {
        for (size_t i = 0; i < expr->as.call.count; i++) {
            if (call_argument_is_array(expr, i)) continue;
            char suffix[32];
            (void)snprintf(suffix, sizeof(suffix), "arg%zu", i);
            emit_temp_declaration(out, compiler, expr, suffix,
                                  expr->as.call.args[i]->type, false, indent);
        }
    }
    if (struct_init_needs_sequence(expr)) {
        for (size_t i = 0; i < expr->as.struct_init.count; i++) {
            char suffix[32];
            (void)snprintf(suffix, sizeof(suffix), "field%zu", i);
            emit_temp_declaration(out, compiler, expr, suffix,
                                  expr->as.struct_init.args[i]->type, false, indent);
        }
    }
    if (array_init_needs_sequence(expr)) {
        for (size_t i = 0; i < expr->as.array.count; i++) {
            char suffix[32];
            (void)snprintf(suffix, sizeof(suffix), "element%zu", i);
            emit_temp_declaration(out, compiler, expr, suffix,
                                  expr->as.array.items[i]->type, false, indent);
        }
    }
    if (assignment_needs_sequence(expr)) {
        emit_temp_declaration(out, compiler, expr, "target",
                              expr->as.assign.target->type, true, indent);
        emit_temp_declaration(out, compiler, expr, "value",
                              expr->as.assign.right->type, false, indent);
    }

    switch (expr->kind) {
        case EXPR_UNARY:
            emit_temp_declarations_expr(compiler, out, expr->as.unary.right, indent);
            break;
        case EXPR_BINARY:
            emit_temp_declarations_expr(compiler, out, expr->as.binary.left, indent);
            emit_temp_declarations_expr(compiler, out, expr->as.binary.right, indent);
            break;
        case EXPR_ASSIGN:
            emit_temp_declarations_expr(compiler, out, expr->as.assign.target, indent);
            emit_temp_declarations_expr(compiler, out, expr->as.assign.right, indent);
            break;
        case EXPR_CALL:
            for (size_t i = 0; i < expr->as.call.count; i++)
                emit_temp_declarations_expr(compiler, out, expr->as.call.args[i], indent);
            break;
        case EXPR_STRUCT_INIT:
            for (size_t i = 0; i < expr->as.struct_init.count; i++)
                emit_temp_declarations_expr(compiler, out, expr->as.struct_init.args[i], indent);
            break;
        case EXPR_MEMBER:
            emit_temp_declarations_expr(compiler, out, expr->as.member.target, indent);
            break;
        case EXPR_ARRAY:
            for (size_t i = 0; i < expr->as.array.count; i++)
                emit_temp_declarations_expr(compiler, out, expr->as.array.items[i], indent);
            break;
        case EXPR_INDEX:
            emit_temp_declarations_expr(compiler, out, expr->as.index.target, indent);
            emit_temp_declarations_expr(compiler, out, expr->as.index.index, indent);
            break;
        case EXPR_FSTRING:
            for (size_t i = 0; i < expr->as.fstring.count; i++)
                emit_temp_declarations_expr(compiler, out,
                    expr->as.fstring.parts[i].expression, indent);
            break;
        case EXPR_ERROR:
        case EXPR_ENUM_VALUE:
        case EXPR_INT:
        case EXPR_FLOAT:
        case EXPR_BOOL:
        case EXPR_CHAR:
        case EXPR_STRING:
        case EXPR_NAME:
            break;
    }
}

static void emit_temp_declarations_stmt(Compiler *compiler, FILE *out,
                                        Stmt *stmt, unsigned indent)
{
    if (!stmt) return;
    switch (stmt->kind) {
        case STMT_BLOCK:
            for (size_t i = 0; i < stmt->as.block.count; i++)
                emit_temp_declarations_stmt(compiler, out, stmt->as.block.items[i], indent);
            break;
        case STMT_VAR: {
            VarDecl *var = stmt->as.variable;
            if (var->type == TYPE_OWNED_STRING) {
                emit_indent(out, indent);
                fprintf(out, "%s %s = NULL;\n",
                        c_base_type(compiler, var->type), var->c_name);
            }
            emit_temp_declarations_expr(compiler, out, var->initializer, indent);
            break;
        }
        case STMT_PRINT:
            for (size_t i = 0; i < stmt->as.print.count; i++)
                emit_temp_declarations_expr(compiler, out, stmt->as.print.args[i], indent);
            break;
        case STMT_IF:
            emit_temp_declarations_expr(compiler, out, stmt->as.if_stmt.condition, indent);
            emit_temp_declarations_stmt(compiler, out, stmt->as.if_stmt.then_branch, indent);
            emit_temp_declarations_stmt(compiler, out, stmt->as.if_stmt.else_branch, indent);
            break;
        case STMT_LOOP:
            emit_temp_declarations_stmt(compiler, out, stmt->as.loop_body, indent);
            break;
        case STMT_WHILE:
            emit_temp_declarations_expr(compiler, out, stmt->as.while_stmt.condition, indent);
            emit_temp_declarations_stmt(compiler, out, stmt->as.while_stmt.body, indent);
            break;
        case STMT_FOR:
            emit_temp_declarations_stmt(compiler, out, stmt->as.for_stmt.initializer, indent);
            emit_temp_declarations_expr(compiler, out, stmt->as.for_stmt.condition, indent);
            emit_temp_declarations_expr(compiler, out, stmt->as.for_stmt.increment, indent);
            emit_temp_declarations_stmt(compiler, out, stmt->as.for_stmt.body, indent);
            break;
        case STMT_RETURN:
            emit_temp_declarations_expr(compiler, out, stmt->as.return_value, indent);
            break;
        case STMT_MATCH:
            emit_temp_declarations_expr(compiler, out, stmt->as.match_stmt.value, indent);
            for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++)
                emit_temp_declarations_stmt(compiler, out, stmt->as.match_stmt.arms[i]->body, indent);
            break;
        case STMT_EXPR:
            emit_temp_declarations_expr(compiler, out, stmt->as.expression, indent);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
    }
}

static void emit_temp_assignment(StringBuilder *sb, Expr *owner,
                                 const char *suffix, Expr *expr)
{
    sb_append(sb, "(");
    emit_temp_name(sb, owner, suffix);
    sb_append(sb, " = ");
    emit_expr(sb, expr);
    sb_append(sb, ")");
}

static void emit_binary_operation_with_temps(StringBuilder *sb, Expr *expr)
{
    Expr *left = expr->as.binary.left;
    Expr *right = expr->as.binary.right;
    TokenType op = expr->as.binary.op.type;

    if (expr->type == TYPE_INT && op == TOKEN_PLUS) {
        sb_append(sb, "yl_add_i64(");
        emit_temp_name(sb, expr, "left"); sb_append(sb, ", ");
        emit_temp_name(sb, expr, "right"); sb_append(sb, ")");
    } else if (expr->type == TYPE_INT && op == TOKEN_MINUS) {
        sb_append(sb, "yl_sub_i64(");
        emit_temp_name(sb, expr, "left"); sb_append(sb, ", ");
        emit_temp_name(sb, expr, "right"); sb_append(sb, ")");
    } else if (expr->type == TYPE_INT && op == TOKEN_STAR) {
        sb_append(sb, "yl_mul_i64(");
        emit_temp_name(sb, expr, "left"); sb_append(sb, ", ");
        emit_temp_name(sb, expr, "right"); sb_append(sb, ")");
    } else if (expr->type == TYPE_INT && op == TOKEN_SLASH) {
        sb_append(sb, "yl_div_i64(");
        emit_temp_name(sb, expr, "left"); sb_append(sb, ", ");
        emit_temp_name(sb, expr, "right"); sb_append(sb, ")");
    } else if (expr->type == TYPE_INT && op == TOKEN_PERCENT) {
        sb_append(sb, "yl_mod_i64(");
        emit_temp_name(sb, expr, "left"); sb_append(sb, ", ");
        emit_temp_name(sb, expr, "right"); sb_append(sb, ")");
    } else if (left->type == TYPE_FLOAT && op == TOKEN_SLASH) {
        sb_append(sb, "yl_div_f64(");
        emit_temp_name(sb, expr, "left"); sb_append(sb, ", ");
        emit_temp_name(sb, expr, "right"); sb_append(sb, ")");
    } else if (is_c_string_type(left->type) && is_c_string_type(right->type) &&
               (op == TOKEN_LESS || op == TOKEN_LESS_EQUAL ||
                op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL ||
                op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL)) {
        sb_append(sb, "(strcmp(");
        emit_temp_name(sb, expr, "left"); sb_append(sb, ", ");
        emit_temp_name(sb, expr, "right"); sb_append(sb, ") ");
        switch (op) {
            case TOKEN_LESS: sb_append(sb, "< 0)"); break;
            case TOKEN_LESS_EQUAL: sb_append(sb, "<= 0)"); break;
            case TOKEN_GREATER: sb_append(sb, "> 0)"); break;
            case TOKEN_GREATER_EQUAL: sb_append(sb, ">= 0)"); break;
            case TOKEN_EQUAL_EQUAL: sb_append(sb, "== 0)"); break;
            case TOKEN_BANG_EQUAL: sb_append(sb, "!= 0)"); break;
            default: sb_append(sb, "0)"); break;
        }
    } else {
        sb_append(sb, "(");
        emit_temp_name(sb, expr, "left"); sb_append(sb, " ");
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
        sb_append(sb, " ");
        emit_temp_name(sb, expr, "right");
        sb_append(sb, ")");
    }
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
                case TYPE_BORROWED_STRING:
                case TYPE_OWNED_STRING:
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
    if (strcmp(name, "string.own") == 0) return "yl_string_own";
    if (strcmp(name, "owned_string.concat") == 0) return "yl_string_concat";
    if (strcmp(name, "owned_string.replace") == 0) return "yl_string_replace";
    if (strcmp(name, "owned_string.upper") == 0) return "yl_string_upper";
    if (strcmp(name, "owned_string.lower") == 0) return "yl_string_lower";
    if (strcmp(name, "io.read_line_owned") == 0) return "yl_read_line";
    if (strcmp(name, "io.read_file_owned") == 0) return "yl_read_file";
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
    if (strcmp(name, "gui.init") == 0) return "ylgui_api_init";
    if (strcmp(name, "gui.create_window") == 0) return "ylgui_api_create_window";
    if (strcmp(name, "gui.poll") == 0) return "ylgui_api_poll";
    if (strcmp(name, "gui.key_down") == 0) return "ylgui_api_key_down";
    if (strcmp(name, "gui.mouse_x") == 0) return "ylgui_api_mouse_x";
    if (strcmp(name, "gui.mouse_y") == 0) return "ylgui_api_mouse_y";
    if (strcmp(name, "gui.mouse_button_down") == 0) return "ylgui_api_mouse_button_down";
    if (strcmp(name, "gui.ticks_ms") == 0) return "ylgui_api_ticks_ms";
    if (strcmp(name, "gui.clear") == 0) return "ylgui_api_clear";
    if (strcmp(name, "gui.draw_rect") == 0) return "ylgui_api_draw_rect";
    if (strcmp(name, "gui.present") == 0) return "ylgui_api_present";
    if (strcmp(name, "gui.last_error") == 0) return "ylgui_api_last_error";
    if (strcmp(name, "gui.close") == 0) return "ylgui_api_close";
    if (strcmp(name, "gui.shutdown") == 0) return "ylgui_api_shutdown";
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
            bool sequence = struct_init_needs_sequence(expr);
            if (sequence) {
                sb_append(sb, "(");
                for (size_t i = 0; i < expr->as.struct_init.count; i++) {
                    if (i) sb_append(sb, ", ");
                    char suffix[32];
                    (void)snprintf(suffix, sizeof(suffix), "field%zu", i);
                    emit_temp_assignment(sb, expr, suffix, expr->as.struct_init.args[i]);
                }
                sb_append(sb, ", ");
            }
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
                    if (sequence) {
                        char suffix[32];
                        (void)snprintf(suffix, sizeof(suffix), "field%zu", i);
                        emit_temp_name(sb, expr, suffix);
                    } else {
                        emit_expr(sb, expr->as.struct_init.args[i]);
                    }
                }
            }
            sb_append(sb, "}");
            if (sequence) sb_append(sb, ")");
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
        case EXPR_CHAR: {
            unsigned char *bytes = NULL;
            size_t length = 0;
            if (decode_quoted_literal(expr->token, &bytes, &length) &&
                length == 1U) {
                sb_appendf(sb, "((char)0x%02X)", (unsigned)bytes[0]);
            } else {
                sb_append(sb, "((char)0)");
            }
            free(bytes);
            break;
        }
        case EXPR_STRING: {
            unsigned char *bytes = NULL;
            size_t length = 0;
            if (decode_quoted_literal(expr->token, &bytes, &length)) {
                append_c_quoted_bytes(sb, bytes, length);
            } else {
                sb_append(sb, "\"\"");
            }
            free(bytes);
            break;
        }
        case EXPR_FSTRING:
            emit_fstring_expr(sb, expr);
            break;
        case EXPR_ARRAY: {
            bool sequence = array_init_needs_sequence(expr);
            sb_append(sb, "{");
            for (size_t i = 0; i < expr->as.array.count; i++) {
                if (i) sb_append(sb, ", ");
                if (sequence) {
                    char suffix[32];
                    (void)snprintf(suffix, sizeof(suffix), "element%zu", i);
                    emit_temp_name(sb, expr, suffix);
                } else {
                    emit_expr(sb, expr->as.array.items[i]);
                }
            }
            sb_append(sb, "}");
            break;
        }
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
        case EXPR_ASSIGN: {
            if (assignment_needs_sequence(expr)) {
                sb_append(sb, "(");
                emit_temp_name(sb, expr, "target");
                sb_append(sb, " = &(");
                emit_expr(sb, expr->as.assign.target);
                sb_append(sb, "), ");
                emit_temp_name(sb, expr, "value");
                sb_append(sb, " = ");
                emit_expr(sb, expr->as.assign.right);
                sb_append(sb, ", ");
                if (expr->as.assign.target &&
                    expr->as.assign.target->type == TYPE_OWNED_STRING) {
                    sb_append(sb, "yl_release(*");
                    emit_temp_name(sb, expr, "target");
                    sb_append(sb, "), ");
                }
                if (expr->as.assign.is_compound &&
                    expr->as.assign.variable &&
                    expr->as.assign.target->type == TYPE_INT) {
                    sb_append(sb, "yl_add_assign_i64(");
                    emit_temp_name(sb, expr, "target"); sb_append(sb, ", ");
                    emit_temp_name(sb, expr, "value"); sb_append(sb, ")");
                } else if (expr->as.assign.is_compound &&
                           expr->as.assign.variable &&
                           expr->as.assign.target->type == TYPE_FLOAT) {
                    sb_append(sb, "yl_add_assign_f64(");
                    emit_temp_name(sb, expr, "target"); sb_append(sb, ", ");
                    emit_temp_name(sb, expr, "value"); sb_append(sb, ")");
                } else {
                    sb_append(sb, "(*");
                    emit_temp_name(sb, expr, "target"); sb_append(sb, " = ");
                    emit_temp_name(sb, expr, "value"); sb_append(sb, ")");
                }
                sb_append(sb, ")");
            } else if (expr->as.assign.is_compound &&
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
        }
        case EXPR_CALL: {
            /* move(name) is a compile-time ownership transfer; its runtime
               representation is still the same string pointer. */
            if (!expr->as.call.function &&
                strcmp(expr->as.call.name, "move") == 0 &&
                expr->as.call.count == 1U) {
                Expr *argument = expr->as.call.args[0];
                VarDecl *source = argument && argument->kind == EXPR_NAME ?
                    argument->as.name.variable : NULL;
                if (source && source->type == TYPE_OWNED_STRING) {
                    sb_appendf(sb, "yl_move(&%s)", source->c_name);
                } else {
                    emit_expr(sb, argument);
                }
                break;
            }
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
                    case TYPE_BORROWED_STRING:
                        sb_append(sb, "yl_string_own("); emit_expr(sb, arg); sb_append(sb, ")"); break;
                    case TYPE_OWNED_STRING:
                        sb_append(sb, "yl_string_own("); emit_expr(sb, arg); sb_append(sb, ")"); break;
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
            bool sequence = call_needs_sequence(expr);
            if (sequence) {
                sb_append(sb, "(");
                bool wrote_assignment = false;
                for (size_t i = 0; i < expr->as.call.count; i++) {
                    if (call_argument_is_array(expr, i)) continue;
                    if (wrote_assignment) sb_append(sb, ", ");
                    char suffix[32];
                    (void)snprintf(suffix, sizeof(suffix), "arg%zu", i);
                    emit_temp_assignment(sb, expr, suffix, expr->as.call.args[i]);
                    wrote_assignment = true;
                }
                sb_append(sb, ", ");
            }

            const char *builtin_name = builtin_c_name(expr->as.call.name);
            sb_append(sb, expr->as.call.function ? expr->as.call.function->c_name :
                         (builtin_name ? builtin_name : "yl_missing_function"));
            sb_append(sb, "(");
            for (size_t i = 0; i < expr->as.call.count; i++) {
                if (i) sb_append(sb, ", ");
                Expr *arg = expr->as.call.args[i];
                bool array_param = call_argument_is_array(expr, i);
                if (sequence && !array_param) {
                    char suffix[32];
                    (void)snprintf(suffix, sizeof(suffix), "arg%zu", i);
                    emit_temp_name(sb, expr, suffix);
                } else {
                    emit_expr(sb, arg);
                }
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
            if (sequence) sb_append(sb, ")");
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
            if (binary_needs_sequence(expr)) {
                sb_append(sb, "(");
                emit_temp_assignment(sb, expr, "left", left);
                sb_append(sb, ", ");
                emit_temp_assignment(sb, expr, "right", right);
                sb_append(sb, ", ");
                emit_binary_operation_with_temps(sb, expr);
                sb_append(sb, ")");
            } else if (expr->type == TYPE_INT && op == TOKEN_PLUS) {
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
            } else if (is_c_string_type(left->type) && is_c_string_type(right->type) &&
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
            } else if ((op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL) &&
                       is_c_string_type(left->type) && is_c_string_type(right->type)) {
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
    if (var->is_const && !is_c_string_type(var->type)) fputs("const ", out);
    fputs(c_base_type(compiler, var->type), out);
    if (var->is_const && is_c_string_type(var->type)) fputs(" const", out);
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
        case TYPE_BORROWED_STRING:
        case TYPE_OWNED_STRING:
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

typedef struct {
    VarDecl **vars;
    size_t count;
} OwnedScopeFrame;

typedef struct {
    Compiler *compiler;
    Function *function;
    FILE *out;
    OwnedScopeFrame *scopes;
    size_t scope_count;
    size_t scope_capacity;
    size_t *loop_marks;
    size_t loop_count;
    size_t loop_capacity;
} CodegenContext;

static void emit_stmt(CodegenContext *ctx, Stmt *stmt, unsigned indent);
static void emit_control_body(CodegenContext *ctx, Stmt *body, unsigned indent);

static void codegen_fatal_out_of_memory(void)
{
    fputs("ylang: out of memory while preparing owned-string cleanup\n", stderr);
    exit(70);
}

static OwnedScopeFrame make_owned_scope_frame(Stmt *scope_stmt)
{
    OwnedScopeFrame frame = { .vars = NULL, .count = 0U };
    if (!scope_stmt) return frame;
    if (scope_stmt->kind == STMT_VAR) {
        VarDecl *var = scope_stmt->as.variable;
        if (var && var->type == TYPE_OWNED_STRING) frame.count = 1U;
    } else if (scope_stmt->kind == STMT_BLOCK) {
        for (size_t i = 0; i < scope_stmt->as.block.count; i++) {
            Stmt *item = scope_stmt->as.block.items[i];
            if (item && item->kind == STMT_VAR &&
                item->as.variable->type == TYPE_OWNED_STRING) {
                frame.count++;
            }
        }
    }
    if (frame.count == 0U) return frame;
    if (frame.count > SIZE_MAX / sizeof(*frame.vars)) codegen_fatal_out_of_memory();
    frame.vars = malloc(frame.count * sizeof(*frame.vars));
    if (!frame.vars) codegen_fatal_out_of_memory();
    size_t index = 0U;
    if (scope_stmt->kind == STMT_VAR) {
        frame.vars[index++] = scope_stmt->as.variable;
    } else {
        for (size_t i = 0; i < scope_stmt->as.block.count; i++) {
            Stmt *item = scope_stmt->as.block.items[i];
            if (item && item->kind == STMT_VAR &&
                item->as.variable->type == TYPE_OWNED_STRING) {
                frame.vars[index++] = item->as.variable;
            }
        }
    }
    return frame;
}

static void push_owned_scope(CodegenContext *ctx, Stmt *scope_stmt)
{
    if (ctx->scope_count == ctx->scope_capacity) {
        size_t capacity = ctx->scope_capacity ? ctx->scope_capacity * 2U : 8U;
        if (capacity < ctx->scope_capacity ||
            capacity > SIZE_MAX / sizeof(*ctx->scopes)) codegen_fatal_out_of_memory();
        OwnedScopeFrame *grown = realloc(ctx->scopes, capacity * sizeof(*grown));
        if (!grown) codegen_fatal_out_of_memory();
        ctx->scopes = grown;
        ctx->scope_capacity = capacity;
    }
    ctx->scopes[ctx->scope_count++] = make_owned_scope_frame(scope_stmt);
}

static void emit_cleanup_scope_frame(CodegenContext *ctx,
                                     const OwnedScopeFrame *frame,
                                     unsigned indent)
{
    for (size_t i = frame->count; i > 0U; i--) {
        VarDecl *var = frame->vars[i - 1U];
        emit_indent(ctx->out, indent);
        fprintf(ctx->out, "yl_release(%s);\n", var->c_name);
        emit_indent(ctx->out, indent);
        fprintf(ctx->out, "%s = NULL;\n", var->c_name);
    }
}

static void pop_owned_scope(CodegenContext *ctx, unsigned indent, bool emit_cleanup)
{
    if (ctx->scope_count == 0U) return;
    OwnedScopeFrame *frame = &ctx->scopes[ctx->scope_count - 1U];
    if (emit_cleanup) emit_cleanup_scope_frame(ctx, frame, indent);
    free(frame->vars);
    frame->vars = NULL;
    frame->count = 0U;
    ctx->scope_count--;
}

static void push_loop_mark(CodegenContext *ctx)
{
    if (ctx->loop_count == ctx->loop_capacity) {
        size_t capacity = ctx->loop_capacity ? ctx->loop_capacity * 2U : 8U;
        if (capacity < ctx->loop_capacity ||
            capacity > SIZE_MAX / sizeof(*ctx->loop_marks)) codegen_fatal_out_of_memory();
        size_t *grown = realloc(ctx->loop_marks, capacity * sizeof(*grown));
        if (!grown) codegen_fatal_out_of_memory();
        ctx->loop_marks = grown;
        ctx->loop_capacity = capacity;
    }
    ctx->loop_marks[ctx->loop_count++] = ctx->scope_count;
}

static void pop_loop_mark(CodegenContext *ctx)
{
    if (ctx->loop_count > 0U) ctx->loop_count--;
}

static void emit_cleanup_to_loop_mark(CodegenContext *ctx, unsigned indent)
{
    if (ctx->loop_count == 0U) return;
    size_t mark = ctx->loop_marks[ctx->loop_count - 1U];
    for (size_t i = ctx->scope_count; i > mark; i--) {
        emit_cleanup_scope_frame(ctx, &ctx->scopes[i - 1U], indent);
    }
}

static void emit_cleanup_function_owned(CodegenContext *ctx, unsigned indent)
{
    /* Owned parameters belong to the callee and are released after locals. */
    for (size_t i = ctx->function->param_count; i > 0U; i--) {
        VarDecl *param = ctx->function->params[i - 1U];
        if (param->type != TYPE_OWNED_STRING) continue;
        emit_indent(ctx->out, indent);
        fprintf(ctx->out, "yl_release(%s);\n", param->c_name);
        emit_indent(ctx->out, indent);
        fprintf(ctx->out, "%s = NULL;\n", param->c_name);
    }
}

static void emit_cleanup_active_owned(CodegenContext *ctx, unsigned indent)
{
    /* Clean only scopes that are active at this exact control-flow point. */
    for (size_t i = ctx->scope_count; i > 0U; i--) {
        emit_cleanup_scope_frame(ctx, &ctx->scopes[i - 1U], indent);
    }
    emit_cleanup_function_owned(ctx, indent);
}

static const char *source_path_for_token(Compiler *compiler, Token token)
{
    uintptr_t address = (uintptr_t)(const void *)token.start;
    for (size_t i = 0; i < compiler->module_count; i++) {
        Module *module = compiler->modules[i];
        uintptr_t base = (uintptr_t)(const void *)module->source;
        if (address >= base && address - base <= module->source_length) {
            return module->path;
        }
    }
    return compiler->filename ? compiler->filename : "<unknown>";
}

/*
 * Preserve YLang source locations in GCC/Clang debug information. The path is
 * escaped as a C-preprocessor line-marker filename, including unusual quotes
 * and backslashes in paths. These directives are harmless in normal builds
 * and also improve attribution of native compiler diagnostics.
 */
static void emit_source_line_directive(Compiler *compiler, FILE *out, Token token)
{
    if (!token.start || token.line == 0U) return;
    const char *path = source_path_for_token(compiler, token);
    if (!path || !*path) path = "<unknown>";
    fprintf(out, "#line %zu \"", token.line);
    for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
        switch (*p) {
            case '\\\\': fputs("\\\\\\\\", out); break;
            case '"': fputs("\\\\\"", out); break;
            case '\\n': fputs("\\\\n", out); break;
            case '\\r': fputs("\\\\r", out); break;
            case '\\t': fputs("\\\\t", out); break;
            default: fputc((int)*p, out); break;
        }
    }
    fputs("\"\\n", out);
}

static void emit_stmt(CodegenContext *ctx, Stmt *stmt, unsigned indent)
{
    if (!stmt) return;
    Compiler *c = ctx->compiler;
    FILE *out = ctx->out;
    emit_source_line_directive(c, out, stmt->token);
    switch (stmt->kind) {
        case STMT_BLOCK:
            push_owned_scope(ctx, stmt);
            emit_indent(out, indent); fputs("{\n", out);
            for (size_t i = 0; i < stmt->as.block.count; i++)
                emit_stmt(ctx, stmt->as.block.items[i], indent + 1U);
            pop_owned_scope(ctx, indent + 1U, true);
            emit_indent(out, indent); fputs("}\n", out);
            break;
        case STMT_VAR: {
            VarDecl *var = stmt->as.variable;
            if (var->type == TYPE_OWNED_STRING) {
                if (var->initializer) {
                    emit_indent(out, indent);
                    fprintf(out, "%s = ", var->c_name);
                    emit_expr_to_file(out, var->initializer);
                    fputs(";\n", out);
                }
                break;
            }
            if (var->is_array && array_init_needs_sequence(var->initializer)) {
                Expr *array = var->initializer;
                for (size_t i = 0; i < array->as.array.count; i++) {
                    emit_indent(out, indent);
                    fprintf(out, "yl_codegen_tmp_%zu_element%zu = ",
                            array->codegen_id, i);
                    emit_expr_to_file(out, array->as.array.items[i]);
                    fputs(";\n", out);
                }
            }
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
                if (expr->type == TYPE_OWNED_STRING && expr->kind != EXPR_NAME) {
                    char temporary[96];
                    (void)snprintf(temporary, sizeof(temporary), "yl_print_owned_%zu_%zu_%zu",
                                   stmt->token.line, stmt->token.column, i);
                    emit_indent(out, indent);
                    fprintf(out, "const char *%s = ", temporary);
                    emit_expr_to_file(out, expr);
                    fputs(";\n", out);
                    emit_indent(out, indent);
                    fprintf(out, "fputs((const char *)(%s), stdout);\n", temporary);
                    emit_indent(out, indent);
                    fprintf(out, "yl_release(%s);\n", temporary);
                } else if (expr->kind == EXPR_FSTRING) {
                    emit_fstring_print(out, expr, indent);
                } else {
                    emit_print_value(out, expr, indent);
                }
            }
            emit_indent(out, indent); fputs("putchar('\\n');\n", out);
            break;
        case STMT_IF:
            emit_indent(out, indent); fputs("if (", out);
            emit_expr_to_file(out, stmt->as.if_stmt.condition);
            fputs(")\n", out);
            emit_control_body(ctx, stmt->as.if_stmt.then_branch, indent);
            if (stmt->as.if_stmt.else_branch) {
                emit_indent(out, indent); fputs("else\n", out);
                emit_control_body(ctx, stmt->as.if_stmt.else_branch, indent);
            }
            break;
        case STMT_LOOP:
            emit_indent(out, indent); fputs("for (;;)\n", out);
            push_loop_mark(ctx);
            emit_control_body(ctx, stmt->as.loop_body, indent);
            pop_loop_mark(ctx);
            break;
        case STMT_WHILE:
            emit_indent(out, indent); fputs("while (", out);
            emit_expr_to_file(out, stmt->as.while_stmt.condition);
            fputs(")\n", out);
            push_loop_mark(ctx);
            emit_control_body(ctx, stmt->as.while_stmt.body, indent);
            pop_loop_mark(ctx);
            break;
        case STMT_FOR:
            emit_indent(out, indent); fputs("{\n", out);
            push_owned_scope(ctx, stmt->as.for_stmt.initializer);
            if (stmt->as.for_stmt.initializer)
                emit_stmt(ctx, stmt->as.for_stmt.initializer, indent + 1U);
            emit_indent(out, indent + 1U); fputs("for (; ", out);
            if (stmt->as.for_stmt.condition)
                emit_expr_to_file(out, stmt->as.for_stmt.condition);
            fputs("; ", out);
            if (stmt->as.for_stmt.increment)
                emit_expr_to_file(out, stmt->as.for_stmt.increment);
            fputs(")\n", out);
            push_loop_mark(ctx);
            emit_control_body(ctx, stmt->as.for_stmt.body, indent + 1U);
            pop_loop_mark(ctx);
            pop_owned_scope(ctx, indent + 1U, true);
            emit_indent(out, indent); fputs("}\n", out);
            break;
        case STMT_BREAK:
            emit_cleanup_to_loop_mark(ctx, indent);
            emit_indent(out, indent); fputs("break;\n", out);
            break;
        case STMT_CONTINUE:
            emit_cleanup_to_loop_mark(ctx, indent);
            emit_indent(out, indent); fputs("continue;\n", out);
            break;
        case STMT_RETURN:
            if (stmt->as.return_value && ctx->function->return_type != TYPE_VOID) {
                char temporary[96];
                (void)snprintf(temporary, sizeof(temporary), "yl_return_%zu_%zu",
                               stmt->token.line, stmt->token.column);
                emit_indent(out, indent);
                fprintf(out, "%s %s = ", c_base_type(c, ctx->function->return_type),
                        temporary);
                emit_expr_to_file(out, stmt->as.return_value);
                fputs(";\n", out);
                emit_cleanup_active_owned(ctx, indent);
                emit_indent(out, indent);
                fprintf(out, "return %s;\n", temporary);
            } else {
                emit_cleanup_function_owned(ctx, indent);
                emit_indent(out, indent);
                fputs("return;\n", out);
            }
            break;
        case STMT_MATCH: {
            char match_name[96];
            (void)snprintf(match_name, sizeof(match_name), "yl_match_%zu_%zu",
                           stmt->token.line, stmt->token.column);
            emit_indent(out, indent); fputs("{\n", out);
            emit_indent(out, indent + 1U);
            fprintf(out, "int64_t %s = ", match_name);
            emit_expr_to_file(out, stmt->as.match_stmt.value);
            fputs(";\n", out);
            for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++) {
                MatchArm *arm = stmt->as.match_stmt.arms[i];
                emit_indent(out, indent + 1U);
                fputs(i == 0 ? "if (" : "else if (", out);
                fprintf(out, "%s == INT64_C(%lld))\n", match_name,
                        (long long)(arm->variant ? arm->variant->value : 0));
                emit_control_body(ctx, arm->body, indent + 1U);
            }
            emit_indent(out, indent); fputs("}\n", out);
            break;
        }
        case STMT_EXPR:
            if (stmt->as.expression &&
                stmt->as.expression->type == TYPE_OWNED_STRING &&
                stmt->as.expression->kind != EXPR_ASSIGN) {
                char temporary[96];
                (void)snprintf(temporary, sizeof(temporary), "yl_discard_owned_%zu_%zu",
                               stmt->token.line, stmt->token.column);
                emit_indent(out, indent);
                fprintf(out, "const char *%s = ", temporary);
                emit_expr_to_file(out, stmt->as.expression);
                fputs(";\n", out);
                emit_indent(out, indent);
                fprintf(out, "yl_release(%s);\n", temporary);
            } else {
                emit_indent(out, indent);
                emit_expr_to_file(out, stmt->as.expression);
                fputs(";\n", out);
            }
            break;
    }
}

static void emit_control_body(CodegenContext *ctx, Stmt *body, unsigned indent)
{
    if (!body) {
        emit_indent(ctx->out, indent); fputs("{}\n", ctx->out);
        return;
    }
    if (body->kind == STMT_BLOCK) {
        emit_stmt(ctx, body, indent);
        return;
    }
    emit_indent(ctx->out, indent); fputs("{\n", ctx->out);
    push_owned_scope(ctx, body);
    emit_stmt(ctx, body, indent + 1U);
    pop_owned_scope(ctx, indent + 1U, true);
    emit_indent(ctx->out, indent); fputs("}\n", ctx->out);
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
    size_t next_codegen_id = 1U;
    for (size_t i = 0; i < c->program->global_count; i++)
        assign_expr_codegen_ids(c->program->globals[i]->initializer, &next_codegen_id);
    for (size_t i = 0; i < c->program->function_count; i++)
        assign_stmt_codegen_ids(c->program->functions[i]->body, &next_codegen_id);

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
        fputs(") {\n", out);
        emit_temp_declarations_stmt(c, out, fn->body, 1U);
        CodegenContext context = {
            .compiler = c,
            .function = fn,
            .out = out,
            .scopes = NULL,
            .scope_count = 0U,
            .scope_capacity = 0U,
            .loop_marks = NULL,
            .loop_count = 0U,
            .loop_capacity = 0U
        };
        /* Keep the source function body's block scope, including parameter shadowing rules. */
        emit_stmt(&context, fn->body, 1U);
        emit_cleanup_function_owned(&context, 1U);
        free(context.scopes);
        free(context.loop_marks);
        fputs("}\n\n", out);
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

