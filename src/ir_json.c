#include "ylang/ir.h"

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

static bool json_fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) (void)snprintf(error, capacity, "%s", message);
    return false;
}

static const char *type_name(YIRType type)
{
    switch (type) {
        case YIR_TYPE_VOID: return "void";
        case YIR_TYPE_BOOL: return "bool";
        case YIR_TYPE_I64: return "i64";
        case YIR_TYPE_F64: return "f64";
        case YIR_TYPE_PTR: return "ptr";
    }
    return "invalid";
}

static const char *opcode_name(YIROpcode opcode)
{
    switch (opcode) {
        case YIR_OP_CONST_I64: return "const.i64";
        case YIR_OP_CONST_F64: return "const.f64";
        case YIR_OP_ADD_I64: return "add.i64";
        case YIR_OP_SUB_I64: return "sub.i64";
        case YIR_OP_MUL_I64: return "mul.i64";
        case YIR_OP_DIV_I64: return "div.i64";
        case YIR_OP_MOD_I64: return "mod.i64";
        case YIR_OP_NEG_I64: return "neg.i64";
        case YIR_OP_I64_TO_F64: return "i64.to_f64";
        case YIR_OP_F64_TO_I64: return "f64.to_i64";
        case YIR_OP_ADD_F64: return "add.f64";
        case YIR_OP_SUB_F64: return "sub.f64";
        case YIR_OP_MUL_F64: return "mul.f64";
        case YIR_OP_DIV_F64: return "div.f64";
        case YIR_OP_NEG_F64: return "neg.f64";
        case YIR_OP_EQ_F64: return "eq.f64";
        case YIR_OP_NE_F64: return "ne.f64";
        case YIR_OP_LT_F64: return "lt.f64";
        case YIR_OP_LE_F64: return "le.f64";
        case YIR_OP_GT_F64: return "gt.f64";
        case YIR_OP_GE_F64: return "ge.f64";
        case YIR_OP_EQ_I64: return "eq.i64";
        case YIR_OP_NE_I64: return "ne.i64";
        case YIR_OP_LT_I64: return "lt.i64";
        case YIR_OP_LE_I64: return "le.i64";
        case YIR_OP_GT_I64: return "gt.i64";
        case YIR_OP_GE_I64: return "ge.i64";
        case YIR_OP_CONST_BOOL: return "const.bool";
        case YIR_OP_NOT_BOOL: return "not.bool";
        case YIR_OP_BRANCH: return "br";
        case YIR_OP_COND_BRANCH: return "cond_br";
        case YIR_OP_RETURN: return "return";
        case YIR_OP_CALL: return "call";
    }
    return "invalid";
}

static void write_string(FILE *stream, const char *value)
{
    if (!value) {
        (void)fputs("null", stream);
        return;
    }
    (void)fputc('"', stream);
    for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        switch (*p) {
            case '"': (void)fputs("\\\"", stream); break;
            case '\\': (void)fputs("\\\\", stream); break;
            case '\b': (void)fputs("\\b", stream); break;
            case '\f': (void)fputs("\\f", stream); break;
            case '\n': (void)fputs("\\n", stream); break;
            case '\r': (void)fputs("\\r", stream); break;
            case '\t': (void)fputs("\\t", stream); break;
            default:
                if (*p < 0x20) (void)fprintf(stream, "\\u%04x", (unsigned)*p);
                else (void)fputc((int)*p, stream);
                break;
        }
    }
    (void)fputc('"', stream);
}

static void write_id(FILE *stream, uint32_t id)
{
    if (id == 0) (void)fputs("null", stream);
    else (void)fprintf(stream, "%" PRIu32, id);
}

static void write_ids(FILE *stream, const uint32_t *values, size_t count)
{
    (void)fputc('[', stream);
    for (size_t i = 0; i < count; ++i) {
        if (i) (void)fputc(',', stream);
        if (values) (void)fprintf(stream, "%" PRIu32, values[i]);
        else (void)fputs("null", stream);
    }
    (void)fputc(']', stream);
}

static void write_types(FILE *stream, const YIRType *types, size_t count)
{
    (void)fputc('[', stream);
    for (size_t i = 0; i < count; ++i) {
        if (i) (void)fputc(',', stream);
        if (types) write_string(stream, type_name(types[i]));
        else (void)fputs("null", stream);
    }
    (void)fputc(']', stream);
}

static void write_float(FILE *stream, double value)
{
    if (value == 0.0 && signbit(value)) {
        (void)fputs("-0.0", stream);
        return;
    }
    char number[64];
    int length = snprintf(number, sizeof(number), "%.17g", value);
    if (length < 0 || (size_t)length >= sizeof(number)) {
        (void)fputs("null", stream);
        return;
    }
    bool has_fraction_or_exponent = false;
    for (int i = 0; i < length; ++i) {
        if (number[i] == '.' || number[i] == 'e' || number[i] == 'E') {
            has_fraction_or_exponent = true;
            break;
        }
    }
    (void)fputs(number, stream);
    if (!has_fraction_or_exponent) (void)fputs(".0", stream);
}

static void write_instruction(FILE *stream, const YIRInstruction *ins)
{
    (void)fputs("{\"opcode\":", stream);
    write_string(stream, opcode_name(ins->opcode));
    (void)fputs(",\"result_id\":", stream);
    write_id(stream, ins->result_id);
    (void)fputs(",\"result_type\":", stream);
    write_string(stream, type_name(ins->result_type));
    (void)fputs(",\"lhs\":", stream);
    write_id(stream, ins->lhs);
    (void)fputs(",\"rhs\":", stream);
    write_id(stream, ins->rhs);
    (void)fputs(",\"immediate\":", stream);
    if (ins->opcode == YIR_OP_CONST_I64 || ins->opcode == YIR_OP_CONST_BOOL)
        (void)fprintf(stream, "%" PRId64, ins->immediate);
    else
        (void)fputs("null", stream);
    (void)fputs(",\"floating_immediate\":", stream);
    if (ins->opcode == YIR_OP_CONST_F64) write_float(stream, ins->floating_immediate);
    else (void)fputs("null", stream);
    (void)fputs(",\"target\":", stream);
    if (ins->opcode == YIR_OP_BRANCH || ins->opcode == YIR_OP_COND_BRANCH)
        (void)fprintf(stream, "%" PRIu32, ins->target);
    else
        (void)fputs("null", stream);
    (void)fputs(",\"false_target\":", stream);
    if (ins->opcode == YIR_OP_COND_BRANCH)
        (void)fprintf(stream, "%" PRIu32, ins->false_target);
    else
        (void)fputs("null", stream);
    (void)fputs(",\"arguments\":", stream);
    write_ids(stream, ins->arguments, ins->argument_count);
    (void)fputs(",\"false_arguments\":", stream);
    write_ids(stream, ins->false_arguments, ins->false_argument_count);
    (void)fputs(",\"callee\":", stream);
    write_string(stream, ins->callee);
    (void)fputs(",\"callee_symbol\":", stream);
    write_string(stream, ins->callee_symbol);
    (void)fputs(",\"call_parameter_types\":", stream);
    write_types(stream, ins->call_parameter_types, ins->call_parameter_count);
    (void)fputs(",\"source\":", stream);
    if (ins->source_line)
        (void)fprintf(stream, "{\"line\":%zu,\"column\":%zu}",
                      ins->source_line, ins->source_column);
    else
        (void)fputs("null", stream);
    (void)fputc('}', stream);
}

static void write_function(FILE *stream, const YIRFunction *function)
{
    (void)fputs("{\"name\":", stream);
    write_string(stream, function->name);
    (void)fputs(",\"symbol\":", stream);
    write_string(stream, function->symbol);
    (void)fputs(",\"return_type\":", stream);
    write_string(stream, type_name(function->return_type));
    (void)fputs(",\"parameters\":[", stream);
    for (size_t i = 0; i < function->parameter_count; ++i) {
        if (i) (void)fputc(',', stream);
        (void)fprintf(stream, "{\"id\":%" PRIu32 ",\"type\":",
                      function->parameters[i].id);
        write_string(stream, type_name(function->parameters[i].type));
        (void)fputc('}', stream);
    }
    (void)fputs("],\"blocks\":[", stream);
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        if (bi) (void)fputc(',', stream);
        (void)fprintf(stream, "{\"id\":%" PRIu32 ",\"parameters\":[",
                      block->id);
        for (size_t pi = 0; pi < block->parameter_count; ++pi) {
            if (pi) (void)fputc(',', stream);
            (void)fprintf(stream, "{\"id\":%" PRIu32 ",\"type\":",
                          block->parameters[pi].id);
            write_string(stream, type_name(block->parameters[pi].type));
            (void)fputc('}', stream);
        }
        (void)fputs("],\"instructions\":[", stream);
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            if (ii) (void)fputc(',', stream);
            write_instruction(stream, &block->instructions[ii]);
        }
        (void)fputs("]}", stream);
    }
    (void)fputs("]}", stream);
}

bool yir_write_json_module(FILE *stream, const YIRFunction *functions,
                           size_t function_count, char *error,
                           size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!stream)
        return json_fail(error, error_capacity, "JSON writer requires an output stream");
    if (!yir_verify_module(functions, function_count, error, error_capacity))
        return false;

    (void)fputs("{\"format\":\"ylang.mir\",\"version\":1,\"functions\":[",
                stream);
    for (size_t i = 0; i < function_count; ++i) {
        if (i) (void)fputc(',', stream);
        write_function(stream, &functions[i]);
    }
    (void)fputs("]}\n", stream);
    if (ferror(stream))
        return json_fail(error, error_capacity, "failed while writing JSON IR");
    return true;
}
