#include "ylang/ir.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t id;
    YIRType type;
    size_t block_index;
    size_t instruction_index;
    bool is_parameter;
} ValueDef;

static bool fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) (void)snprintf(error, capacity, "%s", message);
    return false;
}

static const YIRBlock *find_block(const YIRFunction *function, uint32_t id,
                                  size_t *index)
{
    for (size_t i = 0; i < function->block_count; ++i) {
        if (function->blocks[i].id == id) {
            if (index) *index = i;
            return &function->blocks[i];
        }
    }
    return NULL;
}

static bool is_terminator(YIROpcode opcode)
{
    return opcode == YIR_OP_BRANCH || opcode == YIR_OP_COND_BRANCH ||
           opcode == YIR_OP_RETURN;
}

static bool valid_type(YIRType type)
{
    return (int)type >= (int)YIR_TYPE_VOID &&
           (int)type <= (int)YIR_TYPE_PTR;
}

static const ValueDef *find_value(const ValueDef *values, size_t count,
                                  uint32_t id)
{
    for (size_t i = 0; i < count; ++i)
        if (values[i].id == id) return &values[i];
    return NULL;
}

static bool check_operand(const ValueDef *values, size_t value_count,
                          const bool *dominators, size_t block_count,
                          size_t use_block, size_t use_instruction,
                          uint32_t id, YIRType expected)
{
    const ValueDef *definition = find_value(values, value_count, id);
    if (!definition || definition->type != expected ||
        definition->block_index >= block_count ||
        !dominators[use_block * block_count + definition->block_index])
        return false;
    return definition->block_index != use_block ||
           definition->is_parameter ||
           definition->instruction_index < use_instruction;
}

bool yir_verify_function(const YIRFunction *function, char *error,
                         size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!function || !function->blocks || function->block_count == 0)
        return fail(error, error_capacity,
                    "function must contain at least one block");
    if (!valid_type(function->return_type))
        return fail(error, error_capacity, "function has an invalid return type");

    const size_t block_count = function->block_count;
    if (block_count > SIZE_MAX / block_count)
        return fail(error, error_capacity, "function has too many basic blocks");

    if (function->parameter_count && !function->parameters)
        return fail(error, error_capacity,
                    "function parameters require a parameter array");

    size_t instruction_capacity = 0;
    size_t parameter_capacity = function->parameter_count;
    for (size_t bi = 0; bi < block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        if (!block->instructions || block->instruction_count == 0)
            return fail(error, error_capacity,
                        "basic block must contain instructions");
        if (block->instruction_count > SIZE_MAX - instruction_capacity ||
            block->parameter_count > SIZE_MAX - parameter_capacity)
            return fail(error, error_capacity,
                        "function has too many IR values");
        if (block->parameter_count && !block->parameters)
            return fail(error, error_capacity,
                        "block parameters require a parameter array");
        if (bi == 0 && block->parameter_count)
            return fail(error, error_capacity,
                        "entry block parameters are unsupported; use function parameters instead");
        instruction_capacity += block->instruction_count;
        parameter_capacity += block->parameter_count;
        for (size_t prior = 0; prior < bi; ++prior)
            if (function->blocks[prior].id == block->id)
                return fail(error, error_capacity,
                            "basic block IDs must be unique");
    }

    if (parameter_capacity > SIZE_MAX - instruction_capacity)
        return fail(error, error_capacity, "function has too many IR values");
    const size_t value_capacity = instruction_capacity + parameter_capacity;
    ValueDef *values = calloc(value_capacity, sizeof(*values));
    bool *edges = calloc(block_count * block_count, sizeof(*edges));
    bool *reachable = calloc(block_count, sizeof(*reachable));
    bool *dominators = calloc(block_count * block_count, sizeof(*dominators));
    size_t *queue = calloc(block_count, sizeof(*queue));
    if (!values || !edges || !reachable || !dominators || !queue) {
        free(values); free(edges); free(reachable); free(dominators); free(queue);
        return fail(error, error_capacity, "out of memory while verifying IR");
    }

    bool ok = false;
    size_t value_count = 0;
    for (size_t pi = 0; pi < function->parameter_count; ++pi) {
        const YIRFunctionParameter *parameter = &function->parameters[pi];
        if (!parameter->id || !valid_type(parameter->type) ||
            parameter->type == YIR_TYPE_VOID ||
            find_value(values, value_count, parameter->id)) {
            (void)fail(error, error_capacity,
                       "function parameters require unique IDs and valid non-void types");
            goto cleanup;
        }
        values[value_count++] = (ValueDef){
            parameter->id, parameter->type, 0, 0, true
        };
    }
    for (size_t bi = 0; bi < block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        for (size_t pi = 0; pi < block->parameter_count; ++pi) {
            const YIRBlockParameter *parameter = &block->parameters[pi];
            if (!parameter->id || !valid_type(parameter->type) ||
                parameter->type == YIR_TYPE_VOID ||
                find_value(values, value_count, parameter->id)) {
                (void)fail(error, error_capacity,
                           "block parameters require unique IDs and valid non-void types");
                goto cleanup;
            }
            values[value_count++] = (ValueDef){
                parameter->id, parameter->type, bi, 0, true
            };
        }
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *ins = &block->instructions[ii];
            bool last = ii + 1 == block->instruction_count;
            if ((ins->source_line == 0) != (ins->source_column == 0)) {
                (void)fail(error, error_capacity,
                           "IR source line and column must both be present or absent");
                goto cleanup;
            }
            if (is_terminator(ins->opcode) != last) {
                (void)fail(error, error_capacity,
                           "each block must end with exactly one terminator");
                goto cleanup;
            }
            if (is_terminator(ins->opcode) && ins->result_id != 0) {
                (void)fail(error, error_capacity,
                           "terminators cannot define SSA values");
                goto cleanup;
            }
            if (!is_terminator(ins->opcode) && ins->result_id != 0) {
                if (!valid_type(ins->result_type) ||
                    ins->result_type == YIR_TYPE_VOID) {
                    (void)fail(error, error_capacity,
                               "SSA results must have a valid non-void type");
                    goto cleanup;
                }
                if (find_value(values, value_count, ins->result_id)) {
                    (void)fail(error, error_capacity,
                               "SSA result IDs must be unique across the function");
                    goto cleanup;
                }
                values[value_count++] = (ValueDef){
                    ins->result_id, ins->result_type, bi, ii, false
                };
            }
        }
    }

    /* Build the control-flow graph and reject missing destinations early. */
    for (size_t bi = 0; bi < block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        const YIRInstruction *term =
            &block->instructions[block->instruction_count - 1];
        if (term->opcode == YIR_OP_BRANCH ||
            term->opcode == YIR_OP_COND_BRANCH) {
            size_t target = 0;
            if (!find_block(function, term->target, &target)) {
                (void)fail(error, error_capacity,
                           "branch target does not exist");
                goto cleanup;
            }
            edges[bi * block_count + target] = true;
        }
        if (term->opcode == YIR_OP_COND_BRANCH) {
            size_t target = 0;
            if (!find_block(function, term->false_target, &target)) {
                (void)fail(error, error_capacity,
                           "conditional branch target does not exist");
                goto cleanup;
            }
            edges[bi * block_count + target] = true;
        }
        if (term->opcode != YIR_OP_BRANCH &&
            term->opcode != YIR_OP_COND_BRANCH &&
            (term->argument_count || term->false_argument_count ||
             term->arguments || term->false_arguments)) {
            (void)fail(error, error_capacity,
                       "only branch terminators may carry block arguments");
            goto cleanup;
        }
        if ((term->argument_count && !term->arguments) ||
            (term->false_argument_count && !term->false_arguments)) {
            (void)fail(error, error_capacity,
                       "branch argument counts require argument arrays");
            goto cleanup;
        }
        if (term->opcode != YIR_OP_COND_BRANCH &&
            (term->false_argument_count || term->false_arguments)) {
            (void)fail(error, error_capacity,
                       "unconditional branches cannot carry false-edge arguments");
            goto cleanup;
        }
    }

    /* The first block is the function entry. Every block must be reachable. */
    size_t head = 0, tail = 0;
    reachable[0] = true;
    queue[tail++] = 0;
    while (head < tail) {
        size_t from = queue[head++];
        for (size_t to = 0; to < block_count; ++to) {
            if (edges[from * block_count + to] && !reachable[to]) {
                reachable[to] = true;
                queue[tail++] = to;
            }
        }
    }
    for (size_t bi = 0; bi < block_count; ++bi) {
        if (!reachable[bi]) {
            (void)fail(error, error_capacity,
                       "all basic blocks must be reachable from the entry block");
            goto cleanup;
        }
    }

    /* Iterative classical dominator computation over the reachable CFG. */
    for (size_t bi = 0; bi < block_count; ++bi)
        for (size_t d = 0; d < block_count; ++d)
            dominators[bi * block_count + d] = bi == 0 ? d == 0 : true;

    bool changed;
    do {
        changed = false;
        for (size_t bi = 1; bi < block_count; ++bi) {
            bool *row = &dominators[bi * block_count];
            for (size_t d = 0; d < block_count; ++d) {
                bool next = true;
                bool has_predecessor = false;
                for (size_t pred = 0; pred < block_count; ++pred) {
                    if (!edges[pred * block_count + bi]) continue;
                    has_predecessor = true;
                    next = next && dominators[pred * block_count + d];
                }
                next = has_predecessor && (d == bi || next);
                if (row[d] != next) {
                    row[d] = next;
                    changed = true;
                }
            }
        }
    } while (changed);

    /* Validate operation contracts and SSA use dominance. */
    for (size_t bi = 0; bi < block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *ins = &block->instructions[ii];
            bool operands_ok = true;
            switch (ins->opcode) {
                case YIR_OP_CONST_I64:
                    operands_ok = ins->result_id != 0 &&
                                  ins->result_type == YIR_TYPE_I64;
                    break;
                case YIR_OP_ADD_I64:
                case YIR_OP_SUB_I64:
                case YIR_OP_MUL_I64:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_I64 &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->lhs,
                                      YIR_TYPE_I64) &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->rhs,
                                      YIR_TYPE_I64);
                    break;
                case YIR_OP_EQ_I64:
                case YIR_OP_NE_I64:
                case YIR_OP_LT_I64:
                case YIR_OP_LE_I64:
                case YIR_OP_GT_I64:
                case YIR_OP_GE_I64:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_BOOL &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->lhs,
                                      YIR_TYPE_I64) &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->rhs,
                                      YIR_TYPE_I64);
                    break;
                case YIR_OP_CONST_BOOL:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_BOOL &&
                        (ins->immediate == 0 || ins->immediate == 1);
                    break;
                case YIR_OP_BRANCH:
                    operands_ok = true;
                    break;
                case YIR_OP_COND_BRANCH:
                    operands_ok = check_operand(values, value_count,
                        dominators, block_count, bi, ii, ins->lhs,
                        YIR_TYPE_BOOL);
                    break;
                case YIR_OP_RETURN:
                    if (function->return_type == YIR_TYPE_VOID) {
                        operands_ok = ins->lhs == 0;
                    } else {
                        operands_ok = check_operand(values, value_count,
                            dominators, block_count, bi, ii, ins->lhs,
                            function->return_type);
                    }
                    break;
                default:
                    operands_ok = false;
                    break;
            }
            if (!operands_ok) {
                (void)fail(error, error_capacity,
                    "invalid opcode, result, operand type, definition order, or SSA dominance");
                goto cleanup;
            }
        }

        const YIRInstruction *term =
            &block->instructions[block->instruction_count - 1];
        if (term->opcode == YIR_OP_BRANCH ||
            term->opcode == YIR_OP_COND_BRANCH) {
            size_t target_index = 0;
            const YIRBlock *target = find_block(function, term->target,
                                                &target_index);
            if (!target || term->argument_count != target->parameter_count) {
                (void)fail(error, error_capacity,
                           "branch argument count does not match target block parameters");
                goto cleanup;
            }
            for (size_t ai = 0; ai < target->parameter_count; ++ai) {
                const ValueDef *argument =
                    find_value(values, value_count, term->arguments[ai]);
                if (!argument || argument->type != target->parameters[ai].type ||
                    !check_operand(values, value_count, dominators,
                                   block_count, bi,
                                   block->instruction_count - 1,
                                   term->arguments[ai], target->parameters[ai].type)) {
                    (void)fail(error, error_capacity,
                               "branch argument type or SSA dominance mismatch");
                    goto cleanup;
                }
            }
            if (term->opcode == YIR_OP_COND_BRANCH) {
                target = find_block(function, term->false_target,
                                    &target_index);
                if (!target ||
                    term->false_argument_count != target->parameter_count) {
                    (void)fail(error, error_capacity,
                               "false branch argument count does not match target block parameters");
                    goto cleanup;
                }
                for (size_t ai = 0; ai < target->parameter_count; ++ai) {
                    const ValueDef *argument =
                        find_value(values, value_count,
                                   term->false_arguments[ai]);
                    if (!argument ||
                        argument->type != target->parameters[ai].type ||
                        !check_operand(values, value_count, dominators,
                                       block_count, bi,
                                       block->instruction_count - 1,
                                       term->false_arguments[ai],
                                       target->parameters[ai].type)) {
                        (void)fail(error, error_capacity,
                                   "false branch argument type or SSA dominance mismatch");
                        goto cleanup;
                    }
                }
            }
        }
    }

    ok = true;
cleanup:
    free(values); free(edges); free(reachable); free(dominators); free(queue);
    return ok;
}
