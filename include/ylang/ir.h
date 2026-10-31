#ifndef YLANG_IR_H
#define YLANG_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Backend-neutral typed SSA IR. The first block is the function entry. */
typedef enum {
    YIR_TYPE_VOID,
    YIR_TYPE_BOOL,
    YIR_TYPE_I64,
    YIR_TYPE_F64,
    YIR_TYPE_PTR
} YIRType;

typedef enum {
    YIR_OP_CONST_I64,
    YIR_OP_CONST_F64,
    YIR_OP_ADD_I64,
    YIR_OP_SUB_I64,
    YIR_OP_MUL_I64,
    YIR_OP_DIV_I64,
    YIR_OP_MOD_I64,
    YIR_OP_NEG_I64,
    YIR_OP_I64_TO_F64,
    YIR_OP_F64_TO_I64,
    YIR_OP_ADD_F64,
    YIR_OP_SUB_F64,
    YIR_OP_MUL_F64,
    YIR_OP_DIV_F64,
    YIR_OP_NEG_F64,
    YIR_OP_EQ_F64,
    YIR_OP_NE_F64,
    YIR_OP_LT_F64,
    YIR_OP_LE_F64,
    YIR_OP_GT_F64,
    YIR_OP_GE_F64,
    YIR_OP_EQ_I64,
    YIR_OP_NE_I64,
    YIR_OP_LT_I64,
    YIR_OP_LE_I64,
    YIR_OP_GT_I64,
    YIR_OP_GE_I64,
    YIR_OP_CONST_BOOL,
    YIR_OP_NOT_BOOL,
    YIR_OP_BRANCH,
    YIR_OP_COND_BRANCH,
    YIR_OP_RETURN,
    YIR_OP_CALL
} YIROpcode;

typedef struct {
    uint32_t id;
    YIRType type;
} YIRBlockParameter;

typedef struct {
    YIROpcode opcode;
    uint32_t result_id;
    YIRType result_type;
    uint32_t lhs;
    uint32_t rhs;
    int64_t immediate;
    double floating_immediate;
    uint32_t target;
    uint32_t false_target;
    const uint32_t *arguments;
    size_t argument_count;
    const uint32_t *false_arguments;
    size_t false_argument_count;
    /* Direct user-function call metadata; arguments stores SSA value IDs. */
    const char *callee;
    char *callee_symbol;
    const YIRType *call_parameter_types;
    size_t call_parameter_count;
    size_t source_line;
    size_t source_column;
} YIRInstruction;

typedef struct {
    uint32_t id;
    const YIRBlockParameter *parameters;
    size_t parameter_count;
    const YIRInstruction *instructions;
    size_t instruction_count;
} YIRBlock;

typedef struct {
    uint32_t id;
    YIRType type;
} YIRFunctionParameter;

typedef struct {
    const char *name;
    char *symbol;
    YIRType return_type;
    const YIRFunctionParameter *parameters;
    size_t parameter_count;
    const YIRBlock *blocks;
    size_t block_count;
} YIRFunction;

struct Function;
/* Lowers the currently supported scalar-return subset of a checked AST. */
bool yir_lower_function(const struct Function *function, YIRFunction *out,
                        char *error, size_t error_capacity);
void yir_destroy_function(YIRFunction *function);

/* Writes a NUL-terminated explanation on failure when a buffer is supplied. */
bool yir_verify_function(const YIRFunction *function, char *error,
                         size_t error_capacity);
/* Verifies a set of functions and resolves every direct call by unique name. */
bool yir_verify_module(const YIRFunction *functions, size_t function_count,
                       char *error, size_t error_capacity);

typedef enum {
    YIR_OPT_CONSTANT_FOLD,
    YIR_OPT_SIMPLIFY_CFG,
    YIR_OPT_COMMON_SUBEXPRESSION,
    YIR_OPT_DEAD_CODE
} YIROptPass;

/* Runs a caller-selected sequence of verified optimizer passes. */
bool yir_run_opt_pipeline(YIRFunction *function, const YIROptPass *passes,
                          size_t pass_count, char *error,
                          size_t error_capacity);

/* Folds safe scalar constants and runs the default verified pass pipeline. */
bool yir_optimize_function(YIRFunction *function, char *error,
                           size_t error_capacity);
bool yir_optimize_module(YIRFunction *functions, size_t function_count,
                         char *error, size_t error_capacity);

/* Writes a human-readable textual representation of a function's IR. */
void yir_print_function(FILE *stream, const YIRFunction *function);

/* Writes a verified module as versioned, machine-readable JSON. */
bool yir_write_json_module(FILE *stream, const YIRFunction *functions,
                           size_t function_count, char *error,
                           size_t error_capacity);

/* Emits standalone C for the verified scalar IR module subset. */
bool yir_emit_c_module(FILE *stream, const YIRFunction *functions,
                       size_t function_count, char *error,
                       size_t error_capacity);

#endif
