#include "ylang/ir.h"

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool run_constant_fold_only(YIRFunction *function, char *error,
                                   size_t error_capacity)
{
    const YIROptPass pass[] = {YIR_OPT_CONSTANT_FOLD};
    return yir_run_opt_pipeline(function, pass, 1, error, error_capacity);
}

static void folds_safe_integer_and_boolean_constants(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=20,.source_line=1,.source_column=10},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=22,.source_line=1,.source_column=15},
        {.opcode=YIR_OP_ADD_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2,.source_line=1,.source_column=13},
        {.opcode=YIR_OP_CONST_I64,.result_id=4,.result_type=YIR_TYPE_I64,
         .immediate=2},
        {.opcode=YIR_OP_MUL_I64,.result_id=5,.result_type=YIR_TYPE_I64,
         .lhs=3,.rhs=4},
        {.opcode=YIR_OP_CONST_I64,.result_id=6,.result_type=YIR_TYPE_I64,
         .immediate=3},
        {.opcode=YIR_OP_DIV_I64,.result_id=7,.result_type=YIR_TYPE_I64,
         .lhs=5,.rhs=6},
        {.opcode=YIR_OP_CONST_I64,.result_id=8,.result_type=YIR_TYPE_I64,
         .immediate=43},
        {.opcode=YIR_OP_MOD_I64,.result_id=9,.result_type=YIR_TYPE_I64,
         .lhs=8,.rhs=6},
        {.opcode=YIR_OP_NEG_I64,.result_id=10,.result_type=YIR_TYPE_I64,
         .lhs=9},
        {.opcode=YIR_OP_EQ_I64,.result_id=11,.result_type=YIR_TYPE_BOOL,
         .lhs=7,.rhs=1},
        {.opcode=YIR_OP_NOT_BOOL,.result_id=12,.result_type=YIR_TYPE_BOOL,
         .lhs=11},
        {.opcode=YIR_OP_RETURN,.lhs=12}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,
         .instruction_count=sizeof(instructions)/sizeof(instructions[0])}
    };
    YIRFunction function = {
        .name="constant_chain",.return_type=YIR_TYPE_BOOL,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(instructions[2].opcode==YIR_OP_CONST_I64);
    assert(instructions[2].immediate==42);
    assert(instructions[2].source_line==1);
    assert(instructions[2].source_column==13);
    assert(instructions[4].opcode==YIR_OP_CONST_I64);
    assert(instructions[4].immediate==84);
    assert(instructions[6].opcode==YIR_OP_CONST_I64);
    assert(instructions[6].immediate==28);
    assert(instructions[8].opcode==YIR_OP_CONST_I64);
    assert(instructions[8].immediate==1);
    assert(instructions[9].opcode==YIR_OP_CONST_I64);
    assert(instructions[9].immediate==-1);
    assert(instructions[10].opcode==YIR_OP_CONST_BOOL);
    assert(instructions[10].immediate==0);
    assert(instructions[11].opcode==YIR_OP_CONST_BOOL);
    assert(instructions[11].immediate==1);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void preserves_operations_that_may_trap(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=INT64_MAX},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=1},
        {.opcode=YIR_OP_ADD_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_CONST_I64,.result_id=4,.result_type=YIR_TYPE_I64,
         .immediate=INT64_MIN},
        {.opcode=YIR_OP_NEG_I64,.result_id=5,.result_type=YIR_TYPE_I64,
         .lhs=4},
        {.opcode=YIR_OP_CONST_I64,.result_id=6,.result_type=YIR_TYPE_I64,
         .immediate=0},
        {.opcode=YIR_OP_DIV_I64,.result_id=7,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=6},
        {.opcode=YIR_OP_CONST_I64,.result_id=8,.result_type=YIR_TYPE_I64,
         .immediate=-1},
        {.opcode=YIR_OP_DIV_I64,.result_id=9,.result_type=YIR_TYPE_I64,
         .lhs=4,.rhs=8},
        {.opcode=YIR_OP_MOD_I64,.result_id=10,.result_type=YIR_TYPE_I64,
         .lhs=4,.rhs=6},
        {.opcode=YIR_OP_CONST_I64,.result_id=11,.result_type=YIR_TYPE_I64,
         .immediate=-1},
        {.opcode=YIR_OP_MUL_I64,.result_id=12,.result_type=YIR_TYPE_I64,
         .lhs=4,.rhs=11},
        {.opcode=YIR_OP_RETURN,.lhs=10}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,
         .instruction_count=sizeof(instructions)/sizeof(instructions[0])}
    };
    YIRFunction function = {
        .name="trapping_constants",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(instructions[2].opcode==YIR_OP_ADD_I64);
    assert(instructions[4].opcode==YIR_OP_NEG_I64);
    assert(instructions[6].opcode==YIR_OP_DIV_I64);
    assert(instructions[8].opcode==YIR_OP_DIV_I64);
    assert(instructions[9].opcode==YIR_OP_MOD_I64);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void folds_negative_multiplication_without_signed_overflow(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=-7},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=6},
        {.opcode=YIR_OP_MUL_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=4}
    };
    YIRFunction function = {
        .name="negative_multiply",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(instructions[2].opcode==YIR_OP_CONST_I64);
    assert(instructions[2].immediate==-42);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void folds_the_defined_minimum_modulo_negative_one_case(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=INT64_MIN},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=-1},
        {.opcode=YIR_OP_MOD_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=4}
    };
    YIRFunction function = {
        .name="defined_modulo_edge",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(instructions[2].opcode==YIR_OP_CONST_I64);
    assert(instructions[2].immediate==0);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void folds_constants_across_dominating_blocks(void)
{
    YIRInstruction entry_instructions[] = {
        {.opcode=YIR_OP_BRANCH,.target=2}
    };
    YIRInstruction use_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=22},
        {.opcode=YIR_OP_ADD_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    YIRInstruction dominating_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=20},
        {.opcode=YIR_OP_BRANCH,.target=1}
    };
    /* The defining block is deliberately listed after the use block. */
    YIRBlock blocks[] = {
        {.id=0,.instructions=entry_instructions,.instruction_count=1},
        {.id=1,.instructions=use_instructions,.instruction_count=3},
        {.id=2,.instructions=dominating_instructions,.instruction_count=2}
    };
    YIRFunction function = {
        .name="dominating_constant",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=3
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(use_instructions[1].opcode==YIR_OP_CONST_I64);
    assert(use_instructions[1].immediate==42);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void does_not_fold_block_parameter_as_a_constant(void)
{
    const uint32_t left_argument[] = {1};
    const uint32_t right_argument[] = {2};
    YIRBlockParameter join_parameters[] = {
        {.id=4,.type=YIR_TYPE_I64}
    };
    YIRInstruction entry_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=10},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=20},
        {.opcode=YIR_OP_CONST_BOOL,.result_id=3,.result_type=YIR_TYPE_BOOL,
         .immediate=1},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=3,
         .target=1,.false_target=2}
    };
    YIRInstruction left_instructions[] = {
        {.opcode=YIR_OP_BRANCH,.target=3,
         .arguments=left_argument,.argument_count=1}
    };
    YIRInstruction right_instructions[] = {
        {.opcode=YIR_OP_BRANCH,.target=3,
         .arguments=right_argument,.argument_count=1}
    };
    YIRInstruction join_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=5,.result_type=YIR_TYPE_I64,
         .immediate=1},
        {.opcode=YIR_OP_ADD_I64,.result_id=6,.result_type=YIR_TYPE_I64,
         .lhs=4,.rhs=5},
        {.opcode=YIR_OP_RETURN,.lhs=6}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=entry_instructions,.instruction_count=4},
        {.id=1,.instructions=left_instructions,.instruction_count=1},
        {.id=2,.instructions=right_instructions,.instruction_count=1},
        {.id=3,.parameters=join_parameters,.parameter_count=1,
         .instructions=join_instructions,.instruction_count=3}
    };
    YIRFunction function = {
        .name="block_parameter_is_not_a_constant",
        .return_type=YIR_TYPE_I64,.blocks=blocks,.block_count=4
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(join_instructions[1].opcode==YIR_OP_ADD_I64);
    assert(join_instructions[1].lhs==4);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void folds_safe_f64_arithmetic_and_comparisons(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_F64,.result_id=1,.result_type=YIR_TYPE_F64,
         .floating_immediate=1.25},
        {.opcode=YIR_OP_CONST_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .floating_immediate=2.5},
        {.opcode=YIR_OP_ADD_F64,.result_id=3,.result_type=YIR_TYPE_F64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_CONST_F64,.result_id=4,.result_type=YIR_TYPE_F64,
         .floating_immediate=2.0},
        {.opcode=YIR_OP_MUL_F64,.result_id=5,.result_type=YIR_TYPE_F64,
         .lhs=3,.rhs=4},
        {.opcode=YIR_OP_CONST_F64,.result_id=6,.result_type=YIR_TYPE_F64,
         .floating_immediate=3.0},
        {.opcode=YIR_OP_DIV_F64,.result_id=7,.result_type=YIR_TYPE_F64,
         .lhs=5,.rhs=6},
        {.opcode=YIR_OP_NEG_F64,.result_id=8,.result_type=YIR_TYPE_F64,
         .lhs=7},
        {.opcode=YIR_OP_CONST_F64,.result_id=9,.result_type=YIR_TYPE_F64,
         .floating_immediate=2.5},
        {.opcode=YIR_OP_EQ_F64,.result_id=10,.result_type=YIR_TYPE_BOOL,
         .lhs=8,.rhs=9},
        {.opcode=YIR_OP_SUB_F64,.result_id=11,.result_type=YIR_TYPE_F64,
         .lhs=3,.rhs=2},
        {.opcode=YIR_OP_LT_F64,.result_id=12,.result_type=YIR_TYPE_BOOL,
         .lhs=11,.rhs=2},
        {.opcode=YIR_OP_NE_F64,.result_id=13,.result_type=YIR_TYPE_BOOL,
         .lhs=11,.rhs=1},
        {.opcode=YIR_OP_LE_F64,.result_id=14,.result_type=YIR_TYPE_BOOL,
         .lhs=11,.rhs=1},
        {.opcode=YIR_OP_GT_F64,.result_id=15,.result_type=YIR_TYPE_BOOL,
         .lhs=2,.rhs=1},
        {.opcode=YIR_OP_GE_F64,.result_id=16,.result_type=YIR_TYPE_BOOL,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=10}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,
         .instruction_count=sizeof(instructions)/sizeof(instructions[0])}
    };
    YIRFunction function = {
        .name="safe_f64_folding",.return_type=YIR_TYPE_BOOL,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(instructions[2].opcode==YIR_OP_CONST_F64);
    assert(instructions[2].floating_immediate==3.75);
    assert(instructions[4].opcode==YIR_OP_CONST_F64);
    assert(instructions[4].floating_immediate==7.5);
    assert(instructions[6].opcode==YIR_OP_CONST_F64);
    assert(instructions[6].floating_immediate==2.5);
    assert(instructions[7].opcode==YIR_OP_CONST_F64);
    assert(instructions[7].floating_immediate==-2.5);
    assert(instructions[9].opcode==YIR_OP_CONST_BOOL);
    assert(instructions[9].immediate==0);
    assert(instructions[10].opcode==YIR_OP_CONST_F64);
    assert(instructions[10].floating_immediate==1.25);
    assert(instructions[11].opcode==YIR_OP_CONST_BOOL);
    assert(instructions[11].immediate==1);
    assert(instructions[12].opcode==YIR_OP_CONST_BOOL);
    assert(instructions[12].immediate==0);
    assert(instructions[13].opcode==YIR_OP_CONST_BOOL);
    assert(instructions[13].immediate==1);
    assert(instructions[14].opcode==YIR_OP_CONST_BOOL);
    assert(instructions[14].immediate==1);
    assert(instructions[15].opcode==YIR_OP_CONST_BOOL);
    assert(instructions[15].immediate==0);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void folds_f64_negation_without_losing_signed_zero(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_F64,.result_id=1,.result_type=YIR_TYPE_F64,
         .floating_immediate=0.0},
        {.opcode=YIR_OP_NEG_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .lhs=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=3}
    };
    YIRFunction function = {
        .name="signed_zero_folding",.return_type=YIR_TYPE_F64,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(instructions[1].opcode==YIR_OP_CONST_F64);
    assert(instructions[1].floating_immediate==0.0);
    assert(signbit(instructions[1].floating_immediate));
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void preserves_f64_traps_overflow_and_subnormal_results(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_F64,.result_id=1,.result_type=YIR_TYPE_F64,
         .floating_immediate=1.0},
        {.opcode=YIR_OP_CONST_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .floating_immediate=0.0},
        {.opcode=YIR_OP_DIV_F64,.result_id=3,.result_type=YIR_TYPE_F64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_CONST_F64,.result_id=4,.result_type=YIR_TYPE_F64,
         .floating_immediate=-0.0},
        {.opcode=YIR_OP_DIV_F64,.result_id=5,.result_type=YIR_TYPE_F64,
         .lhs=1,.rhs=4},
        {.opcode=YIR_OP_CONST_F64,.result_id=6,.result_type=YIR_TYPE_F64,
         .floating_immediate=DBL_MAX},
        {.opcode=YIR_OP_CONST_F64,.result_id=7,.result_type=YIR_TYPE_F64,
         .floating_immediate=2.0},
        {.opcode=YIR_OP_MUL_F64,.result_id=8,.result_type=YIR_TYPE_F64,
         .lhs=6,.rhs=7},
        {.opcode=YIR_OP_CONST_F64,.result_id=9,.result_type=YIR_TYPE_F64,
         .floating_immediate=DBL_MIN},
        {.opcode=YIR_OP_CONST_F64,.result_id=10,.result_type=YIR_TYPE_F64,
         .floating_immediate=0.5},
        {.opcode=YIR_OP_MUL_F64,.result_id=11,.result_type=YIR_TYPE_F64,
         .lhs=9,.rhs=10},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,
         .instruction_count=sizeof(instructions)/sizeof(instructions[0])}
    };
    YIRFunction function = {
        .name="f64_traps_and_edges",.return_type=YIR_TYPE_F64,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(instructions[2].opcode==YIR_OP_DIV_F64);
    assert(instructions[4].opcode==YIR_OP_DIV_F64);
    assert(instructions[7].opcode==YIR_OP_MUL_F64);
    assert(instructions[10].opcode==YIR_OP_MUL_F64);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void preserves_calls_and_module_symbols(void)
{
    const uint32_t call_arguments[] = {1};
    const YIRType call_parameter_types[] = {YIR_TYPE_I64};
    YIRInstruction callee_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=1},
        {.opcode=YIR_OP_ADD_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    YIRBlock callee_blocks[] = {
        {.id=0,.instructions=callee_instructions,.instruction_count=3}
    };
    YIRFunctionParameter callee_parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    YIRInstruction caller_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=41},
        {.opcode=YIR_OP_CALL,.result_id=2,.result_type=YIR_TYPE_I64,
         .arguments=call_arguments,.argument_count=1,
         .callee="increment",.callee_symbol="lib::increment",
         .call_parameter_types=call_parameter_types,
         .call_parameter_count=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    YIRBlock caller_blocks[] = {
        {.id=0,.instructions=caller_instructions,.instruction_count=3}
    };
    YIRFunction functions[] = {
        {.name="increment",.symbol="lib::increment",
         .return_type=YIR_TYPE_I64,
         .parameters=callee_parameters,.parameter_count=1,
         .blocks=callee_blocks,.block_count=1},
        {.name="main",.symbol="app::main",
         .return_type=YIR_TYPE_I64,
         .blocks=caller_blocks,.block_count=1}
    };
    char error[256];

    assert(yir_optimize_module(functions,2,error,sizeof(error)));
    assert(callee_instructions[1].opcode==YIR_OP_ADD_I64);
    assert(caller_instructions[1].opcode==YIR_OP_CALL);
    assert(yir_verify_module(functions,2,error,sizeof(error)));
}



static uint32_t *owned_id(uint32_t value)
{
    uint32_t *id = malloc(sizeof(*id));
    assert(id != NULL);
    *id = value;
    return id;
}

static void simplifies_constant_branch_and_frees_unreachable_block(void)
{
    YIRBlock *blocks = calloc(3, sizeof(*blocks));
    assert(blocks != NULL);

    YIRInstruction *entry = calloc(3, sizeof(*entry));
    YIRInstruction *selected = calloc(1, sizeof(*selected));
    YIRInstruction *discarded = calloc(1, sizeof(*discarded));
    YIRBlockParameter *selected_parameter = calloc(1, sizeof(*selected_parameter));
    YIRBlockParameter *discarded_parameter = calloc(1, sizeof(*discarded_parameter));
    assert(entry && selected && discarded &&
           selected_parameter && discarded_parameter);

    selected_parameter[0] = (YIRBlockParameter){.id=3,.type=YIR_TYPE_I64};
    discarded_parameter[0] = (YIRBlockParameter){.id=4,.type=YIR_TYPE_I64};

    entry[0] = (YIRInstruction){
        .opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
        .immediate=42
    };
    entry[1] = (YIRInstruction){
        .opcode=YIR_OP_CONST_BOOL,.result_id=2,.result_type=YIR_TYPE_BOOL,
        .immediate=1
    };
    entry[2] = (YIRInstruction){
        .opcode=YIR_OP_COND_BRANCH,.lhs=2,
        .target=1,.false_target=2,
        .arguments=owned_id(1),.argument_count=1,
        .false_arguments=owned_id(1),.false_argument_count=1
    };
    selected[0] = (YIRInstruction){.opcode=YIR_OP_RETURN,.lhs=3};
    discarded[0] = (YIRInstruction){.opcode=YIR_OP_RETURN,.lhs=4};

    blocks[0] = (YIRBlock){
        .id=0,.instructions=entry,.instruction_count=3
    };
    blocks[1] = (YIRBlock){
        .id=1,.parameters=selected_parameter,.parameter_count=1,
        .instructions=selected,.instruction_count=1
    };
    blocks[2] = (YIRBlock){
        .id=2,.parameters=discarded_parameter,.parameter_count=1,
        .instructions=discarded,.instruction_count=1
    };
    YIRFunction function = {
        .name="constant_branch",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=3
    };
    char error[256];

    assert(yir_optimize_function(&function,error,sizeof(error)));
    assert(function.block_count==2);
    assert(function.blocks[0].id==0);
    assert(function.blocks[0].instruction_count==2);
    assert(function.blocks[0].instructions[0].opcode==YIR_OP_CONST_I64);
    assert(function.blocks[0].instructions[1].opcode==YIR_OP_BRANCH);
    assert(function.blocks[0].instructions[1].target==1);
    assert(function.blocks[0].instructions[1].argument_count==1);
    assert(function.blocks[0].instructions[1].arguments[0]==1);
    assert(function.blocks[0].instructions[1].false_arguments==NULL);
    assert(function.blocks[1].id==1);
    assert(yir_verify_function(&function,error,sizeof(error)));
    yir_destroy_function(&function);
}

static void eliminates_dead_pure_values_but_keeps_checked_arithmetic(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=INT64_MAX},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=1},
        {.opcode=YIR_OP_ADD_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_EQ_I64,.result_id=4,.result_type=YIR_TYPE_BOOL,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=5}
    };
    YIRFunction function = {
        .name="dead_pure_values",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(yir_optimize_function(&function,error,sizeof(error)));
    assert(blocks[0].instruction_count==4);
    assert(instructions[0].opcode==YIR_OP_CONST_I64);
    assert(instructions[1].opcode==YIR_OP_CONST_I64);
    assert(instructions[2].opcode==YIR_OP_ADD_I64);
    assert(instructions[3].opcode==YIR_OP_RETURN);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void keeps_unused_floating_division_that_can_trap(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_F64,.result_id=1,.result_type=YIR_TYPE_F64,
         .floating_immediate=1.0},
        {.opcode=YIR_OP_CONST_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .floating_immediate=-0.0},
        {.opcode=YIR_OP_DIV_F64,.result_id=3,.result_type=YIR_TYPE_F64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=4}
    };
    YIRFunction function = {
        .name="unused_float_division",.return_type=YIR_TYPE_VOID,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(yir_optimize_function(&function,error,sizeof(error)));
    assert(blocks[0].instruction_count==4);
    assert(instructions[2].opcode==YIR_OP_DIV_F64);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void keeps_void_calls_even_when_their_result_is_unused(void)
{
    YIRInstruction callee_instructions[] = {
        {.opcode=YIR_OP_RETURN}
    };
    YIRInstruction caller_instructions[] = {
        {.opcode=YIR_OP_CALL,.result_type=YIR_TYPE_VOID,
         .callee="notify",.callee_symbol="lib::notify"},
        {.opcode=YIR_OP_RETURN}
    };
    YIRBlock callee_blocks[] = {
        {.id=0,.instructions=callee_instructions,.instruction_count=1}
    };
    YIRBlock caller_blocks[] = {
        {.id=0,.instructions=caller_instructions,.instruction_count=2}
    };
    YIRFunction functions[] = {
        {.name="notify",.symbol="lib::notify",.return_type=YIR_TYPE_VOID,
         .blocks=callee_blocks,.block_count=1},
        {.name="main",.symbol="app::main",.return_type=YIR_TYPE_VOID,
         .blocks=caller_blocks,.block_count=1}
    };
    char error[256];

    assert(yir_optimize_module(functions,2,error,sizeof(error)));
    assert(caller_blocks[0].instruction_count==2);
    assert(caller_instructions[0].opcode==YIR_OP_CALL);
    assert(yir_verify_module(functions,2,error,sizeof(error)));
}

static void exposes_ordered_and_validated_optimizer_pipelines(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=40},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=2},
        {.opcode=YIR_OP_ADD_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=4}
    };
    YIRFunction function = {
        .name="custom_pipeline",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=1
    };
    const YIROptPass fold_only[] = {YIR_OPT_CONSTANT_FOLD};
    const YIROptPass invalid[] = {(YIROptPass)99};
    char error[256];

    assert(yir_run_opt_pipeline(&function,fold_only,1,error,sizeof(error)));
    assert(instructions[2].opcode==YIR_OP_CONST_I64);
    assert(instructions[2].immediate==42);
    assert(!yir_run_opt_pipeline(&function,invalid,1,error,sizeof(error)));
    assert(strstr(error,"unknown pass")!=NULL);
    assert(yir_verify_function(&function,error,sizeof(error)));
}


static void common_subexpressions_rewrite_block_edge_uses(void)
{
    uint32_t edge_arguments[] = {3};
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_BOOL}
    };
    const YIRBlockParameter join_parameters[] = {
        {.id=4,.type=YIR_TYPE_BOOL}
    };
    YIRInstruction entry_instructions[] = {
        {.opcode=YIR_OP_NOT_BOOL,.result_id=2,.result_type=YIR_TYPE_BOOL,
         .lhs=1},
        {.opcode=YIR_OP_NOT_BOOL,.result_id=3,.result_type=YIR_TYPE_BOOL,
         .lhs=1},
        {.opcode=YIR_OP_BRANCH,.target=1,
         .arguments=edge_arguments,.argument_count=1}
    };
    YIRInstruction join_instructions[] = {
        {.opcode=YIR_OP_RETURN,.lhs=4}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=entry_instructions,.instruction_count=3},
        {.id=1,.parameters=join_parameters,.parameter_count=1,
         .instructions=join_instructions,.instruction_count=1}
    };
    YIRFunction function = {
        .name="cse_edge_use",.return_type=YIR_TYPE_BOOL,
        .parameters=parameters,.parameter_count=1,
        .blocks=blocks,.block_count=2
    };
    char error[256];

    assert(yir_optimize_function(&function,error,sizeof(error)));
    assert(blocks[0].instruction_count==2);
    assert(entry_instructions[0].opcode==YIR_OP_NOT_BOOL);
    assert(entry_instructions[0].result_id==2);
    assert(entry_instructions[1].opcode==YIR_OP_BRANCH);
    assert(entry_instructions[1].argument_count==1);
    assert(edge_arguments[0]==2);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void common_subexpression_pass_does_not_merge_checked_divisions(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_I64},
        {.id=2,.type=YIR_TYPE_I64}
    };
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_DIV_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_DIV_I64,.result_id=4,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=3}
    };
    YIRFunction function = {
        .name="cse_must_keep_traps",.return_type=YIR_TYPE_VOID,
        .parameters=parameters,.parameter_count=2,
        .blocks=blocks,.block_count=1
    };
    char error[256];

    assert(yir_optimize_function(&function,error,sizeof(error)));
    assert(blocks[0].instruction_count==3);
    assert(instructions[0].opcode==YIR_OP_DIV_I64);
    assert(instructions[1].opcode==YIR_OP_DIV_I64);
    assert(yir_verify_function(&function,error,sizeof(error)));
}


static void folds_safe_numeric_conversions_and_preserves_range_traps(void)
{
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=9},
        {.opcode=YIR_OP_I64_TO_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .lhs=1},
        {.opcode=YIR_OP_CONST_F64,.result_id=3,.result_type=YIR_TYPE_F64,
         .floating_immediate=-3.9},
        {.opcode=YIR_OP_F64_TO_I64,.result_id=4,.result_type=YIR_TYPE_I64,
         .lhs=3},
        {.opcode=YIR_OP_ADD_I64,.result_id=5,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=4},
        {.opcode=YIR_OP_RETURN,.lhs=5}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=6}
    };
    YIRFunction function = {
        .name="safe_conversions",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=1
    };
    char error[256];
    assert(run_constant_fold_only(&function,error,sizeof(error)));
    assert(instructions[1].opcode==YIR_OP_CONST_F64);
    assert(instructions[1].floating_immediate==9.0);
    assert(instructions[3].opcode==YIR_OP_CONST_I64);
    assert(instructions[3].immediate==-3);
    assert(instructions[4].opcode==YIR_OP_CONST_I64);
    assert(instructions[4].immediate==6);
    assert(yir_verify_function(&function,error,sizeof(error)));

    YIRInstruction trapping[] = {
        {.opcode=YIR_OP_CONST_F64,.result_id=1,.result_type=YIR_TYPE_F64,
         .floating_immediate=0x1p63},
        {.opcode=YIR_OP_F64_TO_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .lhs=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    YIRBlock trap_blocks[] = {
        {.id=0,.instructions=trapping,.instruction_count=3}
    };
    YIRFunction trap_function = {
        .name="conversion_range_trap",.return_type=YIR_TYPE_I64,
        .blocks=trap_blocks,.block_count=1
    };
    assert(run_constant_fold_only(&trap_function,error,sizeof(error)));
    assert(trapping[1].opcode==YIR_OP_F64_TO_I64);
    assert(yir_verify_function(&trap_function,error,sizeof(error)));
}


static void copy_propagates_identical_values_from_all_predecessor_edges(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_BOOL}
    };
    const YIRBlockParameter join_parameters[] = {
        {.id=4,.type=YIR_TYPE_I64}
    };
    const uint32_t first_edge_args[] = {2};
    const uint32_t second_edge_args[] = {2};
    YIRInstruction entry[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=42},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=1,.target=1,.false_target=2}
    };
    YIRInstruction first_predecessor[] = {
        {.opcode=YIR_OP_BRANCH,.target=3,
         .arguments=first_edge_args,.argument_count=1}
    };
    YIRInstruction second_predecessor[] = {
        {.opcode=YIR_OP_BRANCH,.target=3,
         .arguments=second_edge_args,.argument_count=1}
    };
    YIRInstruction join[] = {
        {.opcode=YIR_OP_RETURN,.lhs=4}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=entry,.instruction_count=2},
        {.id=1,.instructions=first_predecessor,.instruction_count=1},
        {.id=2,.instructions=second_predecessor,.instruction_count=1},
        {.id=3,.parameters=join_parameters,.parameter_count=1,
         .instructions=join,.instruction_count=1}
    };
    YIRFunction function = {
        .name="copy_propagation",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=1,
        .blocks=blocks,.block_count=4
    };
    char error[256];

    assert(yir_optimize_function(&function,error,sizeof(error)));
    assert(blocks[3].parameter_count==0);
    assert(join[0].lhs==2);
    assert(first_predecessor[0].argument_count==0);
    assert(second_predecessor[0].argument_count==0);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void copy_propagation_keeps_join_parameters_with_different_inputs(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_BOOL}
    };
    const YIRBlockParameter join_parameters[] = {
        {.id=5,.type=YIR_TYPE_I64}
    };
    const uint32_t first_edge_args[] = {3};
    const uint32_t second_edge_args[] = {4};
    YIRInstruction entry[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=7},
        {.opcode=YIR_OP_CONST_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .immediate=10},
        {.opcode=YIR_OP_CONST_I64,.result_id=4,.result_type=YIR_TYPE_I64,
         .immediate=11},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=1,.target=1,.false_target=2}
    };
    YIRInstruction first_predecessor[] = {
        {.opcode=YIR_OP_BRANCH,.target=3,
         .arguments=first_edge_args,.argument_count=1}
    };
    YIRInstruction second_predecessor[] = {
        {.opcode=YIR_OP_BRANCH,.target=3,
         .arguments=second_edge_args,.argument_count=1}
    };
    YIRInstruction join[] = {
        {.opcode=YIR_OP_RETURN,.lhs=5}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=entry,.instruction_count=4},
        {.id=1,.instructions=first_predecessor,.instruction_count=1},
        {.id=2,.instructions=second_predecessor,.instruction_count=1},
        {.id=3,.parameters=join_parameters,.parameter_count=1,
         .instructions=join,.instruction_count=1}
    };
    YIRFunction function = {
        .name="copy_propagation_mismatch",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=1,
        .blocks=blocks,.block_count=4
    };
    char error[256];

    assert(yir_optimize_function(&function,error,sizeof(error)));
    assert(blocks[3].parameter_count==1);
    assert(join[0].lhs==5);
    assert(first_predecessor[0].argument_count==1);
    assert(second_predecessor[0].argument_count==1);
    assert(yir_verify_function(&function,error,sizeof(error)));
}


static void algebraic_simplification_applies_safe_integer_identities(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=0},
        {.opcode=YIR_OP_CONST_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .immediate=1},
        {.opcode=YIR_OP_ADD_I64,.result_id=4,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_SUB_I64,.result_id=5,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_MUL_I64,.result_id=6,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=3},
        {.opcode=YIR_OP_DIV_I64,.result_id=7,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=3},
        {.opcode=YIR_OP_MUL_I64,.result_id=8,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_MOD_I64,.result_id=9,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=3},
        {.opcode=YIR_OP_EQ_I64,.result_id=10,.result_type=YIR_TYPE_BOOL,
         .lhs=1,.rhs=1},
        {.opcode=YIR_OP_LT_I64,.result_id=11,.result_type=YIR_TYPE_BOOL,
         .lhs=1,.rhs=1},
        {.opcode=YIR_OP_RETURN,.lhs=8}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,
         .instruction_count=sizeof(instructions)/sizeof(instructions[0])}
    };
    YIRFunction function = {
        .name="integer_algebra",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=1,
        .blocks=blocks,.block_count=1
    };
    const YIROptPass pass[] = {YIR_OPT_ALGEBRAIC_SIMPLIFY};
    char error[256];

    assert(yir_run_opt_pipeline(&function,pass,1,error,sizeof(error)));
    assert(blocks[0].instruction_count==7);
    bool saw_zero_mul = false;
    bool saw_zero_mod = false;
    bool saw_true_compare = false;
    bool saw_false_compare = false;
    for (size_t i=0;i<blocks[0].instruction_count;i++) {
        const YIRInstruction *ins=&blocks[0].instructions[i];
        if (ins->result_id==8) {
            assert(ins->opcode==YIR_OP_CONST_I64 && ins->immediate==0);
            saw_zero_mul=true;
        } else if (ins->result_id==9) {
            assert(ins->opcode==YIR_OP_CONST_I64 && ins->immediate==0);
            saw_zero_mod=true;
        } else if (ins->result_id==10) {
            assert(ins->opcode==YIR_OP_CONST_BOOL && ins->immediate==1);
            saw_true_compare=true;
        } else if (ins->result_id==11) {
            assert(ins->opcode==YIR_OP_CONST_BOOL && ins->immediate==0);
            saw_false_compare=true;
        }
    }
    assert(saw_zero_mul && saw_zero_mod &&
           saw_true_compare && saw_false_compare);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void algebraic_simplification_does_not_hide_divide_by_zero(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=0},
        {.opcode=YIR_OP_DIV_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_MOD_I64,.result_id=4,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=4}
    };
    YIRFunction function = {
        .name="algebra_keeps_traps",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=1,
        .blocks=blocks,.block_count=1
    };
    const YIROptPass pass[] = {YIR_OPT_ALGEBRAIC_SIMPLIFY};
    char error[256];

    assert(yir_run_opt_pipeline(&function,pass,1,error,sizeof(error)));
    assert(blocks[0].instruction_count==4);
    assert(instructions[1].opcode==YIR_OP_DIV_I64);
    assert(instructions[2].opcode==YIR_OP_MOD_I64);
    assert(yir_verify_function(&function,error,sizeof(error)));
}


static void copy_propagation_compacts_multiple_parameters_on_true_and_false_edges(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_BOOL}
    };
    YIRBlockParameter join_parameters[] = {
        {.id=3,.type=YIR_TYPE_I64},
        {.id=4,.type=YIR_TYPE_I64}
    };
    uint32_t true_arguments[] = {2,2};
    uint32_t false_arguments[] = {2,2};
    YIRInstruction entry[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=21},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=1,.target=1,.false_target=1,
         .arguments=true_arguments,.argument_count=2,
         .false_arguments=false_arguments,.false_argument_count=2}
    };
    YIRInstruction join[] = {
        {.opcode=YIR_OP_ADD_I64,.result_id=5,.result_type=YIR_TYPE_I64,
         .lhs=3,.rhs=4},
        {.opcode=YIR_OP_RETURN,.lhs=5}
    };
    YIRBlock blocks[] = {
        {.id=0,.instructions=entry,.instruction_count=2},
        {.id=1,.parameters=join_parameters,.parameter_count=2,
         .instructions=join,.instruction_count=2}
    };
    YIRFunction function = {
        .name="copy_propagation_multiple",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=1,
        .blocks=blocks,.block_count=2
    };
    char error[256];

    assert(yir_optimize_function(&function,error,sizeof(error)));
    assert(blocks[1].parameter_count==0);
    assert(join[0].lhs==2 && join[0].rhs==2);
    assert(entry[1].argument_count==0);
    assert(entry[1].false_argument_count==0);
    assert(yir_verify_function(&function,error,sizeof(error)));
}

int main(void)
{
    folds_safe_integer_and_boolean_constants();
    preserves_operations_that_may_trap();
    folds_negative_multiplication_without_signed_overflow();
    folds_the_defined_minimum_modulo_negative_one_case();
    folds_constants_across_dominating_blocks();
    does_not_fold_block_parameter_as_a_constant();
    folds_safe_f64_arithmetic_and_comparisons();
    folds_f64_negation_without_losing_signed_zero();
    preserves_f64_traps_overflow_and_subnormal_results();
    preserves_calls_and_module_symbols();
    simplifies_constant_branch_and_frees_unreachable_block();
    eliminates_dead_pure_values_but_keeps_checked_arithmetic();
    keeps_unused_floating_division_that_can_trap();
    keeps_void_calls_even_when_their_result_is_unused();
    exposes_ordered_and_validated_optimizer_pipelines();
    common_subexpressions_rewrite_block_edge_uses();
    common_subexpression_pass_does_not_merge_checked_divisions();
    folds_safe_numeric_conversions_and_preserves_range_traps();
    copy_propagates_identical_values_from_all_predecessor_edges();
    copy_propagation_keeps_join_parameters_with_different_inputs();
    copy_propagation_compacts_multiple_parameters_on_true_and_false_edges();
    algebraic_simplification_applies_safe_integer_identities();
    algebraic_simplification_does_not_hide_divide_by_zero();
    puts("YLang IR optimizer tests passed");
    return 0;
}
