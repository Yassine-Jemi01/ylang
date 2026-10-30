#include "internal.h"
#include "ylang/ir.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    VarDecl *variable;
    YLIRValue value;
} LowerBinding;

typedef struct {
    const Program *source_program;
    YLIRModule *module;
    FILE *diagnostics;
    bool okay;
} LowerProgram;

typedef struct {
    size_t break_target;
    size_t continue_target;
} LowerLoop;

typedef struct {
    LowerProgram *program;
    Function *source_function;
    YLIRFunction *function;
    YLIRBasicBlock *block;
    LowerBinding *bindings;
    size_t binding_count;
    size_t binding_capacity;
    size_t next_block_name;
    LowerLoop *loop;
} LowerFunction;

static void lower_error(LowerFunction *ctx, Token token, const char *message)
{
    FILE *out = ctx && ctx->program && ctx->program->diagnostics
        ? ctx->program->diagnostics : stderr;
    const Program *program = ctx && ctx->program ? ctx->program->source_program : NULL;
    const char *file = program && program->module && program->module->path
        ? program->module->path : "<source>";
    fprintf(out, "%s:%zu:%zu: LLVM IR lowering: %s\n",
            file, token.line, token.column, message);
    if (ctx && ctx->program) ctx->program->okay = false;
}

static YLIRType lower_type(YType type)
{
    switch (type) {
        case TYPE_INT: return YLIR_TYPE_I64;
        case TYPE_BOOL: return YLIR_TYPE_BOOL;
        case TYPE_FLOAT: return YLIR_TYPE_F64;
        default:
            if (type >= TYPE_ENUM_BASE && type < TYPE_STRUCT_BASE)
                return YLIR_TYPE_I64;
            return YLIR_TYPE_INVALID;
    }
}

static char *lower_token_text(Token token)
{
    char *text = malloc(token.length + 1U);
    if (!text) return NULL;
    if (token.length) memcpy(text, token.start, token.length);
    text[token.length] = '\0';
    return text;
}

static bool block_index(const YLIRFunction *function,
                        const YLIRBasicBlock *block, size_t *index)
{
    if (!function || !block || !index) return false;
    for (size_t i = 0; i < function->block_count; i++) {
        if (function->blocks[i] == block) {
            *index = i;
            return true;
        }
    }
    return false;
}

static bool add_block(LowerFunction *ctx, const char *purpose,
                      YLIRBasicBlock **out, size_t *index)
{
    char name[64];
    size_t serial = ctx->next_block_name++;
    int written = snprintf(name, sizeof(name), "b%zu_%s", serial, purpose);
    if (written < 0 || (size_t)written >= sizeof(name) ||
        !ylang_ir_add_block(ctx->function, name, out)) {
        lower_error(ctx, ctx->source_function->token, "could not allocate an IR basic block");
        return false;
    }
    return !index || block_index(ctx->function, *out, index);
}

static bool emit_op(LowerFunction *ctx, YLIRInstructionKind kind,
                    YLIRValue left, YLIRValue right, Token token,
                    YLIRValue *out)
{
    if (!ctx->block ||
        !ylang_ir_emit_op(ctx->block, kind, left, right,
                          token.line, token.column, out)) {
        lower_error(ctx, token, "could not emit a typed IR instruction");
        return false;
    }
    return true;
}

static bool emit_i64(LowerFunction *ctx, int64_t value, Token token,
                     YLIRValue *out)
{
    if (!ctx->block ||
        !ylang_ir_emit_const_i64(ctx->block, value, token.line, token.column, out)) {
        lower_error(ctx, token, "could not emit an integer constant");
        return false;
    }
    return true;
}

static bool emit_bool(LowerFunction *ctx, bool value, Token token,
                      YLIRValue *out)
{
    if (!ctx->block ||
        !ylang_ir_emit_const_bool(ctx->block, value, token.line, token.column, out)) {
        lower_error(ctx, token, "could not emit a boolean constant");
        return false;
    }
    return true;
}

static bool emit_f64(LowerFunction *ctx, double value, Token token,
                     YLIRValue *out)
{
    if (!ctx->block ||
        !ylang_ir_emit_const_f64(ctx->block, value, token.line, token.column, out)) {
        lower_error(ctx, token, "could not emit a floating-point constant");
        return false;
    }
    return true;
}

static size_t find_function_index(const Program *program, const Function *function)
{
    if (!program || !function) return SIZE_MAX;
    for (size_t i = 0; i < program->function_count; i++)
        if (program->functions[i] == function) return i;
    return SIZE_MAX;
}

static LowerBinding *find_binding(LowerFunction *ctx, VarDecl *variable)
{
    for (size_t i = ctx->binding_count; i > 0U; i--)
        if (ctx->bindings[i - 1U].variable == variable) return &ctx->bindings[i - 1U];
    return NULL;
}

static bool set_binding(LowerFunction *ctx, VarDecl *variable, YLIRValue value)
{
    LowerBinding *binding = find_binding(ctx, variable);
    if (binding) {
        binding->value = value;
        return true;
    }
    if (ctx->binding_count == ctx->binding_capacity) {
        size_t next = ctx->binding_capacity ? ctx->binding_capacity * 2U : 16U;
        if (next < ctx->binding_capacity ||
            next > SIZE_MAX / sizeof(*ctx->bindings)) {
            lower_error(ctx, variable->token, "variable binding table is too large");
            return false;
        }
        LowerBinding *grown = realloc(ctx->bindings, next * sizeof(*grown));
        if (!grown) {
            lower_error(ctx, variable->token, "out of memory tracking SSA variables");
            return false;
        }
        ctx->bindings = grown;
        ctx->binding_capacity = next;
    }
    ctx->bindings[ctx->binding_count++] = (LowerBinding){ variable, value };
    return true;
}

static bool branch_to(LowerFunction *ctx, YLIRBasicBlock *from,
                      YLIRBasicBlock *to, Token token)
{
    size_t target = 0U;
    if (!block_index(ctx->function, to, &target) ||
        !ylang_ir_branch(from, target, token.line, token.column)) {
        lower_error(ctx, token, "could not emit an IR branch");
        return false;
    }
    return true;
}

static bool lower_expression(LowerFunction *ctx, Expr *expr, YLIRValue *out);
static bool lower_statement(LowerFunction *ctx, Stmt *stmt);

static bool lower_short_circuit(LowerFunction *ctx, Expr *expr, YLIRValue *out)
{
    YLIRValue left = YLIR_NO_VALUE;
    if (!lower_expression(ctx, expr->as.binary.left, &left)) return false;
    YLIRBasicBlock *left_block = ctx->block;
    size_t left_index = 0U, rhs_index = 0U, join_index = 0U;
    YLIRBasicBlock *rhs_block = NULL, *join_block = NULL;
    if (!block_index(ctx->function, left_block, &left_index) ||
        !add_block(ctx, "logic_rhs", &rhs_block, &rhs_index) ||
        !add_block(ctx, "logic_join", &join_block, &join_index)) return false;

    const bool is_and = expr->as.binary.op.type == TOKEN_AND;
    YLIRValue short_value = YLIR_NO_VALUE;
    if (!emit_bool(ctx, !is_and, expr->token, &short_value)) return false;
    if (!ylang_ir_cond_branch(left_block, left,
            is_and ? rhs_index : join_index,
            is_and ? join_index : rhs_index,
            expr->token.line, expr->token.column)) {
        lower_error(ctx, expr->token, "could not emit short-circuit control flow");
        return false;
    }

    ctx->block = rhs_block;
    YLIRValue rhs = YLIR_NO_VALUE;
    if (!lower_expression(ctx, expr->as.binary.right, &rhs)) return false;
    YLIRBasicBlock *rhs_exit = ctx->block;
    size_t rhs_exit_index = 0U;
    if (!block_index(ctx->function, rhs_exit, &rhs_exit_index) ||
        !branch_to(ctx, rhs_exit, join_block, expr->token)) return false;

    ctx->block = join_block;
    const YLIRPhiIncoming incoming[] = {
        { .predecessor = left_index, .value = short_value },
        { .predecessor = rhs_exit_index, .value = rhs }
    };
    if (!ylang_ir_emit_phi(join_block, YLIR_TYPE_BOOL, incoming, 2U,
                           expr->token.line, expr->token.column, out)) {
        lower_error(ctx, expr->token, "could not emit a short-circuit PHI");
        return false;
    }
    return true;
}

static bool lower_binary(LowerFunction *ctx, Expr *expr, YLIRValue *out)
{
    const TokenType token_op = expr->as.binary.op.type;
    if (token_op == TOKEN_AND || token_op == TOKEN_OR)
        return lower_short_circuit(ctx, expr, out);

    const bool floating = expr->as.binary.left &&
        expr->as.binary.left->type == TYPE_FLOAT;
    if (floating && token_op >= TOKEN_EQUAL_EQUAL && token_op <= TOKEN_GREATER_EQUAL) {
        lower_error(ctx, expr->token, "floating-point comparisons are not yet represented in the typed IR");
        return false;
    }

    YLIRValue left = YLIR_NO_VALUE, right = YLIR_NO_VALUE;
    if (!lower_expression(ctx, expr->as.binary.left, &left) ||
        !lower_expression(ctx, expr->as.binary.right, &right)) return false;

    YLIRInstructionKind op;
    switch (token_op) {
        case TOKEN_PLUS: op = floating ? YLIR_OP_ADD_F64 : YLIR_OP_ADD_I64; break;
        case TOKEN_MINUS: op = floating ? YLIR_OP_SUB_F64 : YLIR_OP_SUB_I64; break;
        case TOKEN_STAR: op = floating ? YLIR_OP_MUL_F64 : YLIR_OP_MUL_I64; break;
        case TOKEN_SLASH: op = floating ? YLIR_OP_DIV_F64 : YLIR_OP_SDIV_I64; break;
        case TOKEN_PERCENT:
            if (floating) {
                lower_error(ctx, expr->token, "remainder on float is not supported in the current IR");
                return false;
            }
            op = YLIR_OP_SREM_I64; break;
        case TOKEN_EQUAL_EQUAL: op = YLIR_OP_EQ_I64; break;
        case TOKEN_BANG_EQUAL: op = YLIR_OP_NE_I64; break;
        case TOKEN_LESS: op = YLIR_OP_SLT_I64; break;
        case TOKEN_LESS_EQUAL: op = YLIR_OP_SLE_I64; break;
        case TOKEN_GREATER: op = YLIR_OP_SGT_I64; break;
        case TOKEN_GREATER_EQUAL: op = YLIR_OP_SGE_I64; break;
        default:
            lower_error(ctx, expr->token, "binary operator is not supported by the current IR");
            return false;
    }
    return emit_op(ctx, op, left, right, expr->token, out);
}

static bool lower_call(LowerFunction *ctx, Expr *expr, YLIRValue *out)
{
    size_t callee_index = find_function_index(ctx->program->source_program,
                                               expr->as.call.function);
    if (callee_index == SIZE_MAX) {
        lower_error(ctx, expr->token,
                    "call target is not a function in the current program module");
        return false;
    }
    size_t count = expr->as.call.count;
    YLIRValue *arguments = count ? malloc(count * sizeof(*arguments)) : NULL;
    if (count && !arguments) {
        lower_error(ctx, expr->token, "out of memory lowering function arguments");
        return false;
    }
    bool okay = true;
    for (size_t i = 0; i < count; i++) {
        if (!lower_expression(ctx, expr->as.call.args[i], &arguments[i])) {
            okay = false;
            break;
        }
    }
    if (okay) {
        YLIRValue result = YLIR_NO_VALUE;
        okay = ylang_ir_emit_call(ctx->block, callee_index, arguments, count,
                                  expr->token.line, expr->token.column, &result);
        if (!okay) lower_error(ctx, expr->token, "could not emit a typed IR call");
        else *out = result;
    }
    free(arguments);
    return okay;
}

static bool lower_assignment(LowerFunction *ctx, Expr *expr, YLIRValue *out)
{
    Expr *target = expr->as.assign.target;
    VarDecl *variable = expr->as.assign.variable;
    if (!target || target->kind != EXPR_NAME || !variable || variable->is_array) {
        lower_error(ctx, expr->token,
                    "the scalar IR subset only supports assignment to named scalar locals");
        return false;
    }
    if (variable->is_global) {
        lower_error(ctx, expr->token, "global variable access requires IR memory operations");
        return false;
    }
    LowerBinding *binding = find_binding(ctx, variable);
    YLIRValue previous = binding ? binding->value : YLIR_NO_VALUE;
    if (expr->as.assign.is_compound && previous == YLIR_NO_VALUE) {
        lower_error(ctx, expr->token, "compound assignment has no current SSA value");
        return false;
    }
    YLIRValue right = YLIR_NO_VALUE;
    if (!lower_expression(ctx, expr->as.assign.right, &right)) return false;
    if (expr->as.assign.is_compound) {
        YLIRValue result = YLIR_NO_VALUE;
        YLIRInstructionKind op = variable->type == TYPE_FLOAT
            ? YLIR_OP_ADD_F64 : YLIR_OP_ADD_I64;
        if (!emit_op(ctx, op, previous, right, expr->token, &result)) return false;
        right = result;
    }
    if (!set_binding(ctx, variable, right)) return false;
    *out = right;
    return true;
}

static bool lower_expression(LowerFunction *ctx, Expr *expr, YLIRValue *out)
{
    if (out) *out = YLIR_NO_VALUE;
    if (!expr || !ctx->block || !out) {
        lower_error(ctx, expr ? expr->token : ctx->source_function->token,
                    "expression has no active IR block");
        return false;
    }
    switch (expr->kind) {
        case EXPR_INT: {
            char *literal = lower_token_text(expr->token);
            if (!literal) {
                lower_error(ctx, expr->token, "out of memory reading integer token");
                return false;
            }
            char *end = NULL;
            int base = expr->token.length > 2U && literal[0] == '0' &&
                (literal[1] == 'x' || literal[1] == 'X') ? 16 : 10;
            errno = 0;
            long long parsed = strtoll(literal, &end, base);
            bool valid = errno != ERANGE && end != literal && end && *end == '\0';
            free(literal);
            if (!valid) {
                lower_error(ctx, expr->token, "integer literal is outside the IR i64 range");
                return false;
            }
            return emit_i64(ctx, (int64_t)parsed, expr->token, out);
        }
        case EXPR_FLOAT: {
            char *literal = lower_token_text(expr->token);
            if (!literal) {
                lower_error(ctx, expr->token, "out of memory reading float token");
                return false;
            }
            char *end = NULL;
            errno = 0;
            double parsed = strtod(literal, &end);
            bool valid = errno != ERANGE && end != literal && end && *end == '\0';
            free(literal);
            if (!valid) {
                lower_error(ctx, expr->token, "invalid or out-of-range float literal");
                return false;
            }
            return emit_f64(ctx, parsed, expr->token, out);
        }
        case EXPR_BOOL:
            return emit_bool(ctx, expr->token.type == TOKEN_TRUE, expr->token, out);
        case EXPR_ENUM_VALUE:
            return emit_i64(ctx, expr->as.enum_value.variant->value, expr->token, out);
        case EXPR_NAME: {
            VarDecl *variable = expr->as.name.variable;
            if (!variable || variable->is_global) {
                lower_error(ctx, expr->token, "global or unresolved name requires IR memory support");
                return false;
            }
            LowerBinding *binding = find_binding(ctx, variable);
            if (!binding) {
                lower_error(ctx, expr->token, "variable has no available SSA definition");
                return false;
            }
            *out = binding->value;
            return true;
        }
        case EXPR_UNARY: {
            if (expr->is_min_int)
                return emit_i64(ctx, INT64_MIN, expr->token, out);
            YLIRValue operand = YLIR_NO_VALUE;
            if (!lower_expression(ctx, expr->as.unary.right, &operand)) return false;
            YLIRInstructionKind op;
            if (expr->as.unary.op.type == TOKEN_NOT ||
                expr->as.unary.op.type == TOKEN_BANG)
                op = YLIR_OP_NOT_BOOL;
            else if (expr->as.unary.op.type == TOKEN_MINUS)
                op = expr->type == TYPE_FLOAT ? YLIR_OP_NEG_F64 : YLIR_OP_NEG_I64;
            else {
                lower_error(ctx, expr->token, "unary operator is not supported by the current IR");
                return false;
            }
            return emit_op(ctx, op, operand, YLIR_NO_VALUE, expr->token, out);
        }
        case EXPR_BINARY: return lower_binary(ctx, expr, out);
        case EXPR_ASSIGN: return lower_assignment(ctx, expr, out);
        case EXPR_CALL: return lower_call(ctx, expr, out);
        default:
            lower_error(ctx, expr->token,
                "strings, characters, aggregates, arrays, members, and formatted strings are outside the scalar IR subset");
            return false;
    }
}

static bool emit_phi_for_binding(LowerFunction *ctx, YLIRBasicBlock *header,
                                 size_t predecessor, VarDecl *variable,
                                 YLIRValue initial, YLIRValue *out)
{
    YLIRType type = lower_type(variable->type);
    if (type == YLIR_TYPE_INVALID) {
        lower_error(ctx, variable->token, "loop-carried variable type is unsupported by IR");
        return false;
    }
    const YLIRPhiIncoming incoming[] = {
        { .predecessor = predecessor, .value = initial }
    };
    if (!ylang_ir_emit_phi(header, type, incoming, 1U,
                           variable->token.line, variable->token.column, out)) {
        lower_error(ctx, variable->token, "could not emit a loop-carried PHI");
        return false;
    }
    return true;
}

static bool patch_loop_phi(YLIRBasicBlock *header, YLIRValue phi,
                           size_t predecessor, YLIRValue value)
{
    for (size_t i = 0; i < header->instruction_count; i++) {
        YLIRInstruction *instruction = &header->instructions[i];
        if (instruction->kind != YLIR_OP_PHI || instruction->result != phi) continue;
        if (instruction->incoming_count != 1U) return false;
        YLIRPhiIncoming *grown = realloc(instruction->incoming,
            2U * sizeof(*instruction->incoming));
        if (!grown) return false;
        instruction->incoming = grown;
        instruction->incoming[1] = (YLIRPhiIncoming){
            .predecessor = predecessor, .value = value
        };
        instruction->incoming_count = 2U;
        return true;
    }
    return false;
}

static bool lower_if(LowerFunction *ctx, Stmt *stmt)
{
    YLIRValue condition = YLIR_NO_VALUE;
    if (!lower_expression(ctx, stmt->as.if_stmt.condition, &condition)) return false;
    YLIRBasicBlock *condition_block = ctx->block;
    size_t yes_index = 0U, no_index = 0U, join_index = 0U;
    YLIRBasicBlock *yes_block = NULL, *no_block = NULL, *join_block = NULL;
    if (!add_block(ctx, "if_yes", &yes_block, &yes_index) ||
        !add_block(ctx, "if_no", &no_block, &no_index) ||
        !add_block(ctx, "if_join", &join_block, &join_index)) return false;
    if (!ylang_ir_cond_branch(condition_block, condition, yes_index, no_index,
                              stmt->token.line, stmt->token.column)) {
        lower_error(ctx, stmt->token, "could not emit if condition branch");
        return false;
    }

    size_t base_count = ctx->binding_count;
    LowerBinding *base = base_count ? malloc(base_count * sizeof(*base)) : NULL;
    LowerBinding *yes_values = base_count ? malloc(base_count * sizeof(*yes_values)) : NULL;
    LowerBinding *no_values = base_count ? malloc(base_count * sizeof(*no_values)) : NULL;
    if (base_count && (!base || !yes_values || !no_values)) {
        free(base); free(yes_values); free(no_values);
        lower_error(ctx, stmt->token, "out of memory merging if branches");
        return false;
    }
    if (base_count) memcpy(base, ctx->bindings, base_count * sizeof(*base));

    ctx->block = yes_block;
    if (!lower_statement(ctx, stmt->as.if_stmt.then_branch)) {
        free(base); free(yes_values); free(no_values); return false;
    }
    bool yes_live = ctx->block && ctx->block->terminator.kind == YLIR_TERM_NONE;
    size_t yes_pred = 0U;
    if (yes_live) {
        if (!block_index(ctx->function, ctx->block, &yes_pred) ||
            !branch_to(ctx, ctx->block, join_block, stmt->token)) {
            free(base); free(yes_values); free(no_values); return false;
        }
        if (base_count) memcpy(yes_values, ctx->bindings, base_count * sizeof(*base));
    }

    if (base_count) memcpy(ctx->bindings, base, base_count * sizeof(*base));
    ctx->binding_count = base_count;
    ctx->block = no_block;
    if (!lower_statement(ctx, stmt->as.if_stmt.else_branch)) {
        free(base); free(yes_values); free(no_values); return false;
    }
    bool no_live = ctx->block && ctx->block->terminator.kind == YLIR_TERM_NONE;
    size_t no_pred = 0U;
    if (no_live) {
        if (!block_index(ctx->function, ctx->block, &no_pred) ||
            !branch_to(ctx, ctx->block, join_block, stmt->token)) {
            free(base); free(yes_values); free(no_values); return false;
        }
        if (base_count) memcpy(no_values, ctx->bindings, base_count * sizeof(*base));
    }

    ctx->binding_count = base_count;
    if (base_count) memcpy(ctx->bindings, base, base_count * sizeof(*base));
    ctx->block = join_block;
    if (!yes_live && !no_live) {
        if (!ylang_ir_unreachable(join_block, stmt->token.line, stmt->token.column)) {
            free(base); free(yes_values); free(no_values);
            lower_error(ctx, stmt->token, "could not terminate unreachable if join");
            return false;
        }
        ctx->block = NULL;
    } else if (yes_live && !no_live) {
        if (base_count) memcpy(ctx->bindings, yes_values, base_count * sizeof(*base));
    } else if (!yes_live && no_live) {
        if (base_count) memcpy(ctx->bindings, no_values, base_count * sizeof(*base));
    } else {
        for (size_t i = 0; i < base_count; i++) {
            if (yes_values[i].value == no_values[i].value) continue;
            YLIRType type = lower_type(ctx->bindings[i].variable->type);
            if (type == YLIR_TYPE_INVALID) {
                lower_error(ctx, stmt->token, "if-merged variable type is unsupported by IR");
                free(base); free(yes_values); free(no_values);
                return false;
            }
            const YLIRPhiIncoming incoming[] = {
                { .predecessor = yes_pred, .value = yes_values[i].value },
                { .predecessor = no_pred, .value = no_values[i].value }
            };
            YLIRValue merged = YLIR_NO_VALUE;
            if (!ylang_ir_emit_phi(join_block, type, incoming, 2U,
                                   stmt->token.line, stmt->token.column, &merged)) {
                lower_error(ctx, stmt->token, "could not emit an if-merge PHI");
                free(base); free(yes_values); free(no_values);
                return false;
            }
            ctx->bindings[i].value = merged;
        }
    }
    free(base); free(yes_values); free(no_values);
    return true;
}

static bool lower_while(LowerFunction *ctx, Stmt *stmt)
{
    size_t base_count = ctx->binding_count;
    size_t preheader_index = 0U, header_index = 0U, body_index = 0U, exit_index = 0U;
    if (!block_index(ctx->function, ctx->block, &preheader_index)) return false;
    YLIRBasicBlock *header = NULL, *body = NULL, *exit = NULL;
    if (!add_block(ctx, "while_header", &header, &header_index) ||
        !add_block(ctx, "while_body", &body, &body_index) ||
        !add_block(ctx, "while_exit", &exit, &exit_index) ||
        !branch_to(ctx, ctx->block, header, stmt->token)) return false;

    LowerBinding *initial = base_count ? malloc(base_count * sizeof(*initial)) : NULL;
    YLIRValue *phis = base_count ? malloc(base_count * sizeof(*phis)) : NULL;
    if (base_count && (!initial || !phis)) {
        free(initial); free(phis);
        lower_error(ctx, stmt->token, "out of memory creating loop PHIs");
        return false;
    }
    if (base_count) memcpy(initial, ctx->bindings, base_count * sizeof(*initial));
    ctx->block = header;
    for (size_t i = 0; i < base_count; i++) {
        if (!emit_phi_for_binding(ctx, header, preheader_index,
                                  initial[i].variable, initial[i].value, &phis[i])) {
            free(initial); free(phis); return false;
        }
        ctx->bindings[i].value = phis[i];
    }
    YLIRValue condition = YLIR_NO_VALUE;
    if (!lower_expression(ctx, stmt->as.while_stmt.condition, &condition)) {
        free(initial); free(phis); return false;
    }
    if (!ylang_ir_cond_branch(ctx->block, condition, body_index, exit_index,
                              stmt->token.line, stmt->token.column)) {
        free(initial); free(phis);
        lower_error(ctx, stmt->token, "could not emit while condition branch");
        return false;
    }

    ctx->block = body;
    bool okay = lower_statement(ctx, stmt->as.while_stmt.body);
    if (!okay) { free(initial); free(phis); return false; }
    bool has_backedge = ctx->block && ctx->block->terminator.kind == YLIR_TERM_NONE;
    if (has_backedge) {
        size_t backedge_index = 0U;
        if (!block_index(ctx->function, ctx->block, &backedge_index) ||
            !branch_to(ctx, ctx->block, header, stmt->token)) {
            free(initial); free(phis); return false;
        }
        for (size_t i = 0; i < base_count; i++) {
            if (!patch_loop_phi(header, phis[i], backedge_index, ctx->bindings[i].value)) {
                free(initial); free(phis);
                lower_error(ctx, stmt->token, "could not patch a loop-carried PHI");
                return false;
            }
        }
    }
    ctx->binding_count = base_count;
    for (size_t i = 0; i < base_count; i++) ctx->bindings[i].value = phis[i];
    ctx->block = exit;
    free(initial); free(phis);
    return true;
}

static bool lower_for(LowerFunction *ctx, Stmt *stmt)
{
    size_t outer_count = ctx->binding_count;
    if (!lower_statement(ctx, stmt->as.for_stmt.initializer)) return false;
    size_t loop_count = ctx->binding_count;
    if (!stmt->as.for_stmt.condition) {
        lower_error(ctx, stmt->token,
                    "unbounded for loops require break-aware exit lowering, not yet supported by this IR slice");
        ctx->binding_count = outer_count;
        return false;
    }

    size_t preheader_index = 0U, header_index = 0U, body_index = 0U;
    size_t increment_index = 0U, exit_index = 0U;
    if (!block_index(ctx->function, ctx->block, &preheader_index)) return false;
    YLIRBasicBlock *header = NULL, *body = NULL, *increment = NULL, *exit = NULL;
    if (!add_block(ctx, "for_header", &header, &header_index) ||
        !add_block(ctx, "for_body", &body, &body_index) ||
        !add_block(ctx, "for_increment", &increment, &increment_index) ||
        !add_block(ctx, "for_exit", &exit, &exit_index) ||
        !branch_to(ctx, ctx->block, header, stmt->token)) return false;

    LowerBinding *initial = loop_count ? malloc(loop_count * sizeof(*initial)) : NULL;
    YLIRValue *phis = loop_count ? malloc(loop_count * sizeof(*phis)) : NULL;
    if (loop_count && (!initial || !phis)) {
        free(initial); free(phis);
        lower_error(ctx, stmt->token, "out of memory creating for-loop PHIs");
        return false;
    }
    if (loop_count) memcpy(initial, ctx->bindings, loop_count * sizeof(*initial));
    ctx->block = header;
    for (size_t i = 0; i < loop_count; i++) {
        if (!emit_phi_for_binding(ctx, header, preheader_index,
                                  initial[i].variable, initial[i].value, &phis[i])) {
            free(initial); free(phis); return false;
        }
        ctx->bindings[i].value = phis[i];
    }
    YLIRValue condition = YLIR_NO_VALUE;
    if (!lower_expression(ctx, stmt->as.for_stmt.condition, &condition) ||
        !ylang_ir_cond_branch(ctx->block, condition, body_index, exit_index,
                              stmt->token.line, stmt->token.column)) {
        free(initial); free(phis);
        lower_error(ctx, stmt->token, "could not emit for-loop condition");
        return false;
    }

    ctx->block = body;
    bool okay = lower_statement(ctx, stmt->as.for_stmt.body);
    if (!okay) { free(initial); free(phis); return false; }
    bool body_falls_through = ctx->block && ctx->block->terminator.kind == YLIR_TERM_NONE;
    if (body_falls_through) {
        if (!branch_to(ctx, ctx->block, increment, stmt->token)) {
            free(initial); free(phis); return false;
        }
    } else {
        (void)ylang_ir_unreachable(increment, stmt->token.line, stmt->token.column);
    }

    ctx->binding_count = loop_count;
    ctx->block = increment;
    if (body_falls_through && stmt->as.for_stmt.increment) {
        YLIRValue ignored = YLIR_NO_VALUE;
        if (!lower_expression(ctx, stmt->as.for_stmt.increment, &ignored)) {
            free(initial); free(phis); return false;
        }
    }
    bool has_backedge = body_falls_through &&
        ctx->block && ctx->block->terminator.kind == YLIR_TERM_NONE;
    if (has_backedge) {
        size_t backedge_index = 0U;
        if (!block_index(ctx->function, ctx->block, &backedge_index) ||
            !branch_to(ctx, ctx->block, header, stmt->token)) {
            free(initial); free(phis); return false;
        }
        for (size_t i = 0; i < loop_count; i++) {
            if (!patch_loop_phi(header, phis[i], backedge_index, ctx->bindings[i].value)) {
                free(initial); free(phis);
                lower_error(ctx, stmt->token, "could not patch a for-loop PHI");
                return false;
            }
        }
    }
    ctx->binding_count = outer_count;
    for (size_t i = 0; i < outer_count; i++) ctx->bindings[i].value = phis[i];
    ctx->block = exit;
    free(initial); free(phis);
    return true;
}

static bool lower_statement(LowerFunction *ctx, Stmt *stmt)
{
    if (!stmt || !ctx->block) return true;
    switch (stmt->kind) {
        case STMT_BLOCK: {
            size_t base_count = ctx->binding_count;
            for (size_t i = 0; i < stmt->as.block.count; i++) {
                if (!lower_statement(ctx, stmt->as.block.items[i])) return false;
                if (!ctx->block) break;
            }
            ctx->binding_count = base_count;
            return true;
        }
        case STMT_VAR: {
            VarDecl *variable = stmt->as.variable;
            if (variable->is_global || variable->is_array) {
                lower_error(ctx, stmt->token, "globals and arrays are not in the scalar SSA subset");
                return false;
            }
            if (lower_type(variable->type) == YLIR_TYPE_INVALID) {
                lower_error(ctx, stmt->token, "local variable type is not supported by scalar SSA");
                return false;
            }
            if (!variable->initializer) return true;
            YLIRValue value = YLIR_NO_VALUE;
            if (!lower_expression(ctx, variable->initializer, &value)) return false;
            return set_binding(ctx, variable, value);
        }
        case STMT_IF: return lower_if(ctx, stmt);
        case STMT_WHILE: return lower_while(ctx, stmt);
        case STMT_FOR: return lower_for(ctx, stmt);
        case STMT_RETURN:
            if (stmt->as.return_value) {
                YLIRValue value = YLIR_NO_VALUE;
                if (!lower_expression(ctx, stmt->as.return_value, &value) ||
                    !ylang_ir_return(ctx->block, value, stmt->token.line, stmt->token.column)) {
                    lower_error(ctx, stmt->token, "could not lower return statement");
                    return false;
                }
            } else if (!ylang_ir_return_void(ctx->block, stmt->token.line, stmt->token.column)) {
                lower_error(ctx, stmt->token, "could not lower void return");
                return false;
            }
            ctx->block = NULL;
            return true;
        case STMT_EXPR: {
            YLIRValue ignored = YLIR_NO_VALUE;
            return lower_expression(ctx, stmt->as.expression, &ignored);
        }
        case STMT_PRINT:
            for (size_t i = 0; i < stmt->as.print.count; i++) {
                Expr *argument = stmt->as.print.args[i];
                YLIRType type = lower_type(argument->type);
                if ((argument->type != TYPE_INT &&
                     argument->type != TYPE_FLOAT &&
                     argument->type != TYPE_BOOL) ||
                    (type != YLIR_TYPE_I64 && type != YLIR_TYPE_F64 &&
                     type != YLIR_TYPE_BOOL)) {
                    lower_error(ctx, argument->token,
                                "LLVM print currently supports int, float, and bool values only");
                    return false;
                }
                YLIRValue value = YLIR_NO_VALUE;
                if (!lower_expression(ctx, argument, &value) ||
                    !ylang_ir_emit_print(ctx->block, type, value,
                                         argument->token.line, argument->token.column)) {
                    lower_error(ctx, argument->token, "could not lower scalar print argument");
                    return false;
                }
                if (i + 1U < stmt->as.print.count &&
                    !ylang_ir_emit_print_control(ctx->block, false,
                                                 stmt->token.line, stmt->token.column)) {
                    lower_error(ctx, stmt->token, "could not lower print separator");
                    return false;
                }
            }
            if (!ylang_ir_emit_print_control(ctx->block, true,
                                             stmt->token.line, stmt->token.column)) {
                lower_error(ctx, stmt->token, "could not lower print newline");
                return false;
            }
            return true;
        case STMT_BREAK:
        case STMT_CONTINUE:
            lower_error(ctx, stmt->token, "break and continue are not in this scalar IR slice yet");
            return false;
        case STMT_LOOP:
            lower_error(ctx, stmt->token, "infinite loops require break-aware exit PHIs");
            return false;
        case STMT_MATCH:
            lower_error(ctx, stmt->token, "enum match lowering is not in this scalar IR slice yet");
            return false;
        case STMT_DEFER:
            lower_error(ctx, stmt->token, "defer lowering requires cleanup blocks");
            return false;
    }
    lower_error(ctx, stmt->token, "unknown statement kind");
    return false;
}

static bool lower_function(LowerProgram *program, Function *source,
                           YLIRFunction *function)
{
    LowerFunction ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.program = program;
    ctx.source_function = source;
    ctx.function = function;

    YLIRBasicBlock *entry = NULL;
    if (!add_block(&ctx, "entry", &entry, NULL)) return false;
    ctx.block = entry;
    for (size_t i = 0; i < source->param_count; i++) {
        VarDecl *param = source->params[i];
        if (param->is_array || lower_type(param->type) == YLIR_TYPE_INVALID) {
            lower_error(&ctx, param->token,
                        "array or non-scalar parameters are not in the current IR slice");
            free(ctx.bindings);
            return false;
        }
        if (!set_binding(&ctx, param, (YLIRValue)i)) {
            free(ctx.bindings);
            return false;
        }
    }
    if (!lower_statement(&ctx, source->body)) {
        free(ctx.bindings);
        return false;
    }
    if (ctx.block && ctx.block->terminator.kind == YLIR_TERM_NONE) {
        if (source->return_type == TYPE_VOID) {
            if (!ylang_ir_return_void(ctx.block, source->token.line, source->token.column)) {
                lower_error(&ctx, source->token, "could not add implicit void return");
                free(ctx.bindings);
                return false;
            }
        } else {
            lower_error(&ctx, source->token,
                        "non-void function can reach its end without returning");
            free(ctx.bindings);
            return false;
        }
    }
    free(ctx.bindings);
    return program->okay;
}

bool ylang_ir_lower_program(const Program *program, YLIRModule *module,
                            FILE *diagnostics)
{
    if (!program || !module || !program->functions ||
        program->function_count == 0U || program->global_count != 0U) {
        if (diagnostics)
            fputs("YLang IR lowering: requires functions and rejects global declarations in this milestone.\n",
                  diagnostics);
        return false;
    }
    if (!ylang_ir_module_init(module)) return false;
    LowerProgram lower = {
        .source_program = program,
        .module = module,
        .diagnostics = diagnostics ? diagnostics : stderr,
        .okay = true
    };

    for (size_t i = 0; i < program->function_count; i++) {
        Function *source = program->functions[i];
        YLIRType result = source->return_type == TYPE_VOID
            ? YLIR_TYPE_VOID : lower_type(source->return_type);
        if (result == YLIR_TYPE_INVALID) {
            fprintf(lower.diagnostics, "YLang IR lowering: return type for '%s' is unsupported\n",
                    source->name);
            lower.okay = false;
            break;
        }
        YLIRType *params = source->param_count
            ? malloc(source->param_count * sizeof(*params)) : NULL;
        if (source->param_count && !params) {
            lower.okay = false;
            break;
        }
        for (size_t j = 0; j < source->param_count; j++)
            params[j] = lower_type(source->params[j]->type);
        YLIRFunction *ignored = NULL;
        bool added = ylang_ir_add_function(module, source->c_name, result,
                                            params, source->param_count, &ignored);
        free(params);
        if (!added) {
            fprintf(lower.diagnostics, "YLang IR lowering: could not declare '%s'\n",
                    source->name);
            lower.okay = false;
            break;
        }
    }
    if (lower.okay) {
        for (size_t i = 0; i < program->function_count; i++) {
            if (!lower_function(&lower, program->functions[i], module->functions[i])) {
                lower.okay = false;
                break;
            }
        }
    }
    if (lower.okay && !ylang_ir_verify(module, lower.diagnostics))
        lower.okay = false;
    if (!lower.okay) ylang_ir_module_destroy(module);
    return lower.okay;
}
