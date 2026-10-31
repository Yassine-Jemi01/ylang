#include "ylang/ir.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void assert_stream_equals(FILE *stream, const char *expected)
{
    char actual[512];
    assert(fflush(stream) == 0);
    rewind(stream);
    size_t count = fread(actual, 1, sizeof(actual) - 1, stream);
    assert(!ferror(stream));
    actual[count] = '\0';
    assert(strcmp(actual, expected) == 0);
}

static void prints_scalar_function(void)
{
    const YIRFunctionParameter parameters[] = {
        {.id = 1, .type = YIR_TYPE_I64}
    };
    const YIRInstruction instructions[] = {
        {
            .opcode = YIR_OP_CONST_I64,
            .result_id = 2,
            .result_type = YIR_TYPE_I64,
            .immediate = 42,
            .source_line = 3,
            .source_column = 5
        },
        {
            .opcode = YIR_OP_ADD_I64,
            .result_id = 3,
            .result_type = YIR_TYPE_I64,
            .lhs = 1,
            .rhs = 2
        },
        {
            .opcode = YIR_OP_RETURN,
            .lhs = 3
        }
    };
    const YIRBlock block = {
        .id = 0,
        .instructions = instructions,
        .instruction_count = sizeof(instructions) / sizeof(instructions[0])
    };
    const YIRFunction function = {
        .name = "add_answer",
        .return_type = YIR_TYPE_I64,
        .parameters = parameters,
        .parameter_count = sizeof(parameters) / sizeof(parameters[0]),
        .blocks = &block,
        .block_count = 1
    };

    FILE *stream = tmpfile();
    assert(stream != NULL);
    yir_print_function(stream, &function);
    assert_stream_equals(stream,
        "fn add_answer(%1: i64) -> i64 {\n"
        "bb0:\n"
        "  %2 = const.i64 42  ; @3:5\n"
        "  %3 = add.i64 %1, %2\n"
        "  return %3\n"
        "}\n");
    assert(fclose(stream) == 0);
}

static void prints_typed_control_flow_edges(void)
{
    const YIRBlockParameter then_parameters[] = {
        {.id = 3, .type = YIR_TYPE_I64}
    };
    const YIRBlockParameter else_parameters[] = {
        {.id = 4, .type = YIR_TYPE_I64}
    };
    const uint32_t then_arguments[] = {1};
    const uint32_t else_arguments[] = {1};
    const YIRInstruction entry_instructions[] = {
        {
            .opcode = YIR_OP_CONST_I64,
            .result_id = 1,
            .result_type = YIR_TYPE_I64,
            .immediate = 7
        },
        {
            .opcode = YIR_OP_CONST_BOOL,
            .result_id = 2,
            .result_type = YIR_TYPE_BOOL,
            .immediate = 1
        },
        {
            .opcode = YIR_OP_COND_BRANCH,
            .lhs = 2,
            .target = 1,
            .false_target = 2,
            .arguments = then_arguments,
            .argument_count = 1,
            .false_arguments = else_arguments,
            .false_argument_count = 1
        }
    };
    const YIRInstruction then_instructions[] = {
        {.opcode = YIR_OP_RETURN, .lhs = 3}
    };
    const YIRInstruction else_instructions[] = {
        {.opcode = YIR_OP_RETURN, .lhs = 4}
    };
    const YIRBlock blocks[] = {
        {
            .id = 0,
            .instructions = entry_instructions,
            .instruction_count = sizeof(entry_instructions) / sizeof(entry_instructions[0])
        },
        {
            .id = 1,
            .parameters = then_parameters,
            .parameter_count = 1,
            .instructions = then_instructions,
            .instruction_count = 1
        },
        {
            .id = 2,
            .parameters = else_parameters,
            .parameter_count = 1,
            .instructions = else_instructions,
            .instruction_count = 1
        }
    };
    const YIRFunction function = {
        .name = "choose",
        .return_type = YIR_TYPE_I64,
        .blocks = blocks,
        .block_count = sizeof(blocks) / sizeof(blocks[0])
    };

    FILE *stream = tmpfile();
    assert(stream != NULL);
    yir_print_function(stream, &function);
    assert_stream_equals(stream,
        "fn choose() -> i64 {\n"
        "bb0:\n"
        "  %1 = const.i64 7\n"
        "  %2 = const.bool true\n"
        "  cond_br %2, bb1(%1), bb2(%1)\n"
        "bb1(%3: i64):\n"
        "  return %3\n"
        "bb2(%4: i64):\n"
        "  return %4\n"
        "}\n");
    assert(fclose(stream) == 0);
}

static void prints_direct_function_calls(void)
{
    const uint32_t arguments[] = {1};
    const YIRType parameter_types[] = {YIR_TYPE_I64};
    const YIRInstruction instructions[] = {
        {.opcode=YIR_OP_CONST_I64,.result_id=1,.result_type=YIR_TYPE_I64,
         .immediate=7},
        {.opcode=YIR_OP_CALL,.result_id=2,.result_type=YIR_TYPE_I64,
         .callee="increment",.arguments=arguments,.argument_count=1,
         .call_parameter_types=parameter_types,.call_parameter_count=1},
        {.opcode=YIR_OP_RETURN,.lhs=2}
    };
    const YIRBlock block = {
        .id=0,.instructions=instructions,.instruction_count=3
    };
    const YIRFunction function = {
        .name="caller",.return_type=YIR_TYPE_I64,
        .blocks=&block,.block_count=1
    };
    FILE *stream = tmpfile();
    assert(stream != NULL);
    yir_print_function(stream, &function);
    rewind(stream);

    char output[512] = {0};
    size_t length = fread(output, 1, sizeof(output) - 1, stream);
    output[length] = '\0';
    assert(strstr(output, "%2 = call @increment(%1)") != NULL);
    assert(fclose(stream) == 0);
}

static void prints_division_modulo_and_negation(void)
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
        .name="integer_ops",.return_type=YIR_TYPE_I64,
        .parameters=parameters,.parameter_count=2,
        .blocks=&block,.block_count=1
    };
    FILE *stream = tmpfile();
    assert(stream != NULL);
    yir_print_function(stream,&function);
    assert_stream_equals(stream,
        "fn integer_ops(%1: i64, %2: i64) -> i64 {\n"
        "bb0:\n"
        "  %3 = div.i64 %1, %2\n"
        "  %4 = mod.i64 %1, %2\n"
        "  %5 = neg.i64 %3\n"
        "  return %5\n"
        "}\n");
    assert(fclose(stream) == 0);
}

int main(void)
{
    prints_scalar_function();
    prints_typed_control_flow_edges();
    prints_direct_function_calls();
    prints_division_modulo_and_negation();
    puts("YLang IR printer tests passed");
    return 0;
}
