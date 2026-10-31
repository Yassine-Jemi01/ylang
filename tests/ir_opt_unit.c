#include "ylang/ir.h"

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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

    assert(yir_optimize_function(&function,error,sizeof(error)));
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
    puts("YLang IR optimizer tests passed");
    return 0;
}
