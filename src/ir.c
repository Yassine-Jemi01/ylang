#include "ylang/ir.h"

#include <stdio.h>

typedef struct { uint32_t id; YIRType type; } LocalValue;

static bool fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) (void)snprintf(error, capacity, "%s", message);
    return false;
}

static const YIRBlock *find_block(const YIRFunction *function, uint32_t id)
{
    for (size_t i = 0; i < function->block_count; ++i)
        if (function->blocks[i].id == id) return &function->blocks[i];
    return NULL;
}

static bool find_value(const LocalValue *values, size_t count, uint32_t id, YIRType *type)
{
    for (size_t i = 0; i < count; ++i) {
        if (values[i].id == id) { if (type) *type = values[i].type; return true; }
    }
    return false;
}

static bool is_terminator(YIROpcode opcode)
{
    return opcode == YIR_OP_BRANCH || opcode == YIR_OP_COND_BRANCH || opcode == YIR_OP_RETURN;
}

bool yir_verify_function(const YIRFunction *function, char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!function || !function->blocks || function->block_count == 0)
        return fail(error, error_capacity, "function must contain at least one block");
    if (function->return_type > YIR_TYPE_PTR)
        return fail(error, error_capacity, "function has an invalid return type");

    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        if (!block->instructions || block->instruction_count == 0)
            return fail(error, error_capacity, "basic block must contain instructions");
        for (size_t prior = 0; prior < bi; ++prior)
            if (function->blocks[prior].id == block->id)
                return fail(error, error_capacity, "basic block IDs must be unique");

        /* SSA identifiers are function-wide, even while cross-block uses are
         * deliberately forbidden by this initial IR contract. */
        for (size_t prior_block = 0; prior_block < bi; ++prior_block) {
            const YIRBlock *prior = &function->blocks[prior_block];
            for (size_t prior_instruction = 0;
                 prior_instruction < prior->instruction_count; ++prior_instruction) {
                uint32_t prior_id = prior->instructions[prior_instruction].result_id;
                if (!prior_id) continue;
                for (size_t current_instruction = 0;
                     current_instruction < block->instruction_count; ++current_instruction) {
                    if (block->instructions[current_instruction].result_id == prior_id)
                        return fail(error, error_capacity,
                                    "SSA result IDs must be unique across the function");
                }
            }
        }

        LocalValue values[4096];
        size_t value_count = 0;
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *instruction = &block->instructions[ii];
            bool last = ii + 1 == block->instruction_count;
            YIRType lhs_type = YIR_TYPE_VOID, rhs_type = YIR_TYPE_VOID;
            if (is_terminator(instruction->opcode) != last)
                return fail(error, error_capacity, "each block must end with exactly one terminator");
            if (is_terminator(instruction->opcode) && instruction->result_id != 0)
                return fail(error, error_capacity, "terminators cannot define SSA values");

            switch (instruction->opcode) {
                case YIR_OP_CONST_I64:
                    if (!instruction->result_id || instruction->result_type != YIR_TYPE_I64)
                        return fail(error, error_capacity, "const.i64 requires a nonzero i64 result");
                    break;
                case YIR_OP_ADD_I64: case YIR_OP_SUB_I64: case YIR_OP_MUL_I64:
                    if (!instruction->result_id || instruction->result_type != YIR_TYPE_I64 ||
                        !find_value(values, value_count, instruction->lhs, &lhs_type) ||
                        !find_value(values, value_count, instruction->rhs, &rhs_type) ||
                        lhs_type != YIR_TYPE_I64 || rhs_type != YIR_TYPE_I64)
                        return fail(error, error_capacity, "integer arithmetic requires earlier i64 operands and an i64 result");
                    break;
                case YIR_OP_EQ_I64:
                    if (!instruction->result_id || instruction->result_type != YIR_TYPE_BOOL ||
                        !find_value(values, value_count, instruction->lhs, &lhs_type) ||
                        !find_value(values, value_count, instruction->rhs, &rhs_type) ||
                        lhs_type != YIR_TYPE_I64 || rhs_type != YIR_TYPE_I64)
                        return fail(error, error_capacity, "eq.i64 requires earlier i64 operands and a bool result");
                    break;
                case YIR_OP_BRANCH:
                    if (!find_block(function, instruction->target))
                        return fail(error, error_capacity, "branch target does not exist");
                    break;
                case YIR_OP_COND_BRANCH:
                    if (!find_value(values, value_count, instruction->lhs, &lhs_type) || lhs_type != YIR_TYPE_BOOL ||
                        !find_block(function, instruction->target) || !find_block(function, instruction->false_target))
                        return fail(error, error_capacity, "conditional branch requires an earlier bool and two valid targets");
                    break;
                case YIR_OP_RETURN:
                    if (function->return_type == YIR_TYPE_VOID) {
                        if (instruction->lhs != 0)
                            return fail(error, error_capacity, "void return cannot carry a value");
                    } else if (!instruction->lhs ||
                               !find_value(values, value_count, instruction->lhs, &lhs_type) ||
                               lhs_type != function->return_type) {
                        return fail(error, error_capacity, "return value is missing or has the wrong type");
                    }
                    break;
                default:
                    return fail(error, error_capacity, "unknown IR opcode");
            }

            if (instruction->result_id) {
                for (size_t vi = 0; vi < value_count; ++vi)
                    if (values[vi].id == instruction->result_id)
                        return fail(error, error_capacity, "SSA result IDs must be unique within a block");
                if (value_count == sizeof(values) / sizeof(values[0]))
                    return fail(error, error_capacity, "basic block exceeds verifier limit (4096 values)");
                values[value_count++] = (LocalValue){instruction->result_id, instruction->result_type};
            }
        }
    }
    return true;
}
