#include "internal.h"
#include "ylang/ir.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const VarDecl *variable;
    uint32_t value_id;
} LoweredLocal;

typedef struct {
    const Function *function;
    LoweredLocal *locals;
    size_t local_count;
    size_t local_capacity;
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

static bool lookup_local(const Lowerer *lowerer, const VarDecl *variable,
                         uint32_t *value_id)
{
    for (size_t i = 0; i < lowerer->local_count; ++i) {
        if (lowerer->locals[i].variable == variable) {
            *value_id = lowerer->locals[i].value_id;
            return true;
        }
    }
    return false;
}

static bool is_function_parameter(const Lowerer *lowerer,
                                     const VarDecl *variable)
{
    if (!lowerer->function || !variable) return false;
    for (size_t i = 0; i < lowerer->function->param_count; ++i)
        if (lowerer->function->params[i] == variable) return true;
    return false;
}

static bool bind_local(Lowerer *lowerer, const VarDecl *variable,
                       uint32_t value_id)
{
    for (size_t i = 0; i < lowerer->local_count; ++i) {
        if (lowerer->locals[i].variable == variable) {
            lowerer->locals[i].value_id = value_id;
            return true;
        }
    }
    if (lowerer->local_count == lowerer->local_capacity) {
        size_t next = lowerer->local_capacity ? lowerer->local_capacity * 2 : 8;
        if (next < lowerer->local_capacity ||
            next > SIZE_MAX / sizeof(*lowerer->locals))
            return lower_fail(lowerer, "too many local bindings while lowering function");
        LoweredLocal *locals = realloc(lowerer->locals,
                                       next * sizeof(*lowerer->locals));
        if (!locals)
            return lower_fail(lowerer, "out of memory while lowering local bindings");
        lowerer->locals = locals;
        lowerer->local_capacity = next;
    }
    lowerer->locals[lowerer->local_count++] =
        (LoweredLocal){variable, value_id};
    return true;
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

static bool map_scalar_type(YType type, YIRType *ir_type)
{
    if (type == TYPE_INT) {
        *ir_type = YIR_TYPE_I64;
        return true;
    }
    if (type == TYPE_BOOL) {
        *ir_type = YIR_TYPE_BOOL;
        return true;
    }
    return false;
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

    if (expression->kind == EXPR_NAME) {
        if (expression->type != TYPE_INT || !expression->as.name.variable ||
            !lowerer->function)
            return lower_fail(lowerer, "integer names must refer to scalar parameters or initialized locals");
        if (lookup_local(lowerer, expression->as.name.variable, value_id))
            return true;
        for (size_t i = 0; i < lowerer->function->param_count; ++i) {
            if (lowerer->function->params[i] == expression->as.name.variable) {
                if (i >= UINT32_MAX)
                    return lower_fail(lowerer, "function has too many parameters");
                *value_id = (uint32_t)i + 1U;
                return true;
            }
        }
        return lower_fail(lowerer,
            "name is not a supported scalar function parameter or local variable");
    }

    if (expression->kind == EXPR_INT) {
        int64_t immediate = 0;
        if (expression->type != TYPE_INT ||
            !parse_integer_literal(expression, &immediate))
            return lower_fail(lowerer, "invalid or unsupported integer literal");
        YIRInstruction instruction = {
            .opcode = YIR_OP_CONST_I64,
            .result_type = YIR_TYPE_I64,
            .immediate = immediate,
            .source_line = expression->token.line,
            .source_column = expression->token.column
        };
        return emit(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_UNARY &&
        expression->as.unary.op.type == TOKEN_MINUS) {
        if (expression->type == TYPE_INT && expression->is_min_int) {
            YIRInstruction instruction = {
                .opcode = YIR_OP_CONST_I64,
                .result_type = YIR_TYPE_I64,
                .immediate = INT64_MIN,
                .source_line = expression->token.line,
                .source_column = expression->token.column
            };
            return emit(lowerer, instruction, value_id);
        }
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
            .immediate = -immediate,
            .source_line = expression->token.line,
            .source_column = expression->token.column
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
            .rhs = rhs,
            .source_line = expression->as.binary.op.line,
            .source_column = expression->as.binary.op.column
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
            .immediate = expression->token.type == TOKEN_TRUE ? 1 : 0,
            .source_line = expression->token.line,
            .source_column = expression->token.column
        };
        if (expression->token.type != TOKEN_TRUE &&
            expression->token.type != TOKEN_FALSE)
            return lower_fail(lowerer, "invalid boolean literal in IR lowering");
        return emit(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_NAME) {
        if (!expression->as.name.variable || !lowerer->function)
            return lower_fail(lowerer, "boolean name is not a supported function parameter");
        if (lookup_local(lowerer, expression->as.name.variable, value_id))
            return true;
        for (size_t i = 0; i < lowerer->function->param_count; ++i) {
            if (lowerer->function->params[i] == expression->as.name.variable &&
                lowerer->function->params[i]->type == TYPE_BOOL &&
                !lowerer->function->params[i]->is_array) {
                *value_id = (uint32_t)i + 1U;
                return true;
            }
        }
        return lower_fail(lowerer,
            "name is not a supported scalar bool function parameter or local variable");
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
            .rhs = rhs,
            .source_line = expression->as.binary.op.line,
            .source_column = expression->as.binary.op.column
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

static bool lower_return_statement(Lowerer *lowerer,
                                    const Function *function,
                                    const Stmt *statement,
                                    bool *terminated)
{
    if (statement->as.return_value) {
        uint32_t returned = 0;
        bool lowered = false;
        if (function->return_type == TYPE_INT &&
            statement->as.return_value->type == TYPE_INT)
            lowered = lower_integer_expr(lowerer, statement->as.return_value,
                                         &returned);
        else if (function->return_type == TYPE_BOOL &&
                 statement->as.return_value->type == TYPE_BOOL)
            lowered = lower_boolean_expr(lowerer, statement->as.return_value,
                                         &returned);
        else
            return lower_fail(lowerer,
                "return expression type does not match the function return type");
        if (!lowered) return false;
        YIRInstruction terminator = {
            .opcode=YIR_OP_RETURN,
            .lhs=returned,
            .source_line=statement->token.line,
            .source_column=statement->token.column
        };
        if (!emit(lowerer, terminator, NULL)) return false;
    } else {
        if (function->return_type != TYPE_VOID)
            return lower_fail(lowerer,
                "non-void function must return a value");
        YIRInstruction terminator = {
            .opcode=YIR_OP_RETURN,
            .source_line=statement->token.line,
            .source_column=statement->token.column
        };
        if (!emit(lowerer, terminator, NULL)) return false;
    }
    *terminated = true;
    return true;
}

static bool lower_statement(Lowerer *lowerer, const Function *function,
                            const Stmt *statement, bool *terminated)
{
    if (!statement) return lower_fail(lowerer, "missing statement in IR lowering");
    if (*terminated)
        return lower_fail(lowerer, "statement appears after a return in IR lowering");

    switch (statement->kind) {
        case STMT_BLOCK:
            if (statement->as.block.count && !statement->as.block.items)
                return lower_fail(lowerer, "block statement array is missing");
            for (size_t i = 0; i < statement->as.block.count; ++i) {
                if (!lower_statement(lowerer, function,
                                     statement->as.block.items[i], terminated))
                    return false;
            }
            return true;

        case STMT_VAR: {
            const VarDecl *variable = statement->as.variable;
            if (!variable || variable->is_array ||
                (variable->type != TYPE_INT && variable->type != TYPE_BOOL) ||
                !variable->initializer)
                return lower_fail(lowerer,
                    "only initialized scalar int and bool local variables are supported");
            uint32_t value_id = 0;
            bool lowered = variable->type == TYPE_INT ?
                lower_integer_expr(lowerer, variable->initializer, &value_id) :
                lower_boolean_expr(lowerer, variable->initializer, &value_id);
            if (!lowered) return false;
            return bind_local(lowerer, variable, value_id);
        }

        case STMT_EXPR: {
            const Expr *expression = statement->as.expression;
            if (!expression || expression->kind != EXPR_ASSIGN ||
                expression->as.assign.is_compound ||
                !expression->as.assign.target ||
                expression->as.assign.target->kind != EXPR_NAME)
                return lower_fail(lowerer,
                    "only simple assignments to scalar local names are supported");
            const VarDecl *variable = expression->as.assign.variable;
            if (!variable)
                variable = expression->as.assign.target->as.name.variable;
            if (!variable || variable->is_const || variable->is_array)
                return lower_fail(lowerer,
                    "assignment target is not a mutable scalar local or parameter");
            uint32_t previous_value = 0;
            if (!lookup_local(lowerer, variable, &previous_value) &&
                !is_function_parameter(lowerer, variable))
                return lower_fail(lowerer,
                    "assignment target has no prior local declaration or parameter");
            if (variable->type != TYPE_INT && variable->type != TYPE_BOOL)
                return lower_fail(lowerer,
                    "only int and bool local assignments are supported");
            uint32_t value_id = 0;
            bool lowered = variable->type == TYPE_INT ?
                lower_integer_expr(lowerer, expression->as.assign.right,
                                   &value_id) :
                lower_boolean_expr(lowerer, expression->as.assign.right,
                                   &value_id);
            if (!lowered) return false;
            return bind_local(lowerer, variable, value_id);
        }

        case STMT_RETURN:
            return lower_return_statement(lowerer, function, statement,
                                          terminated);

        default:
            return lower_fail(lowerer,
                "statement is outside the initial scalar IR lowering subset");
    }
}

static bool lower_return_block(const Function *function, const Stmt *body,
                               uint32_t block_id, uint32_t *next_id,
                               YIRBlock *out, char *error,
                               size_t error_capacity)
{
    Lowerer lowerer = {
        .function = function,
        .next_id = *next_id,
        .error = error,
        .error_capacity = error_capacity
    };
    bool terminated = false;
    if (!lower_statement(&lowerer, function, body, &terminated))
        goto failed;
    if (!terminated) {
        if (function->return_type != TYPE_VOID) {
            (void)lower_fail(&lowerer,
                "non-void function path does not return a value");
            goto failed;
        }
        YIRInstruction terminator = {
            .opcode=YIR_OP_RETURN,
            .source_line=function->token.line,
            .source_column=function->token.column
        };
        if (!emit(&lowerer, terminator, NULL)) goto failed;
    }

    out->id = block_id;
    out->instructions = lowerer.items;
    out->instruction_count = lowerer.count;
    *next_id = lowerer.next_id;
    free(lowerer.locals);
    return true;

failed:
    free(lowerer.items);
    free(lowerer.locals);
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
    free((void *)function->parameters);
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
    if (function->param_count && !function->params)
        return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                          "function parameter declarations are missing");
    if (function->param_count >= UINT32_MAX)
        return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                          "function has too many parameters for the initial IR lowerer");
    for (size_t i = 0; i < function->param_count; ++i) {
        if (!function->params[i] || function->params[i]->is_array ||
            (function->params[i]->type != TYPE_INT &&
             function->params[i]->type != TYPE_BOOL))
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "only scalar int and bool function parameters are supported by the initial IR lowerer");
    }
    if (function->return_type != TYPE_INT &&
        function->return_type != TYPE_BOOL &&
        function->return_type != TYPE_VOID)
        return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                          "only int, bool, and void functions are supported by the initial IR lowerer");

    const Stmt *root = unwrap_single_statement(function->body);
    YIRBlock *blocks = NULL;
    size_t block_count = 0;
    uint32_t next_id = (uint32_t)function->param_count;
    YIRFunctionParameter *parameters = NULL;
    if (function->param_count) {
        parameters = calloc(function->param_count, sizeof(*parameters));
        if (!parameters)
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "out of memory while lowering function parameters");
        for (size_t i = 0; i < function->param_count; ++i) {
            parameters[i].id = (uint32_t)i + 1U;
            (void)map_scalar_type(function->params[i]->type,
                                  &parameters[i].type);
        }
    }

    if (root && root->kind == STMT_IF) {
        if (!root->as.if_stmt.else_branch) {
            free(parameters);
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "initial IR lowering requires an else branch when lowering if");
        }
        blocks = calloc(3, sizeof(*blocks));
        if (!blocks) {
            free(parameters);
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "out of memory while creating IR control-flow blocks");
        }

        Lowerer entry = {
            .function=function,
            .next_id=next_id,
            .error=error,
            .error_capacity=error_capacity
        };
        uint32_t condition = 0;
        if (!lower_boolean_expr(&entry, root->as.if_stmt.condition, &condition)) {
            free(entry.items); free(blocks); free(parameters); return false;
        }
        YIRInstruction branch = {
            .opcode=YIR_OP_COND_BRANCH,
            .lhs=condition,
            .target=1,
            .false_target=2,
            .source_line=root->token.line,
            .source_column=root->token.column
        };
        if (!emit(&entry, branch, NULL)) {
            free(entry.items); free(blocks); free(parameters); return false;
        }
        blocks[0].id = 0;
        blocks[0].instructions = entry.items;
        blocks[0].instruction_count = entry.count;
        next_id = entry.next_id;

        if (!lower_return_block(function, root->as.if_stmt.then_branch, 1,
                                &next_id, &blocks[1], error, error_capacity)) {
            free((void *)blocks[0].instructions); free(blocks); free(parameters); return false;
        }
        if (!lower_return_block(function, root->as.if_stmt.else_branch, 2,
                                &next_id, &blocks[2], error, error_capacity)) {
            free((void *)blocks[0].instructions);
            free((void *)blocks[1].instructions);
            free(blocks);
            free(parameters);
            return false;
        }
        block_count = 3;
    } else {
        blocks = calloc(1, sizeof(*blocks));
        if (!blocks) {
            free(parameters);
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "out of memory while creating IR entry block");
        }
        if (!lower_return_block(function, function->body, 0, &next_id,
                                &blocks[0], error, error_capacity)) {
            free(blocks);
            free(parameters);
            return false;
        }
        block_count = 1;
    }

    out->name = function->name;
    if (!map_scalar_type(function->return_type, &out->return_type))
        out->return_type = YIR_TYPE_VOID;
    out->parameters = parameters;
    out->parameter_count = function->param_count;
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
