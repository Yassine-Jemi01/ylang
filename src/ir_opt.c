#include "ylang/ir.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t id;
    YIRType type;
    int64_t value;
    double floating_value;
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

static bool safe_f64_constant(double value)
{
    return isfinite(value) && fpclassify(value) != FP_SUBNORMAL;
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

static bool remember_f64_constant(KnownConstant *constants, size_t capacity,
                                  size_t *count, uint32_t id, double value)
{
    if (!safe_f64_constant(value)) return true;
    if (find_constant(constants, *count, id)) return true;
    if (*count >= capacity) return false;
    constants[*count] = (KnownConstant){
        .id=id, .type=YIR_TYPE_F64, .floating_value=value
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
                             int64_t *immediate,
                             double *floating_immediate)
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

        case YIR_OP_I64_TO_F64:
            left = find_constant(constants, constant_count,
                                 instruction->lhs);
            if (!left || left->type != YIR_TYPE_I64) return false;
            *floating_immediate = (double)left->value;
            if (!safe_f64_constant(*floating_immediate)) return false;
            *opcode = YIR_OP_CONST_F64;
            *type = YIR_TYPE_F64;
            return true;

        case YIR_OP_F64_TO_I64:
            left = find_constant(constants, constant_count,
                                 instruction->lhs);
            if (!left || left->type != YIR_TYPE_F64 ||
                !isfinite(left->floating_value) ||
                left->floating_value < -9223372036854775808.0 ||
                left->floating_value >= 9223372036854775808.0)
                return false;
            *immediate = (int64_t)left->floating_value;
            *opcode = YIR_OP_CONST_I64;
            *type = YIR_TYPE_I64;
            return true;

        case YIR_OP_NEG_F64:
            left = find_constant(constants, constant_count,
                                 instruction->lhs);
            if (!left || left->type != YIR_TYPE_F64 ||
                !safe_f64_constant(left->floating_value))
                return false;
            *floating_immediate = -left->floating_value;
            if (!safe_f64_constant(*floating_immediate)) return false;
            *opcode = YIR_OP_CONST_F64;
            *type = YIR_TYPE_F64;
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

        case YIR_OP_ADD_F64:
        case YIR_OP_SUB_F64:
        case YIR_OP_MUL_F64:
        case YIR_OP_DIV_F64:
        case YIR_OP_EQ_F64:
        case YIR_OP_NE_F64:
        case YIR_OP_LT_F64:
        case YIR_OP_LE_F64:
        case YIR_OP_GT_F64:
        case YIR_OP_GE_F64:
            left = find_constant(constants, constant_count,
                                 instruction->lhs);
            right = find_constant(constants, constant_count,
                                  instruction->rhs);
            if (!left || !right || left->type != YIR_TYPE_F64 ||
                right->type != YIR_TYPE_F64 ||
                !safe_f64_constant(left->floating_value) ||
                !safe_f64_constant(right->floating_value))
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

        case YIR_OP_ADD_F64:
        case YIR_OP_SUB_F64:
        case YIR_OP_MUL_F64:
        case YIR_OP_DIV_F64: {
            double result = 0.0;
            if (instruction->opcode == YIR_OP_DIV_F64 &&
                right->floating_value == 0.0)
                return false;
            switch (instruction->opcode) {
                case YIR_OP_ADD_F64:
                    result = left->floating_value + right->floating_value;
                    break;
                case YIR_OP_SUB_F64:
                    result = left->floating_value - right->floating_value;
                    break;
                case YIR_OP_MUL_F64:
                    result = left->floating_value * right->floating_value;
                    break;
                case YIR_OP_DIV_F64:
                    result = left->floating_value / right->floating_value;
                    break;
                default:
                    return false;
            }
            if (!safe_f64_constant(result)) return false;
            if (result == 0.0 &&
                instruction->opcode != YIR_OP_ADD_F64 &&
                instruction->opcode != YIR_OP_SUB_F64 &&
                left->floating_value != 0.0 &&
                right->floating_value != 0.0)
                return false;
            *floating_immediate = result;
            *opcode = YIR_OP_CONST_F64;
            *type = YIR_TYPE_F64;
            return true;
        }

        case YIR_OP_EQ_F64:
            *immediate = left->floating_value == right->floating_value;
            break;
        case YIR_OP_NE_F64:
            *immediate = left->floating_value != right->floating_value;
            break;
        case YIR_OP_LT_F64:
            *immediate = left->floating_value < right->floating_value;
            break;
        case YIR_OP_LE_F64:
            *immediate = left->floating_value <= right->floating_value;
            break;
        case YIR_OP_GT_F64:
            *immediate = left->floating_value > right->floating_value;
            break;
        case YIR_OP_GE_F64:
            *immediate = left->floating_value >= right->floating_value;
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

static bool constant_fold_function(YIRFunction *function, char *error,
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
                                continue;
                }
                if (instruction->opcode == YIR_OP_CONST_F64) {
                    if (safe_f64_constant(instruction->floating_immediate)) {
                        size_t previous_count = constant_count;
                        if (!remember_f64_constant(constants,
                                instruction_count, &constant_count,
                                instruction->result_id,
                                instruction->floating_immediate)) {
                            free(constants);
                            return optimize_fail(error, error_capacity,
                                "constant table exceeded the instruction count");
                        }
                        if (constant_count != previous_count)
                            changed = true;
                    }
                    continue;
                }

                YIROpcode folded_opcode = instruction->opcode;
                YIRType folded_type = instruction->result_type;
                int64_t folded_value = 0;
                double folded_float = 0.0;
                if (!fold_instruction(instruction, constants,
                        constant_count, &folded_opcode, &folded_type,
                        &folded_value, &folded_float))
                    continue;

                size_t source_line = instruction->source_line;
                size_t source_column = instruction->source_column;
                uint32_t result_id = instruction->result_id;
                *instruction = (YIRInstruction){
                    .opcode=folded_opcode,
                    .result_id=result_id,
                    .result_type=folded_type,
                    .immediate=folded_value,
                    .floating_immediate=folded_float,
                    .source_line=source_line,
                    .source_column=source_column
                };
                bool remembered = folded_opcode == YIR_OP_CONST_F64 ?
                    remember_f64_constant(constants, instruction_count,
                        &constant_count, result_id, folded_float) :
                    remember_constant(constants, instruction_count,
                        &constant_count, result_id, folded_type, folded_value);
                if (!remembered) {
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


static size_t find_block_index(const YIRFunction *function, uint32_t id)
{
    for (size_t i = 0; i < function->block_count; ++i)
        if (function->blocks[i].id == id) return i;
    return SIZE_MAX;
}

static void free_instruction_storage(YIRInstruction *instruction)
{
    free((void *)instruction->arguments);
    free((void *)instruction->false_arguments);
    free((void *)instruction->call_parameter_types);
    free(instruction->callee_symbol);
}

static void free_block_storage(YIRBlock *block)
{
    YIRInstruction *instructions = (YIRInstruction *)block->instructions;
    for (size_t i = 0; i < block->instruction_count; ++i)
        free_instruction_storage(&instructions[i]);
    free(instructions);
    free((void *)block->parameters);
}

static bool remove_unreachable_blocks(YIRFunction *function, char *error,
                                      size_t error_capacity)
{
    const size_t count = function->block_count;
    bool *reachable = calloc(count, sizeof(*reachable));
    size_t *queue = calloc(count, sizeof(*queue));
    if (!reachable || !queue) {
        free(reachable);
        free(queue);
        return optimize_fail(error, error_capacity,
                             "out of memory while simplifying the control-flow graph");
    }

    size_t queue_read = 0;
    size_t queue_write = 0;
    reachable[0] = true;
    queue[queue_write++] = 0;
    while (queue_read < queue_write) {
        size_t block_index = queue[queue_read++];
        const YIRBlock *block = &function->blocks[block_index];
        const YIRInstruction *term =
            &block->instructions[block->instruction_count - 1];
        uint32_t targets[2];
        size_t target_count = 0;
        if (term->opcode == YIR_OP_BRANCH) {
            targets[target_count++] = term->target;
        } else if (term->opcode == YIR_OP_COND_BRANCH) {
            targets[target_count++] = term->target;
            targets[target_count++] = term->false_target;
        }
        for (size_t i = 0; i < target_count; ++i) {
            size_t target_index = find_block_index(function, targets[i]);
            if (target_index == SIZE_MAX) {
                free(reachable);
                free(queue);
                return optimize_fail(error, error_capacity,
                                     "control-flow simplification found a missing branch target");
            }
            if (!reachable[target_index]) {
                reachable[target_index] = true;
                queue[queue_write++] = target_index;
            }
        }
    }

    size_t kept_count = 0;
    for (size_t i = 0; i < count; ++i)
        if (reachable[i]) kept_count++;
    if (kept_count == count) {
        free(reachable);
        free(queue);
        return true;
    }

    YIRBlock *old_blocks = (YIRBlock *)function->blocks;
    YIRBlock *kept_blocks = calloc(kept_count, sizeof(*kept_blocks));
    if (!kept_blocks) {
        free(reachable);
        free(queue);
        return optimize_fail(error, error_capacity,
                             "out of memory while compacting reachable basic blocks");
    }

    size_t write_index = 0;
    for (size_t i = 0; i < count; ++i) {
        if (reachable[i]) {
            kept_blocks[write_index++] = old_blocks[i];
        } else {
            free_block_storage(&old_blocks[i]);
        }
    }
    free(old_blocks);
    function->blocks = kept_blocks;
    function->block_count = kept_count;
    free(reachable);
    free(queue);
    return true;
}

static bool simplify_constant_branches(YIRFunction *function, char *error,
                                       size_t error_capacity)
{
    bool count_okay = false;
    size_t instruction_count = count_instructions(function, &count_okay);
    if (!count_okay)
        return optimize_fail(error, error_capacity,
                             "function has too many instructions for CFG simplification");
    KnownConstant *constants = calloc(instruction_count, sizeof(*constants));
    if (!constants)
        return optimize_fail(error, error_capacity,
                             "out of memory while preparing CFG simplification");

    size_t constant_count = 0;
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *ins = &block->instructions[ii];
            if (ins->opcode != YIR_OP_CONST_BOOL) continue;
            if (!remember_constant(constants, instruction_count,
                    &constant_count, ins->result_id, YIR_TYPE_BOOL,
                    ins->immediate)) {
                free(constants);
                return optimize_fail(error, error_capacity,
                                     "CFG constant table exceeded the instruction count");
            }
        }
    }

    for (size_t bi = 0; bi < function->block_count; ++bi) {
        YIRBlock *block = (YIRBlock *)function->blocks + bi;
        YIRInstruction *instructions = (YIRInstruction *)block->instructions;
        YIRInstruction *term = &instructions[block->instruction_count - 1];
        if (term->opcode != YIR_OP_COND_BRANCH) continue;
        const KnownConstant *condition =
            find_constant(constants, constant_count, term->lhs);
        if (!condition || condition->type != YIR_TYPE_BOOL) continue;

        if (condition->value == 0) {
            free((void *)term->arguments);
            term->arguments = term->false_arguments;
            term->argument_count = term->false_argument_count;
            term->false_arguments = NULL;
            term->false_argument_count = 0;
            term->target = term->false_target;
        } else {
            free((void *)term->false_arguments);
            term->false_arguments = NULL;
            term->false_argument_count = 0;
        }
        term->opcode = YIR_OP_BRANCH;
        term->false_target = 0;
        term->lhs = 0;
        term->rhs = 0;
    }
    free(constants);

    if (!remove_unreachable_blocks(function, error, error_capacity))
        return false;
    return yir_verify_function(function, error, error_capacity);
}

static bool mark_live_value(uint32_t *live_values, size_t capacity,
                            size_t *live_count, uint32_t id)
{
    if (id == 0) return true;
    for (size_t i = 0; i < *live_count; ++i)
        if (live_values[i] == id) return true;
    if (*live_count >= capacity) return false;
    live_values[(*live_count)++] = id;
    return true;
}

static bool value_is_live(const uint32_t *live_values, size_t live_count,
                          uint32_t id)
{
    if (id == 0) return false;
    for (size_t i = 0; i < live_count; ++i)
        if (live_values[i] == id) return true;
    return false;
}

static bool mark_instruction_inputs(const YIRInstruction *ins,
                                    uint32_t *live_values, size_t capacity,
                                    size_t *live_count)
{
    switch (ins->opcode) {
        case YIR_OP_ADD_I64:
        case YIR_OP_SUB_I64:
        case YIR_OP_MUL_I64:
        case YIR_OP_DIV_I64:
        case YIR_OP_MOD_I64:
        case YIR_OP_ADD_F64:
        case YIR_OP_SUB_F64:
        case YIR_OP_MUL_F64:
        case YIR_OP_DIV_F64:
        case YIR_OP_EQ_I64:
        case YIR_OP_NE_I64:
        case YIR_OP_LT_I64:
        case YIR_OP_LE_I64:
        case YIR_OP_GT_I64:
        case YIR_OP_GE_I64:
        case YIR_OP_EQ_F64:
        case YIR_OP_NE_F64:
        case YIR_OP_LT_F64:
        case YIR_OP_LE_F64:
        case YIR_OP_GT_F64:
        case YIR_OP_GE_F64:
            return mark_live_value(live_values, capacity, live_count, ins->lhs) &&
                   mark_live_value(live_values, capacity, live_count, ins->rhs);
        case YIR_OP_NEG_I64:
        case YIR_OP_NEG_F64:
        case YIR_OP_I64_TO_F64:
        case YIR_OP_F64_TO_I64:
        case YIR_OP_NOT_BOOL:
            return mark_live_value(live_values, capacity, live_count, ins->lhs);
        case YIR_OP_COND_BRANCH:
            if (!mark_live_value(live_values, capacity, live_count, ins->lhs))
                return false;
            for (size_t i = 0; i < ins->argument_count; ++i)
                if (!mark_live_value(live_values, capacity, live_count,
                                     ins->arguments[i])) return false;
            for (size_t i = 0; i < ins->false_argument_count; ++i)
                if (!mark_live_value(live_values, capacity, live_count,
                                     ins->false_arguments[i])) return false;
            return true;
        case YIR_OP_BRANCH:
            for (size_t i = 0; i < ins->argument_count; ++i)
                if (!mark_live_value(live_values, capacity, live_count,
                                     ins->arguments[i])) return false;
            return true;
        case YIR_OP_RETURN:
            return mark_live_value(live_values, capacity, live_count, ins->lhs);
        case YIR_OP_CALL:
            for (size_t i = 0; i < ins->argument_count; ++i)
                if (!mark_live_value(live_values, capacity, live_count,
                                     ins->arguments[i])) return false;
            return true;
        case YIR_OP_CONST_I64:
        case YIR_OP_CONST_F64:
        case YIR_OP_CONST_BOOL:
            return true;
    }
    return false;
}

static bool instruction_may_have_effects_or_trap(YIROpcode opcode)
{
    switch (opcode) {
        case YIR_OP_ADD_I64:
        case YIR_OP_SUB_I64:
        case YIR_OP_MUL_I64:
        case YIR_OP_DIV_I64:
        case YIR_OP_MOD_I64:
        case YIR_OP_NEG_I64:
        case YIR_OP_DIV_F64:
        case YIR_OP_F64_TO_I64:
        case YIR_OP_CALL:
            return true;
        default:
            return false;
    }
}

static bool eliminate_dead_code(YIRFunction *function, char *error,
                                size_t error_capacity)
{
    bool count_okay = false;
    size_t instruction_count = count_instructions(function, &count_okay);
    if (!count_okay)
        return optimize_fail(error, error_capacity,
                             "function has too many instructions for dead-code elimination");

    size_t value_capacity = instruction_count;
    if (function->parameter_count > SIZE_MAX - value_capacity)
        return optimize_fail(error, error_capacity,
                             "function has too many values for dead-code elimination");
    value_capacity += function->parameter_count;
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        if (function->blocks[bi].parameter_count > SIZE_MAX - value_capacity)
            return optimize_fail(error, error_capacity,
                                 "function has too many block parameters for dead-code elimination");
        value_capacity += function->blocks[bi].parameter_count;
    }
    uint32_t *live_values = calloc(value_capacity ? value_capacity : 1,
                                   sizeof(*live_values));
    if (!live_values)
        return optimize_fail(error, error_capacity,
                             "out of memory while preparing dead-code elimination");
    size_t live_count = 0;

    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *ins = &block->instructions[ii];
            bool root = ii + 1 == block->instruction_count ||
                        instruction_may_have_effects_or_trap(ins->opcode);
            if (root && !mark_instruction_inputs(ins, live_values,
                                                  value_capacity, &live_count)) {
                free(live_values);
                return optimize_fail(error, error_capacity,
                                     "live-value table exceeded the function value count");
            }
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t bi = 0; bi < function->block_count; ++bi) {
            const YIRBlock *block = &function->blocks[bi];
            for (size_t ii = 0; ii < block->instruction_count; ++ii) {
                const YIRInstruction *ins = &block->instructions[ii];
                if (!ins->result_id ||
                    !value_is_live(live_values, live_count, ins->result_id))
                    continue;
                size_t previous_count = live_count;
                if (!mark_instruction_inputs(ins, live_values,
                                              value_capacity, &live_count)) {
                    free(live_values);
                    return optimize_fail(error, error_capacity,
                                         "live-value table exceeded the function value count");
                }
                if (live_count != previous_count) changed = true;
            }
        }
    }

    for (size_t bi = 0; bi < function->block_count; ++bi) {
        YIRBlock *block = (YIRBlock *)function->blocks + bi;
        YIRInstruction *instructions = (YIRInstruction *)block->instructions;
        size_t write_index = 0;
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            YIRInstruction instruction = instructions[ii];
            bool keep = ii + 1 == block->instruction_count ||
                        instruction_may_have_effects_or_trap(instruction.opcode) ||
                        value_is_live(live_values, live_count,
                                      instruction.result_id);
            if (keep) {
                if (write_index != ii) instructions[write_index] = instruction;
                write_index++;
            } else {
                free_instruction_storage(&instructions[ii]);
            }
        }
        block->instruction_count = write_index;
    }
    free(live_values);
    return yir_verify_function(function, error, error_capacity);
}

static bool is_cse_safe_opcode(YIROpcode opcode)
{
    switch (opcode) {
        case YIR_OP_ADD_F64:
        case YIR_OP_SUB_F64:
        case YIR_OP_MUL_F64:
        case YIR_OP_NEG_F64:
        case YIR_OP_EQ_F64:
        case YIR_OP_NE_F64:
        case YIR_OP_LT_F64:
        case YIR_OP_LE_F64:
        case YIR_OP_GT_F64:
        case YIR_OP_GE_F64:
        case YIR_OP_EQ_I64:
        case YIR_OP_NE_I64:
        case YIR_OP_LT_I64:
        case YIR_OP_LE_I64:
        case YIR_OP_GT_I64:
        case YIR_OP_GE_I64:
        case YIR_OP_NOT_BOOL:
            return true;
        default:
            return false;
    }
}

static bool same_pure_expression(const YIRInstruction *left,
                                 const YIRInstruction *right)
{
    if (!is_cse_safe_opcode(left->opcode) ||
        left->opcode != right->opcode ||
        left->result_type != right->result_type ||
        left->lhs != right->lhs)
        return false;

    switch (left->opcode) {
        case YIR_OP_ADD_F64:
        case YIR_OP_SUB_F64:
        case YIR_OP_MUL_F64:
        case YIR_OP_EQ_F64:
        case YIR_OP_NE_F64:
        case YIR_OP_LT_F64:
        case YIR_OP_LE_F64:
        case YIR_OP_GT_F64:
        case YIR_OP_GE_F64:
        case YIR_OP_EQ_I64:
        case YIR_OP_NE_I64:
        case YIR_OP_LT_I64:
        case YIR_OP_LE_I64:
        case YIR_OP_GT_I64:
        case YIR_OP_GE_I64:
            return left->rhs == right->rhs;
        case YIR_OP_NEG_F64:
        case YIR_OP_I64_TO_F64:
        case YIR_OP_NOT_BOOL:
            return true;
        default:
            return false;
    }
}

static void replace_value_uses(YIRFunction *function, uint32_t old_id,
                               uint32_t replacement_id)
{
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        YIRBlock *block = (YIRBlock *)function->blocks + bi;
        YIRInstruction *instructions = (YIRInstruction *)block->instructions;
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            YIRInstruction *ins = &instructions[ii];
            if (ins->lhs == old_id) ins->lhs = replacement_id;
            if (ins->rhs == old_id) ins->rhs = replacement_id;
            for (size_t ai = 0; ai < ins->argument_count; ++ai) {
                if (ins->arguments[ai] == old_id)
                    ((uint32_t *)ins->arguments)[ai] = replacement_id;
            }
            for (size_t ai = 0; ai < ins->false_argument_count; ++ai) {
                if (ins->false_arguments[ai] == old_id)
                    ((uint32_t *)ins->false_arguments)[ai] = replacement_id;
            }
        }
    }
}

static bool eliminate_common_subexpressions(YIRFunction *function,
                                            char *error,
                                            size_t error_capacity)
{
    if (!yir_verify_function(function, error, error_capacity)) return false;

    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        YIRInstruction *instructions = (YIRInstruction *)block->instructions;
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            YIRInstruction *candidate = &instructions[ii];
            if (!candidate->result_id ||
                !is_cse_safe_opcode(candidate->opcode))
                continue;

            for (size_t prior = 0; prior < ii; ++prior) {
                const YIRInstruction *available = &instructions[prior];
                if (!available->result_id ||
                    !same_pure_expression(available, candidate))
                    continue;
                replace_value_uses(function, candidate->result_id,
                                   available->result_id);
                break;
            }
        }
    }
    return yir_verify_function(function, error, error_capacity);
}

static void remove_edge_argument(YIRInstruction *terminator,
                                 bool false_edge, size_t argument_index)
{
    size_t *count = false_edge ? &terminator->false_argument_count :
                                 &terminator->argument_count;
    uint32_t *arguments = false_edge ?
        (uint32_t *)terminator->false_arguments :
        (uint32_t *)terminator->arguments;
    if (!arguments || argument_index >= *count) return;
    if (argument_index + 1 < *count) {
        memmove(&arguments[argument_index], &arguments[argument_index + 1],
                (*count - argument_index - 1) * sizeof(*arguments));
    }
    --*count;
}

static void remove_target_edge_argument(YIRFunction *function,
                                        uint32_t target_id,
                                        size_t argument_index)
{
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        YIRBlock *block = (YIRBlock *)function->blocks + bi;
        YIRInstruction *term =
            (YIRInstruction *)block->instructions +
            block->instruction_count - 1;
        if (term->opcode == YIR_OP_BRANCH ||
            term->opcode == YIR_OP_COND_BRANCH) {
            if (term->target == target_id)
                remove_edge_argument(term, false, argument_index);
            if (term->opcode == YIR_OP_COND_BRANCH &&
                term->false_target == target_id)
                remove_edge_argument(term, true, argument_index);
        }
    }
}

static bool propagate_identical_block_arguments(YIRFunction *function,
                                                char *error,
                                                size_t error_capacity)
{
    for (size_t target_index = 0;
         target_index < function->block_count; ++target_index) {
        YIRBlock *target = (YIRBlock *)function->blocks + target_index;
        size_t parameter_index = 0;
        while (parameter_index < target->parameter_count) {
            const YIRBlockParameter *parameter =
                &target->parameters[parameter_index];
            uint32_t common_value = 0;
            bool saw_predecessor = false;
            bool identical = true;

            for (size_t bi = 0; bi < function->block_count; ++bi) {
                const YIRBlock *predecessor = &function->blocks[bi];
                const YIRInstruction *term =
                    &predecessor->instructions[
                        predecessor->instruction_count - 1];
                if ((term->opcode == YIR_OP_BRANCH ||
                     term->opcode == YIR_OP_COND_BRANCH) &&
                    term->target == target->id) {
                    uint32_t value = term->arguments[parameter_index];
                    if (!saw_predecessor) {
                        common_value = value;
                        saw_predecessor = true;
                    } else if (value != common_value) {
                        identical = false;
                    }
                }
                if (term->opcode == YIR_OP_COND_BRANCH &&
                    term->false_target == target->id) {
                    uint32_t value = term->false_arguments[parameter_index];
                    if (!saw_predecessor) {
                        common_value = value;
                        saw_predecessor = true;
                    } else if (value != common_value) {
                        identical = false;
                    }
                }
            }

            if (!saw_predecessor || !identical ||
                common_value == parameter->id) {
                ++parameter_index;
                continue;
            }

            replace_value_uses(function, parameter->id, common_value);
            remove_target_edge_argument(function, target->id, parameter_index);
            YIRBlockParameter *parameters =
                (YIRBlockParameter *)target->parameters;
            if (parameter_index + 1 < target->parameter_count) {
                memmove(&parameters[parameter_index],
                        &parameters[parameter_index + 1],
                        (target->parameter_count - parameter_index - 1) *
                            sizeof(*parameters));
            }
            --target->parameter_count;
            changed = true;
        }
    }

    if (!yir_verify_function(function, error, error_capacity)) return false;
    return true;
}

static bool find_local_i64_constant(const YIRBlock *block,
                                    size_t before_instruction,
                                    uint32_t value_id, int64_t *value)
{
    for (size_t i = before_instruction; i > 0; --i) {
        const YIRInstruction *candidate = &block->instructions[i - 1];
        if (candidate->result_id == value_id &&
            candidate->opcode == YIR_OP_CONST_I64) {
            *value = candidate->immediate;
            return true;
        }
    }
    return false;
}

static void rewrite_instruction_constant(YIRInstruction *instruction,
                                         YIROpcode opcode, YIRType type,
                                         int64_t value)
{
    uint32_t result_id = instruction->result_id;
    size_t source_line = instruction->source_line;
    size_t source_column = instruction->source_column;
    free_instruction_storage(instruction);
    *instruction = (YIRInstruction){
        .opcode=opcode,
        .result_id=result_id,
        .result_type=type,
        .immediate=value,
        .source_line=source_line,
        .source_column=source_column
    };
}

static void erase_instruction(YIRBlock *block, size_t instruction_index)
{
    YIRInstruction *instructions = (YIRInstruction *)block->instructions;
    free_instruction_storage(&instructions[instruction_index]);
    if (instruction_index + 1 < block->instruction_count) {
        memmove(&instructions[instruction_index],
                &instructions[instruction_index + 1],
                (block->instruction_count - instruction_index - 1) *
                    sizeof(*instructions));
    }
    --block->instruction_count;
}

static bool simplify_integer_algebra(YIRFunction *function, char *error,
                                     size_t error_capacity)
{
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        YIRBlock *block = (YIRBlock *)function->blocks + bi;
        size_t ii = 0;
        while (ii + 1 < block->instruction_count) {
            YIRInstruction *instruction =
                (YIRInstruction *)block->instructions + ii;
            if (!instruction->result_id) {
                ++ii;
                continue;
            }

            int64_t lhs_constant = 0;
            int64_t rhs_constant = 0;
            bool lhs_is_constant = find_local_i64_constant(
                block, ii, instruction->lhs, &lhs_constant);
            bool rhs_is_constant = find_local_i64_constant(
                block, ii, instruction->rhs, &rhs_constant);
            bool removed = false;

            switch (instruction->opcode) {
                case YIR_OP_ADD_I64:
                    if (lhs_is_constant && lhs_constant == 0) {
                        replace_value_uses(function, instruction->result_id,
                                           instruction->rhs);
                        erase_instruction(block, ii);
                        removed = true;
                    } else if (rhs_is_constant && rhs_constant == 0) {
                        replace_value_uses(function, instruction->result_id,
                                           instruction->lhs);
                        erase_instruction(block, ii);
                        removed = true;
                    }
                    break;
                case YIR_OP_SUB_I64:
                    if (rhs_is_constant && rhs_constant == 0) {
                        replace_value_uses(function, instruction->result_id,
                                           instruction->lhs);
                        erase_instruction(block, ii);
                        removed = true;
                    }
                    break;
                case YIR_OP_MUL_I64:
                    if ((lhs_is_constant && lhs_constant == 0) ||
                        (rhs_is_constant && rhs_constant == 0)) {
                        rewrite_instruction_constant(instruction,
                            YIR_OP_CONST_I64, YIR_TYPE_I64, 0);
                    } else if (lhs_is_constant && lhs_constant == 1) {
                        replace_value_uses(function, instruction->result_id,
                                           instruction->rhs);
                        erase_instruction(block, ii);
                        removed = true;
                    } else if (rhs_is_constant && rhs_constant == 1) {
                        replace_value_uses(function, instruction->result_id,
                                           instruction->lhs);
                        erase_instruction(block, ii);
                        removed = true;
                    }
                    break;
                case YIR_OP_DIV_I64:
                    if (rhs_is_constant && rhs_constant == 1) {
                        replace_value_uses(function, instruction->result_id,
                                           instruction->lhs);
                        erase_instruction(block, ii);
                        removed = true;
                    }
                    break;
                case YIR_OP_MOD_I64:
                    if (rhs_is_constant && rhs_constant == 1)
                        rewrite_instruction_constant(instruction,
                            YIR_OP_CONST_I64, YIR_TYPE_I64, 0);
                    break;
                case YIR_OP_EQ_I64:
                case YIR_OP_NE_I64:
                case YIR_OP_LT_I64:
                case YIR_OP_LE_I64:
                case YIR_OP_GT_I64:
                case YIR_OP_GE_I64:
                    if (instruction->lhs == instruction->rhs) {
                        bool truth = instruction->opcode == YIR_OP_EQ_I64 ||
                                     instruction->opcode == YIR_OP_LE_I64 ||
                                     instruction->opcode == YIR_OP_GE_I64;
                        rewrite_instruction_constant(instruction,
                            YIR_OP_CONST_BOOL, YIR_TYPE_BOOL, truth ? 1 : 0);
                    }
                    break;
                default:
                    break;
            }

            if (!removed) ++ii;
        }
    }
    return yir_verify_function(function, error, error_capacity);
}

bool yir_run_opt_pipeline(YIRFunction *function, const YIROptPass *passes,
                          size_t pass_count, char *error,
                          size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!function)
        return optimize_fail(error, error_capacity,
                             "optimization pipeline requires a function");
    if (pass_count && !passes)
        return optimize_fail(error, error_capacity,
                             "optimization pipeline requires a pass array");
    if (!yir_verify_function(function, error, error_capacity)) return false;

    for (size_t i = 0; i < pass_count; ++i) {
        bool success = false;
        switch (passes[i]) {
            case YIR_OPT_CONSTANT_FOLD:
                success = constant_fold_function(function, error, error_capacity);
                break;
            case YIR_OPT_SIMPLIFY_CFG:
                success = simplify_constant_branches(function, error, error_capacity);
                break;
            case YIR_OPT_COMMON_SUBEXPRESSION:
                success = eliminate_common_subexpressions(function, error, error_capacity);
                break;
            case YIR_OPT_COPY_PROPAGATION:
                success = propagate_identical_block_arguments(function, error, error_capacity);
                break;
            case YIR_OPT_ALGEBRAIC_SIMPLIFY:
                success = simplify_integer_algebra(function, error, error_capacity);
                break;
            case YIR_OPT_DEAD_CODE:
                success = eliminate_dead_code(function, error, error_capacity);
                break;
            default:
                return optimize_fail(error, error_capacity,
                                     "optimization pipeline contains an unknown pass");
        }
        if (!success) return false;
        if (!yir_verify_function(function, error, error_capacity)) return false;
    }
    return true;
}

bool yir_optimize_function(YIRFunction *function, char *error,
                           size_t error_capacity)
{
    static const YIROptPass default_pipeline[] = {
        YIR_OPT_CONSTANT_FOLD,
        YIR_OPT_SIMPLIFY_CFG,
        YIR_OPT_COMMON_SUBEXPRESSION,
        YIR_OPT_COPY_PROPAGATION,
        YIR_OPT_ALGEBRAIC_SIMPLIFY,
        YIR_OPT_DEAD_CODE
    };
    return yir_run_opt_pipeline(function, default_pipeline,
        sizeof(default_pipeline) / sizeof(default_pipeline[0]),
        error, error_capacity);
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
