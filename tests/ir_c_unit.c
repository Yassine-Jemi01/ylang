#include "ylang/ir.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t read_stream(FILE *stream, char *buffer, size_t capacity)
{
    assert(stream != NULL);
    assert(capacity != 0);
    assert(fflush(stream) == 0);
    rewind(stream);
    size_t length = fread(buffer, 1, capacity - 1U, stream);
    buffer[length] = '\0';
    assert(!ferror(stream));
    return length;
}

static void emits_scalar_arithmetic_and_direct_calls(void)
{
    const YIRFunctionParameter increment_parameters[] = {
        {.id=1,.type=YIR_TYPE_I64}
    };
    const YIRInstruction increment_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=2,.result_type=YIR_TYPE_I64,
         .immediate=1},
        {.opcode=YIR_OP_ADD_I64,.result_id=3,.result_type=YIR_TYPE_I64,
         .lhs=1,.rhs=2},
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    const YIRBlock increment_block = {
        .id=0,.instructions=increment_instructions,.instruction_count=3
    };
    const YIRFunction increment = {
        .name="increment",.symbol="lib::increment",
        .return_type=YIR_TYPE_I64,
        .parameters=increment_parameters,.parameter_count=1,
        .blocks=&increment_block,.block_count=1
    };

    const uint32_t call_arguments[] = {1};
    const YIRType call_parameter_types[] = {YIR_TYPE_I64};
    const YIRInstruction main_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=41},
        {.opcode=YIR_OP_CALL,.result_id=2,.result_type=YIR_TYPE_I64,
         .callee="increment",.callee_symbol="lib::increment",
         .arguments=call_arguments,.argument_count=1,
         .call_parameter_types=call_parameter_types,.call_parameter_count=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock main_block = {
        .id=0,.instructions=main_instructions,.instruction_count=3
    };
    const YIRFunction main_function = {
        .name="main",.symbol="app::main",
        .return_type=YIR_TYPE_I64,
        .blocks=&main_block,.block_count=1
    };
    const YIRFunction module[] = {increment,main_function};

    FILE *stream = tmpfile();
    assert(stream != NULL);
    char error[256];
    assert(yir_emit_c_module(stream,module,2,error,sizeof(error)));
    char output[8192];
    (void)read_stream(stream,output,sizeof(output));

    assert(strstr(output,"#include <stdint.h>") != NULL);
    assert(strstr(output,"yir_add_i64(yir_v_1, yir_v_2)") != NULL);
    assert(strstr(output,
        "yl_ir_6c69623a3a696e6372656d656e74(yir_v_1)") != NULL);
    assert(strstr(output,"int main(void)") != NULL);
    assert(strstr(output,"integer overflow in addition") != NULL);
    assert(fclose(stream) == 0);
}

static void emits_control_flow_and_block_parameter_assignments(void)
{
    const uint32_t true_arguments[] = {1};
    const uint32_t false_arguments[] = {1};
    const YIRBlockParameter true_parameters[] = {
        {.id=3,.type=YIR_TYPE_I64}
    };
    const YIRBlockParameter false_parameters[] = {
        {.id=4,.type=YIR_TYPE_I64}
    };
    const YIRInstruction entry_instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=7},
        {.opcode=YIR_OP_CONST_BOOL,.result_id=2,.result_type=YIR_TYPE_BOOL,
         .immediate=1},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=2,
         .target=1,.arguments=true_arguments,.argument_count=1,
         .false_target=2,.false_arguments=false_arguments,
         .false_argument_count=1}
    };
    const YIRInstruction true_instructions[] = {
        {.opcode=YIR_OP_RETURN,.lhs=3}
    };
    const YIRInstruction false_instructions[] = {
        {.opcode=YIR_OP_RETURN,.lhs=4}
    };
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry_instructions,.instruction_count=3},
        {.id=1,.parameters=true_parameters,.parameter_count=1,
         .instructions=true_instructions,.instruction_count=1},
        {.id=2,.parameters=false_parameters,.parameter_count=1,
         .instructions=false_instructions,.instruction_count=1}
    };
    const YIRFunction function = {
        .name="choose",.symbol="app::choose",
        .return_type=YIR_TYPE_I64,.blocks=blocks,.block_count=3
    };

    FILE *stream = tmpfile();
    assert(stream != NULL);
    char error[256];
    assert(yir_emit_c_module(stream,&function,1,error,sizeof(error)));
    char output[8192];
    (void)read_stream(stream,output,sizeof(output));

    assert(strstr(output,"if (yir_v_2)") != NULL);
    assert(strstr(output,"yir_v_3 = yir_v_1;") != NULL);
    assert(strstr(output,"yir_v_4 = yir_v_1;") != NULL);
    assert(strstr(output,"goto yir_bb_1;") != NULL);
    assert(strstr(output,"goto yir_bb_2;") != NULL);
    assert(fclose(stream) == 0);
}

static void rejects_float_signatures_until_lowering_exists(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id=1,.type=YIR_TYPE_F64}
    };
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_RETURN,.lhs=1}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=1
    };
    const YIRFunction function = {
        .name="identity_float",.symbol="app::identity_float",
        .return_type=YIR_TYPE_F64,
        .parameters=parameters,.parameter_count=1,
        .blocks=&block,.block_count=1
    };

    FILE *stream = tmpfile();
    assert(stream != NULL);
    char error[256];
    assert(!yir_emit_c_module(stream,&function,1,error,sizeof(error)));
    assert(strstr(error,"only void, bool, and i64 scalar signatures") != NULL);
    assert(fclose(stream) == 0);
}

int main(void)
{
    emits_scalar_arithmetic_and_direct_calls();
    emits_control_flow_and_block_parameter_assignments();
    rejects_float_signatures_until_lowering_exists();
    puts("YLang IR C emitter tests passed");
    return 0;
}
