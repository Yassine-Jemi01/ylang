#include "ylang/ir.h"
#include <assert.h>
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

int main(void)
{
    valid_arithmetic_return(); reject_use_before_definition();
    reject_missing_terminator(); reject_wrong_return_type();
    reject_missing_branch_target(); reject_terminator_result();
    reject_duplicate_ssa_across_blocks(); reject_invalid_return_type();
    accept_value_from_dominating_block(); reject_value_not_dominating_join();
    reject_unreachable_block();
    accept_block_parameter_join(); reject_block_parameter_type_mismatch();
    reject_entry_block_parameters(); reject_missing_block_argument();
    puts("YLang IR verifier tests passed"); return 0;
}
