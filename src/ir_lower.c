#include "internal.h"
#include "ylang/ir.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    YIRInstruction *items;
    size_t count;
    size_t capacity;
    uint32_t next_id;
    char *error;
    size_t error_capacity;
} Lowerer;

static bool lower_fail(Lowerer *lowerer, const char *message)
{
    if (lowerer->error && lowerer->error_capacity)
        (void)snprintf(lowerer->error, lowerer->error_capacity, "%s", message);
    return false;
}

static bool emit(Lowerer *lowerer, YIRInstruction instruction,
                 uint32_t *result_id)
{
    if (lowerer->count == lowerer->capacity) {
        size_t next = lowerer->capacity ? lowerer->capacity * 2 : 8;
        if (next < lowerer->capacity ||
            next > SIZE_MAX / sizeof(*lowerer->items))
            return lower_fail(lowerer, "too many instructions while lowering function");
        YIRInstruction *items = realloc(lowerer->items,
                                        next * sizeof(*lowerer->items));
        if (!items)
            return lower_fail(lowerer, "out of memory while lowering function");
        lowerer->items = items;
        lowerer->capacity = next;
    }
    if (result_id) {
        if (lowerer->next_id == UINT32_MAX)
            return lower_fail(lowerer, "too many SSA values while lowering function");
        instruction.result_id = ++lowerer->next_id;
        *result_id = instruction.result_id;
    }
    lowerer->items[lowerer->count++] = instruction;
    return true;
}

static bool parse_integer_literal(const Expr *expression, int64_t *value)
{
    if (!expression || expression->kind != EXPR_INT ||
        !expression->token.start || expression->token.length == 0 ||
        expression->token.length == SIZE_MAX)
        return false;
    if (expression->is_min_int) {
        *value = INT64_MIN;
        return true;
    }
    size_t length = expression->token.length;
    char *copy = malloc(length + 1);
    if (!copy) return false;
    memcpy(copy, expression->token.start, length);
    copy[length] = '\0';
    int base = length > 2 && copy[0] == '0' &&
               (copy[1] == 'x' || copy[1] == 'X') ? 0 : 10;
    errno = 0;
    char *end = NULL;
    long long parsed = strtoll(copy, &end, base);
    bool valid = errno == 0 && end == copy + length;
    free(copy);
    if (!valid) return false;
    *value = (int64_t)parsed;
    return true;
}

static bool lower_integer_expr(Lowerer *lowerer, const Expr *expression,
                               uint32_t *value_id)
{
    if (!expression)
        return lower_fail(lowerer, "missing expression in IR lowering");

    if (expression->kind == EXPR_INT) {
        int64_t immediate = 0;
        if (expression->type != TYPE_INT ||
            !parse_integer_literal(expression, &immediate))
            return lower_fail(lowerer, "invalid or unsupported integer literal");
        YIRInstruction instruction = {
            .opcode = YIR_OP_CONST_I64,
            .result_type = YIR_TYPE_I64,
            .immediate = immediate
        };
        return emit(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_UNARY &&
        expression->as.unary.op.type == TOKEN_MINUS) {
        int64_t immediate = 0;
        if (expression->type != TYPE_INT ||
            !parse_integer_literal(expression->as.unary.right, &immediate))
            return lower_fail(lowerer,
                "only in-range negative integer literals are supported by the initial IR lowerer");
        if (immediate == INT64_MIN)
            return lower_fail(lowerer,
                "negative integer literal is outside the initial IR literal range");
        YIRInstruction instruction = {
            .opcode = YIR_OP_CONST_I64,
            .result_type = YIR_TYPE_I64,
            .immediate = -immediate
        };
        return emit(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_BINARY) {
        uint32_t lhs = 0, rhs = 0;
        if (expression->type != TYPE_INT)
            return lower_fail(lowerer,
                "only integer arithmetic expressions are supported by the initial IR lowerer");
        if (!lower_integer_expr(lowerer, expression->as.binary.left, &lhs) ||
            !lower_integer_expr(lowerer, expression->as.binary.right, &rhs))
            return false;
        YIROpcode opcode;
        switch (expression->as.binary.op.type) {
            case TOKEN_PLUS: opcode = YIR_OP_ADD_I64; break;
            case TOKEN_MINUS: opcode = YIR_OP_SUB_I64; break;
            case TOKEN_STAR: opcode = YIR_OP_MUL_I64; break;
            default:
                return lower_fail(lowerer,
                    "operator is not supported by the initial scalar IR lowerer");
        }
        YIRInstruction instruction = {
            .opcode = opcode,
            .result_type = YIR_TYPE_I64,
            .lhs = lhs,
            .rhs = rhs
        };
        return emit(lowerer, instruction, value_id);
    }

    return lower_fail(lowerer,
        "expression is outside the initial scalar IR lowering subset");
}

static bool lower_boolean_expr(Lowerer *lowerer, const Expr *expression,
                               uint32_t *value_id)
{
    if (!expression || expression->type != TYPE_BOOL)
        return lower_fail(lowerer, "if condition must lower to bool");

    if (expression->kind == EXPR_BOOL) {
        YIRInstruction instruction = {
            .opcode = YIR_OP_CONST_BOOL,
            .result_type = YIR_TYPE_BOOL,
            .immediate = expression->token.type == TOKEN_TRUE ? 1 : 0
        };
        if (expression->token.type != TOKEN_TRUE &&
            expression->token.type != TOKEN_FALSE)
            return lower_fail(lowerer, "invalid boolean literal in IR lowering");
        return emit(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_BINARY) {
        uint32_t lhs = 0, rhs = 0;
        if (!lower_integer_expr(lowerer, expression->as.binary.left, &lhs) ||
            !lower_integer_expr(lowerer, expression->as.binary.right, &rhs))
            return false;
        YIROpcode opcode;
        switch (expression->as.binary.op.type) {
            case TOKEN_EQUAL_EQUAL: opcode = YIR_OP_EQ_I64; break;
            case TOKEN_BANG_EQUAL: opcode = YIR_OP_NE_I64; break;
            case TOKEN_LESS: opcode = YIR_OP_LT_I64; break;
            case TOKEN_LESS_EQUAL: opcode = YIR_OP_LE_I64; break;
            case TOKEN_GREATER: opcode = YIR_OP_GT_I64; break;
            case TOKEN_GREATER_EQUAL: opcode = YIR_OP_GE_I64; break;
            default:
                return lower_fail(lowerer,
                    "comparison operator is not supported by the initial IR lowerer");
        }
        YIRInstruction instruction = {
            .opcode = opcode,
            .result_type = YIR_TYPE_BOOL,
            .lhs = lhs,
            .rhs = rhs
        };
        return emit(lowerer, instruction, value_id);
    }

    return lower_fail(lowerer,
        "boolean expression is outside the initial scalar IR lowering subset");
}

static const Stmt *unwrap_single_statement(const Stmt *statement)
{
    while (statement && statement->kind == STMT_BLOCK &&
           statement->as.block.count == 1 && statement->as.block.items)
        statement = statement->as.block.items[0];
    return statement;
}

static const Stmt *find_single_return(const Stmt *statement, bool *empty_body)
{
    statement = unwrap_single_statement(statement);
    if (!statement) return NULL;
    if (statement->kind == STMT_RETURN) return statement;
    if (statement->kind == STMT_BLOCK && statement->as.block.count == 0) {
        *empty_body = true;
        return NULL;
    }
    return NULL;
}

static bool lower_return_block(const Function *function, const Stmt *ret,
                               bool empty_body, uint32_t block_id,
                               uint32_t *next_id, YIRBlock *out,
                               char *error, size_t error_capacity)
{
    Lowerer lowerer = {
        .next_id = *next_id,
        .error = error,
        .error_capacity = error_capacity
    };
    if (ret && ret->as.return_value) {
        if (function->return_type != TYPE_INT) {
            (void)lower_fail(&lowerer,
                "void function cannot return a value in the initial IR lowerer");
            goto failed;
        }
        uint32_t returned = 0;
        if (!lower_integer_expr(&lowerer, ret->as.return_value, &returned))
            goto failed;
        YIRInstruction terminator = {.opcode=YIR_OP_RETURN,.lhs=returned};
        if (!emit(&lowerer, terminator, NULL)) goto failed;
    } else {
        if (function->return_type != TYPE_VOID ||
            (!ret && !empty_body)) {
            (void)lower_fail(&lowerer,
                "int functions and non-empty void branches require a return statement");
            goto failed;
        }
        YIRInstruction terminator = {.opcode=YIR_OP_RETURN};
        if (!emit(&lowerer, terminator, NULL)) goto failed;
    }

    out->id = block_id;
    out->instructions = lowerer.items;
    out->instruction_count = lowerer.count;
    *next_id = lowerer.next_id;
    return true;

failed:
    free(lowerer.items);
    return false;
}

void yir_destroy_function(YIRFunction *function)
{
    if (!function) return;
    if (function->blocks) {
        for (size_t i = 0; i < function->block_count; ++i)
            free((void *)function->blocks[i].instructions);
        free((void *)function->blocks);
    }
    memset(function, 0, sizeof(*function));
}

bool yir_lower_function(const Function *function, YIRFunction *out,
                        char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!function || !out || !function->body)
        return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                          "function and output must be provided");
    memset(out, 0, sizeof(*out));
    if (function->param_count != 0)
        return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                          "function parameters are not supported by the initial IR lowerer");
    if (function->return_type != TYPE_INT &&
        function->return_type != TYPE_VOID)
        return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                          "only int and void functions are supported by the initial IR lowerer");

    const Stmt *root = unwrap_single_statement(function->body);
    bool empty_body = false;
    YIRBlock *blocks = NULL;
    size_t block_count = 0;
    uint32_t next_id = 0;

    if (root && root->kind == STMT_IF) {
        if (!root->as.if_stmt.else_branch)
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "initial IR lowering requires an else branch when lowering if");
        bool then_empty = false, else_empty = false;
        const Stmt *then_return =
            find_single_return(root->as.if_stmt.then_branch, &then_empty);
        const Stmt *else_return =
            find_single_return(root->as.if_stmt.else_branch, &else_empty);
        if ((!then_return && !then_empty) || (!else_return && !else_empty))
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "each if branch must contain exactly one return");

        blocks = calloc(3, sizeof(*blocks));
        if (!blocks)
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "out of memory while creating IR control-flow blocks");

        Lowerer entry = {.error=error,.error_capacity=error_capacity};
        uint32_t condition = 0;
        if (!lower_boolean_expr(&entry, root->as.if_stmt.condition, &condition)) {
            free(entry.items); free(blocks); return false;
        }
        YIRInstruction branch = {
            .opcode=YIR_OP_COND_BRANCH,
            .lhs=condition,
            .target=1,
            .false_target=2
        };
        if (!emit(&entry, branch, NULL)) {
            free(entry.items); free(blocks); return false;
        }
        blocks[0].id = 0;
        blocks[0].instructions = entry.items;
        blocks[0].instruction_count = entry.count;
        next_id = entry.next_id;

        if (!lower_return_block(function, then_return, then_empty, 1,
                                &next_id, &blocks[1], error, error_capacity)) {
            free((void *)blocks[0].instructions); free(blocks); return false;
        }
        if (!lower_return_block(function, else_return, else_empty, 2,
                                &next_id, &blocks[2], error, error_capacity)) {
            free((void *)blocks[0].instructions);
            free((void *)blocks[1].instructions);
            free(blocks);
            return false;
        }
        block_count = 3;
    } else {
        const Stmt *ret = find_single_return(root, &empty_body);
        if (!ret && !(empty_body && function->return_type == TYPE_VOID))
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "initial IR lowering requires one return statement");

        blocks = calloc(1, sizeof(*blocks));
        if (!blocks)
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "out of memory while creating IR entry block");
        if (!lower_return_block(function, ret, empty_body, 0, &next_id,
                                &blocks[0], error, error_capacity)) {
            free(blocks);
            return false;
        }
        block_count = 1;
    }

    out->name = function->name;
    out->return_type = function->return_type == TYPE_VOID ?
                       YIR_TYPE_VOID : YIR_TYPE_I64;
    out->blocks = blocks;
    out->block_count = block_count;

    char verify_error[192];
    if (!yir_verify_function(out, verify_error, sizeof(verify_error))) {
        yir_destroy_function(out);
        if (error && error_capacity)
            (void)snprintf(error, error_capacity, "lowered IR failed verification: %s",
                           verify_error);
        return false;
    }
    return true;
}
