#include "ylang/ir.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void writes_escaped_names_and_call_metadata(void)
{
    const YIRFunctionParameter callee_parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction callee_instructions[] = {
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock callee_blocks[] = {
        {.id=0,.instructions=callee_instructions,.instruction_count=1}
    };
    const YIRFunction callee = {
        .name="he\"lper\n",.symbol="module::he\"lper\n",
        .return_type=YIR_TYPE_I64,
        .parameters=callee_parameters,.parameter_count=1,
        .blocks=callee_blocks,.block_count=1
    };

    const uint32_t arguments[] = {1};
    const YIRType call_types[] = {YIR_TYPE_I64};
    const YIRFunctionParameter caller_parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction caller_instructions[] = {
        {.opcode=YIR_OP_CALL,.result_id=2,.result_type=YIR_TYPE_I64,
         .callee="he\"lper\n",.callee_symbol="module::he\"lper\n",
         .arguments=arguments,.argument_count=1,
         .call_parameter_types=call_types,.call_parameter_count=1,
         .source_line=4,.source_column=9},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock caller_blocks[] = {
        {.id=0,.instructions=caller_instructions,.instruction_count=2}
    };
    const YIRFunction caller = {
        .name="caller",.symbol="root::caller",.return_type=YIR_TYPE_I64,
        .parameters=caller_parameters,.parameter_count=1,
        .blocks=caller_blocks,.block_count=1
    };
    const YIRFunction module[] = {callee,caller};
    char error[256] = {0};
    FILE *stream = tmpfile();
    assert(stream != NULL);
    assert(yir_write_json_module(stream,module,2,error,sizeof(error)));
    assert(fflush(stream)==0);
    rewind(stream);

    char output[4096] = {0};
    size_t length=fread(output,1,sizeof(output)-1,stream);
    output[length]='\0';
    assert(!ferror(stream));
    assert(strstr(output,"\"format\":\"ylang.mir\"")!=NULL);
    assert(strstr(output,"\"name\":\"he\\\"lper\\n\"")!=NULL);
    assert(strstr(output,"\"callee\":\"he\\\"lper\\n\"")!=NULL);
    assert(strstr(output,"\"callee_symbol\":\"module::he\\\"lper\\n\"")!=NULL);
    assert(strstr(output,"\"arguments\":[1]")!=NULL);
    assert(strstr(output,"\"call_parameter_types\":[\"i64\"]")!=NULL);
    assert(strstr(output,"\"source\":{\"line\":4,\"column\":9}")!=NULL);
    assert(fclose(stream)==0);
}

static void preserves_negative_zero_as_a_json_number(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_F64,.result_id=1,.result_type=YIR_TYPE_F64,
         .floating_immediate=-0.0},
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=2}
    };
    const YIRFunction function = {
        .name="negative_zero",.return_type=YIR_TYPE_F64,
        .blocks=blocks,.block_count=1
    };
    char error[256]={0};
    FILE *stream=tmpfile();
    assert(stream!=NULL);
    assert(yir_write_json_module(stream,&function,1,error,sizeof(error)));
    assert(fflush(stream)==0);
    rewind(stream);
    char output[1024]={0};
    size_t length=fread(output,1,sizeof(output)-1,stream);
    output[length]='\0';
    assert(strstr(output,"\"floating_immediate\":-0.0")!=NULL);
    assert(fclose(stream)==0);
}

static void rejects_invalid_modules_before_writing(void)
{
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_RETURN,.lhs=99}
    };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=instructions,.instruction_count=1}
    };
    const YIRFunction function = {
        .name="invalid",.return_type=YIR_TYPE_I64,
        .blocks=blocks,.block_count=1
    };
    char error[256]={0};
    FILE *stream=tmpfile();
    assert(stream!=NULL);
    assert(!yir_write_json_module(stream,&function,1,error,sizeof(error)));
    assert(error[0]!='\0');
    assert(fflush(stream)==0);
    assert(ftell(stream)==0);
    assert(fclose(stream)==0);
}


static void serializes_numeric_conversion_opcode_names(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_I64_TO_F64,.result_id=2,.result_type=YIR_TYPE_F64,
         .lhs=1},
        {.opcode=YIR_OP_F64_TO_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=3
    };
    const YIRFunction function = {
        .name="convert",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=1,
        .blocks=&block,.block_count=1
    };
    FILE *stream=tmpfile();
    assert(stream!=NULL);
    char error[256];
    assert(yir_write_json_module(stream,&function,1,error,sizeof(error)));
    char output[2048]={0};
    assert(fflush(stream)==0);
    rewind(stream);
    size_t length=fread(output,1,sizeof(output)-1,stream);
    output[length]='\0';
    assert(strstr(output,"i64.to_f64")!=NULL);
    assert(strstr(output,"f64.to_i64")!=NULL);
    assert(fclose(stream)==0);
}

int main(void)
{
    writes_escaped_names_and_call_metadata();
    preserves_negative_zero_as_a_json_number();
    rejects_invalid_modules_before_writing();
    serializes_numeric_conversion_opcode_names();
    puts("YLang JSON IR tests passed");
    return 0;
}
