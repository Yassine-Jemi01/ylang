#define _POSIX_C_SOURCE 200809L
#include "internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Experimental NASM backend for Linux x86-64.
 *
 * This intentionally implements a documented subset first. It emits NASM
 * source using Linux syscalls, so the resulting program can be assembled with
 * NASM and linked with ld without GCC/Clang or a C runtime.
 */

typedef struct {
    VarDecl *variable;
    size_t slot;
} AsmVarSlot;

typedef struct {
    Expr *expression;
    unsigned char *bytes;
    size_t length;
    size_t id;
} AsmString;

typedef struct AsmLoop {
    unsigned break_label;
    unsigned continue_label;
    const char *break_prefix;
    const char *continue_prefix;
    struct AsmLoop *previous;
} AsmLoop;

typedef struct {
    Compiler *compiler;
    FILE *out;
    bool okay;
    unsigned next_label;
    AsmVarSlot *slots;
    size_t slot_count;
    size_t slot_capacity;
    size_t temporary_depth;
    Function *function;
    AsmLoop *loop;
    AsmString *strings;
    size_t string_count;
    size_t string_capacity;
} AsmContext;

static bool asm_reject(AsmContext *ctx, Token token, const char *feature)
{
    fprintf(stderr,
            "ylang: NASM backend does not support %s yet (%s:%zu:%zu).\n",
            feature, ctx->compiler->filename ? ctx->compiler->filename : "<source>",
            token.line, token.column);
    ctx->okay = false;
    return false;
}

static bool asm_type_supported(AsmContext *ctx, YType type, Token token,
                               bool allow_void)
{
    if (type >= TYPE_STRUCT_BASE) return asm_reject(ctx, token, "struct values");
    if (type >= TYPE_ENUM_BASE) return true;
    switch (type) {
        case TYPE_INT:
        case TYPE_BOOL:
        case TYPE_CHAR:
        case TYPE_STRING:
        case TYPE_BORROWED_STRING:
            return true;
        case TYPE_OWNED_STRING:
            return asm_reject(ctx, token, "owned strings (use the C backend)");
        case TYPE_HTTP_TASK:
            return asm_reject(ctx, token, "HTTP async tasks (use the C backend)");
        case TYPE_VOID:
            if (allow_void) return true;
            return asm_reject(ctx, token, "void values");
        case TYPE_FLOAT:
            return asm_reject(ctx, token, "float values");
        case TYPE_ERROR:
            return asm_reject(ctx, token, "expressions with unresolved types");
    }
    return asm_reject(ctx, token, "this data type");
}

static bool validate_expr(AsmContext *ctx, Expr *expr);

static bool validate_expr_list(AsmContext *ctx, Expr **items, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (!validate_expr(ctx, items[i])) return false;
    }
    return true;
}

static bool validate_expr(AsmContext *ctx, Expr *expr)
{
    if (!expr) return true;
    if (!asm_type_supported(ctx, expr->type, expr->token, true)) return false;

    switch (expr->kind) {
        case EXPR_ENUM_VALUE:
        case EXPR_INT:
        case EXPR_BOOL:
            return true;
        case EXPR_CHAR:
        case EXPR_STRING:
            return true;
        case EXPR_NAME:
            if (!expr->as.name.variable) {
                return asm_reject(ctx, expr->token, "unresolved variable references");
            }
            if (expr->as.name.variable->is_array) {
                return asm_reject(ctx, expr->token, "array values");
            }
            return true;
        case EXPR_UNARY:
            if (expr->as.unary.op.type != TOKEN_MINUS &&
                expr->as.unary.op.type != TOKEN_NOT &&
                expr->as.unary.op.type != TOKEN_BANG) {
                return asm_reject(ctx, expr->token, "this unary operator");
            }
            return validate_expr(ctx, expr->as.unary.right);
        case EXPR_BINARY: {
            TokenType op = expr->as.binary.op.type;
            if (expr->as.binary.left &&
                (expr->as.binary.left->type == TYPE_STRING ||
                 expr->as.binary.left->type == TYPE_BORROWED_STRING)) {
                return asm_reject(ctx, expr->token, "string comparisons");
            }
            if (op != TOKEN_PLUS && op != TOKEN_MINUS && op != TOKEN_STAR &&
                op != TOKEN_SLASH && op != TOKEN_PERCENT &&
                op != TOKEN_EQUAL_EQUAL && op != TOKEN_BANG_EQUAL &&
                op != TOKEN_LESS && op != TOKEN_LESS_EQUAL &&
                op != TOKEN_GREATER && op != TOKEN_GREATER_EQUAL &&
                op != TOKEN_AND && op != TOKEN_OR) {
                return asm_reject(ctx, expr->token, "this binary operator");
            }
            return validate_expr(ctx, expr->as.binary.left) &&
                   validate_expr(ctx, expr->as.binary.right);
        }
        case EXPR_ASSIGN:
            if (!expr->as.assign.target ||
                expr->as.assign.target->kind != EXPR_NAME ||
                !expr->as.assign.variable) {
                return asm_reject(ctx, expr->token,
                                  "assignments except to named scalar variables");
            }
            return validate_expr(ctx, expr->as.assign.right);
        case EXPR_CALL:
            if (strncmp(expr->as.call.name, "gui.", 4) == 0) {
                return asm_reject(ctx, expr->token, "GUI calls (use the C backend with SDL2)");
            }
            if (!expr->as.call.function) {
                return asm_reject(ctx, expr->token,
                                  "standard-library and conversion calls");
            }
            for (size_t i = 0; i < expr->as.call.function->param_count; i++) {
                if (expr->as.call.function->params[i]->is_array) {
                    return asm_reject(ctx, expr->token, "array parameters");
                }
            }
            return validate_expr_list(ctx, expr->as.call.args,
                                      expr->as.call.count);
        case EXPR_FLOAT:
            return asm_reject(ctx, expr->token, "float literals");
        case EXPR_FSTRING:
            return asm_reject(ctx, expr->token, "f-strings");
        case EXPR_ARRAY:
        case EXPR_INDEX:
            return asm_reject(ctx, expr->token, "arrays and array indexing");
        case EXPR_STRUCT_INIT:
        case EXPR_MEMBER:
            return asm_reject(ctx, expr->token, "struct values (not yet supported by the NASM backend)");
        case EXPR_ERROR:
            return asm_reject(ctx, expr->token, "invalid expressions");
    }
    return asm_reject(ctx, expr->token, "this expression");
}

static bool validate_stmt(AsmContext *ctx, Stmt *stmt)
{
    if (!stmt) return true;
    switch (stmt->kind) {
        case STMT_BLOCK:
            for (size_t i = 0; i < stmt->as.block.count; i++) {
                if (!validate_stmt(ctx, stmt->as.block.items[i])) return false;
            }
            return true;
        case STMT_VAR:
            if (stmt->as.variable->is_array) {
                return asm_reject(ctx, stmt->token, "array declarations");
            }
            if (!asm_type_supported(ctx, stmt->as.variable->type,
                                    stmt->token, false)) return false;
            return validate_expr(ctx, stmt->as.variable->initializer);
        case STMT_PRINT:
            return validate_expr_list(ctx, stmt->as.print.args,
                                      stmt->as.print.count);
        case STMT_IF:
            return validate_expr(ctx, stmt->as.if_stmt.condition) &&
                   validate_stmt(ctx, stmt->as.if_stmt.then_branch) &&
                   validate_stmt(ctx, stmt->as.if_stmt.else_branch);
        case STMT_MATCH:
            if (!validate_expr(ctx, stmt->as.match_stmt.value)) return false;
            for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++) {
                if (!validate_stmt(ctx, stmt->as.match_stmt.arms[i]->body)) return false;
            }
            return true;
        case STMT_LOOP:
            return validate_stmt(ctx, stmt->as.loop_body);
        case STMT_WHILE:
            return validate_expr(ctx, stmt->as.while_stmt.condition) &&
                   validate_stmt(ctx, stmt->as.while_stmt.body);
        case STMT_FOR:
            return validate_stmt(ctx, stmt->as.for_stmt.initializer) &&
                   validate_expr(ctx, stmt->as.for_stmt.condition) &&
                   validate_expr(ctx, stmt->as.for_stmt.increment) &&
                   validate_stmt(ctx, stmt->as.for_stmt.body);
        case STMT_BREAK:
        case STMT_CONTINUE:
            return true;
        case STMT_RETURN:
            return validate_expr(ctx, stmt->as.return_value);
        case STMT_EXPR:
            return validate_expr(ctx, stmt->as.expression);
    }
    return asm_reject(ctx, stmt->token, "this statement");
}

static bool add_slot(AsmContext *ctx, VarDecl *variable)
{
    for (size_t i = 0; i < ctx->slot_count; i++) {
        if (ctx->slots[i].variable == variable) return true;
    }
    if (ctx->slot_count == ctx->slot_capacity) {
        size_t next = ctx->slot_capacity ? ctx->slot_capacity * 2 : 16;
        if (next < ctx->slot_capacity ||
            next > SIZE_MAX / sizeof(*ctx->slots)) {
            fputs("ylang: NASM backend ran out of addressable local slots.\n", stderr);
            ctx->okay = false;
            return false;
        }
        AsmVarSlot *grown = realloc(ctx->slots, next * sizeof(*grown));
        if (!grown) {
            fputs("ylang: out of memory in NASM backend.\n", stderr);
            ctx->okay = false;
            return false;
        }
        ctx->slots = grown;
        ctx->slot_capacity = next;
    }
    ctx->slots[ctx->slot_count] = (AsmVarSlot){ variable, ctx->slot_count };
    ctx->slot_count++;
    return true;
}

static bool collect_stmt_slots(AsmContext *ctx, Stmt *stmt)
{
    if (!stmt) return true;
    switch (stmt->kind) {
        case STMT_BLOCK:
            for (size_t i = 0; i < stmt->as.block.count; i++) {
                if (!collect_stmt_slots(ctx, stmt->as.block.items[i])) return false;
            }
            break;
        case STMT_VAR:
            if (!add_slot(ctx, stmt->as.variable)) return false;
            break;
        case STMT_IF:
            return collect_stmt_slots(ctx, stmt->as.if_stmt.then_branch) &&
                   collect_stmt_slots(ctx, stmt->as.if_stmt.else_branch);
        case STMT_MATCH:
            for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++) {
                if (!collect_stmt_slots(ctx, stmt->as.match_stmt.arms[i]->body)) return false;
            }
            break;
        case STMT_LOOP:
            return collect_stmt_slots(ctx, stmt->as.loop_body);
        case STMT_WHILE:
            return collect_stmt_slots(ctx, stmt->as.while_stmt.body);
        case STMT_FOR:
            return collect_stmt_slots(ctx, stmt->as.for_stmt.initializer) &&
                   collect_stmt_slots(ctx, stmt->as.for_stmt.body);
        case STMT_PRINT:
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_RETURN:
        case STMT_EXPR:
            break;
    }
    return true;
}

static size_t slot_offset(AsmContext *ctx, VarDecl *variable)
{
    for (size_t i = 0; i < ctx->slot_count; i++) {
        if (ctx->slots[i].variable == variable) return (ctx->slots[i].slot + 1U) * 8U;
    }
    fprintf(stderr, "ylang: internal error: no NASM stack slot for variable '%s'.\n",
            variable && variable->name ? variable->name : "<unknown>");
    ctx->okay = false;
    return 8;
}

static unsigned new_label(AsmContext *ctx)
{
    return ctx->next_label++;
}

static size_t intern_string(AsmContext *ctx, Expr *expr)
{
    for (size_t i = 0; i < ctx->string_count; i++) {
        if (ctx->strings[i].expression == expr) return ctx->strings[i].id;
    }
    if (ctx->string_count == ctx->string_capacity) {
        size_t next = ctx->string_capacity ? ctx->string_capacity * 2 : 16;
        if (next < ctx->string_capacity ||
            next > SIZE_MAX / sizeof(*ctx->strings)) {
            fputs("ylang: NASM string table is too large.\n", stderr);
            ctx->okay = false;
            return 0;
        }
        AsmString *grown = realloc(ctx->strings, next * sizeof(*grown));
        if (!grown) {
            fputs("ylang: out of memory for NASM string table.\n", stderr);
            ctx->okay = false;
            return 0;
        }
        ctx->strings = grown;
        ctx->string_capacity = next;
    }
    size_t length = 0;
    unsigned char *bytes = NULL;
    if (!decode_quoted_literal(expr->token, &bytes, &length)) {
        fputs("ylang: cannot decode a string literal for NASM output.\n", stderr);
        ctx->okay = false;
        return 0;
    }
    size_t id = ctx->string_count;
    ctx->strings[ctx->string_count++] =
        (AsmString){ expr, bytes, length, id };
    return id;
}

static bool emit_expr(AsmContext *ctx, Expr *expr);

static bool parse_nonnegative_i32_literal(AsmContext *ctx, Expr *expr,
                                          int32_t *value)
{
    if (!expr || expr->kind != EXPR_INT || !value) return false;
    char *literal = token_copy(ctx->compiler, expr->token);
    char *end = NULL;
    int base = (expr->token.length > 2 && literal[0] == '0' &&
                (literal[1] == 'x' || literal[1] == 'X')) ? 16 : 10;
    unsigned long long parsed = strtoull(literal, &end, base);
    if (end == literal || !end || *end != '\0' ||
        parsed > (unsigned long long)INT32_MAX) return false;
    *value = (int32_t)parsed;
    return true;
}

static bool is_local_scalar_name(const Expr *expr)
{
    return expr && expr->kind == EXPR_NAME && expr->as.name.variable &&
           !expr->as.name.variable->is_global &&
           !expr->as.name.variable->is_array;
}

static const char *false_jump_for_comparison(TokenType op)
{
    switch (op) {
        case TOKEN_EQUAL_EQUAL: return "jne";
        case TOKEN_BANG_EQUAL: return "je";
        case TOKEN_LESS: return "jge";
        case TOKEN_LESS_EQUAL: return "jg";
        case TOKEN_GREATER: return "jle";
        case TOKEN_GREATER_EQUAL: return "jl";
        default: return NULL;
    }
}

static bool emit_condition_false_jump(AsmContext *ctx, Expr *condition,
                                      const char *label_prefix, unsigned label)
{
    if (condition && condition->kind == EXPR_BINARY &&
        condition->type == TYPE_BOOL) {
        Expr *left = condition->as.binary.left;
        Expr *right = condition->as.binary.right;
        const char *false_jump =
            false_jump_for_comparison(condition->as.binary.op.type);
        int32_t immediate = 0;
        bool right_name = is_local_scalar_name(right) && right->type == TYPE_INT;
        bool right_immediate = right && right->type == TYPE_INT &&
                               parse_nonnegative_i32_literal(ctx, right, &immediate);
        if (false_jump && is_local_scalar_name(left) &&
            left->type == TYPE_INT && (right_name || right_immediate)) {
            size_t left_offset = slot_offset(ctx, left->as.name.variable);
            fprintf(ctx->out, "    mov rax, [rbp - %zu]\n", left_offset);
            if (right_name) {
                size_t right_offset = slot_offset(ctx, right->as.name.variable);
                fprintf(ctx->out, "    cmp rax, [rbp - %zu]\n", right_offset);
            } else {
                fprintf(ctx->out, "    cmp rax, %d\n", (int)immediate);
            }
            fprintf(ctx->out, "    %s .L%s_%u\n", false_jump, label_prefix, label);
            return ctx->okay;
        }
    }
    if (!emit_expr(ctx, condition)) return false;
    fputs("    test rax, rax\n", ctx->out);
    fprintf(ctx->out, "    jz .L%s_%u\n", label_prefix, label);
    return true;
}

static bool emit_call(AsmContext *ctx, Expr *expr)
{
    static const char *const argument_registers[] = {
        "rdi", "rsi", "rdx", "rcx", "r8", "r9"
    };
    const size_t argument_count = expr->as.call.count;
    if (argument_count > SIZE_MAX / 8U ||
        ctx->temporary_depth > SIZE_MAX - argument_count * 8U) {
        return asm_reject(ctx, expr->token, "calls with an unaddressable argument list");
    }

    /* Evaluate every argument left-to-right before arranging the SysV ABI. */
    for (size_t i = 0; i < argument_count; i++) {
        if (!emit_expr(ctx, expr->as.call.args[i])) return false;
        fputs("    push rax\n", ctx->out);
        ctx->temporary_depth += 8U;
    }

    const size_t stack_argument_count =
        argument_count > 6U ? argument_count - 6U : 0U;
    const size_t stack_bytes = stack_argument_count * 8U;
    if (ctx->temporary_depth > SIZE_MAX - stack_bytes) {
        return asm_reject(ctx, expr->token, "calls with an unaddressable stack argument area");
    }

    /*
     * The first stack argument must be at [rsp] before CALL. Keep the
     * temporary argument values above that outgoing area, and place any
     * alignment padding after the stack arguments as required by SysV.
     */
    const size_t depth_with_stack_args = ctx->temporary_depth + stack_bytes;
    const size_t padding = depth_with_stack_args % 16U == 0U ? 0U : 8U;
    if (stack_bytes > SIZE_MAX - padding) {
        return asm_reject(ctx, expr->token, "calls with an unaddressable alignment area");
    }
    const size_t outgoing_size = stack_bytes + padding;
    if (outgoing_size > 0U) {
        fprintf(ctx->out, "    sub rsp, %zu\n", outgoing_size);
    }

    /* Copy arguments 7..N to the caller's stack area in source order. */
    for (size_t i = 6U; i < argument_count; i++) {
        const size_t source_offset =
            outgoing_size + (argument_count - 1U - i) * 8U;
        const size_t destination_offset = (i - 6U) * 8U;
        fprintf(ctx->out,
                "    mov rax, [rsp + %zu]\n"
                "    mov [rsp + %zu], rax\n",
                source_offset, destination_offset);
    }

    /* Load arguments 1..6 from their saved values without disturbing rsp. */
    const size_t register_argument_count =
        argument_count < 6U ? argument_count : 6U;
    for (size_t i = 0; i < register_argument_count; i++) {
        const size_t source_offset =
            outgoing_size + (argument_count - 1U - i) * 8U;
        fprintf(ctx->out, "    mov %s, [rsp + %zu]\n",
                argument_registers[i], source_offset);
    }

    fprintf(ctx->out, "    call %s\n", expr->as.call.function->c_name);

    const size_t argument_bytes = argument_count * 8U;
    if (outgoing_size > SIZE_MAX - argument_bytes) {
        return asm_reject(ctx, expr->token, "calls with an unaddressable cleanup area");
    }
    const size_t cleanup_bytes = outgoing_size + argument_bytes;
    if (cleanup_bytes > 0U) {
        fprintf(ctx->out, "    add rsp, %zu\n", cleanup_bytes);
    }
    ctx->temporary_depth -= argument_bytes;

    if (expr->as.call.function->return_type == TYPE_VOID) {
        fputs("    xor eax, eax\n", ctx->out);
    }
    return true;
}

static bool emit_expr(AsmContext *ctx, Expr *expr)
{
    if (!expr) {
        fputs("    xor eax, eax\n", ctx->out);
        return true;
    }
    switch (expr->kind) {
        case EXPR_ENUM_VALUE:
            fprintf(ctx->out, "    mov rax, 0x%016" PRIx64 "\n",
                    (uint64_t)expr->as.enum_value.variant->value);
            return true;
        case EXPR_INT: {
            char *literal = token_copy(ctx->compiler, expr->token);
            char *end = NULL;
            int base = (expr->token.length > 2 && literal[0] == '0' &&
                        (literal[1] == 'x' || literal[1] == 'X')) ? 16 : 10;
            unsigned long long value = strtoull(literal, &end, base);
            (void)end;
            fprintf(ctx->out, "    mov rax, 0x%016" PRIx64 "\n", (uint64_t)value);
            return true;
        }
        case EXPR_BOOL:
            fprintf(ctx->out, "    mov eax, %d\n",
                    expr->token.type == TOKEN_TRUE ? 1 : 0);
            return true;
        case EXPR_CHAR: {
            size_t length = 0;
            unsigned char *bytes = NULL;
            if (!decode_quoted_literal(expr->token, &bytes, &length) ||
                length != 1U) {
                free(bytes);
                fputs("ylang: internal error decoding a YLang character.\n", stderr);
                ctx->okay = false;
                return false;
            }
            fprintf(ctx->out, "    mov eax, %u\n", (unsigned)bytes[0]);
            free(bytes);
            return true;
        }
        case EXPR_STRING: {
            size_t id = intern_string(ctx, expr);
            fprintf(ctx->out, "    lea rax, [rel yl_str_%zu]\n", id);
            return ctx->okay;
        }
        case EXPR_NAME: {
            VarDecl *variable = expr->as.name.variable;
            if (variable->is_global) {
                fprintf(ctx->out, "    mov rax, [rel %s]\n", variable->c_name);
            } else {
                size_t offset = slot_offset(ctx, variable);
                fprintf(ctx->out, "    mov rax, [rbp - %zu]\n", offset);
            }
            return ctx->okay;
        }
        case EXPR_UNARY:
            if (expr->is_min_int) {
                fputs("    mov rax, 0x8000000000000000\n", ctx->out);
                return true;
            }
            if (!emit_expr(ctx, expr->as.unary.right)) return false;
            if (expr->as.unary.op.type == TOKEN_MINUS) {
                /* NEG sets OF for INT64_MIN, so a separate comparison is redundant. */
                fputs("    neg rax\n"
                      "    jo __yl_runtime_error\n", ctx->out);
            } else {
                fputs("    test rax, rax\n"
                      "    sete al\n"
                      "    movzx rax, al\n", ctx->out);
            }
            return true;
        case EXPR_BINARY: {
            Expr *left = expr->as.binary.left;
            Expr *right = expr->as.binary.right;
            TokenType op = expr->as.binary.op.type;

            /* Logical operators must short-circuit; the right side may have effects. */
            if (op == TOKEN_AND || op == TOKEN_OR) {
                unsigned short_label = new_label(ctx);
                unsigned end_label = new_label(ctx);
                if (!emit_expr(ctx, left)) return false;
                fputs("    test rax, rax\n", ctx->out);
                fprintf(ctx->out, "    %s .Llogic_short_%u\n",
                        op == TOKEN_AND ? "jz" : "jnz", short_label);
                if (!emit_expr(ctx, right)) return false;
                fputs("    test rax, rax\n"
                      "    setne al\n"
                      "    movzx rax, al\n", ctx->out);
                fprintf(ctx->out, "    jmp .Llogic_end_%u\n"
                                  ".Llogic_short_%u:\n"
                                  "    mov eax, %d\n"
                                  ".Llogic_end_%u:\n",
                        end_label, short_label, op == TOKEN_AND ? 0 : 1, end_label);
                return true;
            }

            /* Use memory operands for simple local integer arithmetic/comparisons. */
            if (is_local_scalar_name(left) && is_local_scalar_name(right) &&
                left->type != TYPE_STRING && right->type != TYPE_STRING &&
                left->type != TYPE_BORROWED_STRING &&
                right->type != TYPE_BORROWED_STRING) {
                bool int_arithmetic = left->type == TYPE_INT && right->type == TYPE_INT;
                bool arithmetic_op = op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR;
                bool comparison_op = op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL ||
                                     op == TOKEN_LESS || op == TOKEN_LESS_EQUAL ||
                                     op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL;
                if ((int_arithmetic && arithmetic_op) || comparison_op) {
                    size_t lo = slot_offset(ctx, left->as.name.variable);
                    size_t ro = slot_offset(ctx, right->as.name.variable);
                    fprintf(ctx->out, "    mov rax, [rbp - %zu]\n", lo);
                    switch (op) {
                        case TOKEN_PLUS:
                            fprintf(ctx->out, "    add rax, [rbp - %zu]\n"
                                              "    jo __yl_runtime_error\n", ro); break;
                        case TOKEN_MINUS:
                            fprintf(ctx->out, "    sub rax, [rbp - %zu]\n"
                                              "    jo __yl_runtime_error\n", ro); break;
                        case TOKEN_STAR:
                            fprintf(ctx->out, "    imul rax, [rbp - %zu]\n"
                                              "    jo __yl_runtime_error\n", ro); break;
                        case TOKEN_EQUAL_EQUAL:
                            fprintf(ctx->out, "    cmp rax, [rbp - %zu]\n", ro);
                            fputs("    sete al\n    movzx rax, al\n", ctx->out); break;
                        case TOKEN_BANG_EQUAL:
                            fprintf(ctx->out, "    cmp rax, [rbp - %zu]\n", ro);
                            fputs("    setne al\n    movzx rax, al\n", ctx->out); break;
                        case TOKEN_LESS:
                            fprintf(ctx->out, "    cmp rax, [rbp - %zu]\n", ro);
                            fputs("    setl al\n    movzx rax, al\n", ctx->out); break;
                        case TOKEN_LESS_EQUAL:
                            fprintf(ctx->out, "    cmp rax, [rbp - %zu]\n", ro);
                            fputs("    setle al\n    movzx rax, al\n", ctx->out); break;
                        case TOKEN_GREATER:
                            fprintf(ctx->out, "    cmp rax, [rbp - %zu]\n", ro);
                            fputs("    setg al\n    movzx rax, al\n", ctx->out); break;
                        case TOKEN_GREATER_EQUAL:
                            fprintf(ctx->out, "    cmp rax, [rbp - %zu]\n", ro);
                            fputs("    setge al\n    movzx rax, al\n", ctx->out); break;
                        default: break;
                    }
                    return ctx->okay;
                }
            }

            /* Use direct immediates instead of pushing constants on the stack. */
            int32_t immediate = 0;
            if (is_local_scalar_name(left) && left->type == TYPE_INT &&
                right && right->type == TYPE_INT &&
                parse_nonnegative_i32_literal(ctx, right, &immediate)) {
                bool arithmetic_op = op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR;
                bool comparison_op = op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL ||
                                     op == TOKEN_LESS || op == TOKEN_LESS_EQUAL ||
                                     op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL;
                if (arithmetic_op || comparison_op) {
                    size_t offset = slot_offset(ctx, left->as.name.variable);
                    fprintf(ctx->out, "    mov rax, [rbp - %zu]\n", offset);
                    switch (op) {
                        case TOKEN_PLUS:
                            fprintf(ctx->out, "    add rax, %d\n"
                                              "    jo __yl_runtime_error\n", (int)immediate); break;
                        case TOKEN_MINUS:
                            fprintf(ctx->out, "    sub rax, %d\n"
                                              "    jo __yl_runtime_error\n", (int)immediate); break;
                        case TOKEN_STAR:
                            fprintf(ctx->out, "    imul rax, %d\n"
                                              "    jo __yl_runtime_error\n", (int)immediate); break;
                        case TOKEN_EQUAL_EQUAL:
                        case TOKEN_BANG_EQUAL:
                        case TOKEN_LESS:
                        case TOKEN_LESS_EQUAL:
                        case TOKEN_GREATER:
                        case TOKEN_GREATER_EQUAL:
                            fprintf(ctx->out, "    cmp rax, %d\n", (int)immediate);
                            switch (op) {
                                case TOKEN_EQUAL_EQUAL: fputs("    sete al\n", ctx->out); break;
                                case TOKEN_BANG_EQUAL: fputs("    setne al\n", ctx->out); break;
                                case TOKEN_LESS: fputs("    setl al\n", ctx->out); break;
                                case TOKEN_LESS_EQUAL: fputs("    setle al\n", ctx->out); break;
                                case TOKEN_GREATER: fputs("    setg al\n", ctx->out); break;
                                case TOKEN_GREATER_EQUAL: fputs("    setge al\n", ctx->out); break;
                                default: break;
                            }
                            fputs("    movzx rax, al\n", ctx->out); break;
                        default: break;
                    }
                    return ctx->okay;
                }
            }

            if (!emit_expr(ctx, left)) return false;
            fputs("    push rax\n", ctx->out);
            ctx->temporary_depth += 8U;
            if (!emit_expr(ctx, right)) return false;
            fputs("    mov r10, rax\n"
                  "    pop rax\n", ctx->out);
            if (ctx->temporary_depth >= 8U) ctx->temporary_depth -= 8U;
            switch (op) {
                case TOKEN_PLUS:
                    fputs("    add rax, r10\n    jo __yl_runtime_error\n", ctx->out);
                    break;
                case TOKEN_MINUS:
                    fputs("    sub rax, r10\n    jo __yl_runtime_error\n", ctx->out);
                    break;
                case TOKEN_STAR:
                    fputs("    imul rax, r10\n    jo __yl_runtime_error\n", ctx->out);
                    break;
                case TOKEN_SLASH:
                case TOKEN_PERCENT: {
                    unsigned safe = new_label(ctx);
                    fputs("    test r10, r10\n"
                          "    jz __yl_runtime_error\n"
                          "    mov r11, 0x8000000000000000\n"
                          "    cmp rax, r11\n", ctx->out);
                    fprintf(ctx->out, "    jne .Ldiv_safe_%u\n", safe);
                    fputs("    cmp r10, -1\n    je __yl_runtime_error\n", ctx->out);
                    fprintf(ctx->out, ".Ldiv_safe_%u:\n", safe);
                    fputs("    cqo\n    idiv r10\n", ctx->out);
                    if (op == TOKEN_PERCENT) fputs("    mov rax, rdx\n", ctx->out);
                    break;
                }
                case TOKEN_EQUAL_EQUAL:
                    fputs("    cmp rax, r10\n    sete al\n    movzx rax, al\n", ctx->out);
                    break;
                case TOKEN_BANG_EQUAL:
                    fputs("    cmp rax, r10\n    setne al\n    movzx rax, al\n", ctx->out);
                    break;
                case TOKEN_LESS:
                    fputs("    cmp rax, r10\n    setl al\n    movzx rax, al\n", ctx->out);
                    break;
                case TOKEN_LESS_EQUAL:
                    fputs("    cmp rax, r10\n    setle al\n    movzx rax, al\n", ctx->out);
                    break;
                case TOKEN_GREATER:
                    fputs("    cmp rax, r10\n    setg al\n    movzx rax, al\n", ctx->out);
                    break;
                case TOKEN_GREATER_EQUAL:
                    fputs("    cmp rax, r10\n    setge al\n    movzx rax, al\n", ctx->out);
                    break;
                default:
                    fputs("ylang: internal error: unsupported NASM operator.\n", stderr);
                    ctx->okay = false;
                    return false;
            }
            return true;
        }
        case EXPR_ASSIGN: {
            VarDecl *variable = expr->as.assign.variable;
            Expr *target = expr->as.assign.target;
            Expr *value = expr->as.assign.right;
            int32_t increment = 0;
            if (!expr->as.assign.is_compound && variable &&
                variable->type == TYPE_INT && !variable->is_global &&
                !variable->is_array && is_local_scalar_name(target) &&
                value && value->kind == EXPR_BINARY &&
                value->as.binary.op.type == TOKEN_PLUS &&
                value->as.binary.left == target &&
                parse_nonnegative_i32_literal(ctx, value->as.binary.right, &increment) &&
                increment == 1) {
                size_t inc_offset = slot_offset(ctx, variable);
                fprintf(ctx->out, "    inc qword [rbp - %zu]\n"
                                  "    jo __yl_runtime_error\n"
                                  "    mov rax, [rbp - %zu]\n", inc_offset, inc_offset);
                return ctx->okay;
            }
            size_t offset = variable->is_global ? 0U : slot_offset(ctx, variable);
            if (expr->as.assign.is_compound) {
                if (!emit_expr(ctx, expr->as.assign.right)) return false;
                fputs("    mov r10, rax\n", ctx->out);
                if (variable->is_global) {
                    fprintf(ctx->out, "    mov rax, [rel %s]\n", variable->c_name);
                } else {
                    fprintf(ctx->out, "    mov rax, [rbp - %zu]\n", offset);
                }
                fputs("    add rax, r10\n    jo __yl_runtime_error\n", ctx->out);
            } else if (!emit_expr(ctx, expr->as.assign.right)) {
                return false;
            }
            if (variable->is_global) {
                fprintf(ctx->out, "    mov [rel %s], rax\n", variable->c_name);
            } else {
                fprintf(ctx->out, "    mov [rbp - %zu], rax\n", offset);
            }
            return true;
        }
        case EXPR_CALL:
            return emit_call(ctx, expr);
        case EXPR_FLOAT:
        case EXPR_FSTRING:
        case EXPR_ARRAY:
        case EXPR_INDEX:
        case EXPR_STRUCT_INIT:
        case EXPR_MEMBER:
        case EXPR_ERROR:
            fputs("ylang: internal error: unsupported expression reached NASM emitter.\n",
                  stderr);
            ctx->okay = false;
            return false;
    }
    return false;
}

static bool emit_print_value(AsmContext *ctx, Expr *expr)
{
    if (!emit_expr(ctx, expr)) return false;
    switch (expr->type) {
        case TYPE_INT:
            fputs("    mov rdi, rax\n    call yl_print_i64\n", ctx->out);
            break;
        case TYPE_BOOL: {
            unsigned false_label = new_label(ctx);
            unsigned end_label = new_label(ctx);
            fputs("    test rax, rax\n", ctx->out);
            fprintf(ctx->out, "    jz .Lbool_false_%u\n", false_label);
            fputs("    lea rdi, [rel yl_true]\n    call yl_print_cstr\n", ctx->out);
            fprintf(ctx->out, "    jmp .Lbool_end_%u\n", end_label);
            fprintf(ctx->out, ".Lbool_false_%u:\n", false_label);
            fputs("    lea rdi, [rel yl_false]\n    call yl_print_cstr\n", ctx->out);
            fprintf(ctx->out, ".Lbool_end_%u:\n", end_label);
            break;
        }
        case TYPE_CHAR:
            fputs("    mov rdi, rax\n    call yl_print_char\n", ctx->out);
            break;
        case TYPE_STRING:
        case TYPE_BORROWED_STRING:
            fputs("    mov rdi, rax\n    call yl_print_cstr\n", ctx->out);
            break;
        case TYPE_OWNED_STRING:
            return asm_reject(ctx, expr->token, "owned strings (use the C backend)");
        case TYPE_HTTP_TASK:
            return asm_reject(ctx, expr->token, "HTTP task handles (use the C backend)");
        case TYPE_FLOAT:
        case TYPE_VOID:
        case TYPE_ERROR:
            return asm_reject(ctx, expr->token, "printing this value type");
    }
    return true;
}

static bool emit_stmt(AsmContext *ctx, Stmt *stmt)
{
    if (!stmt) return true;
    switch (stmt->kind) {
        case STMT_BLOCK:
            for (size_t i = 0; i < stmt->as.block.count; i++) {
                if (!emit_stmt(ctx, stmt->as.block.items[i])) return false;
            }
            return true;
        case STMT_VAR: {
            VarDecl *var = stmt->as.variable;
            size_t offset = slot_offset(ctx, var);
            if (var->initializer) {
                if (!emit_expr(ctx, var->initializer)) return false;
            } else if (var->type == TYPE_STRING ||
                       var->type == TYPE_BORROWED_STRING) {
                fputs("    lea rax, [rel yl_empty]\n", ctx->out);
            } else {
                fputs("    xor eax, eax\n", ctx->out);
            }
            fprintf(ctx->out, "    mov [rbp - %zu], rax\n", offset);
            return true;
        }
        case STMT_PRINT:
            for (size_t i = 0; i < stmt->as.print.count; i++) {
                if (i != 0U) {
                    fputs("    lea rdi, [rel yl_space]\n"
                          "    call yl_print_cstr\n", ctx->out);
                }
                if (!emit_print_value(ctx, stmt->as.print.args[i])) return false;
            }
            fputs("    lea rdi, [rel yl_newline]\n"
                  "    call yl_print_cstr\n", ctx->out);
            return true;
        case STMT_IF: {
            unsigned else_label = new_label(ctx);
            unsigned end_label = new_label(ctx);
            if (!emit_condition_false_jump(ctx, stmt->as.if_stmt.condition,
                                           "if_else", else_label)) return false;
            if (!emit_stmt(ctx, stmt->as.if_stmt.then_branch)) return false;
            fprintf(ctx->out, "    jmp .Lif_end_%u\n", end_label);
            fprintf(ctx->out, ".Lif_else_%u:\n", else_label);
            if (!emit_stmt(ctx, stmt->as.if_stmt.else_branch)) return false;
            fprintf(ctx->out, ".Lif_end_%u:\n", end_label);
            return true;
        }
        case STMT_LOOP: {
            unsigned start_label = new_label(ctx);
            unsigned end_label = new_label(ctx);
            fprintf(ctx->out, ".Lloop_start_%u:\n", start_label);
            AsmLoop loop = { end_label, start_label, "loop_end", "loop_start", ctx->loop };
            ctx->loop = &loop;
            bool okay = emit_stmt(ctx, stmt->as.loop_body);
            ctx->loop = loop.previous;
            if (!okay) return false;
            fprintf(ctx->out, "    jmp .Lloop_start_%u\n.Lloop_end_%u:\n",
                    start_label, end_label);
            return true;
        }
        case STMT_WHILE: {
            unsigned start_label = new_label(ctx);
            unsigned end_label = new_label(ctx);
            fprintf(ctx->out, ".Lwhile_start_%u:\n", start_label);
            if (!emit_condition_false_jump(ctx, stmt->as.while_stmt.condition,
                                           "while_end", end_label)) return false;
            AsmLoop loop = { end_label, start_label, "while_end", "while_start", ctx->loop };
            ctx->loop = &loop;
            bool okay = emit_stmt(ctx, stmt->as.while_stmt.body);
            ctx->loop = loop.previous;
            if (!okay) return false;
            fprintf(ctx->out, "    jmp .Lwhile_start_%u\n.Lwhile_end_%u:\n",
                    start_label, end_label);
            return true;
        }
        case STMT_FOR: {
            unsigned start_label = new_label(ctx);
            unsigned increment_label = new_label(ctx);
            unsigned end_label = new_label(ctx);
            if (!emit_stmt(ctx, stmt->as.for_stmt.initializer)) return false;
            fprintf(ctx->out, ".Lfor_start_%u:\n", start_label);
            if (stmt->as.for_stmt.condition) {
                if (!emit_condition_false_jump(ctx, stmt->as.for_stmt.condition,
                                               "for_end", end_label)) return false;
            }
            AsmLoop loop = { end_label, increment_label, "for_end", "for_increment", ctx->loop };
            ctx->loop = &loop;
            bool okay = emit_stmt(ctx, stmt->as.for_stmt.body);
            ctx->loop = loop.previous;
            if (!okay) return false;
            fprintf(ctx->out, ".Lfor_increment_%u:\n", increment_label);
            if (!emit_expr(ctx, stmt->as.for_stmt.increment)) return false;
            fprintf(ctx->out, "    jmp .Lfor_start_%u\n.Lfor_end_%u:\n",
                    start_label, end_label);
            return true;
        }
        case STMT_BREAK:
            if (!ctx->loop) {
                return asm_reject(ctx, stmt->token, "break outside a loop");
            }
            fprintf(ctx->out, "    jmp .L%s_%u\n", ctx->loop->break_prefix, ctx->loop->break_label);
            return true;
        case STMT_CONTINUE:
            if (!ctx->loop) {
                return asm_reject(ctx, stmt->token, "continue outside a loop");
            }
            /* The labels are emitted by each loop form. */
            fprintf(ctx->out, "    jmp .L%s_%u\n", ctx->loop->continue_prefix, ctx->loop->continue_label);
            return true;
        case STMT_RETURN:
            if (stmt->as.return_value) {
                if (!emit_expr(ctx, stmt->as.return_value)) return false;
            } else {
                fputs("    xor eax, eax\n", ctx->out);
            }
            fprintf(ctx->out, "    jmp .Lreturn_%s\n", ctx->function->c_name);
            return true;
        case STMT_MATCH: {
            unsigned end_label = new_label(ctx);
            for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++) {
                MatchArm *arm = stmt->as.match_stmt.arms[i];
                unsigned next_label = new_label(ctx);
                if (!emit_expr(ctx, stmt->as.match_stmt.value)) return false;
                fputs("    mov r10, rax\n", ctx->out);
                fprintf(ctx->out, "    mov rax, 0x%016" PRIx64 "\n",
                        (uint64_t)(arm->variant ? arm->variant->value : 0));
                fputs("    cmp r10, rax\n", ctx->out);
                fprintf(ctx->out, "    jne .Lmatch_next_%u\n", next_label);
                if (!emit_stmt(ctx, arm->body)) return false;
                fprintf(ctx->out, "    jmp .Lmatch_end_%u\n", end_label);
                fprintf(ctx->out, ".Lmatch_next_%u:\n", next_label);
            }
            fprintf(ctx->out, ".Lmatch_end_%u:\n", end_label);
            return true;
        }
        case STMT_EXPR:
            return emit_expr(ctx, stmt->as.expression);
    }
    return false;
}

static bool prepare_function(AsmContext *ctx, Function *function)
{
    free(ctx->slots);
    ctx->slots = NULL;
    ctx->slot_count = 0;
    ctx->slot_capacity = 0;
    ctx->temporary_depth = 0;
    ctx->loop = NULL;
    ctx->function = function;

    for (size_t i = 0; i < function->param_count; i++) {
        if (function->params[i]->is_array) {
            return asm_reject(ctx, function->token, "array parameters");
        }
        if (!add_slot(ctx, function->params[i])) return false;
    }
    return collect_stmt_slots(ctx, function->body);
}

static bool eval_global_integer(AsmContext *ctx, Expr *expr, int64_t *value)
{
    if (!expr || !value) return false;
    switch (expr->kind) {
        case EXPR_INT: {
            char *literal = token_copy(ctx->compiler, expr->token);
            char *end = NULL;
            int base = expr->token.length > 2U && literal[0] == '0' &&
                       (literal[1] == 'x' || literal[1] == 'X') ? 16 : 10;
            unsigned long long parsed = strtoull(literal, &end, base);
            if (end == literal || !end || *end != '\0') return false;
            *value = (int64_t)parsed;
            return true;
        }
        case EXPR_BOOL:
            *value = expr->token.type == TOKEN_TRUE ? 1 : 0;
            return true;
        case EXPR_ENUM_VALUE:
            *value = expr->as.enum_value.variant->value;
            return true;
        case EXPR_CHAR: {
            unsigned char *bytes = NULL;
            size_t length = 0;
            if (!decode_quoted_literal(expr->token, &bytes, &length) || length != 1U) {
                free(bytes);
                return false;
            }
            *value = (int64_t)bytes[0];
            free(bytes);
            return true;
        }
        case EXPR_UNARY:
            if (expr->as.unary.op.type == TOKEN_MINUS) {
                if (expr->is_min_int) {
                    *value = INT64_MIN;
                    return true;
                }
                int64_t operand = 0;
                if (!eval_global_integer(ctx, expr->as.unary.right, &operand) ||
                    operand == INT64_MIN) return false;
                *value = -operand;
                return true;
            }
            if (expr->as.unary.op.type == TOKEN_NOT ||
                expr->as.unary.op.type == TOKEN_BANG) {
                int64_t operand = 0;
                if (!eval_global_integer(ctx, expr->as.unary.right, &operand)) return false;
                *value = operand == 0 ? 1 : 0;
                return true;
            }
            return false;
        default:
            return false;
    }
}

static bool emit_global_storage(AsmContext *ctx)
{
    if (ctx->compiler->program->global_count == 0U) return true;
    fputs("\nsection .data\n", ctx->out);
    for (size_t i = 0; i < ctx->compiler->program->global_count; i++) {
        VarDecl *var = ctx->compiler->program->globals[i];
        if (var->is_array || var->type == TYPE_FLOAT ||
            var->type == TYPE_OWNED_STRING || var->type == TYPE_HTTP_TASK) {
            return asm_reject(ctx, var->token, "this global storage type");
        }
        fprintf(ctx->out, "align 8\n%s:\n", var->c_name);
        if (!var->initializer) {
            if (var->type == TYPE_STRING || var->type == TYPE_BORROWED_STRING) {
                fputs("    dq yl_empty\n", ctx->out);
            } else {
                fputs("    dq 0\n", ctx->out);
            }
            continue;
        }
        if (var->type == TYPE_STRING || var->type == TYPE_BORROWED_STRING) {
            if (var->initializer->kind != EXPR_STRING) {
                return asm_reject(ctx, var->token,
                                  "global strings initialized by non-literal expressions");
            }
            size_t id = intern_string(ctx, var->initializer);
            fprintf(ctx->out, "    dq yl_str_%zu\n", id);
        } else {
            int64_t value = 0;
            if (!eval_global_integer(ctx, var->initializer, &value)) {
                return asm_reject(ctx, var->token,
                                  "global initializers other than scalar constants");
            }
            fprintf(ctx->out, "    dq 0x%016" PRIx64 "\n", (uint64_t)value);
        }
    }
    fputs("section .text\n", ctx->out);
    return ctx->okay;
}

static bool emit_function(AsmContext *ctx, Function *function)
{
    static const char *const argument_registers[] = {
        "rdi", "rsi", "rdx", "rcx", "r8", "r9"
    };
    if (!prepare_function(ctx, function)) return false;

    size_t raw_frame_size = ctx->slot_count * 8U;
    size_t frame_size = (raw_frame_size + 15U) & ~(size_t)15U;
    fprintf(ctx->out, "\n%s:\n    push rbp\n    mov rbp, rsp\n",
            function->c_name);
    if (frame_size != 0U) fprintf(ctx->out, "    sub rsp, %zu\n", frame_size);
    for (size_t i = 0; i < function->param_count; i++) {
        size_t offset = slot_offset(ctx, function->params[i]);
        if (i < sizeof(argument_registers) / sizeof(argument_registers[0])) {
            fprintf(ctx->out, "    mov [rbp - %zu], %s\n", offset,
                    argument_registers[i]);
        } else {
            size_t incoming_offset = 16U + (i - 6U) * 8U;
            fprintf(ctx->out,
                    "    mov rax, [rbp + %zu]\n"
                    "    mov [rbp - %zu], rax\n",
                    incoming_offset, offset);
        }
    }
    if (!emit_stmt(ctx, function->body)) return false;
    fputs("    xor eax, eax\n", ctx->out);
    fprintf(ctx->out, ".Lreturn_%s:\n    leave\n    ret\n", function->c_name);
    return ctx->okay;
}

static void emit_bytes(FILE *out, const char *label,
                       const unsigned char *bytes, size_t length,
                       bool terminate)
{
    fprintf(out, "%s:", label);
    if (length == 0U) {
        if (terminate) fputs(" db 0", out);
        fputc('\n', out);
        return;
    }

    fputs(" db ", out);
    for (size_t i = 0; i < length; i++) {
        if (i != 0U) {
            if (i % 16U == 0U) fputs("\n    db ", out);
            else fputs(", ", out);
        }
        fprintf(out, "%u", (unsigned)bytes[i]);
    }
    if (terminate) fputs(", 0", out);
    fputc('\n', out);
}

static void emit_runtime(AsmContext *ctx)
{
    static const unsigned char true_bytes[] = { 't', 'r', 'u', 'e' };
    static const unsigned char false_bytes[] = { 'f', 'a', 'l', 's', 'e' };
    static const unsigned char empty_bytes[] = { 0 };
    static const unsigned char space_bytes[] = { ' ' };
    static const unsigned char newline_bytes[] = { '\n' };
    static const unsigned char runtime_error_bytes[] = {
        'Y','L','a','n','g',' ','r','u','n','t','i','m','e',' ','e','r','r','o','r','\n'
    };

    fputs("\nsection .rodata\n", ctx->out);
    for (size_t i = 0; i < ctx->string_count; i++) {
        char label[64];
        (void)snprintf(label, sizeof(label), "yl_str_%zu", ctx->strings[i].id);
        emit_bytes(ctx->out, label, ctx->strings[i].bytes,
                   ctx->strings[i].length, true);
    }
    emit_bytes(ctx->out, "yl_true", true_bytes, sizeof(true_bytes), true);
    emit_bytes(ctx->out, "yl_false", false_bytes, sizeof(false_bytes), true);
    emit_bytes(ctx->out, "yl_empty", empty_bytes, 0, true);
    emit_bytes(ctx->out, "yl_space", space_bytes, sizeof(space_bytes), true);
    emit_bytes(ctx->out, "yl_newline", newline_bytes, sizeof(newline_bytes), true);
    emit_bytes(ctx->out, "yl_runtime_error_text", runtime_error_bytes,
               sizeof(runtime_error_bytes), false);

    fputs(
        "\nsection .text\n"
        "yl_print_cstr:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    mov rsi, rdi\n"
        "    xor edx, edx\n"
        ".Lprint_cstr_count:\n"
        "    cmp byte [rsi + rdx], 0\n"
        "    je .Lprint_cstr_write\n"
        "    inc rdx\n"
        "    jmp .Lprint_cstr_count\n"
        ".Lprint_cstr_write:\n"
        "    test rdx, rdx\n"
        "    jz .Lprint_cstr_done\n"
        "    mov eax, 1\n"
        "    mov edi, 1\n"
        "    syscall\n"
        ".Lprint_cstr_done:\n"
        "    leave\n"
        "    ret\n"
        "\n"
        "yl_print_char:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    sub rsp, 16\n"
        "    mov byte [rbp - 1], dil\n"
        "    mov eax, 1\n"
        "    mov edi, 1\n"
        "    lea rsi, [rbp - 1]\n"
        "    mov edx, 1\n"
        "    syscall\n"
        "    leave\n"
        "    ret\n"
        "\n"
        "yl_print_i64:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
        "    sub rsp, 32\n"
        "    mov rax, rdi\n"
        "    xor r8d, r8d\n"
        "    test rax, rax\n"
        "    jns .Lprint_i64_magnitude\n"
        "    mov r8d, 1\n"
        "    neg rax\n"
        ".Lprint_i64_magnitude:\n"
        "    lea rsi, [rbp - 1]\n"
        "    mov r10, 10\n"
        ".Lprint_i64_digits:\n"
        "    xor edx, edx\n"
        "    div r10\n"
        "    add dl, '0'\n"
        "    mov byte [rsi], dl\n"
        "    dec rsi\n"
        "    test rax, rax\n"
        "    jnz .Lprint_i64_digits\n"
        "    test r8d, r8d\n"
        "    jz .Lprint_i64_write\n"
        "    mov byte [rsi], '-'\n"
        "    dec rsi\n"
        ".Lprint_i64_write:\n"
        "    lea rsi, [rsi + 1]\n"
        "    lea rdx, [rbp - 1]\n"
        "    sub rdx, rsi\n"
        "    inc rdx\n"
        "    mov eax, 1\n"
        "    mov edi, 1\n"
        "    syscall\n"
        "    leave\n"
        "    ret\n"
        "\n"
        "__yl_runtime_error:\n"
        "    mov eax, 1\n"
        "    mov edi, 2\n"
        "    lea rsi, [rel yl_runtime_error_text]\n"
        "    mov edx, 20\n"
        "    syscall\n"
        "    mov eax, 60\n"
        "    mov edi, 70\n"
        "    syscall\n"
        "\nsection .note.GNU-stack noalloc noexec nowrite progbits\n",
        ctx->out);
}

bool generate_asm(Compiler *compiler, const char *path)
{
    if (!compiler || !compiler->program || !path) return false;
    AsmContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.compiler = compiler;
    ctx.okay = true;

    Function *main_function = find_function(compiler, "main");
    if (!main_function) {
        Token token = {0};
        (void)asm_reject(&ctx, token, "a program without function main");
    }

    for (size_t i = 0; ctx.okay && i < compiler->program->global_count; i++) {
        VarDecl *var = compiler->program->globals[i];
        if (var->is_array ||
            !asm_type_supported(&ctx, var->type, var->token, false) ||
            !validate_expr(&ctx, var->initializer)) break;
    }

    for (size_t i = 0; ctx.okay && i < compiler->program->function_count; i++) {
        Function *function = compiler->program->functions[i];
        if (!asm_type_supported(&ctx, function->return_type,
                                function->token, true)) break;
        for (size_t j = 0; j < function->param_count; j++) {
            if (function->params[j]->is_array) {
                (void)asm_reject(&ctx, function->params[j]->token,
                                 "array parameters");
                break;
            }
            if (!asm_type_supported(&ctx, function->params[j]->type,
                                    function->params[j]->token, false)) break;
        }
        if (!ctx.okay || !validate_stmt(&ctx, function->body)) break;
    }

    if (!ctx.okay) {
        for (size_t i = 0; i < ctx.string_count; i++) free(ctx.strings[i].bytes);
        free(ctx.strings);
        free(ctx.slots);
        return false;
    }

    FILE *out = fopen(path, "wb");
    if (!out) {
        fprintf(stderr, "ylang: cannot write NASM source '%s'.\n", path);
        free(ctx.strings);
        free(ctx.slots);
        return false;
    }
    ctx.out = out;
    fputs(
        "; Generated by YLang experimental NASM backend.\n"
        "; Target: Linux x86-64; assemble with nasm -f elf64 and link with ld.\n"
        "default rel\n"
        "global _start\n"
        "section .text\n"
        "_start:\n"
        "    and rsp, -16\n"
        "    call ",
        out);
    fprintf(out, "%s\n", main_function->c_name);
    fputs(
        "    mov rdi, rax\n"
        "    mov eax, 60\n"
        "    syscall\n",
        out);

    if (ctx.okay && !emit_global_storage(&ctx)) ctx.okay = false;
    for (size_t i = 0; ctx.okay &&
         i < compiler->program->function_count; i++) {
        if (!emit_function(&ctx, compiler->program->functions[i])) break;
    }
    if (ctx.okay) emit_runtime(&ctx);
    bool okay = ctx.okay && !ferror(out);
    if (fclose(out) != 0) okay = false;
    if (!okay) fprintf(stderr, "ylang: failed while writing NASM source '%s'.\n", path);

    for (size_t i = 0; i < ctx.string_count; i++) free(ctx.strings[i].bytes);
    free(ctx.strings);
    free(ctx.slots);
    return okay;
}
