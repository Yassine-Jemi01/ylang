#ifndef YLANG_IR_H
#define YLANG_IR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    YIR_OP_ADD_I64,
    YIR_OP_SUB_I64,
    YIR_OP_MUL_I64,
    YIR_OP_EQ_I64,
    YIR_OP_BRANCH,
    YIR_OP_COND_BRANCH,
    YIR_OP_RETURN
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
    uint32_t target;
    uint32_t false_target;
    const uint32_t *arguments;
    size_t argument_count;
    const uint32_t *false_arguments;
    size_t false_argument_count;
} YIRInstruction;

typedef struct {
    uint32_t id;
    const YIRBlockParameter *parameters;
    size_t parameter_count;
    const YIRInstruction *instructions;
    size_t instruction_count;
} YIRBlock;

typedef struct {
    const char *name;
    YIRType return_type;
    const YIRBlock *blocks;
    size_t block_count;
} YIRFunction;

/* Writes a NUL-terminated explanation on failure when a buffer is supplied. */
bool yir_verify_function(const YIRFunction *function, char *error,
                         size_t error_capacity);

#endif
