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
        case YLIR_OP_NEG_I64: return "neg.i64";
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
        case YLIR_OP_NEG_F64: return "neg.f64";
        case YLIR_OP_PHI: return "phi";
        case YLIR_OP_CALL: return "call";
        case YLIR_OP_PRINT_I64: return "print.i64";
        case YLIR_OP_PRINT_F64: return "print.f64";
        case YLIR_OP_PRINT_BOOL: return "print.bool";
        case YLIR_OP_PRINT_SPACE: return "print.space";
        case YLIR_OP_PRINT_NEWLINE: return "print.newline";
        case YLIR_OP_LOAD_GLOBAL: return "load.global";
        case YLIR_OP_STORE_GLOBAL: return "store.global";
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
    for (size_t i = 0; i < module->global_count; i++)
        free(module->globals[i].name);
    free(module->globals);
    for (size_t i = 0; i < module->function_count; i++) {
        YLIRFunction *fn = module->functions[i];
        if (!fn) continue;
        for (size_t j = 0; j < fn->block_count; j++) {
            YLIRBasicBlock *block = fn->blocks[j];
            if (!block) continue;
            free(block->name);
            for (size_t k = 0; k < block->instruction_count; k++) {
                free(block->instructions[k].incoming);
                free(block->instructions[k].arguments);
            }
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

static bool valid_symbol_name(const char *name)
{
    if (!name || !*name) return false;
    unsigned char ch = (unsigned char)name[0];
    if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_'))
        return false;
    for (size_t i = 1; name[i]; i++) {
        ch = (unsigned char)name[i];
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
}

bool ylang_ir_add_global(YLIRModule *module, const char *name, YLIRType type,
                         uint64_t initial_bits, bool is_mutable)
{
    if (!module || !valid_symbol_name(name) || !valid_type(type, false))
        return false;
    for (size_t i = 0; i < module->global_count; i++)
        if (strcmp(module->globals[i].name, name) == 0) return false;
    for (size_t i = 0; i < module->function_count; i++)
        if (strcmp(module->functions[i]->name, name) == 0) return false;
    if (module->global_count == module->global_capacity &&
        !grow_vector((void **)&module->globals, &module->global_capacity,
                     sizeof(*module->globals))) return false;
    char *copy = copy_text(name);
    if (!copy) return false;
    module->globals[module->global_count++] = (YLIRGlobal){
        .name = copy, .type = type, .initial_bits = initial_bits,
        .is_mutable = is_mutable
    };
    return true;
}

bool ylang_ir_emit_load_global(YLIRBasicBlock *block, size_t global_index,
                               size_t line, size_t column, YLIRValue *out)
{
    if (!block || !block->function || !block->function->module ||
        global_index >= block->function->module->global_count) return false;
    YLIRInstruction inst = {
        .kind = YLIR_OP_LOAD_GLOBAL, .result = YLIR_NO_VALUE,
        .type = block->function->module->globals[global_index].type,
        .left = YLIR_NO_VALUE, .right = YLIR_NO_VALUE,
        .immediate = global_index, .source_line = line, .source_column = column
    };
    return append_inst(block, inst, out);
}

bool ylang_ir_emit_store_global(YLIRBasicBlock *block, size_t global_index,
                                YLIRValue value, size_t line, size_t column)
{
    if (!block || !block->function || !block->function->module ||
        global_index >= block->function->module->global_count ||
        !block->function->module->globals[global_index].is_mutable) return false;
    YLIRInstruction inst = {
        .kind = YLIR_OP_STORE_GLOBAL, .result = YLIR_NO_VALUE,
        .type = YLIR_TYPE_VOID, .left = value, .right = YLIR_NO_VALUE,
        .immediate = global_index, .source_line = line, .source_column = column
    };
    return append_void_inst(block, inst);
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

static bool append_void_inst(YLIRBasicBlock *block, YLIRInstruction inst)
{
    if (!block || !block->function ||
        block->terminator.kind != YLIR_TERM_NONE) return false;
    if (block->instruction_count == block->instruction_capacity &&
        !grow_vector((void **)&block->instructions, &block->instruction_capacity,
                     sizeof(*block->instructions))) return false;
    inst.result = YLIR_NO_VALUE;
    block->instructions[block->instruction_count++] = inst;
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
    if (kind < YLIR_OP_ADD_I64 || kind > YLIR_OP_NEG_F64) return false;
    YLIRInstruction inst = {
        .kind = kind, .result = YLIR_NO_VALUE, .type = YLIR_TYPE_INVALID,
        .left = left, .right = right, .source_line = line, .source_column = column
    };
    return append_inst(block, inst, out);
}

bool ylang_ir_emit_phi(YLIRBasicBlock *block, YLIRType type,
                       const YLIRPhiIncoming *incoming, size_t incoming_count,
                       size_t line, size_t column, YLIRValue *out)
{
    if (out) *out = YLIR_NO_VALUE;
    if (!block || !block->function || !valid_type(type, false) ||
        !incoming || incoming_count == 0U ||
        incoming_count > SIZE_MAX / sizeof(*incoming)) return false;
    YLIRPhiIncoming *copy = malloc(incoming_count * sizeof(*copy));
    if (!copy) return false;
    memcpy(copy, incoming, incoming_count * sizeof(*copy));
    YLIRInstruction inst = {
        .kind = YLIR_OP_PHI, .result = YLIR_NO_VALUE, .type = type,
        .left = YLIR_NO_VALUE, .right = YLIR_NO_VALUE,
        .incoming = copy, .incoming_count = incoming_count,
        .source_line = line, .source_column = column
    };
    if (!append_inst(block, inst, out)) {
        free(copy);
        return false;
    }
    return true;
}

bool ylang_ir_emit_call(YLIRBasicBlock *block, size_t callee_index,
                        const YLIRValue *arguments, size_t argument_count,
                        size_t line, size_t column, YLIRValue *out)
{
    if (out) *out = YLIR_NO_VALUE;
    if (!block || !block->function || !block->function->module || !out ||
        callee_index >= block->function->module->function_count ||
        (argument_count > 0U && !arguments) ||
        argument_count > SIZE_MAX / sizeof(*arguments)) return false;
    const YLIRFunction *callee = block->function->module->functions[callee_index];
    if (!callee || argument_count != callee->parameter_count) return false;

    YLIRValue *copy = NULL;
    if (argument_count > 0U) {
        copy = malloc(argument_count * sizeof(*copy));
        if (!copy) return false;
        memcpy(copy, arguments, argument_count * sizeof(*copy));
    }
    YLIRInstruction inst = {
        .kind = YLIR_OP_CALL,
        .result = YLIR_NO_VALUE,
        .type = callee->return_type,
        .left = YLIR_NO_VALUE,
        .right = YLIR_NO_VALUE,
        .callee_index = callee_index,
        .arguments = copy,
        .argument_count = argument_count,
        .source_line = line,
        .source_column = column
    };
    bool okay = callee->return_type == YLIR_TYPE_VOID ?
        append_void_inst(block, inst) : append_inst(block, inst, out);
    if (!okay) free(copy);
    return okay;
}

bool ylang_ir_emit_print(YLIRBasicBlock *block, YLIRType value_type,
                         YLIRValue value, size_t line, size_t column)
{
    YLIRInstructionKind kind;
    switch (value_type) {
        case YLIR_TYPE_I64: kind = YLIR_OP_PRINT_I64; break;
        case YLIR_TYPE_F64: kind = YLIR_OP_PRINT_F64; break;
        case YLIR_TYPE_BOOL: kind = YLIR_OP_PRINT_BOOL; break;
        default: return false;
    }
    YLIRInstruction inst = {
        .kind = kind, .result = YLIR_NO_VALUE, .type = YLIR_TYPE_VOID,
        .left = value, .right = YLIR_NO_VALUE,
        .source_line = line, .source_column = column
    };
    return append_void_inst(block, inst);
}

bool ylang_ir_emit_print_control(YLIRBasicBlock *block, bool newline,
                                 size_t line, size_t column)
{
    YLIRInstruction inst = {
        .kind = newline ? YLIR_OP_PRINT_NEWLINE : YLIR_OP_PRINT_SPACE,
        .result = YLIR_NO_VALUE, .type = YLIR_TYPE_VOID,
        .left = YLIR_NO_VALUE, .right = YLIR_NO_VALUE,
        .source_line = line, .source_column = column
    };
    return append_void_inst(block, inst);
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
                         const YLIRDefinition *defs, const uint8_t *dominators,
                         size_t block_count, YLIRValue value, YLIRType type,
                         size_t order, size_t line, size_t column)
{
    if (value == YLIR_NO_VALUE || value >= fn->next_value_id)
        return ir_error(out, fn, block, line, column, "invalid SSA value ID");
    const YLIRDefinition *def = &defs[value];
    if (!def->defined)
        return ir_error(out, fn, block, line, column, "value used before definition");
    if (def->block_index >= 0) {
        size_t def_block = (size_t)def->block_index;
        if (def_block == block_index) {
            if (def->instruction_index >= order)
                return ir_error(out, fn, block, line, column,
                                "value used before its definition in this block");
        } else if (def_block >= block_count ||
                   !dominators[block_index * block_count + def_block]) {
            return ir_error(out, fn, block, line, column,
                            "value definition does not dominate its use");
        }
    }
    if (def->type != type)
        return ir_error(out, fn, block, line, column, "operand type mismatch");
    return true;
}

static bool result_type_for_instruction(FILE *out, const YLIRFunction *fn,
                                        const YLIRBasicBlock *block,
                                        const YLIRInstruction *inst,
                                        YLIRType *result_type)
{
    switch (inst->kind) {
        case YLIR_OP_CONST_BOOL: *result_type = YLIR_TYPE_BOOL; break;
        case YLIR_OP_CONST_I64: *result_type = YLIR_TYPE_I64; break;
        case YLIR_OP_CONST_F64: *result_type = YLIR_TYPE_F64; break;
        case YLIR_OP_ADD_I64: case YLIR_OP_SUB_I64: case YLIR_OP_MUL_I64:
        case YLIR_OP_SDIV_I64: case YLIR_OP_SREM_I64: case YLIR_OP_NEG_I64:
            *result_type = YLIR_TYPE_I64; break;
        case YLIR_OP_EQ_I64: case YLIR_OP_NE_I64: case YLIR_OP_SLT_I64:
        case YLIR_OP_SLE_I64: case YLIR_OP_SGT_I64: case YLIR_OP_SGE_I64:
        case YLIR_OP_NOT_BOOL:
            *result_type = YLIR_TYPE_BOOL; break;
        case YLIR_OP_ADD_F64: case YLIR_OP_SUB_F64:
        case YLIR_OP_MUL_F64: case YLIR_OP_DIV_F64: case YLIR_OP_NEG_F64:
            *result_type = YLIR_TYPE_F64; break;
        case YLIR_OP_PHI:
            if (!valid_type(inst->type, false))
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "PHI has invalid or void type");
            *result_type = inst->type; break;
        case YLIR_OP_CALL:
            if (!fn->module || inst->callee_index >= fn->module->function_count ||
                !fn->module->functions[inst->callee_index])
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "call references an unknown function");
            *result_type = fn->module->functions[inst->callee_index]->return_type;
            break;
        case YLIR_OP_LOAD_GLOBAL:
            if (!fn->module || inst->immediate >= fn->module->global_count)
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "global load references an unknown global");
            *result_type = fn->module->globals[inst->immediate].type;
            break;
        case YLIR_OP_STORE_GLOBAL:
            if (!fn->module || inst->immediate >= fn->module->global_count ||
                !fn->module->globals[inst->immediate].is_mutable)
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "global store references an unknown or immutable global");
            *result_type = YLIR_TYPE_VOID;
            break;
        case YLIR_OP_PRINT_I64: case YLIR_OP_PRINT_F64:
        case YLIR_OP_PRINT_BOOL: case YLIR_OP_PRINT_SPACE:
        case YLIR_OP_PRINT_NEWLINE:
            *result_type = YLIR_TYPE_VOID;
            break;
        default:
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "unknown instruction opcode");
    }
    if (inst->type != *result_type)
        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                        "instruction result type does not match opcode");
    return true;
}

static bool collect_definition(FILE *out, const YLIRFunction *fn,
                               const YLIRBasicBlock *block, size_t block_index,
                               YLIRDefinition *defs, const YLIRInstruction *inst,
                               size_t order)
{
    YLIRType result_type = YLIR_TYPE_INVALID;
    if (!result_type_for_instruction(out, fn, block, inst, &result_type)) return false;
    if (result_type == YLIR_TYPE_VOID) {
        if ((inst->kind != YLIR_OP_CALL &&
             inst->kind != YLIR_OP_STORE_GLOBAL &&
             inst->kind != YLIR_OP_PRINT_I64 &&
             inst->kind != YLIR_OP_PRINT_F64 &&
             inst->kind != YLIR_OP_PRINT_BOOL &&
             inst->kind != YLIR_OP_PRINT_SPACE &&
             inst->kind != YLIR_OP_PRINT_NEWLINE) ||
            inst->result != YLIR_NO_VALUE)
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "void instruction must not define an SSA result");
        return true;
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

static bool verify_phi(FILE *out, const YLIRFunction *fn,
                       const YLIRBasicBlock *block, size_t block_index,
                       const YLIRDefinition *defs, const uint8_t *preds,
                       const uint8_t *reachable, const uint8_t *dominators,
                       size_t block_count, const YLIRInstruction *inst)
{
    if (!inst->incoming || inst->incoming_count == 0U ||
        inst->left != YLIR_NO_VALUE || inst->right != YLIR_NO_VALUE)
        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                        "malformed PHI incoming list or ordinary operands");
    uint8_t *seen = calloc(block_count, sizeof(*seen));
    if (!seen)
        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                        "out of memory verifying PHI");
    size_t expected = 0U;
    for (size_t p = 0; p < block_count; p++)
        if (reachable[p] && preds[block_index * block_count + p]) expected++;
    if (inst->incoming_count != expected) {
        free(seen);
        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                        "PHI must have one incoming value per reachable predecessor");
    }
    for (size_t i = 0; i < inst->incoming_count; i++) {
        const YLIRPhiIncoming *in = &inst->incoming[i];
        const size_t pred = in->predecessor;
        if (pred >= block_count || !reachable[pred] ||
            !preds[block_index * block_count + pred] || seen[pred]) {
            free(seen);
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "PHI input must name a unique reachable predecessor");
        }
        seen[pred] = 1U;
        if (in->value == YLIR_NO_VALUE || in->value >= fn->next_value_id ||
            !defs[in->value].defined || defs[in->value].type != inst->type) {
            free(seen);
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "PHI input is undefined or has an incompatible type");
        }
        const int32_t defined_in = defs[in->value].block_index;
        if (defined_in >= 0) {
            const size_t def_block = (size_t)defined_in;
            if (def_block == pred) {
                if (defs[in->value].instruction_index >= fn->blocks[pred]->instruction_count) {
                    free(seen);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "PHI input is unavailable at predecessor exit");
                }
            } else if (def_block >= block_count ||
                       !dominators[pred * block_count + def_block]) {
                free(seen);
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "PHI input definition does not dominate its predecessor");
            }
        }
    }
    free(seen);
    return true;
}

static bool verify_instruction_operands(FILE *out, const YLIRFunction *fn,
                                        const YLIRBasicBlock *block, size_t block_index,
                                        const YLIRDefinition *defs,
                                        const uint8_t *dominators, size_t block_count,
                                        const YLIRInstruction *inst, size_t order)
{
    YLIRType operand_type = YLIR_TYPE_INVALID;
    bool constant = false, unary = false, no_operand = false;
    switch (inst->kind) {
        case YLIR_OP_CONST_BOOL:
            constant = true;
            if (inst->immediate > 1U)
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "boolean constants must be zero or one");
            break;
        case YLIR_OP_CONST_I64: case YLIR_OP_CONST_F64:
            constant = true; break;
        case YLIR_OP_ADD_I64: case YLIR_OP_SUB_I64:
        case YLIR_OP_MUL_I64: case YLIR_OP_SDIV_I64:
        case YLIR_OP_SREM_I64: case YLIR_OP_EQ_I64:
        case YLIR_OP_NE_I64: case YLIR_OP_SLT_I64:
        case YLIR_OP_SLE_I64: case YLIR_OP_SGT_I64:
        case YLIR_OP_SGE_I64:
            operand_type = YLIR_TYPE_I64; break;
        case YLIR_OP_NOT_BOOL:
            operand_type = YLIR_TYPE_BOOL; unary = true; break;
        case YLIR_OP_NEG_I64:
            operand_type = YLIR_TYPE_I64; unary = true; break;
        case YLIR_OP_NEG_F64:
            operand_type = YLIR_TYPE_F64; unary = true; break;
        case YLIR_OP_ADD_F64: case YLIR_OP_SUB_F64:
        case YLIR_OP_MUL_F64: case YLIR_OP_DIV_F64:
            operand_type = YLIR_TYPE_F64; break;
        case YLIR_OP_PRINT_I64:
            operand_type = YLIR_TYPE_I64; unary = true; break;
        case YLIR_OP_PRINT_F64:
            operand_type = YLIR_TYPE_F64; unary = true; break;
        case YLIR_OP_PRINT_BOOL:
            operand_type = YLIR_TYPE_BOOL; unary = true; break;
        case YLIR_OP_PRINT_SPACE:
        case YLIR_OP_PRINT_NEWLINE:
            no_operand = true; break;
        case YLIR_OP_LOAD_GLOBAL:
            if (!fn->module || inst->immediate >= fn->module->global_count)
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "global load references an unknown global");
            no_operand = true; break;
        case YLIR_OP_STORE_GLOBAL:
            if (!fn->module || inst->immediate >= fn->module->global_count ||
                !fn->module->globals[inst->immediate].is_mutable)
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "global store references an unknown or immutable global");
            operand_type = fn->module->globals[inst->immediate].type;
            unary = true; break;
        case YLIR_OP_PHI:
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "internal verifier error: PHI reached ordinary operand pass");
        default:
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "unknown instruction opcode");
    }
    if (constant || no_operand) {
        if (inst->left != YLIR_NO_VALUE || inst->right != YLIR_NO_VALUE)
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "constant or no-operand instruction unexpectedly has operands");
        return true;
    }
    if (!expect_value(out, fn, block, block_index, defs, dominators, block_count,
                      inst->left, operand_type, order,
                      inst->source_line, inst->source_column)) return false;
    if (unary) {
        if (inst->right != YLIR_NO_VALUE)
            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                            "unary instruction has an unexpected second operand");
        return true;
    }
    return expect_value(out, fn, block, block_index, defs, dominators, block_count,
                        inst->right, operand_type, order,
                        inst->source_line, inst->source_column);
}

static bool verify_terminator(FILE *out, const YLIRFunction *fn,
                              const YLIRBasicBlock *block, size_t block_index,
                              const YLIRDefinition *defs, const uint8_t *dominators,
                              size_t block_count)
{
    const YLIRTerminator *term = &block->terminator;
    switch (term->kind) {
        case YLIR_TERM_RETURN:
            if (fn->return_type == YLIR_TYPE_VOID)
                return ir_error(out, fn, block, term->source_line, term->source_column,
                                "void function must use return.void");
            return expect_value(out, fn, block, block_index, defs, dominators, block_count,
                                term->value, fn->return_type, block->instruction_count,
                                term->source_line, term->source_column);
        case YLIR_TERM_RETURN_VOID:
            if (fn->return_type != YLIR_TYPE_VOID)
                return ir_error(out, fn, block, term->source_line, term->source_column,
                                "non-void function must return a value");
            return true;
        case YLIR_TERM_BRANCH:
            return term->target < fn->block_count ||
                ir_error(out, fn, block, term->source_line, term->source_column,
                         "branch target block does not exist");
        case YLIR_TERM_COND_BRANCH:
            if (!expect_value(out, fn, block, block_index, defs, dominators, block_count,
                              term->value, YLIR_TYPE_BOOL, block->instruction_count,
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
    const size_t n = fn->block_count;
    if (!n) return ir_error(out, fn, NULL, 0U, 0U, "function has no basic blocks");
    if (fn->next_value_id < fn->parameter_count)
        return ir_error(out, fn, NULL, 0U, 0U, "SSA counter precedes parameter IDs");
    if (n > SIZE_MAX / n || n > SIZE_MAX / sizeof(size_t))
        return ir_error(out, fn, NULL, 0U, 0U, "control-flow graph is too large");
    const size_t matrix_size = n * n;
    const size_t value_count = (size_t)fn->next_value_id;
    if (value_count > SIZE_MAX / sizeof(YLIRDefinition))
        return ir_error(out, fn, NULL, 0U, 0U, "SSA definition table too large");
    YLIRDefinition *defs = calloc(value_count, sizeof(*defs));
    uint8_t *preds = calloc(matrix_size, sizeof(*preds));
    uint8_t *dom = calloc(matrix_size, sizeof(*dom));
    uint8_t *reachable = calloc(n, sizeof(*reachable));
    size_t *queue = malloc(n * sizeof(*queue));
    if ((value_count && !defs) || !preds || !dom || !reachable || !queue) {
        free(defs); free(preds); free(dom); free(reachable); free(queue);
        return ir_error(out, fn, NULL, 0U, 0U, "out of memory verifying function");
    }
    for (size_t p = 0; p < fn->parameter_count; p++) {
        if (!valid_type(fn->parameter_types[p], false)) {
            free(defs); free(preds); free(dom); free(reachable); free(queue);
            return ir_error(out, fn, NULL, 0U, 0U, "invalid parameter type");
        }
        defs[p] = (YLIRDefinition){ true, -1, 0U, fn->parameter_types[p] };
    }

    /* Verify block storage and collect predecessor edges from each terminator. */
    for (size_t b = 0; b < n; b++) {
        const YLIRBasicBlock *block = fn->blocks[b];
        if (!block || !block->name || block->function != fn ||
            (block->instruction_count && !block->instructions)) {
            free(defs); free(preds); free(dom); free(reachable); free(queue);
            return ir_error(out, fn, block, 0U, 0U, "invalid basic-block storage or owner");
        }
        for (size_t prior = 0; prior < b; prior++) {
            if (strcmp(fn->blocks[prior]->name, block->name) == 0) {
                free(defs); free(preds); free(dom); free(reachable); free(queue);
                return ir_error(out, fn, block, 0U, 0U, "duplicate basic-block name");
            }
        }
        const YLIRTerminator *term = &block->terminator;
        if (term->kind == YLIR_TERM_BRANCH) {
            if (term->target >= n) {
                free(defs); free(preds); free(dom); free(reachable); free(queue);
                return ir_error(out, fn, block, term->source_line, term->source_column,
                                "branch target block does not exist");
            }
            preds[term->target * n + b] = 1U;
        } else if (term->kind == YLIR_TERM_COND_BRANCH) {
            if (term->target >= n || term->false_target >= n) {
                free(defs); free(preds); free(dom); free(reachable); free(queue);
                return ir_error(out, fn, block, term->source_line, term->source_column,
                                "conditional branch target block does not exist");
            }
            preds[term->target * n + b] = 1U;
            preds[term->false_target * n + b] = 1U;
        } else if (term->kind != YLIR_TERM_RETURN &&
                   term->kind != YLIR_TERM_RETURN_VOID &&
                   term->kind != YLIR_TERM_UNREACHABLE &&
                   term->kind != YLIR_TERM_NONE) {
            free(defs); free(preds); free(dom); free(reachable); free(queue);
            return ir_error(out, fn, block, term->source_line, term->source_column,
                            "unknown terminator");
        }
    }

    /* Reachability starts at block zero. Unreachable blocks are verified locally. */
    size_t read_index = 0U, write_index = 0U;
    reachable[0] = 1U; queue[write_index++] = 0U;
    while (read_index < write_index) {
        size_t from = queue[read_index++];
        const YLIRTerminator *term = &fn->blocks[from]->terminator;
        if (term->kind == YLIR_TERM_BRANCH || term->kind == YLIR_TERM_COND_BRANCH) {
            size_t target = term->target;
            if (!reachable[target]) { reachable[target] = 1U; queue[write_index++] = target; }
        }
        if (term->kind == YLIR_TERM_COND_BRANCH) {
            size_t target = term->false_target;
            if (!reachable[target]) { reachable[target] = 1U; queue[write_index++] = target; }
        }
    }

    /* Iterative dominator sets. Disconnected blocks only dominate themselves. */
    for (size_t b = 0; b < n; b++) {
        if (!reachable[b] || b == 0U) dom[b * n + b] = 1U;
        else for (size_t k = 0; k < n; k++) dom[b * n + k] = 1U;
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t b = 1U; b < n; b++) {
            if (!reachable[b]) continue;
            bool has_pred = false;
            for (size_t p = 0; p < n; p++)
                if (reachable[p] && preds[b * n + p]) has_pred = true;
            if (!has_pred) {
                free(defs); free(preds); free(dom); free(reachable); free(queue);
                return ir_error(out, fn, fn->blocks[b], 0U, 0U,
                                "reachable block has no reachable predecessor");
            }
            for (size_t k = 0; k < n; k++) {
                bool value = true;
                for (size_t p = 0; p < n; p++) {
                    if (reachable[p] && preds[b * n + p] && !dom[p * n + k]) {
                        value = false; break;
                    }
                }
                if (k == b) value = true;
                if (dom[b * n + k] != (uint8_t)value) {
                    dom[b * n + k] = (uint8_t)value; changed = true;
                }
            }
        }
    }

    /* Collect all definitions first, allowing uses in dominated successor blocks. */
    for (size_t b = 0; b < n; b++) {
        const YLIRBasicBlock *block = fn->blocks[b];
        for (size_t i = 0; i < block->instruction_count; i++) {
            const YLIRInstruction *inst = &block->instructions[i];
            YLIRType result_type = YLIR_TYPE_INVALID;
            if (inst->kind == YLIR_OP_PHI) {
                if (!valid_type(inst->type, false)) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "PHI has invalid or void result type");
                }
                result_type = inst->type;
            } else {
                switch (inst->kind) {
                    case YLIR_OP_CONST_BOOL: result_type = YLIR_TYPE_BOOL; break;
                    case YLIR_OP_CONST_I64: result_type = YLIR_TYPE_I64; break;
                    case YLIR_OP_CONST_F64: result_type = YLIR_TYPE_F64; break;
                    case YLIR_OP_ADD_I64: case YLIR_OP_SUB_I64: case YLIR_OP_MUL_I64:
                    case YLIR_OP_SDIV_I64: case YLIR_OP_SREM_I64:
                        result_type = YLIR_TYPE_I64; break;
                    case YLIR_OP_EQ_I64: case YLIR_OP_NE_I64: case YLIR_OP_SLT_I64:
                    case YLIR_OP_SLE_I64: case YLIR_OP_SGT_I64: case YLIR_OP_SGE_I64:
                    case YLIR_OP_NOT_BOOL:
                        result_type = YLIR_TYPE_BOOL; break;
                    case YLIR_OP_ADD_F64: case YLIR_OP_SUB_F64:
                    case YLIR_OP_MUL_F64: case YLIR_OP_DIV_F64:
                        result_type = YLIR_TYPE_F64; break;
                    default:
                        free(defs); free(preds); free(dom); free(reachable); free(queue);
                        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                        "unknown instruction opcode");
                }
                if (inst->type != result_type) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "instruction result type does not match opcode");
                }
            }
            if (inst->result == YLIR_NO_VALUE || inst->result >= fn->next_value_id ||
                inst->result < fn->parameter_count || defs[inst->result].defined) {
                free(defs); free(preds); free(dom); free(reachable); free(queue);
                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                "invalid or duplicate instruction result ID");
            }
            defs[inst->result] = (YLIRDefinition){
                .defined = true, .block_index = (int32_t)b,
                .instruction_index = i, .type = result_type
            };
        }
    }
    for (size_t id = fn->parameter_count; id < fn->next_value_id; id++) {
        if (!defs[id].defined) {
            free(defs); free(preds); free(dom); free(reachable); free(queue);
            return ir_error(out, fn, NULL, 0U, 0U, "SSA value ID was allocated but never defined");
        }
    }

    /* Verify PHIs and regular operands after all definitions and dominators exist. */
    for (size_t b = 0; b < n; b++) {
        const YLIRBasicBlock *block = fn->blocks[b];
        bool saw_regular_instruction = false;
        for (size_t i = 0; i < block->instruction_count; i++) {
            const YLIRInstruction *inst = &block->instructions[i];
            if (inst->kind == YLIR_OP_CALL) {
                if (inst->left != YLIR_NO_VALUE || inst->right != YLIR_NO_VALUE ||
                    (inst->argument_count > 0U && !inst->arguments) ||
                    inst->callee_index >= fn->module->function_count) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "malformed function call operands");
                }
                const YLIRFunction *callee = fn->module->functions[inst->callee_index];
                if (!callee || inst->argument_count != callee->parameter_count) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "call argument count does not match the callee signature");
                }
                for (size_t arg = 0; arg < inst->argument_count; arg++) {
                    if (!expect_value(out, fn, block, b, defs, dom, n,
                                      inst->arguments[arg], callee->parameter_types[arg],
                                      i, inst->source_line, inst->source_column)) {
                        free(defs); free(preds); free(dom); free(reachable); free(queue);
                        return false;
                    }
                }
                continue;
            }
            if (inst->kind == YLIR_OP_PHI) {
                if (saw_regular_instruction) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "PHI instructions must appear before ordinary instructions");
                }
                if (!inst->incoming || inst->incoming_count == 0U ||
                    inst->left != YLIR_NO_VALUE || inst->right != YLIR_NO_VALUE) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "malformed PHI incoming list");
                }
                uint8_t *seen = calloc(n, sizeof(*seen));
                if (!seen) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "out of memory verifying PHI");
                }
                size_t expected = 0U;
                for (size_t p = 0; p < n; p++)
                    if (reachable[p] && preds[b * n + p]) expected++;
                if (inst->incoming_count != expected) {
                    free(seen); free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "PHI must have one incoming value per reachable predecessor");
                }
                for (size_t j = 0; j < inst->incoming_count; j++) {
                    const YLIRPhiIncoming *in = &inst->incoming[j];
                    size_t pred = in->predecessor;
                    if (pred >= n || !reachable[pred] ||
                        !preds[b * n + pred] || seen[pred]) {
                        free(seen); free(defs); free(preds); free(dom); free(reachable); free(queue);
                        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                        "PHI input must name a unique reachable predecessor");
                    }
                    seen[pred] = 1U;
                    if (in->value == YLIR_NO_VALUE || in->value >= fn->next_value_id ||
                        !defs[in->value].defined || defs[in->value].type != inst->type) {
                        free(seen); free(defs); free(preds); free(dom); free(reachable); free(queue);
                        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                        "PHI input is undefined or has the wrong type");
                    }
                    int32_t def_idx = defs[in->value].block_index;
                    if (def_idx >= 0) {
                        size_t def_block = (size_t)def_idx;
                        if (def_block == pred) {
                            if (defs[in->value].instruction_index >=
                                fn->blocks[pred]->instruction_count) {
                                free(seen); free(defs); free(preds); free(dom); free(reachable); free(queue);
                                return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                                "PHI value unavailable at predecessor exit");
                            }
                        } else if (!dom[pred * n + def_block]) {
                            free(seen); free(defs); free(preds); free(dom); free(reachable); free(queue);
                            return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                            "PHI value definition does not dominate predecessor");
                        }
                    }
                }
                free(seen);
                continue;
            }
            saw_regular_instruction = true;
            YLIRType operand_type = YLIR_TYPE_INVALID;
            bool constant = false, unary = false;
            switch (inst->kind) {
                case YLIR_OP_CONST_BOOL:
                    constant = true;
                    if (inst->immediate > 1U) {
                        free(defs); free(preds); free(dom); free(reachable); free(queue);
                        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                        "boolean constants must be zero or one");
                    }
                    break;
                case YLIR_OP_CONST_I64: case YLIR_OP_CONST_F64:
                    constant = true; break;
                case YLIR_OP_ADD_I64: case YLIR_OP_SUB_I64:
                case YLIR_OP_MUL_I64: case YLIR_OP_SDIV_I64:
                case YLIR_OP_SREM_I64: case YLIR_OP_EQ_I64:
                case YLIR_OP_NE_I64: case YLIR_OP_SLT_I64:
                case YLIR_OP_SLE_I64: case YLIR_OP_SGT_I64:
                case YLIR_OP_SGE_I64:
                    operand_type = YLIR_TYPE_I64; break;
                case YLIR_OP_NOT_BOOL:
                    operand_type = YLIR_TYPE_BOOL; unary = true; break;
                case YLIR_OP_ADD_F64: case YLIR_OP_SUB_F64:
                case YLIR_OP_MUL_F64: case YLIR_OP_DIV_F64:
                    operand_type = YLIR_TYPE_F64; break;
                default:
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "unknown instruction opcode");
            }
            if (constant) {
                if (inst->left != YLIR_NO_VALUE || inst->right != YLIR_NO_VALUE) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                    "constant instruction unexpectedly has operands");
                }
            } else {
                if (!expect_value(out, fn, block, b, defs, dom, n, inst->left,
                                  operand_type, i, inst->source_line, inst->source_column)) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return false;
                }
                if (unary) {
                    if (inst->right != YLIR_NO_VALUE) {
                        free(defs); free(preds); free(dom); free(reachable); free(queue);
                        return ir_error(out, fn, block, inst->source_line, inst->source_column,
                                        "unary instruction has unexpected second operand");
                    }
                } else if (!expect_value(out, fn, block, b, defs, dom, n, inst->right,
                                         operand_type, i, inst->source_line, inst->source_column)) {
                    free(defs); free(preds); free(dom); free(reachable); free(queue);
                    return false;
                }
            }
        }
        if (!verify_terminator(out, fn, block, b, defs, dom, n)) {
            free(defs); free(preds); free(dom); free(reachable); free(queue);
            return false;
        }
    }
    free(defs); free(preds); free(dom); free(reachable); free(queue);
    return true;
}
bool ylang_ir_verify(const YLIRModule *module, FILE *diagnostics)
{
    if (!module || (module->function_count && !module->functions) ||
        (module->global_count && !module->globals))
        return ir_error(diagnostics, NULL, NULL, 0U, 0U, "invalid module storage");
    for (size_t i = 0; i < module->global_count; i++) {
        const YLIRGlobal *global = &module->globals[i];
        if (!global->name || !valid_symbol_name(global->name) ||
            !valid_type(global->type, false))
            return ir_error(diagnostics, NULL, NULL, 0U, 0U, "module contains an invalid global");
        for (size_t j = 0; j < i; j++)
            if (strcmp(global->name, module->globals[j].name) == 0)
                return ir_error(diagnostics, NULL, NULL, 0U, 0U, "duplicate global name");
    }
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
        for (size_t g = 0; g < module->global_count; g++)
            if (strcmp(fn->name, module->globals[g].name) == 0)
                return ir_error(diagnostics, fn, NULL, 0U, 0U, "global/function symbol collision");
        if (!verify_function(diagnostics, fn)) return false;
    }
    return true;
}
