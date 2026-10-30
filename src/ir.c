#include "ylang/ir.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool defined;
    int32_t block_index; /* -1 is a function parameter. */
    size_t instruction_index;
    YLIRType type;
} YLIRDefinition;

static char *copy_text(const char *text)
{
    if (!text) return NULL;
    size_t length = strlen(text);
    if (length == SIZE_MAX) return NULL;
    char *copy = malloc(length + 1U);
    if (copy) memcpy(copy, text, length + 1U);
    return copy;
}

static bool grow_vector(void **items, size_t *capacity, size_t size)
{
    if (!items || !capacity || size == 0U) return false;
    size_t next = *capacity ? *capacity * 2U : 8U;
    if (next < *capacity || next > SIZE_MAX / size) return false;
    void *grown = realloc(*items, next * size);
    if (!grown) return false;
    *items = grown;
    *capacity = next;
    return true;
}

static bool valid_type(YLIRType type, bool allow_void)
{
    switch (type) {
        case YLIR_TYPE_BOOL: case YLIR_TYPE_I8: case YLIR_TYPE_I64:
        case YLIR_TYPE_F64: case YLIR_TYPE_PTR: return true;
        case YLIR_TYPE_VOID: return allow_void;
        case YLIR_TYPE_INVALID: return false;
    }
    return false;
}

const char *ylang_ir_type_name(YLIRType type)
{
    switch (type) {
        case YLIR_TYPE_INVALID: return "invalid";
        case YLIR_TYPE_VOID: return "void";
        case YLIR_TYPE_BOOL: return "bool";
        case YLIR_TYPE_I8: return "i8";
        case YLIR_TYPE_I64: return "i64";
        case YLIR_TYPE_F64: return "f64";
        case YLIR_TYPE_PTR: return "ptr";
    }
    return "unknown";
}

const char *ylang_ir_instruction_name(YLIRInstructionKind op)
{
    switch (op) {
        case YLIR_OP_CONST_BOOL: return "const.bool";
        case YLIR_OP_CONST_I64: return "const.i64";
        case YLIR_OP_CONST_F64: return "const.f64";
        case YLIR_OP_ADD_I64: return "add.i64";
        case YLIR_OP_SUB_I64: return "sub.i64";
        case YLIR_OP_MUL_I64: return "mul.i64";
        case YLIR_OP_SDIV_I64: return "sdiv.i64";
        case YLIR_OP_SREM_I64: return "srem.i64";
        case YLIR_OP_EQ_I64: return "eq.i64";
        case YLIR_OP_NE_I64: return "ne.i64";
        case YLIR_OP_SLT_I64: return "slt.i64";
        case YLIR_OP_SLE_I64: return "sle.i64";
        case YLIR_OP_SGT_I64: return "sgt.i64";
        case YLIR_OP_SGE_I64: return "sge.i64";
        case YLIR_OP_NOT_BOOL: return "not.bool";
        case YLIR_OP_ADD_F64: return "add.f64";
        case YLIR_OP_SUB_F64: return "sub.f64";
        case YLIR_OP_MUL_F64: return "mul.f64";
        case YLIR_OP_DIV_F64: return "div.f64";
    }
    return "unknown";
}

const char *ylang_ir_terminator_name(YLIRTerminatorKind term)
{
    switch (term) {
        case YLIR_TERM_NONE: return "none";
        case YLIR_TERM_RETURN: return "return";
        case YLIR_TERM_RETURN_VOID: return "return.void";
        case YLIR_TERM_BRANCH: return "branch";
        case YLIR_TERM_COND_BRANCH: return "cond.branch";
        case YLIR_TERM_UNREACHABLE: return "unreachable";
    }
    return "unknown";
}

bool ylang_ir_module_init(YLIRModule *module)
{
    if (!module) return false;
    memset(module, 0, sizeof(*module));
    return true;
}

void ylang_ir_module_destroy(YLIRModule *module)
{
    if (!module) return;
    for (size_t i = 0; i < module->function_count; i++) {
        YLIRFunction *fn = module->functions[i];
        if (!fn) continue;
        for (size_t j = 0; j < fn->block_count; j++) {
            YLIRBasicBlock *block = fn->blocks[j];
            if (!block) continue;
            free(block->name);
            free(block->instructions);
            free(block);
        }
        free(fn->blocks);
        free(fn->parameter_types);
        free(fn->name);
        free(fn);
    }
    free(module->functions);
    memset(module, 0, sizeof(*module));
}

bool ylang_ir_add_function(YLIRModule *module, const char *name,
                           YLIRType result_type, const YLIRType *params,
                           size_t param_count, YLIRFunction **out_fn)
{
    if (out_fn) *out_fn = NULL;
    if (!module || !name || !*name || !out_fn ||
        param_count >= (size_t)UINT32_MAX ||
        (param_count && !params) || !valid_type(result_type, true)) return false;
    for (size_t i = 0; i < module->function_count; i++)
        if (module->functions[i] && strcmp(module->functions[i]->name, name) == 0)
            return false;
    for (size_t i = 0; i < param_count; i++)
        if (!valid_type(params[i], false)) return false;
    if (module->function_count == module->function_capacity &&
        !grow_vector((void **)&module->functions, &module->function_capacity,
                     sizeof(*module->functions))) return false;

    YLIRFunction *fn = calloc(1U, sizeof(*fn));
    if (!fn) return false;
    fn->name = copy_text(name);
    if (!fn->name) { free(fn); return false; }
    if (param_count) {
        if (param_count > SIZE_MAX / sizeof(*fn->parameter_types)) {
            free(fn->name); free(fn); return false;
        }
        fn->parameter_types = malloc(param_count * sizeof(*fn->parameter_types));
        if (!fn->parameter_types) { free(fn->name); free(fn); return false; }
        memcpy(fn->parameter_types, params, param_count * sizeof(*params));
    }
    fn->module = module;
    fn->return_type = result_type;
    fn->parameter_count = param_count;
    fn->next_value_id = (YLIRValue)param_count;
    module->functions[module->function_count++] = fn;
    *out_fn = fn;
    return true;
}

bool ylang_ir_add_block(YLIRFunction *fn, const char *name, YLIRBasicBlock **out)
{
    if (out) *out = NULL;
    if (!fn || !name || !*name || !out ||
        fn->block_count >= (size_t)INT32_MAX) return false;
    for (size_t i = 0; i < fn->block_count; i++)
        if (fn->blocks[i] && strcmp(fn->blocks[i]->name, name) == 0) return false;
    if (fn->block_count == fn->block_capacity &&
        !grow_vector((void **)&fn->blocks, &fn->block_capacity,
                     sizeof(*fn->blocks))) return false;
    YLIRBasicBlock *block = calloc(1U, sizeof(*block));
    if (!block) return false;
    block->name = copy_text(name);
    if (!block->name) { free(block); return false; }
    block->function = fn;
    fn->blocks[fn->block_count++] = block;
    *out = block;
    return true;
}

static bool append_inst(YLIRBasicBlock *block, YLIRInstruction inst, YLIRValue *out)
{
    if (out) *out = YLIR_NO_VALUE;
    if (!block || !block->function || !out ||
        block->terminator.kind != YLIR_TERM_NONE ||
        block->function->next_value_id == YLIR_NO_VALUE) return false;
    if (block->instruction_count == block->instruction_capacity &&
        !grow_vector((void **)&block->instructions, &block->instruction_capacity,
                     sizeof(*block->instructions))) return false;
    inst.result = block->function->next_value_id++;
    block->instructions[block->instruction_count++] = inst;
    *out = inst.result;
    return true;
}

bool ylang_ir_emit_const_bool(YLIRBasicBlock *block, bool value,
                              size_t line, size_t column, YLIRValue *out)
{
    YLIRInstruction inst = {
        .kind = YLIR_OP_CONST_BOOL, .result = YLIR_NO_VALUE,
        .type = YLIR_TYPE_BOOL, .left = YLIR_NO_VALUE, .right = YLIR_NO_VALUE,
        .immediate = value ? 1U : 0U, .source_line = line, .source_column = column
    };
    return append_inst(block, inst, out);
}

bool ylang_ir_emit_const_i64(YLIRBasicBlock *block, int64_t value,
                             size_t line, size_t column, YLIRValue *out)
{
    YLIRInstruction inst = {
        .kind = YLIR_OP_CONST_I64, .result = YLIR_NO_VALUE,
        .type = YLIR_TYPE_I64, .left = YLIR_NO_VALUE, .right = YLIR_NO_VALUE,
        .immediate = (uint64_t)value, .source_line = line, .source_column = column
    };
    return append_inst(block, inst, out);
}

bool ylang_ir_emit_const_f64(YLIRBasicBlock *block, double value,
                             size_t line, size_t column, YLIRValue *out)
{
    uint64_t bits = 0U;
    if (sizeof(value) != sizeof(bits)) return false;
    memcpy(&bits, &value, sizeof(bits));
    YLIRInstruction inst = {
        .kind = YLIR_OP_CONST_F64, .result = YLIR_NO_VALUE,
        .type = YLIR_TYPE_F64, .left = YLIR_NO_VALUE, .right = YLIR_NO_VALUE,
        .immediate = bits, .source_line = line, .source_column = column
    };
    return append_inst(block, inst, out);
}

bool ylang_ir_emit_op(YLIRBasicBlock *block, YLIRInstructionKind kind,
                      YLIRValue left, YLIRValue right,
                      size_t line, size_t column, YLIRValue *out)
{
    if (kind < YLIR_OP_ADD_I64 || kind > YLIR_OP_DIV_F64) return false;
    YLIRInstruction inst = {
        .kind = kind, .result = YLIR_NO_VALUE, .type = YLIR_TYPE_INVALID,
        .left = left, .right = right, .source_line = line, .source_column = column
    };
    return append_inst(block, inst, out);
}

static bool set_term(YLIRBasicBlock *block, YLIRTerminator term)
{
    if (!block || !block->function || block->terminator.kind != YLIR_TERM_NONE)
        return false;
    block->terminator = term;
    return true;
}

bool ylang_ir_return(YLIRBasicBlock *block, YLIRValue value,
                     size_t line, size_t column)
{
    YLIRTerminator term = {
        .kind = YLIR_TERM_RETURN, .value = value,
        .source_line = line, .source_column = column
    };
    return set_term(block, term);
}

bool ylang_ir_return_void(YLIRBasicBlock *block, size_t line, size_t column)
{
    YLIRTerminator term = {
        .kind = YLIR_TERM_RETURN_VOID, .value = YLIR_NO_VALUE,
        .source_line = line, .source_column = column
    };
    return set_term(block, term);
}

bool ylang_ir_branch(YLIRBasicBlock *block, size_t target,
                     size_t line, size_t column)
{
    YLIRTerminator term = {
        .kind = YLIR_TERM_BRANCH, .value = YLIR_NO_VALUE,
        .target = target, .source_line = line, .source_column = column
    };
    return set_term(block, term);
}

bool ylang_ir_cond_branch(YLIRBasicBlock *block, YLIRValue condition,
                          size_t yes, size_t no, size_t line, size_t column)
{
    YLIRTerminator term = {
        .kind = YLIR_TERM_COND_BRANCH, .value = condition,
        .target = yes, .false_target = no,
        .source_line = line, .source_column = column
    };
    return set_term(block, term);
}

bool ylang_ir_unreachable(YLIRBasicBlock *block, size_t line, size_t column)
{
    YLIRTerminator term = {
        .kind = YLIR_TERM_UNREACHABLE, .value = YLIR_NO_VALUE,
        .source_line = line, .source_column = column
    };
    return set_term(block, term);
}

static bool ir_error(FILE *out, const YLIRFunction *fn,
                     const YLIRBasicBlock *block, size_t line,
                     size_t column, const char *message)
{
    if (!out) out = stderr;
    fputs("YLang IR verification error", out);
    if (fn && fn->name) fprintf(out, " in function '%s'", fn->name);
    if (block && block->name) fprintf(out, ", block '%s'", block->name);
    if (line) fprintf(out, " at %zu:%zu", line, column);
    fprintf(out, ": %s
", message);
    return false;
}

static bool expect_value(FILE *out, const YLIRFunction *fn,
                         const YLIRBasicBlock *block, size_t block_index,
                         const YLIRDefinition *defs, YLIRValue value,
                         YLIRType type, size_t order, size_t line, size_t column)
{
    if (value == YLIR_NO_VALUE || value >= fn->next_value_id)
        return ir_error(out, fn, block, line, column, "invalid SSA value ID");
    const YLIRDefinition *def = &defs[value];
    if (!def->defined)
        return ir_error(out, fn, block, line, column, "value used before definition");
    if (def->block_index >= 0 &&
        ((size_t)def->block_index != block_index || def->instruction_index >= order))
        return ir_error(out, fn, block, line, column,
                        "cross-block values and forward references require PHI nodes");
    if (def->type != type)
        return ir_error(out, fn, block, line, column, "operand type mismatch");
    return true;
}

static bool verify_inst(FILE *out, const YLIRFunction *fn,
                        const YLIRBasicBlock *block, size_t block_index,
                        YLIRDefinition *defs, const YLIRInstruction *inst,
                        size_t order)
{
    YLIRType operand_type = YLIR_TYPE_INVALID, result_type = YLIR_TYPE_INVALID;
    bool unary = false, constant = false;
    switch (inst->kind) {
        case YLIR_OP_CONST_BOOL:
            result_type = YLIR_TYPE_BOOL; constant = true;
            if (inst->immediate > 1U)
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "boolean constants must be zero or one");
            break;
        case YLIR_OP_CONST_I64: result_type = YLIR_TYPE_I64; constant = true; break;
        case YLIR_OP_CONST_F64: result_type = YLIR_TYPE_F64; constant = true; break;
        case YLIR_OP_ADD_I64: case YLIR_OP_SUB_I64: case YLIR_OP_MUL_I64:
        case YLIR_OP_SDIV_I64: case YLIR_OP_SREM_I64:
            operand_type = YLIR_TYPE_I64; result_type = YLIR_TYPE_I64; break;
        case YLIR_OP_EQ_I64: case YLIR_OP_NE_I64: case YLIR_OP_SLT_I64:
        case YLIR_OP_SLE_I64: case YLIR_OP_SGT_I64: case YLIR_OP_SGE_I64:
            operand_type = YLIR_TYPE_I64; result_type = YLIR_TYPE_BOOL; break;
        case YLIR_OP_NOT_BOOL:
            operand_type = YLIR_TYPE_BOOL; result_type = YLIR_TYPE_BOOL;
            unary = true; break;
        case YLIR_OP_ADD_F64: case YLIR_OP_SUB_F64:
        case YLIR_OP_MUL_F64: case YLIR_OP_DIV_F64:
            operand_type = YLIR_TYPE_F64; result_type = YLIR_TYPE_F64; break;
        default:
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "unknown instruction opcode");
    }
    if (inst->type != result_type)
        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                        "instruction result type does not match opcode");
    if (constant) {
        if (inst->left != YLIR_NO_VALUE || inst->right != YLIR_NO_VALUE)
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "constant instruction unexpectedly has operands");
    } else {
        if (!expect_value(out, fn, block, block_index, defs, inst->left,
                          operand_type, order, inst->source_line, inst->source_column))
            return false;
        if (unary) {
            if (inst->right != YLIR_NO_VALUE)
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "unary instruction has an unexpected second operand");
        } else if (!expect_value(out, fn, block, block_index, defs, inst->right,
                                 operand_type, order, inst->source_line, inst->source_column)) {
            return false;
        }
    }
    if (inst->result == YLIR_NO_VALUE || inst->result >= fn->next_value_id ||
        inst->result < fn->parameter_count || defs[inst->result].defined)
        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                        "invalid or duplicate instruction result ID");
    defs[inst->result] = (YLIRDefinition){
        .defined = true, .block_index = (int32_t)block_index,
        .instruction_index = order, .type = result_type
    };
    return true;
}

static bool verify_term(FILE *out, const YLIRFunction *fn,
                        const YLIRBasicBlock *block, size_t block_index,
                        const YLIRDefinition *defs)
{
    const YLIRTerminator *term = &block->terminator;
    switch (term->kind) {
        case YLIR_TERM_RETURN:
            if (fn->return_type == YLIR_TYPE_VOID)
                return ir_error(out, fn, block, term->source_line, term->source_column,
                                "void function must use return.void");
            return expect_value(out, fn, block, block_index, defs, term->value,
                                fn->return_type, block->instruction_count,
                                term->source_line, term->source_column);
        case YLIR_TERM_RETURN_VOID:
            if (fn->return_type != YLIR_TYPE_VOID)
                return ir_error(out, fn, block, term->source_line, term->source_column,
                                "non-void function must return a value");
            return true;
        case YLIR_TERM_BRANCH:
            if (term->target >= fn->block_count)
                return ir_error(out, fn, block, term->source_line, term->source_column,
                                "branch target block does not exist");
            return true;
        case YLIR_TERM_COND_BRANCH:
            if (!expect_value(out, fn, block, block_index, defs, term->value,
                              YLIR_TYPE_BOOL, block->instruction_count,
                              term->source_line, term->source_column)) return false;
            if (term->target >= fn->block_count || term->false_target >= fn->block_count)
                return ir_error(out, fn, block, term->source_line, term->source_column,
                                "conditional branch target block does not exist");
            return true;
        case YLIR_TERM_UNREACHABLE: return true;
        case YLIR_TERM_NONE:
            return ir_error(out, fn, block, term->source_line, term->source_column,
                            "basic block has no terminator");
    }
    return ir_error(out, fn, block, term->source_line, term->source_column,
                    "unknown terminator");
}

static bool verify_function(FILE *out, const YLIRFunction *fn)
{
    if (!fn || !fn->name || !valid_type(fn->return_type, true) ||
        fn->parameter_count >= (size_t)UINT32_MAX ||
        (fn->parameter_count && !fn->parameter_types) ||
        (fn->block_count && !fn->blocks) ||
        fn->block_count > (size_t)INT32_MAX)
        return ir_error(out, fn, NULL, 0U, 0U, "invalid function signature or storage");
    if (!fn->block_count)
        return ir_error(out, fn, NULL, 0U, 0U, "function has no basic blocks");
    if (fn->next_value_id < fn->parameter_count)
        return ir_error(out, fn, NULL, 0U, 0U, "SSA counter precedes parameter IDs");

    size_t count = (size_t)fn->next_value_id;
    if (count > SIZE_MAX / sizeof(YLIRDefinition))
        return ir_error(out, fn, NULL, 0U, 0U, "SSA definition table too large");
    YLIRDefinition *defs = calloc(count, sizeof(*defs));
    if (!defs && count)
        return ir_error(out, fn, NULL, 0U, 0U, "out of memory verifying SSA values");
    for (size_t i = 0; i < fn->parameter_count; i++) {
        if (!valid_type(fn->parameter_types[i], false)) {
            free(defs);
            return ir_error(out, fn, NULL, 0U, 0U, "invalid parameter type");
        }
        defs[i] = (YLIRDefinition){ true, -1, 0U, fn->parameter_types[i] };
    }
    for (size_t b = 0; b < fn->block_count; b++) {
        const YLIRBasicBlock *block = fn->blocks[b];
        if (!block || !block->name || block->function != fn ||
            (block->instruction_count && !block->instructions)) {
            free(defs);
            return ir_error(out, fn, block, 0U, 0U, "invalid basic-block storage or owner");
        }
        for (size_t prior = 0; prior < b; prior++) {
            if (fn->blocks[prior] && strcmp(fn->blocks[prior]->name, block->name) == 0) {
                free(defs);
                return ir_error(out, fn, block, 0U, 0U, "duplicate basic-block name");
            }
        }
        for (size_t i = 0; i < block->instruction_count; i++) {
            if (!verify_inst(out, fn, block, b, defs, &block->instructions[i], i)) {
                free(defs); return false;
            }
        }
        if (!verify_term(out, fn, block, b, defs)) {
            free(defs); return false;
        }
    }
    free(defs);
    return true;
}

bool ylang_ir_verify(const YLIRModule *module, FILE *diagnostics)
{
    if (!module || (module->function_count && !module->functions))
        return ir_error(diagnostics, NULL, NULL, 0U, 0U, "invalid module storage");
    for (size_t i = 0; i < module->function_count; i++) {
        const YLIRFunction *fn = module->functions[i];
        if (!fn || !fn->name)
            return ir_error(diagnostics, fn, NULL, 0U, 0U, "module contains invalid function");
        for (size_t j = 0; j < i; j++) {
            if (!module->functions[j] || !module->functions[j]->name)
                return ir_error(diagnostics, fn, NULL, 0U, 0U, "module contains invalid function");
            if (strcmp(fn->name, module->functions[j]->name) == 0)
                return ir_error(diagnostics, fn, NULL, 0U, 0U, "duplicate function name");
        }
        if (!verify_function(diagnostics, fn)) return false;
    }
    return true;
}
