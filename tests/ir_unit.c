#include "ylang/ir.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void valid_arithmetic_return(void)
{
    const YIRInstruction ins[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,.immediate=20},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,.immediate=22},
        {.opcode=YIR_OP_ADD_I64,.result_id=3,.result_type=YIR_TYPE_I64,.lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    const YIRBlock block={.id=0,.instructions=ins,.instruction_count=4};
    const YIRFunction fn={.name="answer",.return_type=YIR_TYPE_I64,.blocks=&block,.block_count=1};
    char error[160]; assert(yir_verify_function(&fn,error,sizeof(error))); assert(error[0]=='\0');
}
static void reject_use_before_definition(void)
{
    const YIRInstruction ins[]={
        {.opcode=YIR_OP_ADD_I64,.result_id=2,.result_type=YIR_TYPE_I64,.lhs=1,.rhs=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock block={.id=0,.instructions=ins,.instruction_count=2};
    const YIRFunction fn={.name="bad",.return_type=YIR_TYPE_I64,.blocks=&block,.block_count=1};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"SSA dominance")!=NULL);
}
static void reject_missing_terminator(void)
{
    const YIRInstruction ins[]={ {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64} };
    const YIRBlock block={.id=0,.instructions=ins,.instruction_count=1};
    const YIRFunction fn={.name="bad",.return_type=YIR_TYPE_VOID,.blocks=&block,.block_count=1};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"terminator")!=NULL);
}
static void reject_wrong_return_type(void)
{
    const YIRInstruction ins[]={
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock block={.id=0,.instructions=ins,.instruction_count=2};
    const YIRFunction fn={.name="bad",.return_type=YIR_TYPE_BOOL,.blocks=&block,.block_count=1};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"SSA dominance")!=NULL);
}

static void reject_missing_branch_target(void)
{
    const YIRInstruction ins[]={ {.opcode=YIR_OP_BRANCH,.target=99} };
    const YIRBlock block={.id=0,.instructions=ins,.instruction_count=1};
    const YIRFunction fn={.name="bad",.return_type=YIR_TYPE_VOID,.blocks=&block,.block_count=1};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"target does not exist")!=NULL);
}
static void reject_terminator_result(void)
{
    const YIRInstruction ins[]={
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_RETURN,.result_id=2,.lhs=1}
    };
    const YIRBlock block={.id=0,.instructions=ins,.instruction_count=2};
    const YIRFunction fn={.name="bad",.return_type=YIR_TYPE_I64,.blocks=&block,.block_count=1};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"terminators cannot define")!=NULL);
}


static void reject_duplicate_ssa_across_blocks(void)
{
    const YIRInstruction first_ins[]={
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_BRANCH,.target=1}
    };
    const YIRInstruction second_ins[]={
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock blocks[]={
        {.id=0,.instructions=first_ins,.instruction_count=2},
        {.id=1,.instructions=second_ins,.instruction_count=2}
    };
    const YIRFunction fn={.name="duplicate",.return_type=YIR_TYPE_I64,.blocks=blocks,.block_count=2};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"unique across the function")!=NULL);
}


static void reject_invalid_return_type(void)
{
    const YIRInstruction ins[]={ {.opcode=YIR_OP_RETURN} };
    const YIRBlock block={.id=0,.instructions=ins,.instruction_count=1};
    const YIRFunction fn={.name="bad",.return_type=(YIRType)-1,.blocks=&block,.block_count=1};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"invalid return type")!=NULL);
}


static void accept_value_from_dominating_block(void)
{
    const YIRInstruction entry_ins[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=10,.result_type=YIR_TYPE_I64,.immediate=42},
        {.opcode=YIR_OP_BRANCH,.target=1}
    };
    const YIRInstruction exit_ins[] = { {.opcode=YIR_OP_RETURN,.lhs=10} };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry_ins,.instruction_count=2},
        {.id=1,.instructions=exit_ins,.instruction_count=1}
    };
    const YIRFunction fn={.name="dominated",.return_type=YIR_TYPE_I64,
                          .blocks=blocks,.block_count=2};
    char error[160]; assert(yir_verify_function(&fn,error,sizeof(error)));
}

static void reject_value_not_dominating_join(void)
{
    const YIRInstruction entry_ins[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_EQ_I64,.result_id=3,.result_type=YIR_TYPE_BOOL,.lhs=1,.rhs=2},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=3,.target=1,.false_target=2}
    };
    const YIRInstruction left_ins[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=4,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_BRANCH,.target=3}
    };
    const YIRInstruction right_ins[] = { {.opcode=YIR_OP_BRANCH,.target=3} };
    const YIRInstruction join_ins[] = { {.opcode=YIR_OP_RETURN,.lhs=4} };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry_ins,.instruction_count=4},
        {.id=1,.instructions=left_ins,.instruction_count=2},
        {.id=2,.instructions=right_ins,.instruction_count=1},
        {.id=3,.instructions=join_ins,.instruction_count=1}
    };
    const YIRFunction fn={.name="bad_join",.return_type=YIR_TYPE_I64,
                          .blocks=blocks,.block_count=4};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"SSA dominance")!=NULL);
}

static void reject_unreachable_block(void)
{
    const YIRInstruction entry_ins[] = { {.opcode=YIR_OP_RETURN} };
    const YIRInstruction dead_ins[] = { {.opcode=YIR_OP_RETURN} };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry_ins,.instruction_count=1},
        {.id=1,.instructions=dead_ins,.instruction_count=1}
    };
    const YIRFunction fn={.name="dead",.return_type=YIR_TYPE_VOID,
                          .blocks=blocks,.block_count=2};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"reachable")!=NULL);
}


static void accept_block_parameter_join(void)
{
    const uint32_t left_args[] = {1};
    const uint32_t right_args[] = {2};
    const YIRBlockParameter join_params[] = {
        {.id=4,.type=YIR_TYPE_I64}
    };
    const YIRInstruction entry_ins[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_EQ_I64,.result_id=3,.result_type=YIR_TYPE_BOOL,.lhs=1,.rhs=2},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=3,.target=1,.false_target=2}
    };
    const YIRInstruction left_ins[] = {
        {.opcode=YIR_OP_BRANCH,.target=3,.arguments=left_args,.argument_count=1}
    };
    const YIRInstruction right_ins[] = {
        {.opcode=YIR_OP_BRANCH,.target=3,.arguments=right_args,.argument_count=1}
    };
    const YIRInstruction join_ins[] = { {.opcode=YIR_OP_RETURN,.lhs=4} };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry_ins,.instruction_count=4},
        {.id=1,.instructions=left_ins,.instruction_count=1},
        {.id=2,.instructions=right_ins,.instruction_count=1},
        {.id=3,.parameters=join_params,.parameter_count=1,
         .instructions=join_ins,.instruction_count=1}
    };
    const YIRFunction fn={.name="block_parameter_join",
        .return_type=YIR_TYPE_I64,.blocks=blocks,.block_count=4};
    char error[160]; assert(yir_verify_function(&fn,error,sizeof(error)));
}

static void reject_block_parameter_type_mismatch(void)
{
    const uint32_t args[] = {1};
    const YIRBlockParameter params[] = {
        {.id=2,.type=YIR_TYPE_BOOL}
    };
    const YIRInstruction entry_ins[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_BRANCH,.target=1,.arguments=args,.argument_count=1}
    };
    const YIRInstruction exit_ins[] = { {.opcode=YIR_OP_RETURN} };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry_ins,.instruction_count=2},
        {.id=1,.parameters=params,.parameter_count=1,
         .instructions=exit_ins,.instruction_count=1}
    };
    const YIRFunction fn={.name="bad_block_parameter",
        .return_type=YIR_TYPE_VOID,.blocks=blocks,.block_count=2};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"branch argument type")!=NULL);
}


static void reject_entry_block_parameters(void)
{
    const YIRBlockParameter params[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction ins[] = { {.opcode=YIR_OP_RETURN,.lhs=1} };
    const YIRBlock block = {
        .id=0,.parameters=params,.parameter_count=1,
        .instructions=ins,.instruction_count=1
    };
    const YIRFunction fn={.name="entry_parameter",.return_type=YIR_TYPE_I64,
                          .blocks=&block,.block_count=1};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"entry block parameters")!=NULL);
}

static void reject_missing_block_argument(void)
{
    const YIRBlockParameter params[] = {
        {.id=2,.type=YIR_TYPE_I64}
    };
    const YIRInstruction entry_ins[] = {
        {.opcode=YIR_OP_BRANCH,.target=1}
    };
    const YIRInstruction exit_ins[] = { {.opcode=YIR_OP_RETURN,.lhs=2} };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry_ins,.instruction_count=1},
        {.id=1,.parameters=params,.parameter_count=1,
         .instructions=exit_ins,.instruction_count=1}
    };
    const YIRFunction fn={.name="missing_argument",.return_type=YIR_TYPE_I64,
                          .blocks=blocks,.block_count=2};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"argument count")!=NULL);
}


static void accept_function_parameter_return(void)
{
    const YIRFunctionParameter params[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction ins[] = { {.opcode=YIR_OP_RETURN,.lhs=1} };
    const YIRBlock block = {.id=0,.instructions=ins,.instruction_count=1};
    const YIRFunction fn = {
        .name="identity",.return_type=YIR_TYPE_I64,
        .parameters=params,.parameter_count=1,.blocks=&block,.block_count=1
    };
    char error[160]; assert(yir_verify_function(&fn,error,sizeof(error)));
}

static void reject_duplicate_function_parameter_ids(void)
{
    const YIRFunctionParameter params[] = {
        {.id=1,.type=YIR_TYPE_I64},
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction ins[] = { {.opcode=YIR_OP_RETURN} };
    const YIRBlock block = {.id=0,.instructions=ins,.instruction_count=1};
    const YIRFunction fn = {
        .name="duplicate_parameters",.return_type=YIR_TYPE_VOID,
        .parameters=params,.parameter_count=2,.blocks=&block,.block_count=1
    };
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"function parameters")!=NULL);
}


static void reject_incomplete_source_location(void)
{
    const YIRInstruction ins[] = {
        {.opcode=YIR_OP_RETURN,.source_line=4,.source_column=0}
    };
    const YIRBlock block={.id=0,.instructions=ins,.instruction_count=1};
    const YIRFunction fn={.name="bad_span",.return_type=YIR_TYPE_VOID,
                          .blocks=&block,.block_count=1};
    char error[160]; assert(!yir_verify_function(&fn,error,sizeof(error)));
    assert(strstr(error,"source line and column")!=NULL);
}

static void accept_typed_direct_call(void)
{
    const uint32_t call_arguments[] = {1};
    const YIRType call_parameter_types[] = {YIR_TYPE_I64};
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=41},
        {.opcode=YIR_OP_CALL,.result_id=2,.result_type=YIR_TYPE_I64,
         .callee="increment",.arguments=call_arguments,.argument_count=1,
         .call_parameter_types=call_parameter_types,.call_parameter_count=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=3
    };
    const YIRFunction function = {
        .name="caller",.return_type=YIR_TYPE_I64,
        .blocks=&block,.block_count=1
    };
    char error[160];
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void reject_call_argument_type_mismatch(void)
{
    const uint32_t call_arguments[] = {1};
    const YIRType call_parameter_types[] = {YIR_TYPE_BOOL};
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_CALL,.result_id=2,.result_type=YIR_TYPE_I64,
         .callee="expects_bool",.arguments=call_arguments,.argument_count=1,
         .call_parameter_types=call_parameter_types,.call_parameter_count=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=3
    };
    const YIRFunction function = {
        .name="bad_call",.return_type=YIR_TYPE_I64,
        .blocks=&block,.block_count=1
    };
    char error[160];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"SSA dominance")!=NULL);
}

static void reject_call_without_callee(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CALL,.callee=NULL,
         .result_type=YIR_TYPE_VOID},
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=2
    };
    const YIRFunction function = {
        .name="bad_call_metadata",.return_type=YIR_TYPE_VOID,
        .blocks=&block,.block_count=1
    };
    char error[160];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"call instructions require")!=NULL);
}

static void accept_module_resolved_call(void)
{
    const YIRFunctionParameter callee_parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction callee_instructions[] = {
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock callee_block = {
        .id=0,.instructions=callee_instructions,.instruction_count=1
    };
    const YIRFunction callee = {
        .name="identity",.return_type=YIR_TYPE_I64,
        .parameters=callee_parameters,.parameter_count=1,
        .blocks=&callee_block,.block_count=1
    };

    const uint32_t arguments[] = {1};
    const YIRType argument_types[] = {YIR_TYPE_I64};
    const YIRInstruction caller_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=42},
        {.opcode=YIR_OP_CALL,.result_id=2,.result_type=YIR_TYPE_I64,
         .callee="identity",.arguments=arguments,.argument_count=1,
         .call_parameter_types=argument_types,.call_parameter_count=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock caller_block = {
        .id=0,.instructions=caller_instructions,.instruction_count=3
    };
    const YIRFunction caller = {
        .name="main",.return_type=YIR_TYPE_I64,
        .blocks=&caller_block,.block_count=1
    };
    const YIRFunction module[] = {callee,caller};
    char error[192];
    assert(yir_verify_module(module,2,error,sizeof(error)));
}

static void reject_module_missing_call_target(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CALL,.result_type=YIR_TYPE_VOID,.callee="missing"},
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=2
    };
    const YIRFunction function = {
        .name="caller",.return_type=YIR_TYPE_VOID,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_module(&function,1,error,sizeof(error)));
    assert(strstr(error,"calls missing function symbol 'missing'")!=NULL);
}

static void reject_module_call_signature_mismatch(void)
{
    const YIRFunctionParameter callee_parameters[] = {
        {.id=1,.type=YIR_TYPE_BOOL}
    };
    const YIRInstruction callee_instructions[] = {
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock callee_block = {
        .id=0,.instructions=callee_instructions,.instruction_count=1
    };
    const YIRFunction callee = {
        .name="expects_bool",.return_type=YIR_TYPE_BOOL,
        .parameters=callee_parameters,.parameter_count=1,
        .blocks=&callee_block,.block_count=1
    };

    const uint32_t arguments[] = {1};
    const YIRType argument_types[] = {YIR_TYPE_I64};
    const YIRInstruction caller_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64},
        {.opcode=YIR_OP_CALL,.result_id=2,.result_type=YIR_TYPE_BOOL,
         .callee="expects_bool",.arguments=arguments,.argument_count=1,
         .call_parameter_types=argument_types,.call_parameter_count=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock caller_block = {
        .id=0,.instructions=caller_instructions,.instruction_count=3
    };
    const YIRFunction caller = {
        .name="caller",.return_type=YIR_TYPE_BOOL,
        .blocks=&caller_block,.block_count=1
    };
    const YIRFunction module[] = {callee,caller};
    char error[192];
    assert(!yir_verify_module(module,2,error,sizeof(error)));
    assert(strstr(error,"wrong type")!=NULL);
}

static void accept_module_qualified_same_name_call_targets(void)
{
    const YIRInstruction return_instructions[] = {
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock target_block = {
        .id=0,.instructions=return_instructions,.instruction_count=1
    };
    const YIRFunction first_target = {
        .name="helper",.symbol="lib_a::helper",
        .return_type=YIR_TYPE_VOID,
        .blocks=&target_block,.block_count=1
    };
    const YIRFunction second_target = {
        .name="helper",.symbol="lib_b::helper",
        .return_type=YIR_TYPE_VOID,
        .blocks=&target_block,.block_count=1
    };
    const YIRInstruction caller_instructions[] = {
        {.opcode=YIR_OP_CALL,.result_type=YIR_TYPE_VOID,
         .callee="helper",.callee_symbol="lib_b::helper"},
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock caller_block = {
        .id=0,.instructions=caller_instructions,.instruction_count=2
    };
    const YIRFunction caller = {
        .name="main",.symbol="app::main",
        .return_type=YIR_TYPE_VOID,
        .blocks=&caller_block,.block_count=1
    };
    const YIRFunction module[] = {first_target,second_target,caller};
    char error[192];
    assert(yir_verify_module(module,3,error,sizeof(error)));
}

static void accept_module_qualified_same_name_call_targets(void)
{
    const YIRInstruction return_instructions[] = {
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock target_block = {
        .id=0,.instructions=return_instructions,.instruction_count=1
    };
    const YIRFunction first_target = {
        .name="helper",.symbol="lib_a::helper",
        .return_type=YIR_TYPE_VOID,
        .blocks=&target_block,.block_count=1
    };
    const YIRFunction second_target = {
        .name="helper",.symbol="lib_b::helper",
        .return_type=YIR_TYPE_VOID,
        .blocks=&target_block,.block_count=1
    };
    const YIRInstruction caller_instructions[] = {
        {.opcode=YIR_OP_CALL,.result_type=YIR_TYPE_VOID,
         .callee="helper",.callee_symbol="lib_b::helper"},
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock caller_block = {
        .id=0,.instructions=caller_instructions,.instruction_count=2
    };
    const YIRFunction caller = {
        .name="main",.symbol="app::main",
        .return_type=YIR_TYPE_VOID,
        .blocks=&caller_block,.block_count=1
    };
    const YIRFunction module[] = {first_target,second_target,caller};
    char error[192];
    assert(yir_verify_module(module,3,error,sizeof(error)));
}

static void reject_duplicate_qualified_function_symbols(void)
{
    const YIRInstruction return_instructions[] = {
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock block = {
        .id=0,.instructions=return_instructions,.instruction_count=1
    };
    const YIRFunction functions[] = {
        {.name="helper",.symbol="lib::helper",.return_type=YIR_TYPE_VOID,
         .blocks=&block,.block_count=1},
        {.name="other",.symbol="lib::helper",.return_type=YIR_TYPE_VOID,
         .blocks=&block,.block_count=1}
    };
    char error[192];
    assert(!yir_verify_module(functions,2,error,sizeof(error)));
    assert(strstr(error,"duplicate function symbol 'lib::helper'")!=NULL);
}

static void reject_duplicate_symbol_module_call_target(void)
{
    const YIRInstruction return_instructions[] = {
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock target_block = {
        .id=0,.instructions=return_instructions,.instruction_count=1
    };
    const YIRFunction first_target = {
        .name="duplicate",.return_type=YIR_TYPE_VOID,
        .blocks=&target_block,.block_count=1
    };
    const YIRFunction second_target = {
        .name="duplicate",.return_type=YIR_TYPE_VOID,
        .blocks=&target_block,.block_count=1
    };
    const YIRInstruction caller_instructions[] = {
        {.opcode=YIR_OP_CALL,.result_type=YIR_TYPE_VOID,.callee="duplicate"},
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock caller_block = {
        .id=0,.instructions=caller_instructions,.instruction_count=2
    };
    const YIRFunction caller = {
        .name="caller",.return_type=YIR_TYPE_VOID,
        .blocks=&caller_block,.block_count=1
    };
    const YIRFunction module[] = {first_target,second_target,caller};
    char error[192];
    assert(!yir_verify_module(module,3,error,sizeof(error)));
    assert(strstr(error,"duplicate function symbol 'duplicate'")!=NULL);
}

static void accept_integer_division_modulo_and_negation(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_I64},
        {.id=2,.type=YIR_TYPE_I64}
    };
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_DIV_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_MOD_I64,.result_id=4,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_NEG_I64,.result_id=5,.result_type=YIR_TYPE_I64,
         .lhs=3},
        {.opcode=YIR_OP_RETURN,.lhs=5}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=4
    };
    const YIRFunction function = {
        .name="integer_ops",.symbol="math::integer_ops",
        .return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=2,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void reject_non_integer_negation_operand(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_BOOL}
    };
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_NEG_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .lhs=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=2
    };
    const YIRFunction function = {
        .name="bad_negation",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=1,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"operand type")!=NULL);
}

static void accept_typed_f64_arithmetic_and_comparison(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_F64},
        {.id=2,.type=YIR_TYPE_F64}
    };
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_ADD_F64,.result_id=3,.result_type=YIR_TYPE_F64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_DIV_F64,.result_id=4,.result_type=YIR_TYPE_F64,
         .lhs=3,.rhs=2},
        {.opcode=YIR_OP_NEG_F64,.result_id=5,.result_type=YIR_TYPE_F64,
         .lhs=4},
        {.opcode=YIR_OP_LT_F64,.result_id=6,.result_type=YIR_TYPE_BOOL,
         .lhs=5,.rhs=1},
        {.opcode=YIR_OP_RETURN,.lhs=6}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=5
    };
    const YIRFunction function = {
        .name="compare_f64",.return_type=YIR_TYPE_BOOL,
        .parameters=parameters,.parameter_count=2,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void reject_non_finite_f64_constant(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_F64,.result_id=1,.result_type=YIR_TYPE_F64,
         .floating_immediate=NAN},
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=2
    };
    const YIRFunction function = {
        .name="non_finite",.return_type=YIR_TYPE_F64,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"operand type") != NULL);
}

static void reject_f64_operation_with_i64_operand(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=1},
        {.opcode=YIR_OP_CONST_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .floating_immediate=2.0},
        {.opcode=YIR_OP_ADD_F64,.result_id=3,.result_type=YIR_TYPE_F64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=4
    };
    const YIRFunction function = {
        .name="bad_mixed_float",.return_type=YIR_TYPE_F64,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"operand type") != NULL);
}


static void reject_invalid_opcode_value(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=(YIROpcode)255},
        {.opcode=YIR_OP_RETURN}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=2
    };
    const YIRFunction function = {
        .name="bad_opcode",.return_type=YIR_TYPE_VOID,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"invalid opcode")!=NULL);
}

static void reject_branch_metadata_on_arithmetic_instruction(void)
{
    const uint32_t edge_value[] = {1};
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_ADD_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=1,.arguments=edge_value,.argument_count=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=2
    };
    const YIRFunction function = {
        .name="bad_edge_metadata",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=1,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"edge argument metadata")!=NULL);
}

static void reject_module_symbol_on_non_call_instruction(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=42,.callee_symbol=(char *)"unexpected"},
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=2
    };
    const YIRFunction function = {
        .name="bad_symbol_metadata",.return_type=YIR_TYPE_I64,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"call metadata")!=NULL);
}

static void reject_missing_false_edge_argument_storage(void)
{
    const YIRBlockParameter target_parameter[] = {
        {.id=3,.type=YIR_TYPE_I64}
    };
    const YIRInstruction entry[] = {
        {.opcode=YIR_OP_CONST_BOOL,.result_id=1,.result_type=YIR_TYPE_BOOL,
         .immediate=1},
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=7},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=1,.target=1,.false_target=2,
         .arguments=(const uint32_t[]){2},.argument_count=1,
         .false_argument_count=1}
    };
    const YIRInstruction selected[] = {{.opcode=YIR_OP_RETURN,.lhs=3}};
    const YIRInstruction fallback[] = {{.opcode=YIR_OP_RETURN}};
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry,.instruction_count=3},
        {.id=1,.parameters=target_parameter,.parameter_count=1,
         .instructions=selected,.instruction_count=1},
        {.id=2,.instructions=fallback,.instruction_count=1}
    };
    const YIRFunction function = {
        .name="missing_false_edge_storage",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=3
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"branch argument counts and arrays must agree")!=NULL);
}


static void reject_unexpected_true_edge_argument_storage(void)
{
    const uint32_t unused_arguments[] = {1};
    const YIRInstruction entry[] = {
        {.opcode=YIR_OP_CONST_BOOL,.result_id=1,.result_type=YIR_TYPE_BOOL,
         .immediate=1},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=1,.target=1,.false_target=2,
         .arguments=unused_arguments,.argument_count=0}
    };
    const YIRInstruction exit[] = {{.opcode=YIR_OP_RETURN}};
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry,.instruction_count=2},
        {.id=1,.instructions=exit,.instruction_count=1},
        {.id=2,.instructions=exit,.instruction_count=1}
    };
    const YIRFunction function = {
        .name="unexpected_true_edge_storage",.return_type=YIR_TYPE_VOID,
        .blocks=blocks,.block_count=3
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"branch argument counts and arrays must agree")!=NULL);
}

static void reject_unexpected_false_edge_argument_storage(void)
{
    const uint32_t unused_arguments[] = {1};
    const YIRInstruction entry[] = {
        {.opcode=YIR_OP_CONST_BOOL,.result_id=1,.result_type=YIR_TYPE_BOOL,
         .immediate=1},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=1,.target=1,.false_target=2,
         .false_arguments=unused_arguments,.false_argument_count=0}
    };
    const YIRInstruction exit[] = {{.opcode=YIR_OP_RETURN}};
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry,.instruction_count=2},
        {.id=1,.instructions=exit,.instruction_count=1},
        {.id=2,.instructions=exit,.instruction_count=1}
    };
    const YIRFunction function = {
        .name="unexpected_false_edge_storage",.return_type=YIR_TYPE_VOID,
        .blocks=blocks,.block_count=3
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"branch argument counts and arrays must agree")!=NULL);
}

static void accept_explicit_numeric_conversion_ops(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=17},
        {.opcode=YIR_OP_I64_TO_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .lhs=1},
        {.opcode=YIR_OP_F64_TO_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=4
    };
    const YIRFunction function = {
        .name="numeric_conversions",.return_type=YIR_TYPE_I64,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(yir_verify_function(&function,error,sizeof(error)));
}

static void reject_numeric_conversion_operand_type_mismatch(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=17},
        {.opcode=YIR_OP_F64_TO_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .lhs=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=3
    };
    const YIRFunction function = {
        .name="bad_numeric_conversion",.return_type=YIR_TYPE_I64,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"operand type") != NULL);
}


static void reject_i64_to_f64_with_float_operand(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_F64,.result_id=1,.result_type=YIR_TYPE_F64,
         .floating_immediate=2.5},
        {.opcode=YIR_OP_I64_TO_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .lhs=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=3
    };
    const YIRFunction function = {
        .name="bad_i64_to_f64",.return_type=YIR_TYPE_F64,
        .blocks=&block,.block_count=1
    };
    char error[192];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"operand type") != NULL);
}

int main(void)
{
    valid_arithmetic_return(); reject_use_before_definition();
    reject_missing_terminator(); reject_wrong_return_type();
    reject_missing_branch_target(); reject_terminator_result();
    reject_duplicate_ssa_across_blocks(); reject_invalid_return_type();
    accept_value_from_dominating_block(); reject_value_not_dominating_join();
    reject_unreachable_block();
    accept_function_parameter_return();
    reject_duplicate_function_parameter_ids();
    reject_incomplete_source_location();
    accept_block_parameter_join(); reject_block_parameter_type_mismatch();
    reject_entry_block_parameters(); reject_missing_block_argument();
    accept_typed_direct_call();
    reject_call_argument_type_mismatch();
    reject_call_without_callee();
    accept_module_resolved_call();
    reject_module_missing_call_target();
    reject_module_call_signature_mismatch();
    reject_duplicate_symbol_module_call_target();
    accept_module_qualified_same_name_call_targets();
    accept_integer_division_modulo_and_negation();
    reject_non_integer_negation_operand();
    accept_typed_f64_arithmetic_and_comparison();
    reject_non_finite_f64_constant();
    reject_f64_operation_with_i64_operand();
    reject_invalid_opcode_value();
    reject_branch_metadata_on_arithmetic_instruction();
    reject_module_symbol_on_non_call_instruction();
    reject_missing_false_edge_argument_storage();
    reject_unexpected_true_edge_argument_storage();
    reject_unexpected_false_edge_argument_storage();
    accept_explicit_numeric_conversion_ops();
    reject_numeric_conversion_operand_type_mismatch();
    reject_i64_to_f64_with_float_operand();
    reject_duplicate_qualified_function_symbols();
    accept_module_qualified_same_name_call_targets();
    puts("YLang IR verifier tests passed"); return 0;
}
