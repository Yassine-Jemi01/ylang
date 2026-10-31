#include "ylang/ir.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t id;
    YIRType type;
    int64_t value;
} KnownConstant;

static bool optimize_fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity)
        (void)snprintf(error, capacity, "%s", message);
    return false;
}

static const KnownConstant *find_constant(const KnownConstant *constants,
                                          size_t count, uint32_t id)
{
    for (size_t i = 0; i < count; ++i)
        if (constants[i].id == id) return &constants[i];
    return NULL;
}

static bool remember_constant(KnownConstant *constants, size_t capacity,
                              size_t *count, uint32_t id, YIRType type,
                              int64_t value)
{
    if (find_constant(constants, *count, id)) return true;
    if (*count >= capacity) return false;
    constants[*count] = (KnownConstant){
        .id=id, .type=type, .value=value
    };
    (*count)++;
    return true;
}

static bool checked_add_i64(int64_t left, int64_t right, int64_t *result)
{
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right))
        return false;
    *result = left + right;
    return true;
}

static bool checked_sub_i64(int64_t left, int64_t right, int64_t *result)
{
    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right))
        return false;
    *result = left - right;
    return true;
}

static bool checked_mul_i64(int64_t left, int64_t right, int64_t *result)
{
    if (left == 0 || right == 0) {
        *result = 0;
        return true;
    }
    if (left > 0) {
        if ((right > 0 && left > INT64_MAX / right) ||
            (right < 0 && right < INT64_MIN / left))
            return false;
    } else {
        if ((right > 0 && left < INT64_MIN / right) ||
            (right < 0 && left < INT64_MAX / right))
            return false;
    }
    *result = left * right;
    return true;
}

static bool fold_instruction(const YIRInstruction *instruction,
                             const KnownConstant *constants,
                             size_t constant_count,
                             YIROpcode *opcode, YIRType *type,
                             int64_t *immediate)
{
    const KnownConstant *left = NULL;
    const KnownConstant *right = NULL;

    switch (instruction->opcode) {
        case YIR_OP_NOT_BOOL:
            left = find_constant(constants, constant_count,
                                 instruction->lhs);
            if (!left || left->type != YIR_TYPE_BOOL) return false;
            *opcode = YIR_OP_CONST_BOOL;
            *type = YIR_TYPE_BOOL;
            *immediate = left->value == 0 ? 1 : 0;
            return true;

        case YIR_OP_NEG_I64:
            left = find_constant(constants, constant_count,
                                 instruction->lhs);
            if (!left || left->type != YIR_TYPE_I64 ||
                left->value == INT64_MIN)
                return false;
            *opcode = YIR_OP_CONST_I64;
            *type = YIR_TYPE_I64;
            *immediate = -left->value;
            return true;

        case YIR_OP_ADD_I64:
        case YIR_OP_SUB_I64:
        case YIR_OP_MUL_I64:
        case YIR_OP_DIV_I64:
        case YIR_OP_MOD_I64:
        case YIR_OP_EQ_I64:
        case YIR_OP_NE_I64:
        case YIR_OP_LT_I64:
        case YIR_OP_LE_I64:
        case YIR_OP_GT_I64:
        case YIR_OP_GE_I64:
            left = find_constant(constants, constant_count,
                                 instruction->lhs);
            right = find_constant(constants, constant_count,
                                  instruction->rhs);
            if (!left || !right || left->type != YIR_TYPE_I64 ||
                right->type != YIR_TYPE_I64)
                return false;
            break;

        default:
            return false;
    }

    switch (instruction->opcode) {
        case YIR_OP_ADD_I64:
            if (!checked_add_i64(left->value, right->value, immediate))
                return false;
            *opcode = YIR_OP_CONST_I64;
            *type = YIR_TYPE_I64;
            return true;

        case YIR_OP_SUB_I64:
            if (!checked_sub_i64(left->value, right->value, immediate))
                return false;
            *opcode = YIR_OP_CONST_I64;
            *type = YIR_TYPE_I64;
            return true;

        case YIR_OP_MUL_I64:
            if (!checked_mul_i64(left->value, right->value, immediate))
                return false;
            *opcode = YIR_OP_CONST_I64;
            *type = YIR_TYPE_I64;
            return true;

        case YIR_OP_DIV_I64:
            if (right->value == 0 ||
                (left->value == INT64_MIN && right->value == -1))
                return false;
            *immediate = left->value / right->value;
            *opcode = YIR_OP_CONST_I64;
            *type = YIR_TYPE_I64;
            return true;

        case YIR_OP_MOD_I64:
            if (right->value == 0) return false;
            *immediate =
                left->value == INT64_MIN && right->value == -1 ?
                0 : left->value % right->value;
            *opcode = YIR_OP_CONST_I64;
            *type = YIR_TYPE_I64;
            return true;

        case YIR_OP_EQ_I64:
            *immediate = left->value == right->value;
            break;
        case YIR_OP_NE_I64:
            *immediate = left->value != right->value;
            break;
        case YIR_OP_LT_I64:
            *immediate = left->value < right->value;
            break;
        case YIR_OP_LE_I64:
            *immediate = left->value <= right->value;
            break;
        case YIR_OP_GT_I64:
            *immediate = left->value > right->value;
            break;
        case YIR_OP_GE_I64:
            *immediate = left->value >= right->value;
            break;
        default:
            return false;
    }

    *opcode = YIR_OP_CONST_BOOL;
    *type = YIR_TYPE_BOOL;
    return true;
}

static size_t count_instructions(const YIRFunction *function, bool *okay)
{
    size_t count = 0;
    *okay = false;
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        if (block->instruction_count > SIZE_MAX - count)
            return 0;
        count += block->instruction_count;
    }
    *okay = true;
    return count;
}

bool yir_optimize_function(YIRFunction *function, char *error,
                           size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!function)
        return optimize_fail(error, error_capacity,
                             "optimizer requires a function");

    if (!yir_verify_function(function, error, error_capacity))
        return false;

    bool count_okay = false;
    const size_t instruction_count = count_instructions(function,
                                                        &count_okay);
    if (!count_okay)
        return optimize_fail(error, error_capacity,
                             "function has too many instructions to optimize");

    KnownConstant *constants = calloc(instruction_count,
                                      sizeof(*constants));
    if (!constants)
        return optimize_fail(error, error_capacity,
                             "out of memory while preparing constant folding");

    size_t constant_count = 0;
    size_t pass_limit = instruction_count == SIZE_MAX ?
                        SIZE_MAX : instruction_count + 1;
    for (size_t pass = 0; pass < pass_limit; ++pass) {
        bool changed = false;
        for (size_t bi = 0; bi < function->block_count; ++bi) {
            YIRBlock *block = (YIRBlock *)function->blocks + bi;
            YIRInstruction *instructions =
                (YIRInstruction *)block->instructions;
            for (size_t ii = 0; ii < block->instruction_count; ++ii) {
                YIRInstruction *instruction = &instructions[ii];
                if (instruction->result_id == 0) continue;

                if (instruction->opcode == YIR_OP_CONST_I64 ||
                    instruction->opcode == YIR_OP_CONST_BOOL) {
                    size_t previous_count = constant_count;
                    if (!remember_constant(constants, instruction_count,
                            &constant_count, instruction->result_id,
                            instruction->result_type,
                            instruction->immediate)) {
                        free(constants);
                        return optimize_fail(error, error_capacity,
                            "constant table exceeded the instruction count");
                    }
                    if (constant_count != previous_count)
                        changed = true;
                    continue;
                }

                YIROpcode folded_opcode = instruction->opcode;
                YIRType folded_type = instruction->result_type;
                int64_t folded_value = 0;
                if (!fold_instruction(instruction, constants,
                        constant_count, &folded_opcode, &folded_type,
                        &folded_value))
                    continue;

                size_t source_line = instruction->source_line;
                size_t source_column = instruction->source_column;
                uint32_t result_id = instruction->result_id;
                *instruction = (YIRInstruction){
                    .opcode=folded_opcode,
                    .result_id=result_id,
                    .result_type=folded_type,
                    .immediate=folded_value,
                    .source_line=source_line,
                    .source_column=source_column
                };
                if (!remember_constant(constants, instruction_count,
                        &constant_count, result_id, folded_type,
                        folded_value)) {
                    free(constants);
                    return optimize_fail(error, error_capacity,
                        "constant table exceeded the instruction count");
                }
                changed = true;
            }
        }
        if (!changed) break;
    }
    free(constants);

    return yir_verify_function(function, error, error_capacity);
}

bool yir_optimize_module(YIRFunction *functions, size_t function_count,
                         char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!yir_verify_module(functions, function_count,
                           error, error_capacity))
        return false;

    for (size_t i = 0; i < function_count; ++i) {
        char function_error[256];
        if (!yir_optimize_function(&functions[i], function_error,
                                   sizeof(function_error))) {
            if (error && error_capacity)
                (void)snprintf(error, error_capacity,
                    "function '%s' failed optimization: %s",
                    functions[i].name ? functions[i].name : "<anonymous>",
                    function_error[0] ? function_error : "unknown optimizer error");
            return false;
        }
    }

    return yir_verify_module(functions, function_count,
                             error, error_capacity);
}
