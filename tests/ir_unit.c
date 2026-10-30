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
    assert(strstr(error,"earlier i64 operands")!=NULL);
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
    assert(strstr(error,"wrong type")!=NULL);
}
int main(void)
{
    valid_arithmetic_return(); reject_use_before_definition();
    reject_missing_terminator(); reject_wrong_return_type();
    puts("YLang IR verifier tests passed"); return 0;
}
