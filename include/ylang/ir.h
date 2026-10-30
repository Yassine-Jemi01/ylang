#ifndef YLANG_IR_H
#define YLANG_IR_H
/*
 * Backend-neutral typed YLang IR.
 * Values are function-local SSA IDs. Parameter IDs are [0, parameter_count);
 * instruction IDs follow. PHI inputs name predecessor blocks explicitly;
 * ordinary operands must be dominated by their definitions.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef uint32_t YLIRValue;
#define YLIR_NO_VALUE UINT32_MAX

typedef enum {
    YLIR_TYPE_INVALID = 0, YLIR_TYPE_VOID, YLIR_TYPE_BOOL,
    YLIR_TYPE_I8, YLIR_TYPE_I64, YLIR_TYPE_F64, YLIR_TYPE_PTR
} YLIRType;

typedef enum {
    YLIR_OP_CONST_BOOL, YLIR_OP_CONST_I64, YLIR_OP_CONST_F64,
    YLIR_OP_ADD_I64, YLIR_OP_SUB_I64, YLIR_OP_MUL_I64,
    YLIR_OP_SDIV_I64, YLIR_OP_SREM_I64,
    YLIR_OP_EQ_I64, YLIR_OP_NE_I64, YLIR_OP_SLT_I64,
    YLIR_OP_SLE_I64, YLIR_OP_SGT_I64, YLIR_OP_SGE_I64,
    YLIR_OP_NOT_BOOL,
    YLIR_OP_ADD_F64, YLIR_OP_SUB_F64, YLIR_OP_MUL_F64, YLIR_OP_DIV_F64,
    YLIR_OP_PHI,
    YLIR_OP_CALL
} YLIRInstructionKind;

typedef enum {
    YLIR_TERM_NONE = 0, YLIR_TERM_RETURN, YLIR_TERM_RETURN_VOID,
    YLIR_TERM_BRANCH, YLIR_TERM_COND_BRANCH, YLIR_TERM_UNREACHABLE
} YLIRTerminatorKind;

typedef struct YLIRModule YLIRModule;
typedef struct YLIRFunction YLIRFunction;
typedef struct YLIRBasicBlock YLIRBasicBlock;

typedef struct {
    size_t predecessor;
    YLIRValue value;
} YLIRPhiIncoming;

typedef struct {
    YLIRInstructionKind kind;
    YLIRValue result;
    YLIRType type;
    YLIRValue left;
    YLIRValue right;
    uint64_t immediate;
    YLIRPhiIncoming *incoming;
    size_t incoming_count;
    size_t callee_index;
    YLIRValue *arguments;
    size_t argument_count;
    size_t source_line;
    size_t source_column;
} YLIRInstruction;

typedef struct {
    YLIRTerminatorKind kind;
    YLIRValue value;
    size_t target;
    size_t false_target;
    size_t source_line;
    size_t source_column;
} YLIRTerminator;

struct YLIRBasicBlock {
    YLIRFunction *function;
    char *name;
    YLIRInstruction *instructions;
    size_t instruction_count;
    size_t instruction_capacity;
    YLIRTerminator terminator;
};

struct YLIRFunction {
    YLIRModule *module;
    char *name;
    YLIRType return_type;
    YLIRType *parameter_types;
    size_t parameter_count;
    YLIRBasicBlock **blocks;
    size_t block_count;
    size_t block_capacity;
    YLIRValue next_value_id;
};

struct YLIRModule {
    YLIRFunction **functions;
    size_t function_count;
    size_t function_capacity;
};

bool ylang_ir_module_init(YLIRModule *module);
void ylang_ir_module_destroy(YLIRModule *module);
bool ylang_ir_add_function(YLIRModule *module, const char *name,
                           YLIRType return_type,
                           const YLIRType *parameter_types,
                           size_t parameter_count,
                           YLIRFunction **out_function);
bool ylang_ir_add_block(YLIRFunction *function, const char *name,
                        YLIRBasicBlock **out_block);
bool ylang_ir_emit_const_bool(YLIRBasicBlock *block, bool value,
                              size_t line, size_t column, YLIRValue *out);
bool ylang_ir_emit_const_i64(YLIRBasicBlock *block, int64_t value,
                             size_t line, size_t column, YLIRValue *out);
bool ylang_ir_emit_const_f64(YLIRBasicBlock *block, double value,
                             size_t line, size_t column, YLIRValue *out);
bool ylang_ir_emit_op(YLIRBasicBlock *block, YLIRInstructionKind kind,
                      YLIRValue left, YLIRValue right,
                      size_t line, size_t column, YLIRValue *out);
bool ylang_ir_emit_phi(YLIRBasicBlock *block, YLIRType type,
                       const YLIRPhiIncoming *incoming, size_t incoming_count,
                       size_t line, size_t column, YLIRValue *out);
bool ylang_ir_emit_call(YLIRBasicBlock *block, size_t callee_index,
                        const YLIRValue *arguments, size_t argument_count,
                        size_t line, size_t column, YLIRValue *out);
bool ylang_ir_return(YLIRBasicBlock *block, YLIRValue value,
                     size_t line, size_t column);
bool ylang_ir_return_void(YLIRBasicBlock *block, size_t line, size_t column);
bool ylang_ir_branch(YLIRBasicBlock *block, size_t target,
                     size_t line, size_t column);
bool ylang_ir_cond_branch(YLIRBasicBlock *block, YLIRValue condition,
                          size_t true_target, size_t false_target,
                          size_t line, size_t column);
bool ylang_ir_unreachable(YLIRBasicBlock *block, size_t line, size_t column);
bool ylang_ir_verify(const YLIRModule *module, FILE *diagnostics);
const char *ylang_ir_type_name(YLIRType type);
const char *ylang_ir_instruction_name(YLIRInstructionKind kind);
const char *ylang_ir_terminator_name(YLIRTerminatorKind kind);
#endif
