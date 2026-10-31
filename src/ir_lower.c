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
    uint32_t id;
    YIRBlockParameter *parameters;
    size_t parameter_count;
    size_t parameter_capacity;
    YIRInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
} BuildingBlock;

typedef struct LoopContext {
    const VarDecl **variables;
    size_t variable_count;
    uint32_t header_block;
    uint32_t continue_block;
    uint32_t exit_block;
    uint32_t *header_parameters;
    uint32_t *continue_parameters;
    uint32_t *exit_parameters;
    bool has_exit;
    struct LoopContext *parent;
} LoopContext;

typedef struct {
    const Function *function;
    LoweredLocal *locals;
    size_t local_count;
    size_t local_capacity;
    BuildingBlock *blocks;
    size_t block_count;
    size_t block_capacity;
    size_t current_block;
    uint32_t next_id;
    LoopContext *loop;
    char *error;
    size_t error_capacity;
} Lowerer;

typedef struct {
    const VarDecl **variables;
    size_t count;
    uint32_t *values;
} TrackedValues;

typedef struct {
    size_t block_index;
    bool live;
    LoweredLocal *locals;
    size_t local_count;
    uint32_t *values;
} BranchState;

static bool lower_fail(Lowerer *lowerer, const char *message)
{
    if (lowerer->error && lowerer->error_capacity)
        (void)snprintf(lowerer->error, lowerer->error_capacity, "%s", message);
    return false;
}

static char *make_function_symbol(const Function *function)
{
    if (!function || !function->name || function->name[0] == '\0')
        return NULL;

    const char *module_path = function->module ?
                              function->module->canonical_path : NULL;
    if (!module_path || module_path[0] == '\0') {
        size_t name_length = strlen(function->name);
        if (name_length == SIZE_MAX) return NULL;
        char *symbol = malloc(name_length + 1U);
        if (!symbol) return NULL;
        memcpy(symbol, function->name, name_length + 1U);
        return symbol;
    }

    size_t module_length = strlen(module_path);
    size_t name_length = strlen(function->name);
    if (name_length > SIZE_MAX - 3U ||
        module_length > SIZE_MAX - name_length - 3U) return NULL;
    size_t total = module_length + 2U + name_length + 1U;
    char *symbol = malloc(total);
    if (!symbol) return NULL;
    memcpy(symbol, module_path, module_length);
    symbol[module_length] = ':';
    symbol[module_length + 1U] = ':';
    memcpy(symbol + module_length + 2U, function->name, name_length + 1U);
    return symbol;
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

static bool is_function_parameter(const Lowerer *lowerer,
                                  const VarDecl *variable,
                                  size_t *index)
{
    if (!lowerer->function || !variable) return false;
    for (size_t i = 0; i < lowerer->function->param_count; ++i) {
        if (lowerer->function->params[i] == variable) {
            if (index) *index = i;
            return true;
        }
    }
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

static bool current_value(const Lowerer *lowerer, const VarDecl *variable,
                          uint32_t *value_id)
{
    if (lookup_local(lowerer, variable, value_id)) return true;
    size_t index = 0;
    if (is_function_parameter(lowerer, variable, &index) &&
        index < UINT32_MAX) {
        *value_id = (uint32_t)index + 1U;
        return true;
    }
    return false;
}

static bool has_local(const Lowerer *lowerer, const VarDecl *variable)
{
    for (size_t i = 0; i < lowerer->local_count; ++i)
        if (lowerer->locals[i].variable == variable) return true;
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

static bool allocate_value_id(Lowerer *lowerer, uint32_t *value_id)
{
    if (lowerer->next_id == UINT32_MAX)
        return lower_fail(lowerer, "too many SSA values while lowering function");
    *value_id = ++lowerer->next_id;
    return true;
}

static bool block_is_terminated(const BuildingBlock *block)
{
    if (!block || block->instruction_count == 0) return false;
    YIROpcode opcode = block->instructions[block->instruction_count - 1].opcode;
    return opcode == YIR_OP_BRANCH || opcode == YIR_OP_COND_BRANCH ||
           opcode == YIR_OP_RETURN;
}

static BuildingBlock *current_block(Lowerer *lowerer)
{
    if (lowerer->current_block >= lowerer->block_count) return NULL;
    return &lowerer->blocks[lowerer->current_block];
}

static bool new_block(Lowerer *lowerer, uint32_t *block_id)
{
    if (lowerer->block_count >= UINT32_MAX)
        return lower_fail(lowerer, "too many basic blocks while lowering function");
    if (lowerer->block_count == lowerer->block_capacity) {
        size_t next = lowerer->block_capacity ? lowerer->block_capacity * 2 : 8;
        if (next < lowerer->block_capacity ||
            next > SIZE_MAX / sizeof(*lowerer->blocks))
            return lower_fail(lowerer, "too many basic blocks while lowering function");
        BuildingBlock *blocks = realloc(lowerer->blocks,
                                        next * sizeof(*lowerer->blocks));
        if (!blocks)
            return lower_fail(lowerer, "out of memory while creating IR blocks");
        lowerer->blocks = blocks;
        lowerer->block_capacity = next;
    }
    uint32_t id = (uint32_t)lowerer->block_count;
    lowerer->blocks[lowerer->block_count++] = (BuildingBlock){.id = id};
    *block_id = id;
    return true;
}

static void select_block(Lowerer *lowerer, uint32_t block_id)
{
    lowerer->current_block = (size_t)block_id;
}

static bool add_block_parameter(Lowerer *lowerer, uint32_t block_id,
                                YIRType type, uint32_t *value_id)
{
    if ((size_t)block_id >= lowerer->block_count)
        return lower_fail(lowerer, "internal error: missing IR block");
    BuildingBlock *block = &lowerer->blocks[block_id];
    if (block->parameter_count == block->parameter_capacity) {
        size_t next = block->parameter_capacity ?
                      block->parameter_capacity * 2 : 4;
        if (next < block->parameter_capacity ||
            next > SIZE_MAX / sizeof(*block->parameters))
            return lower_fail(lowerer, "too many block parameters while lowering function");
        YIRBlockParameter *parameters = realloc(
            block->parameters, next * sizeof(*block->parameters));
        if (!parameters)
            return lower_fail(lowerer, "out of memory while adding block parameters");
        block->parameters = parameters;
        block->parameter_capacity = next;
    }
    uint32_t id = 0;
    if (!allocate_value_id(lowerer, &id)) return false;
    block->parameters[block->parameter_count++] =
        (YIRBlockParameter){.id=id,.type=type};
    *value_id = id;
    return true;
}

static bool append_instruction(Lowerer *lowerer, YIRInstruction instruction,
                               uint32_t *result_id)
{
    BuildingBlock *block = current_block(lowerer);
    if (!block) {
        free((void *)instruction.arguments);
        free((void *)instruction.false_arguments);
        free((void *)instruction.call_parameter_types);
        free(instruction.callee_symbol);
        return lower_fail(lowerer, "internal error: no current IR block");
    }
    if (block_is_terminated(block)) {
        free((void *)instruction.arguments);
        free((void *)instruction.false_arguments);
        free((void *)instruction.call_parameter_types);
        free(instruction.callee_symbol);
        return lower_fail(lowerer, "attempted to emit an instruction after a block terminator");
    }
    if (block->instruction_count == block->instruction_capacity) {
        size_t next = block->instruction_capacity ?
                      block->instruction_capacity * 2 : 8;
        if (next < block->instruction_capacity ||
            next > SIZE_MAX / sizeof(*block->instructions)) {
            free((void *)instruction.arguments);
            free((void *)instruction.false_arguments);
            free((void *)instruction.call_parameter_types);
            free(instruction.callee_symbol);
            return lower_fail(lowerer, "too many instructions in an IR block");
        }
        YIRInstruction *instructions = realloc(
            block->instructions, next * sizeof(*block->instructions));
        if (!instructions) {
            free((void *)instruction.arguments);
            free((void *)instruction.false_arguments);
            free((void *)instruction.call_parameter_types);
            free(instruction.callee_symbol);
            return lower_fail(lowerer, "out of memory while lowering IR instructions");
        }
        block->instructions = instructions;
        block->instruction_capacity = next;
    }
    if (result_id) {
        if (!allocate_value_id(lowerer, &instruction.result_id)) {
            free((void *)instruction.arguments);
            free((void *)instruction.false_arguments);
            free((void *)instruction.call_parameter_types);
        free(instruction.callee_symbol);
            return false;
        }
        *result_id = instruction.result_id;
    }
    block->instructions[block->instruction_count++] = instruction;
    return true;
}

static uint32_t *copy_edge_values(Lowerer *lowerer,
                                  const uint32_t *values, size_t count)
{
    if (count == 0) return NULL;
    if (!values || count > SIZE_MAX / sizeof(*values)) {
        (void)lower_fail(lowerer, "invalid or excessively large IR edge argument list");
        return NULL;
    }
    uint32_t *copy = malloc(count * sizeof(*copy));
    if (!copy) {
        (void)lower_fail(lowerer, "out of memory while creating IR edge arguments");
        return NULL;
    }
    memcpy(copy, values, count * sizeof(*copy));
    return copy;
}

static bool emit_branch(Lowerer *lowerer, uint32_t target,
                        const uint32_t *arguments, size_t argument_count,
                        Token source)
{
    YIRInstruction instruction = {
        .opcode=YIR_OP_BRANCH,
        .target=target,
        .arguments=copy_edge_values(lowerer, arguments, argument_count),
        .argument_count=argument_count,
        .source_line=source.line,
        .source_column=source.column
    };
    if (argument_count && !instruction.arguments) return false;
    return append_instruction(lowerer, instruction, NULL);
}

static bool emit_cond_branch(Lowerer *lowerer, uint32_t condition,
                             uint32_t true_target,
                             const uint32_t *true_arguments,
                             size_t true_argument_count,
                             uint32_t false_target,
                             const uint32_t *false_arguments,
                             size_t false_argument_count,
                             Token source)
{
    YIRInstruction instruction = {
        .opcode=YIR_OP_COND_BRANCH,
        .lhs=condition,
        .target=true_target,
        .arguments=copy_edge_values(lowerer, true_arguments,
                                    true_argument_count),
        .argument_count=true_argument_count,
        .false_target=false_target,
        .false_arguments=copy_edge_values(lowerer, false_arguments,
                                          false_argument_count),
        .false_argument_count=false_argument_count,
        .source_line=source.line,
        .source_column=source.column
    };
    if ((true_argument_count && !instruction.arguments) ||
        (false_argument_count && !instruction.false_arguments)) {
        free((void *)instruction.arguments);
        free((void *)instruction.false_arguments);
        free((void *)instruction.call_parameter_types);
        free(instruction.callee_symbol);
        return false;
    }
    return append_instruction(lowerer, instruction, NULL);
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
                               uint32_t *value_id);
static bool lower_boolean_expr(Lowerer *lowerer, const Expr *expression,
                               uint32_t *value_id);

static bool lower_call_expression(Lowerer *lowerer, const Expr *expression,
                                  uint32_t *value_id)
{
    if (!expression || expression->kind != EXPR_CALL)
        return lower_fail(lowerer, "missing function call expression");

    const Function *callee = expression->as.call.function;
    if (!callee || !callee->name)
        return lower_fail(lowerer,
            "only resolved direct user-function calls are supported by the scalar IR lowerer");
    if (callee->param_count != expression->as.call.count ||
        (callee->param_count && (!callee->params || !expression->as.call.args)))
        return lower_fail(lowerer,
            "function call argument count or parameter metadata is invalid");

    YIRType return_type = YIR_TYPE_VOID;
    if (callee->return_type != TYPE_VOID &&
        !map_scalar_type(callee->return_type, &return_type))
        return lower_fail(lowerer,
            "only int, bool, and void user-function return types are supported by the scalar IR lowerer");

    char *callee_symbol = make_function_symbol(callee);
    if (!callee_symbol)
        return lower_fail(lowerer,
            "out of memory while creating a qualified function symbol");

    uint32_t *arguments = callee->param_count ?
        calloc(callee->param_count, sizeof(*arguments)) : NULL;
    YIRType *parameter_types = callee->param_count ?
        calloc(callee->param_count, sizeof(*parameter_types)) : NULL;
    if (callee->param_count && (!arguments || !parameter_types)) {
        free(arguments);
        free(parameter_types);
        free(callee_symbol);
        return lower_fail(lowerer,
            "out of memory while lowering function call arguments");
    }

    for (size_t i = 0; i < callee->param_count; ++i) {
        const VarDecl *parameter = callee->params[i];
        const Expr *argument = expression->as.call.args[i];
        if (!parameter || !argument || parameter->is_array ||
            !map_scalar_type(parameter->type, &parameter_types[i]) ||
            argument->type != parameter->type) {
            free(arguments);
            free(parameter_types);
            free(callee_symbol);
            return lower_fail(lowerer,
                "function calls in the initial IR lowerer require matching scalar int or bool parameters");
        }

        bool lowered = parameter->type == TYPE_INT ?
            lower_integer_expr(lowerer, argument, &arguments[i]) :
            lower_boolean_expr(lowerer, argument, &arguments[i]);
        if (!lowered) {
            free(arguments);
            free(parameter_types);
            free(callee_symbol);
            return false;
        }
    }

    YIRInstruction instruction = {
        .opcode=YIR_OP_CALL,
        .result_type=return_type,
        .arguments=arguments,
        .argument_count=callee->param_count,
        .callee=callee->name,
        .callee_symbol=callee_symbol,
        .call_parameter_types=parameter_types,
        .call_parameter_count=callee->param_count,
        .source_line=expression->token.line,
        .source_column=expression->token.column
    };

    if (return_type == YIR_TYPE_VOID) {
        if (value_id) *value_id = 0;
        return append_instruction(lowerer, instruction, NULL);
    }

    uint32_t ignored_result = 0;
    return append_instruction(lowerer, instruction,
                              value_id ? value_id : &ignored_result);
}

static bool lower_integer_expr(Lowerer *lowerer, const Expr *expression,
                               uint32_t *value_id)
{
    if (!expression)
        return lower_fail(lowerer, "missing expression in IR lowering");

    if (expression->kind == EXPR_CALL) {
        if (expression->type != TYPE_INT)
            return lower_fail(lowerer,
                "integer expression call must return int");
        return lower_call_expression(lowerer, expression, value_id);
    }

    if (expression->kind == EXPR_NAME) {
        if (expression->type != TYPE_INT || !expression->as.name.variable)
            return lower_fail(lowerer, "integer names must refer to scalar parameters or initialized locals");
        if (current_value(lowerer, expression->as.name.variable, value_id))
            return true;
        return lower_fail(lowerer,
            "name is not a supported scalar function parameter or local variable");
    }

    if (expression->kind == EXPR_INT) {
        int64_t immediate = 0;
        if (expression->type != TYPE_INT ||
            !parse_integer_literal(expression, &immediate))
            return lower_fail(lowerer, "invalid or unsupported integer literal");
        YIRInstruction instruction = {
            .opcode=YIR_OP_CONST_I64,
            .result_type=YIR_TYPE_I64,
            .immediate=immediate,
            .source_line=expression->token.line,
            .source_column=expression->token.column
        };
        return append_instruction(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_UNARY &&
        expression->as.unary.op.type == TOKEN_MINUS) {
        if (expression->type == TYPE_INT && expression->is_min_int) {
            YIRInstruction instruction = {
                .opcode=YIR_OP_CONST_I64,
                .result_type=YIR_TYPE_I64,
                .immediate=INT64_MIN,
                .source_line=expression->token.line,
                .source_column=expression->token.column
            };
            return append_instruction(lowerer, instruction, value_id);
        }
        if (expression->type != TYPE_INT)
            return lower_fail(lowerer,
                "integer negation requires an int operand");

        int64_t immediate = 0;
        if (parse_integer_literal(expression->as.unary.right, &immediate)) {
            if (immediate == INT64_MIN)
                return lower_fail(lowerer,
                    "negative integer literal is outside the initial IR literal range");
            YIRInstruction literal = {
                .opcode=YIR_OP_CONST_I64,
                .result_type=YIR_TYPE_I64,
                .immediate=-immediate,
                .source_line=expression->token.line,
                .source_column=expression->token.column
            };
            return append_instruction(lowerer, literal, value_id);
        }

        uint32_t operand = 0;
        if (!lower_integer_expr(lowerer, expression->as.unary.right, &operand))
            return false;
        YIRInstruction instruction = {
            .opcode=YIR_OP_NEG_I64,
            .result_type=YIR_TYPE_I64,
            .lhs=operand,
            .source_line=expression->as.unary.op.line,
            .source_column=expression->as.unary.op.column
        };
        return append_instruction(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_BINARY) {
        uint32_t lhs = 0, rhs = 0;
        if (expression->type != TYPE_INT)
            return lower_fail(lowerer,
                "only integer arithmetic expressions are supported by the scalar IR lowerer");
        if (!lower_integer_expr(lowerer, expression->as.binary.left, &lhs) ||
            !lower_integer_expr(lowerer, expression->as.binary.right, &rhs))
            return false;
        YIROpcode opcode;
        switch (expression->as.binary.op.type) {
            case TOKEN_PLUS: opcode = YIR_OP_ADD_I64; break;
            case TOKEN_MINUS: opcode = YIR_OP_SUB_I64; break;
            case TOKEN_STAR: opcode = YIR_OP_MUL_I64; break;
            case TOKEN_SLASH: opcode = YIR_OP_DIV_I64; break;
            case TOKEN_PERCENT: opcode = YIR_OP_MOD_I64; break;
            default:
                return lower_fail(lowerer,
                    "operator is not supported by the scalar IR lowerer");
        }
        YIRInstruction instruction = {
            .opcode=opcode,
            .result_type=YIR_TYPE_I64,
            .lhs=lhs,
            .rhs=rhs,
            .source_line=expression->as.binary.op.line,
            .source_column=expression->as.binary.op.column
        };
        return append_instruction(lowerer, instruction, value_id);
    }

    return lower_fail(lowerer,
        "expression is outside the scalar IR lowering subset");
}

static bool lower_boolean_expr(Lowerer *lowerer, const Expr *expression,
                               uint32_t *value_id)
{
    if (!expression || expression->type != TYPE_BOOL)
        return lower_fail(lowerer, "control-flow condition must lower to bool");

    if (expression->kind == EXPR_CALL) {
        if (expression->type != TYPE_BOOL)
            return lower_fail(lowerer,
                "boolean expression call must return bool");
        return lower_call_expression(lowerer, expression, value_id);
    }

    if (expression->kind == EXPR_BOOL) {
        if (expression->token.type != TOKEN_TRUE &&
            expression->token.type != TOKEN_FALSE)
            return lower_fail(lowerer, "invalid boolean literal in IR lowering");
        YIRInstruction instruction = {
            .opcode=YIR_OP_CONST_BOOL,
            .result_type=YIR_TYPE_BOOL,
            .immediate=expression->token.type == TOKEN_TRUE ? 1 : 0,
            .source_line=expression->token.line,
            .source_column=expression->token.column
        };
        return append_instruction(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_UNARY &&
        (expression->as.unary.op.type == TOKEN_NOT ||
         expression->as.unary.op.type == TOKEN_BANG)) {
        uint32_t operand = 0;
        if (!lower_boolean_expr(lowerer, expression->as.unary.right, &operand))
            return false;
        YIRInstruction instruction = {
            .opcode=YIR_OP_NOT_BOOL,
            .result_type=YIR_TYPE_BOOL,
            .lhs=operand,
            .source_line=expression->as.unary.op.line,
            .source_column=expression->as.unary.op.column
        };
        return append_instruction(lowerer, instruction, value_id);
    }

    if (expression->kind == EXPR_NAME) {
        uint32_t value = 0;
        if (!expression->as.name.variable ||
            expression->type != TYPE_BOOL ||
            !current_value(lowerer, expression->as.name.variable, &value))
            return lower_fail(lowerer,
                "boolean name is not a supported scalar bool parameter or local variable");
        *value_id = value;
        return true;
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
                    "comparison operator is not supported by the scalar IR lowerer");
        }
        YIRInstruction instruction = {
            .opcode=opcode,
            .result_type=YIR_TYPE_BOOL,
            .lhs=lhs,
            .rhs=rhs,
            .source_line=expression->as.binary.op.line,
            .source_column=expression->as.binary.op.column
        };
        return append_instruction(lowerer, instruction, value_id);
    }

    return lower_fail(lowerer,
        "boolean expression is outside the scalar IR lowering subset");
}

static bool clone_locals(Lowerer *lowerer, LoweredLocal **copy,
                         size_t *count)
{
    *copy = NULL;
    *count = lowerer->local_count;
    if (*count == 0) return true;
    if (*count > SIZE_MAX / sizeof(**copy))
        return lower_fail(lowerer, "too many local bindings to snapshot");
    *copy = malloc(*count * sizeof(**copy));
    if (!*copy)
        return lower_fail(lowerer, "out of memory while snapshotting local bindings");
    memcpy(*copy, lowerer->locals, *count * sizeof(**copy));
    return true;
}

static bool restore_locals(Lowerer *lowerer, const LoweredLocal *locals,
                           size_t count)
{
    if (count > lowerer->local_capacity) {
        if (count > SIZE_MAX / sizeof(*lowerer->locals))
            return lower_fail(lowerer, "too many local bindings while restoring control flow");
        LoweredLocal *copy = realloc(lowerer->locals,
                                     count * sizeof(*lowerer->locals));
        if (!copy)
            return lower_fail(lowerer, "out of memory while restoring local bindings");
        lowerer->locals = copy;
        lowerer->local_capacity = count;
    }
    if (count) memcpy(lowerer->locals, locals, count * sizeof(*locals));
    lowerer->local_count = count;
    return true;
}

static bool tracked_contains(const TrackedValues *tracked,
                              const VarDecl *variable)
{
    for (size_t i = 0; i < tracked->count; ++i)
        if (tracked->variables[i] == variable) return true;
    return false;
}

static bool collect_tracked_values(Lowerer *lowerer, TrackedValues *tracked)
{
    memset(tracked, 0, sizeof(*tracked));
    if (lowerer->function->param_count >
        SIZE_MAX - lowerer->local_count)
        return lower_fail(lowerer, "too many values to track across control flow");
    size_t capacity = lowerer->function->param_count + lowerer->local_count;
    if (capacity > SIZE_MAX / sizeof(*tracked->variables) ||
        capacity > SIZE_MAX / sizeof(*tracked->values))
        return lower_fail(lowerer, "too many values to track across control flow");
    if (capacity == 0) return true;
    tracked->variables = malloc(capacity * sizeof(*tracked->variables));
    tracked->values = malloc(capacity * sizeof(*tracked->values));
    if (!tracked->variables || !tracked->values) {
        free(tracked->variables);
        free(tracked->values);
        memset(tracked, 0, sizeof(*tracked));
        return lower_fail(lowerer, "out of memory while tracking control-flow values");
    }

    for (size_t i = 0; i < lowerer->function->param_count; ++i) {
        const VarDecl *variable = lowerer->function->params[i];
        if (!tracked_contains(tracked, variable)) {
            uint32_t value = 0;
            if (!current_value(lowerer, variable, &value)) {
                free(tracked->variables); free(tracked->values);
                memset(tracked, 0, sizeof(*tracked));
                return lower_fail(lowerer, "could not resolve a function parameter while lowering control flow");
            }
            tracked->variables[tracked->count] = variable;
            tracked->values[tracked->count++] = value;
        }
    }
    for (size_t i = 0; i < lowerer->local_count; ++i) {
        const VarDecl *variable = lowerer->locals[i].variable;
        if (!tracked_contains(tracked, variable)) {
            tracked->variables[tracked->count] = variable;
            tracked->values[tracked->count++] = lowerer->locals[i].value_id;
        }
    }
    return true;
}

static void destroy_tracked_values(TrackedValues *tracked)
{
    if (!tracked) return;
    free(tracked->variables);
    free(tracked->values);
    memset(tracked, 0, sizeof(*tracked));
}

static bool capture_tracked_values(Lowerer *lowerer,
                                   const TrackedValues *tracked,
                                   uint32_t *values)
{
    for (size_t i = 0; i < tracked->count; ++i) {
        if (!current_value(lowerer, tracked->variables[i], &values[i]))
            return lower_fail(lowerer, "control-flow value disappeared during IR lowering");
    }
    return true;
}

static bool lower_return_statement(Lowerer *lowerer,
                                   const Stmt *statement)
{
    uint32_t returned = 0;
    if (statement->as.return_value) {
        bool lowered = false;
        if (lowerer->function->return_type == TYPE_INT &&
            statement->as.return_value->type == TYPE_INT)
            lowered = lower_integer_expr(lowerer,
                                          statement->as.return_value,
                                          &returned);
        else if (lowerer->function->return_type == TYPE_BOOL &&
                 statement->as.return_value->type == TYPE_BOOL)
            lowered = lower_boolean_expr(lowerer,
                                          statement->as.return_value,
                                          &returned);
        else
            return lower_fail(lowerer,
                "return expression type does not match the function return type");
        if (!lowered) return false;
    } else if (lowerer->function->return_type != TYPE_VOID) {
        return lower_fail(lowerer, "non-void function must return a value");
    }

    YIRInstruction terminator = {
        .opcode=YIR_OP_RETURN,
        .lhs=returned,
        .source_line=statement->token.line,
        .source_column=statement->token.column
    };
    return append_instruction(lowerer, terminator, NULL);
}

static bool lower_expression_statement(Lowerer *lowerer,
                                       const Expr *expression)
{
    if (expression && expression->kind == EXPR_CALL)
        return lower_call_expression(lowerer, expression, NULL);
    if (!expression || expression->kind != EXPR_ASSIGN ||
        expression->as.assign.is_compound ||
        !expression->as.assign.target ||
        expression->as.assign.target->kind != EXPR_NAME)
        return lower_fail(lowerer,
            "only simple assignments to scalar local names or parameters are supported");

    const VarDecl *variable = expression->as.assign.variable;
    if (!variable)
        variable = expression->as.assign.target->as.name.variable;
    if (!variable || variable->is_const || variable->is_array)
        return lower_fail(lowerer,
            "assignment target is not a mutable scalar local or parameter");
    if (!has_local(lowerer, variable) &&
        !is_function_parameter(lowerer, variable, NULL))
        return lower_fail(lowerer,
            "assignment target has no prior local declaration or parameter");
    if (variable->type != TYPE_INT && variable->type != TYPE_BOOL)
        return lower_fail(lowerer,
            "only int and bool local assignments are supported");

    uint32_t value_id = 0;
    bool lowered = variable->type == TYPE_INT ?
        lower_integer_expr(lowerer, expression->as.assign.right, &value_id) :
        lower_boolean_expr(lowerer, expression->as.assign.right, &value_id);
    if (!lowered) return false;
    return bind_local(lowerer, variable, value_id);
}

static bool contains_break_for_current_loop(const Stmt *statement)
{
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_BREAK:
            return true;
        case STMT_BLOCK:
            for (size_t i = 0; i < statement->as.block.count; ++i)
                if (contains_break_for_current_loop(statement->as.block.items[i]))
                    return true;
            return false;
        case STMT_IF:
            return contains_break_for_current_loop(statement->as.if_stmt.then_branch) ||
                   contains_break_for_current_loop(statement->as.if_stmt.else_branch);
        /* A nested loop owns its own break statements. */
        case STMT_LOOP:
        case STMT_WHILE:
        case STMT_FOR:
            return false;
        default:
            return false;
    }
}

static bool contains_continue_for_current_loop(const Stmt *statement)
{
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_CONTINUE:
            return true;
        case STMT_BLOCK:
            for (size_t i = 0; i < statement->as.block.count; ++i)
                if (contains_continue_for_current_loop(statement->as.block.items[i]))
                    return true;
            return false;
        case STMT_IF:
            return contains_continue_for_current_loop(statement->as.if_stmt.then_branch) ||
                   contains_continue_for_current_loop(statement->as.if_stmt.else_branch);
        case STMT_LOOP:
        case STMT_WHILE:
        case STMT_FOR:
            return false;
        default:
            return false;
    }
}

static bool statement_may_fall_through(const Stmt *statement)
{
    if (!statement) return true;
    switch (statement->kind) {
        case STMT_RETURN:
        case STMT_BREAK:
        case STMT_CONTINUE:
            return false;
        case STMT_BLOCK:
            for (size_t i = 0; i < statement->as.block.count; ++i) {
                if (!statement_may_fall_through(statement->as.block.items[i]))
                    return false;
            }
            return true;
        case STMT_IF:
            return !statement->as.if_stmt.else_branch ||
                   statement_may_fall_through(statement->as.if_stmt.then_branch) ||
                   statement_may_fall_through(statement->as.if_stmt.else_branch);
        case STMT_LOOP:
            return contains_break_for_current_loop(statement->as.loop_body);
        case STMT_WHILE:
        case STMT_FOR:
            return true;
        default:
            return true;
    }
}

static bool lower_statement(Lowerer *lowerer, const Stmt *statement);

static bool lower_if_statement(Lowerer *lowerer, const Stmt *statement)
{
    uint32_t condition = 0;
    if (!lower_boolean_expr(lowerer, statement->as.if_stmt.condition,
                            &condition))
        return false;

    TrackedValues tracked;
    if (!collect_tracked_values(lowerer, &tracked)) return false;
    LoweredLocal *base_locals = NULL;
    size_t base_local_count = 0;
    if (!clone_locals(lowerer, &base_locals, &base_local_count)) {
        destroy_tracked_values(&tracked);
        return false;
    }

    uint32_t then_entry = 0, else_entry = 0;
    if (!new_block(lowerer, &then_entry) ||
        !new_block(lowerer, &else_entry)) {
        free(base_locals);
        destroy_tracked_values(&tracked);
        return false;
    }
    if (!emit_cond_branch(lowerer, condition, then_entry, NULL, 0,
                          else_entry, NULL, 0, statement->token)) {
        free(base_locals);
        destroy_tracked_values(&tracked);
        return false;
    }

    BranchState then_state = {0}, else_state = {0};
    then_state.block_index = then_entry;
    select_block(lowerer, then_entry);
    if (statement->as.if_stmt.then_branch &&
        !lower_statement(lowerer, statement->as.if_stmt.then_branch))
        goto failed;
    then_state.block_index = lowerer->current_block;
    then_state.live = !block_is_terminated(current_block(lowerer));
    if (!clone_locals(lowerer, &then_state.locals, &then_state.local_count))
        goto failed;
    if (tracked.count) {
        then_state.values = malloc(tracked.count * sizeof(*then_state.values));
        if (!then_state.values ||
            !capture_tracked_values(lowerer, &tracked, then_state.values))
            goto failed;
    }

    if (!restore_locals(lowerer, base_locals, base_local_count))
        goto failed;
    else_state.block_index = else_entry;
    select_block(lowerer, else_entry);
    if (statement->as.if_stmt.else_branch &&
        !lower_statement(lowerer, statement->as.if_stmt.else_branch))
        goto failed;
    else_state.block_index = lowerer->current_block;
    else_state.live = !block_is_terminated(current_block(lowerer));
    if (!clone_locals(lowerer, &else_state.locals, &else_state.local_count))
        goto failed;
    if (tracked.count) {
        else_state.values = malloc(tracked.count * sizeof(*else_state.values));
        if (!else_state.values ||
            !capture_tracked_values(lowerer, &tracked, else_state.values))
            goto failed;
    }

    if (then_state.live && !else_state.live) {
        if (!restore_locals(lowerer, then_state.locals,
                            then_state.local_count)) goto failed;
        select_block(lowerer, (uint32_t)then_state.block_index);
    } else if (!then_state.live && else_state.live) {
        if (!restore_locals(lowerer, else_state.locals,
                            else_state.local_count)) goto failed;
        select_block(lowerer, (uint32_t)else_state.block_index);
    } else if (!then_state.live && !else_state.live) {
        if (!restore_locals(lowerer, base_locals, base_local_count))
            goto failed;
        select_block(lowerer, (uint32_t)else_state.block_index);
    } else {
        uint32_t merge_block = 0;
        if (!new_block(lowerer, &merge_block)) goto failed;
        uint32_t *merge_ids = tracked.count ?
            calloc(tracked.count, sizeof(*merge_ids)) : NULL;
        uint32_t *then_arguments = tracked.count ?
            malloc(tracked.count * sizeof(*then_arguments)) : NULL;
        uint32_t *else_arguments = tracked.count ?
            malloc(tracked.count * sizeof(*else_arguments)) : NULL;
        if (tracked.count &&
            (!merge_ids || !then_arguments || !else_arguments)) {
            free(merge_ids); free(then_arguments); free(else_arguments);
            (void)lower_fail(lowerer, "out of memory while merging if branches");
            goto failed;
        }
        size_t merge_count = 0;
        for (size_t i = 0; i < tracked.count; ++i) {
            if (then_state.values[i] == else_state.values[i]) continue;
            YIRType type;
            if (!map_scalar_type(tracked.variables[i]->type, &type) ||
                !add_block_parameter(lowerer, merge_block, type,
                                     &merge_ids[merge_count])) {
                free(merge_ids); free(then_arguments); free(else_arguments);
                goto failed;
            }
            then_arguments[merge_count] = then_state.values[i];
            else_arguments[merge_count] = else_state.values[i];
            merge_count++;
        }

        select_block(lowerer, (uint32_t)then_state.block_index);
        if (!emit_branch(lowerer, merge_block, then_arguments,
                         merge_count, statement->token)) {
            free(merge_ids); free(then_arguments); free(else_arguments);
            goto failed;
        }
        select_block(lowerer, (uint32_t)else_state.block_index);
        if (!emit_branch(lowerer, merge_block, else_arguments,
                         merge_count, statement->token)) {
            free(merge_ids); free(then_arguments); free(else_arguments);
            goto failed;
        }
        if (!restore_locals(lowerer, base_locals, base_local_count)) {
            free(merge_ids); free(then_arguments); free(else_arguments);
            goto failed;
        }
        select_block(lowerer, merge_block);
        size_t merged_index = 0;
        for (size_t i = 0; i < tracked.count; ++i) {
            if (then_state.values[i] != else_state.values[i]) {
                if (!bind_local(lowerer, tracked.variables[i],
                                merge_ids[merged_index++])) {
                    free(merge_ids); free(then_arguments); free(else_arguments);
                    goto failed;
                }
            }
        }
        free(merge_ids);
        free(then_arguments);
        free(else_arguments);
    }

    free(base_locals);
    free(then_state.locals);
    free(then_state.values);
    free(else_state.locals);
    free(else_state.values);
    destroy_tracked_values(&tracked);
    return true;

failed:
    free(base_locals);
    free(then_state.locals);
    free(then_state.values);
    free(else_state.locals);
    free(else_state.values);
    destroy_tracked_values(&tracked);
    return false;
}

static bool emit_loop_edge(Lowerer *lowerer, uint32_t target,
                           const LoopContext *loop, Token source)
{
    uint32_t *values = loop->variable_count ?
        malloc(loop->variable_count * sizeof(*values)) : NULL;
    if (loop->variable_count && !values)
        return lower_fail(lowerer, "out of memory while creating loop edge arguments");
    if (!capture_tracked_values(lowerer,
            &(TrackedValues){.variables=loop->variables,
                             .count=loop->variable_count},
            values)) {
        free(values);
        return false;
    }
    bool ok = emit_branch(lowerer, target, values, loop->variable_count,
                          source);
    free(values);
    return ok;
}

static bool restore_loop_exit_bindings(Lowerer *lowerer,
                                       const LoweredLocal *base_locals,
                                       size_t base_local_count,
                                       const LoopContext *loop)
{
    if (!restore_locals(lowerer, base_locals, base_local_count))
        return false;
    for (size_t i = 0; i < loop->variable_count; ++i) {
        const VarDecl *variable = loop->variables[i];
        if (is_function_parameter(lowerer, variable, NULL) ||
            has_local(lowerer, variable)) {
            if (!bind_local(lowerer, variable, loop->exit_parameters[i]))
                return false;
        }
    }
    return true;
}

static bool lower_loop_statement(Lowerer *lowerer, const Stmt *statement,
                                 StmtKind kind)
{
    LoopContext loop = {.parent=lowerer->loop};
    LoweredLocal *base_locals = NULL;
    size_t base_local_count = 0;
    if (!clone_locals(lowerer, &base_locals, &base_local_count)) return false;

    if (kind == STMT_FOR && statement->as.for_stmt.initializer &&
        !lower_statement(lowerer, statement->as.for_stmt.initializer)) {
        free(base_locals);
        return false;
    }

    TrackedValues tracked;
    if (!collect_tracked_values(lowerer, &tracked)) {
        free(base_locals);
        return false;
    }

    uint32_t *header_parameters = tracked.count ?
        calloc(tracked.count, sizeof(*header_parameters)) : NULL;
    uint32_t *exit_parameters = tracked.count ?
        calloc(tracked.count, sizeof(*exit_parameters)) : NULL;
    if (tracked.count && (!header_parameters || !exit_parameters)) {
        free(header_parameters); free(exit_parameters);
        free(base_locals); destroy_tracked_values(&tracked);
        return lower_fail(lowerer, "out of memory while creating loop-carried values");
    }

    uint32_t header_block = 0, body_block = 0, increment_block = 0;
    uint32_t exit_block = 0;
    bool has_break = contains_break_for_current_loop(
        kind == STMT_LOOP ? statement->as.loop_body :
        kind == STMT_WHILE ? statement->as.while_stmt.body :
                             statement->as.for_stmt.body);
    bool has_exit = kind == STMT_WHILE ||
                    (kind == STMT_FOR &&
                     (statement->as.for_stmt.condition != NULL || has_break)) ||
                    (kind == STMT_LOOP && has_break);
    const Stmt *body = kind == STMT_LOOP ? statement->as.loop_body :
                       kind == STMT_WHILE ? statement->as.while_stmt.body :
                                            statement->as.for_stmt.body;
    const Expr *increment = kind == STMT_FOR ?
                            statement->as.for_stmt.increment : NULL;
    bool need_increment_block = kind == STMT_FOR && increment &&
        (statement_may_fall_through(body) ||
         contains_continue_for_current_loop(body));

    if (!new_block(lowerer, &header_block) ||
        !new_block(lowerer, &body_block)) goto failed;
    if (need_increment_block &&
        !new_block(lowerer, &increment_block)) goto failed;
    if (has_exit && !new_block(lowerer, &exit_block)) goto failed;

    for (size_t i = 0; i < tracked.count; ++i) {
        YIRType type;
        if (!map_scalar_type(tracked.variables[i]->type, &type) ||
            !add_block_parameter(lowerer, header_block, type,
                                 &header_parameters[i]))
            goto failed;
    }
    if (need_increment_block) {
        loop.continue_parameters = tracked.count ?
            calloc(tracked.count, sizeof(*loop.continue_parameters)) : NULL;
        if (tracked.count && !loop.continue_parameters) {
            (void)lower_fail(lowerer, "out of memory while creating for-increment parameters");
            goto failed;
        }
        for (size_t i = 0; i < tracked.count; ++i) {
            YIRType type;
            if (!map_scalar_type(tracked.variables[i]->type, &type) ||
                !add_block_parameter(lowerer, increment_block, type,
                                     &loop.continue_parameters[i]))
                goto failed;
        }
    }
    if (has_exit) {
        for (size_t i = 0; i < tracked.count; ++i) {
            YIRType type;
            if (!map_scalar_type(tracked.variables[i]->type, &type) ||
                !add_block_parameter(lowerer, exit_block, type,
                                     &exit_parameters[i]))
                goto failed;
        }
    }

    if (!emit_branch(lowerer, header_block, tracked.values,
                     tracked.count, statement->token))
        goto failed;

    /* The header's block parameters are the loop-carried SSA values. */
    for (size_t i = 0; i < tracked.count; ++i) {
        if (!bind_local(lowerer, tracked.variables[i], header_parameters[i]))
            goto failed;
    }

    loop.variables = tracked.variables;
    loop.variable_count = tracked.count;
    loop.header_block = header_block;
    loop.continue_block = need_increment_block ? increment_block : header_block;
    loop.exit_block = exit_block;
    loop.header_parameters = header_parameters;
    loop.exit_parameters = exit_parameters;
    loop.has_exit = has_exit;

    select_block(lowerer, header_block);
    if (kind == STMT_LOOP ||
        (kind == STMT_FOR && !statement->as.for_stmt.condition)) {
        if (!emit_branch(lowerer, body_block, NULL, 0, statement->token))
            goto failed;
    } else {
        const Expr *condition = kind == STMT_WHILE ?
                                statement->as.while_stmt.condition :
                                statement->as.for_stmt.condition;
        uint32_t condition_id = 0;
        if (condition) {
            if (!lower_boolean_expr(lowerer, condition, &condition_id))
                goto failed;
        } else {
            YIRInstruction truth = {
                .opcode=YIR_OP_CONST_BOOL,
                .result_type=YIR_TYPE_BOOL,
                .immediate=1,
                .source_line=statement->token.line,
                .source_column=statement->token.column
            };
            if (!append_instruction(lowerer, truth, &condition_id))
                goto failed;
        }
        if (!emit_cond_branch(lowerer, condition_id, body_block, NULL, 0,
                              exit_block, has_exit ? header_parameters : NULL,
                              has_exit ? tracked.count : 0, statement->token))
            goto failed;
    }

    lowerer->loop = &loop;
    select_block(lowerer, body_block);
    if (body && !lower_statement(lowerer, body)) goto failed;

    if (!block_is_terminated(current_block(lowerer))) {
        if (need_increment_block) {
            if (!emit_loop_edge(lowerer, increment_block, &loop,
                                statement->token)) goto failed;
        } else if (!emit_loop_edge(lowerer, header_block, &loop,
                                   statement->token)) {
            goto failed;
        }
    }

    if (need_increment_block) {
        for (size_t i = 0; i < tracked.count; ++i) {
            if (!bind_local(lowerer, tracked.variables[i],
                            loop.continue_parameters[i]))
                goto failed;
        }
        select_block(lowerer, increment_block);
        Stmt increment_statement = {
            .kind=STMT_EXPR,
            .token=increment->token,
            .as.expression=(Expr *)increment
        };
        if (!lower_statement(lowerer, &increment_statement))
            goto failed;
        uint32_t *updated = tracked.count ?
            malloc(tracked.count * sizeof(*updated)) : NULL;
        if (tracked.count && !updated) {
            (void)lower_fail(lowerer, "out of memory while lowering for increment");
            goto failed;
        }
        if (!capture_tracked_values(lowerer, &tracked, updated) ||
            !emit_branch(lowerer, header_block, updated, tracked.count,
                         statement->token)) {
            free(updated);
            goto failed;
        }
        free(updated);
    }

    lowerer->loop = loop.parent;
    if (has_exit) {
        if (!restore_loop_exit_bindings(lowerer, base_locals,
                                        base_local_count, &loop))
            goto failed;
        select_block(lowerer, exit_block);
    } else {
        if (!restore_locals(lowerer, base_locals, base_local_count))
            goto failed;
        select_block(lowerer, header_block);
    }

    free(loop.continue_parameters);
    free(header_parameters);
    free(exit_parameters);
    free(base_locals);
    destroy_tracked_values(&tracked);
    return true;

failed:
    lowerer->loop = loop.parent;
    free(loop.continue_parameters);
    free(header_parameters);
    free(exit_parameters);
    free(base_locals);
    destroy_tracked_values(&tracked);
    return false;
}

static bool lower_statement(Lowerer *lowerer, const Stmt *statement)
{
    if (!statement)
        return lower_fail(lowerer, "missing statement in IR lowering");
    if (block_is_terminated(current_block(lowerer)))
        return lower_fail(lowerer, "statement appears after a terminated control-flow path");

    switch (statement->kind) {
        case STMT_BLOCK: {
            if (statement->as.block.count && !statement->as.block.items)
                return lower_fail(lowerer, "block statement array is missing");
            size_t scope_count = lowerer->local_count;
            for (size_t i = 0; i < statement->as.block.count; ++i) {
                if (!lower_statement(lowerer, statement->as.block.items[i])) {
                    lowerer->local_count = scope_count;
                    return false;
                }
            }
            lowerer->local_count = scope_count;
            return true;
        }

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

        case STMT_EXPR:
            return lower_expression_statement(lowerer, statement->as.expression);

        case STMT_RETURN:
            return lower_return_statement(lowerer, statement);

        case STMT_IF:
            return lower_if_statement(lowerer, statement);

        case STMT_WHILE:
        case STMT_FOR:
        case STMT_LOOP:
            return lower_loop_statement(lowerer, statement, statement->kind);

        case STMT_BREAK:
            if (!lowerer->loop || !lowerer->loop->has_exit)
                return lower_fail(lowerer, "break is not supported outside a loop with an exit path");
            return emit_loop_edge(lowerer, lowerer->loop->exit_block,
                                  lowerer->loop, statement->token);

        case STMT_CONTINUE:
            if (!lowerer->loop)
                return lower_fail(lowerer, "continue is not supported outside a loop");
            return emit_loop_edge(lowerer, lowerer->loop->continue_block,
                                  lowerer->loop, statement->token);

        default:
            return lower_fail(lowerer,
                "statement is outside the current scalar IR lowering subset");
    }
}

static void destroy_building_blocks(BuildingBlock *blocks, size_t block_count)
{
    if (!blocks) return;
    for (size_t bi = 0; bi < block_count; ++bi) {
        for (size_t ii = 0; ii < blocks[bi].instruction_count; ++ii) {
            free((void *)blocks[bi].instructions[ii].arguments);
            free((void *)blocks[bi].instructions[ii].false_arguments);
            free((void *)blocks[bi].instructions[ii].call_parameter_types);
            free(blocks[bi].instructions[ii].callee_symbol);
        }
        free(blocks[bi].parameters);
        free(blocks[bi].instructions);
    }
    free(blocks);
}

void yir_destroy_function(YIRFunction *function)
{
    if (!function) return;
    if (function->blocks) {
        YIRBlock *blocks = (YIRBlock *)function->blocks;
        for (size_t bi = 0; bi < function->block_count; ++bi) {
            YIRInstruction *instructions =
                (YIRInstruction *)blocks[bi].instructions;
            for (size_t ii = 0; ii < blocks[bi].instruction_count; ++ii) {
                free((void *)instructions[ii].arguments);
                free((void *)instructions[ii].false_arguments);
                free((void *)instructions[ii].call_parameter_types);
                free(instructions[ii].callee_symbol);
            }
            free(instructions);
            free((void *)blocks[bi].parameters);
        }
        free(blocks);
    }
    free((void *)function->parameters);
    free(function->symbol);
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
                          "function has too many parameters for the IR lowerer");
    for (size_t i = 0; i < function->param_count; ++i) {
        if (!function->params[i] || function->params[i]->is_array ||
            (function->params[i]->type != TYPE_INT &&
             function->params[i]->type != TYPE_BOOL))
            return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                              "only scalar int and bool function parameters are supported by the IR lowerer");
    }
    if (function->return_type != TYPE_INT &&
        function->return_type != TYPE_BOOL &&
        function->return_type != TYPE_VOID)
        return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                          "only int, bool, and void functions are supported by the IR lowerer");

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

    Lowerer lowerer = {
        .function=function,
        .next_id=(uint32_t)function->param_count,
        .error=error,
        .error_capacity=error_capacity
    };
    uint32_t entry_block = 0;
    if (!new_block(&lowerer, &entry_block)) {
        free(parameters);
        destroy_building_blocks(lowerer.blocks, lowerer.block_count);
        return false;
    }
    select_block(&lowerer, entry_block);

    if (!lower_statement(&lowerer, function->body)) {
        free(parameters);
        free(lowerer.locals);
        destroy_building_blocks(lowerer.blocks, lowerer.block_count);
        return false;
    }

    if (!block_is_terminated(current_block(&lowerer))) {
        if (function->return_type != TYPE_VOID) {
            free(parameters);
            free(lowerer.locals);
            destroy_building_blocks(lowerer.blocks, lowerer.block_count);
            return lower_fail(&lowerer,
                "non-void function path does not return a value");
        }
        YIRInstruction terminator = {
            .opcode=YIR_OP_RETURN,
            .source_line=function->token.line,
            .source_column=function->token.column
        };
        if (!append_instruction(&lowerer, terminator, NULL)) {
            free(parameters);
            free(lowerer.locals);
            destroy_building_blocks(lowerer.blocks, lowerer.block_count);
            return false;
        }
    }

    YIRBlock *blocks = calloc(lowerer.block_count, sizeof(*blocks));
    if (!blocks) {
        free(parameters);
        free(lowerer.locals);
        destroy_building_blocks(lowerer.blocks, lowerer.block_count);
        return lower_fail(&lowerer,
            "out of memory while finalizing lowered IR blocks");
    }
    for (size_t i = 0; i < lowerer.block_count; ++i) {
        blocks[i].id = lowerer.blocks[i].id;
        blocks[i].parameters = lowerer.blocks[i].parameters;
        blocks[i].parameter_count = lowerer.blocks[i].parameter_count;
        blocks[i].instructions = lowerer.blocks[i].instructions;
        blocks[i].instruction_count = lowerer.blocks[i].instruction_count;
        lowerer.blocks[i].parameters = NULL;
        lowerer.blocks[i].instructions = NULL;
        lowerer.blocks[i].parameter_count = 0;
        lowerer.blocks[i].instruction_count = 0;
    }
    free(lowerer.blocks);
    free(lowerer.locals);

    out->name = function->name;
    out->symbol = make_function_symbol(function);
    if (!out->symbol) {
        out->blocks = blocks;
        out->block_count = lowerer.block_count;
        out->parameters = parameters;
        out->parameter_count = function->param_count;
        yir_destroy_function(out);
        return lower_fail(&(Lowerer){.error=error,.error_capacity=error_capacity},
                          "out of memory while creating the function's qualified symbol");
    }
    out->return_type = function->return_type == TYPE_VOID ? YIR_TYPE_VOID :
                       function->return_type == TYPE_BOOL ? YIR_TYPE_BOOL :
                                                           YIR_TYPE_I64;
    out->parameters = parameters;
    out->parameter_count = function->param_count;
    out->blocks = blocks;
    out->block_count = lowerer.block_count;

    if (!yir_verify_function(out, error, error_capacity)) {
        yir_destroy_function(out);
        return false;
    }
    return true;
}
