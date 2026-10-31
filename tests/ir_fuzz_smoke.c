#include "ylang/ir.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    MAX_BLOCKS = 4,
    MAX_INSTRUCTIONS = 8,
    MAX_PARAMETERS = 3,
    MAX_ARGUMENTS = 3,
    FUZZ_BYTES = 128
};

static uint8_t byte_at(const uint8_t *data, size_t size, size_t offset)
{
    return size ? data[offset % size] : 0;
}

static double float_at(const uint8_t *data, size_t size, size_t offset)
{
    uint64_t bits = 0;
    for (size_t i = 0; i < sizeof(bits); ++i)
        bits |= (uint64_t)byte_at(data, size, offset + i) << (i * 8);
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (!data && size) return 0;

    YIRBlock blocks[MAX_BLOCKS];
    YIRInstruction instructions[MAX_BLOCKS][MAX_INSTRUCTIONS];
    YIRBlockParameter block_parameters[MAX_BLOCKS][MAX_PARAMETERS];
    YIRFunctionParameter function_parameters[MAX_PARAMETERS];
    uint32_t arguments[MAX_BLOCKS][MAX_INSTRUCTIONS][MAX_ARGUMENTS];
    uint32_t false_arguments[MAX_BLOCKS][MAX_INSTRUCTIONS][MAX_ARGUMENTS];
    YIRType call_parameter_types[MAX_BLOCKS][MAX_INSTRUCTIONS][MAX_ARGUMENTS];
    memset(blocks, 0, sizeof(blocks));
    memset(instructions, 0, sizeof(instructions));
    memset(block_parameters, 0, sizeof(block_parameters));
    memset(function_parameters, 0, sizeof(function_parameters));
    memset(arguments, 0, sizeof(arguments));
    memset(false_arguments, 0, sizeof(false_arguments));
    memset(call_parameter_types, 0, sizeof(call_parameter_types));

    size_t block_count = 1u + (size_t)(byte_at(data, size, 0) % MAX_BLOCKS);
    size_t parameter_count = (size_t)(byte_at(data, size, 1) % (MAX_PARAMETERS + 1));
    for (size_t pi = 0; pi < MAX_PARAMETERS; ++pi) {
        function_parameters[pi].id = (uint32_t)byte_at(data, size, 2 + pi);
        function_parameters[pi].type =
            (YIRType)(byte_at(data, size, 5 + pi) % (YIR_TYPE_PTR + 2));
    }

    size_t cursor = 8;
    for (size_t bi = 0; bi < block_count; ++bi) {
        blocks[bi].id = (uint32_t)byte_at(data, size, cursor++);
        blocks[bi].parameter_count = bi == 0 ? 0 :
            (size_t)(byte_at(data, size, cursor++) % (MAX_PARAMETERS + 1));
        for (size_t pi = 0; pi < MAX_PARAMETERS; ++pi) {
            block_parameters[bi][pi].id = byte_at(data, size, cursor++);
            block_parameters[bi][pi].type =
                (YIRType)(byte_at(data, size, cursor++) % (YIR_TYPE_PTR + 2));
        }
        blocks[bi].parameters =
            byte_at(data, size, cursor++) % 5 == 0 ? NULL :
            block_parameters[bi];

        blocks[bi].instruction_count =
            1u + (size_t)(byte_at(data, size, cursor++) % MAX_INSTRUCTIONS);
        blocks[bi].instructions =
            byte_at(data, size, cursor++) % 7 == 0 ? NULL :
            instructions[bi];

        for (size_t ii = 0; ii < blocks[bi].instruction_count; ++ii) {
            YIRInstruction *ins = &instructions[bi][ii];
            ins->opcode = (YIROpcode)(
                byte_at(data, size, cursor++) % (YIR_OP_CALL + 2));
            ins->result_id = (uint32_t)byte_at(data, size, cursor++);
            ins->result_type = (YIRType)(
                byte_at(data, size, cursor++) % (YIR_TYPE_PTR + 2));
            ins->lhs = (uint32_t)byte_at(data, size, cursor++);
            ins->rhs = (uint32_t)byte_at(data, size, cursor++);
            ins->immediate = (int64_t)(int8_t)byte_at(data, size, cursor++);
            ins->floating_immediate = float_at(data, size, cursor);
            cursor += sizeof(uint64_t);
            ins->target = (uint32_t)byte_at(data, size, cursor++);
            ins->false_target = (uint32_t)byte_at(data, size, cursor++);
            ins->argument_count =
                byte_at(data, size, cursor++) % (MAX_ARGUMENTS + 1);
            ins->false_argument_count =
                byte_at(data, size, cursor++) % (MAX_ARGUMENTS + 1);
            ins->call_parameter_count =
                byte_at(data, size, cursor++) % (MAX_ARGUMENTS + 1);
            for (size_t ai = 0; ai < MAX_ARGUMENTS; ++ai) {
                arguments[bi][ii][ai] = byte_at(data, size, cursor++);
                false_arguments[bi][ii][ai] = byte_at(data, size, cursor++);
                call_parameter_types[bi][ii][ai] = (YIRType)(
                    byte_at(data, size, cursor++) % (YIR_TYPE_PTR + 2));
            }
            ins->arguments =
                byte_at(data, size, cursor++) % 6 == 0 ? NULL :
                arguments[bi][ii];
            ins->false_arguments =
                byte_at(data, size, cursor++) % 6 == 0 ? NULL :
                false_arguments[bi][ii];
            ins->call_parameter_types =
                byte_at(data, size, cursor++) % 6 == 0 ? NULL :
                call_parameter_types[bi][ii];
            ins->callee = byte_at(data, size, cursor++) % 3 == 0 ?
                NULL : "fuzz";
            ins->callee_symbol = byte_at(data, size, cursor++) % 3 == 0 ?
                NULL : (char *)"fuzz";
            ins->source_line = (size_t)byte_at(data, size, cursor++);
            ins->source_column = (size_t)byte_at(data, size, cursor++);
        }
    }

    YIRFunction function = {
        .name = "fuzz",
        .symbol = (char *)"fuzz",
        .return_type = (YIRType)(
            byte_at(data, size, cursor++) % (YIR_TYPE_PTR + 2)),
        .parameters = byte_at(data, size, cursor++) % 5 == 0 ?
            NULL : function_parameters,
        .parameter_count = parameter_count,
        .blocks = blocks,
        .block_count = block_count
    };
    char error[256];
    (void)yir_verify_function(&function, error, sizeof(error));
    (void)yir_verify_module(&function, 1, error, sizeof(error));
    return 0;
}

static void run_valid_seed_modules(void)
{
    const YIRInstruction entry_instructions[] = {
        {.opcode=YIR_OP_CONST_BOOL,.result_id=1,.result_type=YIR_TYPE_BOOL,
         .immediate=1,.source_line=1,.source_column=1},
        {.opcode=YIR_OP_COND_BRANCH,.lhs=1,.target=1,.false_target=2}
    };
    const YIRInstruction then_instructions[] = {{.opcode=YIR_OP_RETURN}};
    const YIRInstruction else_instructions[] = {{.opcode=YIR_OP_RETURN}};
    const YIRBlock blocks[] = {
        {.id=0,.instructions=entry_instructions,.instruction_count=2},
        {.id=1,.instructions=then_instructions,.instruction_count=1},
        {.id=2,.instructions=else_instructions,.instruction_count=1}
    };
    const YIRFunction function = {
        .name="valid_seed",.symbol="fuzz::valid_seed",
        .return_type=YIR_TYPE_VOID,.blocks=blocks,.block_count=3
    };
    char error[256];
    assert(yir_verify_function(&function,error,sizeof(error)));
    assert(yir_verify_module(&function,1,error,sizeof(error)));
}

static void verify_resource_limits_fail_before_dereferencing_large_ranges(void)
{
    const YIRInstruction return_instruction[] = {{.opcode=YIR_OP_RETURN}};
    const YIRBlock block = {
        .id=0,.instructions=return_instruction,.instruction_count=1
    };
    const YIRFunctionParameter parameter = {.id=1,.type=YIR_TYPE_I64};
    YIRFunction function = {
        .name="limits",.return_type=YIR_TYPE_VOID,
        .blocks=&block,.block_count=SIZE_MAX
    };
    char error[256];
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"too many basic blocks")!=NULL);

    function.block_count = 1;
    function.parameters = &parameter;
    function.parameter_count = SIZE_MAX;
    assert(!yir_verify_function(&function,error,sizeof(error)));
    assert(strstr(error,"too many IR values")!=NULL);
}

#ifndef YLANG_IR_FUZZ_LIBFUZZER
int main(void)
{
    run_valid_seed_modules();
    verify_resource_limits_fail_before_dereferencing_large_ranges();

    uint32_t state = UINT32_C(0x6d2b79f5);
    uint8_t input[FUZZ_BYTES];
    for (size_t iteration = 0; iteration < 4096; ++iteration) {
        for (size_t i = 0; i < sizeof(input); ++i) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            input[i] = (uint8_t)(state >> 9);
        }
        (void)LLVMFuzzerTestOneInput(input, sizeof(input));
    }
    puts("YLang IR verifier fuzz smoke passed (4096 inputs)");
    return 0;
}
#endif
