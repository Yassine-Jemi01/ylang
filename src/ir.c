#include "ylang/ir.h"

#include <math.h>
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

            if (ins->opcode == YIR_OP_CALL) {
                if (!ins->callee || ins->callee[0] == '\0' ||
                    (ins->callee_symbol && ins->callee_symbol[0] == '\0') ||
                    ins->argument_count != ins->call_parameter_count ||
                    (ins->argument_count &&
                     (!ins->arguments || !ins->call_parameter_types)) ||
                    (ins->argument_count == 0 &&
                     (ins->arguments || ins->call_parameter_types)) ||
                    ins->false_argument_count || ins->false_arguments) {
                    (void)fail(error, error_capacity,
                               "call instructions require a callee and matching argument metadata");
                    goto cleanup;
                }
            } else if (ins->callee || ins->call_parameter_types ||
                       ins->call_parameter_count) {
                (void)fail(error, error_capacity,
                           "call metadata is only valid on call instructions");
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
                case YIR_OP_CONST_F64:
                    operands_ok = ins->result_id != 0 &&
                                  ins->result_type == YIR_TYPE_F64 &&
                                  isfinite(ins->floating_immediate);
                    break;
                case YIR_OP_ADD_I64:
                case YIR_OP_SUB_I64:
                case YIR_OP_MUL_I64:
                case YIR_OP_DIV_I64:
                case YIR_OP_MOD_I64:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_I64 &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->lhs,
                                      YIR_TYPE_I64) &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->rhs,
                                      YIR_TYPE_I64);
                    break;
                case YIR_OP_NEG_I64:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_I64 &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->lhs,
                                      YIR_TYPE_I64);
                    break;
                case YIR_OP_ADD_F64:
                case YIR_OP_SUB_F64:
                case YIR_OP_MUL_F64:
                case YIR_OP_DIV_F64:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_F64 &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->lhs,
                                      YIR_TYPE_F64) &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->rhs,
                                      YIR_TYPE_F64);
                    break;
                case YIR_OP_NEG_F64:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_F64 &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->lhs,
                                      YIR_TYPE_F64);
                    break;
                case YIR_OP_EQ_F64:
                case YIR_OP_NE_F64:
                case YIR_OP_LT_F64:
                case YIR_OP_LE_F64:
                case YIR_OP_GT_F64:
                case YIR_OP_GE_F64:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_BOOL &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->lhs,
                                      YIR_TYPE_F64) &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->rhs,
                                      YIR_TYPE_F64);
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
                case YIR_OP_NOT_BOOL:
                    operands_ok = ins->result_id != 0 &&
                        ins->result_type == YIR_TYPE_BOOL &&
                        check_operand(values, value_count, dominators,
                                      block_count, bi, ii, ins->lhs,
                                      YIR_TYPE_BOOL);
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
                case YIR_OP_CALL:
                    operands_ok = valid_type(ins->result_type) &&
                        ((ins->result_type == YIR_TYPE_VOID &&
                          ins->result_id == 0) ||
                         (ins->result_type != YIR_TYPE_VOID &&
                          ins->result_id != 0));
                    for (size_t ai = 0;
                         operands_ok && ai < ins->argument_count; ++ai) {
                        YIRType argument_type = ins->call_parameter_types[ai];
                        operands_ok = valid_type(argument_type) &&
                            argument_type != YIR_TYPE_VOID &&
                            check_operand(values, value_count, dominators,
                                          block_count, bi, ii,
                                          ins->arguments[ai], argument_type);
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

static const char *module_function_symbol(const YIRFunction *function)
{
    if (function->symbol && function->symbol[0] != '\0')
        return function->symbol;
    return function->name;
}

bool yir_verify_module(const YIRFunction *functions, size_t function_count,
                      char *error, size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!functions || function_count == 0)
        return fail(error, error_capacity,
                    "IR module must contain at least one function");

    for (size_t fi = 0; fi < function_count; ++fi) {
        const YIRFunction *function = &functions[fi];
        char function_error[256];
        const char *symbol = module_function_symbol(function);
        if (!function->name || function->name[0] == '\0' ||
            !symbol || symbol[0] == '\0') {
            (void)fail(error, error_capacity,
                       "IR module functions require non-empty names and symbols");
            return false;
        }
        for (size_t prior = 0; prior < fi; ++prior) {
            if (strcmp(module_function_symbol(&functions[prior]), symbol) == 0) {
                if (error && error_capacity)
                    (void)snprintf(error, error_capacity,
                        "IR module contains duplicate function symbol '%s'",
                        symbol);
                return false;
            }
        }
        if (!yir_verify_function(function, function_error,
                                 sizeof(function_error))) {
            if (error && error_capacity)
                (void)snprintf(error, error_capacity,
                    "function '%s' failed verification: %s",
                    function->name,
                    function_error[0] ? function_error : "unknown verifier error");
            return false;
        }
    }

    for (size_t fi = 0; fi < function_count; ++fi) {
        const YIRFunction *caller = &functions[fi];
        for (size_t bi = 0; bi < caller->block_count; ++bi) {
            const YIRBlock *block = &caller->blocks[bi];
            for (size_t ii = 0; ii < block->instruction_count; ++ii) {
                const YIRInstruction *instruction = &block->instructions[ii];
                if (instruction->opcode != YIR_OP_CALL) continue;

                const char *requested_symbol =
                    instruction->callee_symbol && instruction->callee_symbol[0] ?
                    instruction->callee_symbol : instruction->callee;
                const YIRFunction *callee = NULL;
                size_t matches = 0;
                for (size_t target_index = 0;
                     target_index < function_count; ++target_index) {
                    const char *target_symbol =
                        module_function_symbol(&functions[target_index]);
                    if (strcmp(target_symbol, requested_symbol) == 0) {
                        callee = &functions[target_index];
                        matches++;
                    }
                }

                if (matches == 0) {
                    if (error && error_capacity)
                        (void)snprintf(error, error_capacity,
                            "function '%s' calls missing function symbol '%s'",
                            caller->name, requested_symbol);
                    return false;
                }
                if (matches > 1) {
                    if (error && error_capacity)
                        (void)snprintf(error, error_capacity,
                            "function '%s' calls ambiguous function symbol '%s'",
                            caller->name, requested_symbol);
                    return false;
                }

                if (instruction->argument_count != callee->parameter_count ||
                    instruction->call_parameter_count != callee->parameter_count ||
                    instruction->result_type != callee->return_type) {
                    if (error && error_capacity)
                        (void)snprintf(error, error_capacity,
                            "call to '%s' in function '%s' does not match the callee signature",
                            callee->name, caller->name);
                    return false;
                }

                for (size_t pi = 0; pi < callee->parameter_count; ++pi) {
                    if (instruction->call_parameter_types[pi] !=
                        callee->parameters[pi].type) {
                        if (error && error_capacity)
                            (void)snprintf(error, error_capacity,
                                "argument %zu of call to '%s' in function '%s' has the wrong type",
                                pi + 1, callee->name, caller->name);
                        return false;
                    }
                }
            }
        }
    }

    return true;
}
