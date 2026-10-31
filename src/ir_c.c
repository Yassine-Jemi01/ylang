#include "ylang/ir.h"

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool emit_fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity)
        (void)snprintf(error, capacity, "%s", message);
    return false;
}

static const char *function_symbol(const YIRFunction *function)
{
    return function->symbol && function->symbol[0] ?
           function->symbol : function->name;
}

static const char *c_type(YIRType type)
{
    switch (type) {
        case YIR_TYPE_VOID: return "void";
        case YIR_TYPE_BOOL: return "bool";
        case YIR_TYPE_I64: return "int64_t";
        case YIR_TYPE_F64: return "double";
        default: return NULL;
    }
}

static void emit_value(FILE *stream, uint32_t id)
{
    (void)fprintf(stream, "yir_v_%" PRIu32, id);
}

static void emit_symbol(FILE *stream, const char *symbol)
{
    static const char hex[] = "0123456789abcdef";
    (void)fputs("yl_ir_", stream);
    for (const unsigned char *p = (const unsigned char *)symbol; *p; ++p) {
        (void)fputc(hex[*p >> 4], stream);
        (void)fputc(hex[*p & 0x0f], stream);
    }
}

static const YIRBlock *find_block(const YIRFunction *function, uint32_t id)
{
    for (size_t i = 0; i < function->block_count; ++i)
        if (function->blocks[i].id == id) return &function->blocks[i];
    return NULL;
}

static const YIRFunction *find_callee(const YIRFunction *functions,
                                      size_t function_count,
                                      const YIRInstruction *instruction)
{
    const char *symbol = instruction->callee_symbol &&
                         instruction->callee_symbol[0] ?
                         instruction->callee_symbol : instruction->callee;
    for (size_t i = 0; i < function_count; ++i)
        if (strcmp(function_symbol(&functions[i]), symbol) == 0)
            return &functions[i];
    return NULL;
}

static bool supported_function_type(const YIRFunction *function)
{
    if (function->return_type != YIR_TYPE_I64 &&
        function->return_type != YIR_TYPE_BOOL &&
        function->return_type != YIR_TYPE_VOID)
        return false;
    if (function->parameter_count && !function->parameters) return false;
    for (size_t i = 0; i < function->parameter_count; ++i)
        if (!c_type(function->parameters[i].type)) return false;
    return true;
}

static bool supported_instruction_types(const YIRFunction *function)
{
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        for (size_t pi = 0; pi < block->parameter_count; ++pi)
            if (!c_type(block->parameters[pi].type)) return false;
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *instruction = &block->instructions[ii];
            if (instruction->result_id &&
                !c_type(instruction->result_type)) return false;
            if (instruction->opcode == YIR_OP_CALL) {
                if (!c_type(instruction->result_type)) return false;
                for (size_t ai = 0; ai < instruction->call_parameter_count; ++ai)
                    if (!c_type(instruction->call_parameter_types[ai]))
                        return false;
            }
        }
    }
    return true;
}

static void emit_signature(FILE *stream, const YIRFunction *function,
                           bool prototype)
{
    (void)fprintf(stream, "%s ", c_type(function->return_type));
    emit_symbol(stream, function_symbol(function));
    (void)fputc('(', stream);
    if (function->parameter_count == 0) {
        (void)fputs("void", stream);
    } else {
        for (size_t i = 0; i < function->parameter_count; ++i) {
            if (i) (void)fputs(", ", stream);
            (void)fprintf(stream, "%s ", c_type(function->parameters[i].type));
            emit_value(stream, function->parameters[i].id);
        }
    }
    (void)fputc(')', stream);
    (void)fputs(prototype ? ";\n" : " {\n", stream);
}

static void emit_local_declarations(FILE *stream, const YIRFunction *function)
{
    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        for (size_t pi = 0; pi < block->parameter_count; ++pi) {
            const YIRBlockParameter *parameter = &block->parameters[pi];
            (void)fprintf(stream, "    %s ", c_type(parameter->type));
            emit_value(stream, parameter->id);
            (void)fputs(";\n", stream);
        }
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *instruction = &block->instructions[ii];
            if (!instruction->result_id) continue;
            (void)fprintf(stream, "    %s ", c_type(instruction->result_type));
            emit_value(stream, instruction->result_id);
            (void)fputs(";\n", stream);
        }
    }
}

static void emit_edge(FILE *stream, const YIRFunction *function,
                      uint32_t target_id, const uint32_t *arguments,
                      size_t argument_count)
{
    const YIRBlock *target = find_block(function, target_id);
    for (size_t i = 0; i < argument_count; ++i) {
        (void)fputs("        ", stream);
        emit_value(stream, target->parameters[i].id);
        (void)fputs(" = ", stream);
        emit_value(stream, arguments[i]);
        (void)fputs(";\n", stream);
    }
    (void)fprintf(stream, "        goto yir_bb_%" PRIu32 ";\n", target_id);
}

static const char *comparison_operator(YIROpcode opcode)
{
    switch (opcode) {
        case YIR_OP_EQ_I64: return "==";
        case YIR_OP_NE_I64: return "!=";
        case YIR_OP_LT_I64: return "<";
        case YIR_OP_LE_I64: return "<=";
        case YIR_OP_GT_I64: return ">";
        case YIR_OP_GE_I64: return ">=";
        default: return NULL;
    }
}

static bool emit_instruction(FILE *stream, const YIRFunction *function,
                             const YIRFunction *functions,
                             size_t function_count,
                             const YIRInstruction *instruction,
                             char *error, size_t error_capacity)
{
    switch (instruction->opcode) {
        case YIR_OP_CONST_I64:
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            if (instruction->immediate == INT64_MIN)
                (void)fputs(" = INT64_MIN;\n", stream);
            else
                (void)fprintf(stream, " = INT64_C(%" PRId64 ");\n",
                              instruction->immediate);
            return true;

        case YIR_OP_CONST_F64:
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fprintf(stream, " = %a;\n",
                          instruction->floating_immediate);
            return true;

        case YIR_OP_CONST_BOOL:
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fprintf(stream, " = %s;\n",
                          instruction->immediate ? "true" : "false");
            return true;

        case YIR_OP_ADD_I64:
        case YIR_OP_SUB_I64:
        case YIR_OP_MUL_I64:
        case YIR_OP_DIV_I64:
        case YIR_OP_MOD_I64: {
            const char *helper = instruction->opcode == YIR_OP_ADD_I64 ?
                "yir_add_i64" : instruction->opcode == YIR_OP_SUB_I64 ?
                "yir_sub_i64" : instruction->opcode == YIR_OP_MUL_I64 ?
                "yir_mul_i64" : instruction->opcode == YIR_OP_DIV_I64 ?
                "yir_div_i64" : "yir_mod_i64";
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fprintf(stream, " = %s(", helper);
            emit_value(stream, instruction->lhs);
            (void)fputs(", ", stream);
            emit_value(stream, instruction->rhs);
            (void)fputs(");\n", stream);
            return true;
        }

        case YIR_OP_ADD_F64:
        case YIR_OP_SUB_F64:
        case YIR_OP_MUL_F64: {
            const char *operator_text = instruction->opcode == YIR_OP_ADD_F64 ?
                "+" : instruction->opcode == YIR_OP_SUB_F64 ? "-" : "*";
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fputs(" = (", stream);
            emit_value(stream, instruction->lhs);
            (void)fprintf(stream, " %s ", operator_text);
            emit_value(stream, instruction->rhs);
            (void)fputs(");\n", stream);
            return true;
        }

        case YIR_OP_DIV_F64:
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fputs(" = yir_div_f64(", stream);
            emit_value(stream, instruction->lhs);
            (void)fputs(", ", stream);
            emit_value(stream, instruction->rhs);
            (void)fputs(");\n", stream);
            return true;

        case YIR_OP_NEG_F64:
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fputs(" = -", stream);
            emit_value(stream, instruction->lhs);
            (void)fputs(";\n", stream);
            return true;

        case YIR_OP_EQ_F64:
        case YIR_OP_NE_F64:
        case YIR_OP_LT_F64:
        case YIR_OP_LE_F64:
        case YIR_OP_GT_F64:
        case YIR_OP_GE_F64: {
            const char *operator_text = instruction->opcode == YIR_OP_EQ_F64 ?
                "==" : instruction->opcode == YIR_OP_NE_F64 ? "!=" :
                instruction->opcode == YIR_OP_LT_F64 ? "<" :
                instruction->opcode == YIR_OP_LE_F64 ? "<=" :
                instruction->opcode == YIR_OP_GT_F64 ? ">" : ">=";
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fputs(" = (", stream);
            emit_value(stream, instruction->lhs);
            (void)fprintf(stream, " %s ", operator_text);
            emit_value(stream, instruction->rhs);
            (void)fputs(");\n", stream);
            return true;
        }

        case YIR_OP_EQ_I64:
        case YIR_OP_NE_I64:
        case YIR_OP_LT_I64:
        case YIR_OP_LE_I64:
        case YIR_OP_GT_I64:
        case YIR_OP_GE_I64: {
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fputs(" = (", stream);
            emit_value(stream, instruction->lhs);
            (void)fprintf(stream, " %s ", comparison_operator(instruction->opcode));
            emit_value(stream, instruction->rhs);
            (void)fputs(");\n", stream);
            return true;
        }

        case YIR_OP_NEG_I64:
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fputs(" = yir_neg_i64(", stream);
            emit_value(stream, instruction->lhs);
            (void)fputs(");\n", stream);
            return true;

        case YIR_OP_NOT_BOOL:
            (void)fputs("    ", stream);
            emit_value(stream, instruction->result_id);
            (void)fputs(" = !", stream);
            emit_value(stream, instruction->lhs);
            (void)fputs(";\n", stream);
            return true;

        case YIR_OP_CALL: {
            const YIRFunction *callee =
                find_callee(functions, function_count, instruction);
            if (!callee)
                return emit_fail(error, error_capacity,
                                 "C emitter could not resolve a verified call target");
            (void)fputs("    ", stream);
            if (instruction->result_id) {
                emit_value(stream, instruction->result_id);
                (void)fputs(" = ", stream);
            }
            emit_symbol(stream, function_symbol(callee));
            (void)fputc('(', stream);
            for (size_t ai = 0; ai < instruction->argument_count; ++ai) {
                if (ai) (void)fputs(", ", stream);
                emit_value(stream, instruction->arguments[ai]);
            }
            (void)fputs(");\n", stream);
            return true;
        }

        case YIR_OP_BRANCH:
            emit_edge(stream, function, instruction->target,
                      instruction->arguments, instruction->argument_count);
            return true;

        case YIR_OP_COND_BRANCH:
            (void)fputs("    if (", stream);
            emit_value(stream, instruction->lhs);
            (void)fputs(") {\n", stream);
            emit_edge(stream, function, instruction->target,
                      instruction->arguments, instruction->argument_count);
            (void)fputs("    } else {\n", stream);
            emit_edge(stream, function, instruction->false_target,
                      instruction->false_arguments,
                      instruction->false_argument_count);
            (void)fputs("    }\n", stream);
            return true;

        case YIR_OP_RETURN:
            (void)fputs("    return", stream);
            if (instruction->lhs) {
                (void)fputc(' ', stream);
                emit_value(stream, instruction->lhs);
            }
            (void)fputs(";\n", stream);
            return true;
    }

    return emit_fail(error, error_capacity,
                     "C emitter encountered an unsupported IR opcode");
}

static bool emit_function(FILE *stream, const YIRFunction *function,
                          const YIRFunction *functions,
                          size_t function_count,
                          char *error, size_t error_capacity)
{
    emit_signature(stream, function, false);
    emit_local_declarations(stream, function);
    (void)fprintf(stream, "    goto yir_bb_%" PRIu32 ";\n",
                  function->blocks[0].id);

    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = &function->blocks[bi];
        (void)fprintf(stream, "yir_bb_%" PRIu32 ": ;\n", block->id);
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            if (!emit_instruction(stream, function, functions,
                                  function_count, &block->instructions[ii],
                                  error, error_capacity))
                return false;
        }
    }

    (void)fputs("}\n\n", stream);
    return true;
}

static void emit_runtime_helpers(FILE *stream, bool need_add,
                                 bool need_sub, bool need_mul,
                                 bool need_div, bool need_mod,
                                 bool need_neg, bool need_f64_div)
{
    if (need_add || need_sub || need_mul ||
        need_div || need_mod || need_neg || need_f64_div) {
        (void)fputs(
            "static void yir_runtime_error(const char *message) {\n"
            "    (void)fprintf(stderr, \"YLang runtime error: %s\\n\", message);\n"
            "    exit(70);\n"
            "}\n", stream);
    }
    if (need_add) {
        (void)fputs(
            "static int64_t yir_add_i64(int64_t a, int64_t b) {\n"
            "    if ((b > 0 && a > INT64_MAX - b) ||\n"
            "        (b < 0 && a < INT64_MIN - b))\n"
            "        yir_runtime_error(\"integer overflow in addition\");\n"
            "    return a + b;\n"
            "}\n", stream);
    }
    if (need_sub) {
        (void)fputs(
            "static int64_t yir_sub_i64(int64_t a, int64_t b) {\n"
            "    if ((b < 0 && a > INT64_MAX + b) ||\n"
            "        (b > 0 && a < INT64_MIN + b))\n"
            "        yir_runtime_error(\"integer overflow in subtraction\");\n"
            "    return a - b;\n"
            "}\n", stream);
    }
    if (need_mul) {
        (void)fputs(
            "static int64_t yir_mul_i64(int64_t a, int64_t b) {\n"
            "    if (a > 0) {\n"
            "        if ((b > 0 && a > INT64_MAX / b) ||\n"
            "            (b < 0 && b < INT64_MIN / a))\n"
            "            yir_runtime_error(\"integer overflow in multiplication\");\n"
            "    } else if (a < 0) {\n"
            "        if ((b > 0 && a < INT64_MIN / b) ||\n"
            "            (b < 0 && a < INT64_MAX / b))\n"
            "            yir_runtime_error(\"integer overflow in multiplication\");\n"
            "    }\n"
            "    return a * b;\n"
            "}\n", stream);
    }
    if (need_div) {
        (void)fputs(
            "static int64_t yir_div_i64(int64_t a, int64_t b) {\n"
            "    if (b == 0) yir_runtime_error(\"integer division by zero\");\n"
            "    if (a == INT64_MIN && b == -1)\n"
            "        yir_runtime_error(\"integer overflow in division\");\n"
            "    return a / b;\n"
            "}\n", stream);
    }
    if (need_mod) {
        (void)fputs(
            "static int64_t yir_mod_i64(int64_t a, int64_t b) {\n"
            "    if (b == 0) yir_runtime_error(\"integer modulo by zero\");\n"
            "    if (a == INT64_MIN && b == -1) return 0;\n"
            "    return a % b;\n"
            "}\n", stream);
    }
    if (need_neg) {
        (void)fputs(
            "static int64_t yir_neg_i64(int64_t value) {\n"
            "    if (value == INT64_MIN)\n"
            "        yir_runtime_error(\"integer overflow in negation\");\n"
            "    return -value;\n"
            "}\n", stream);
    }
    if (need_f64_div) {
        (void)fputs(
            "static double yir_div_f64(double a, double b) {\n"
            "    if (b == 0.0) yir_runtime_error(\"floating-point division by zero\");\n"
            "    return a / b;\n"
            "}\n", stream);
    }
    if (need_add || need_sub || need_mul ||
        need_div || need_mod || need_neg || need_f64_div)
        (void)fputc('\n', stream);
}

bool yir_emit_c_module(FILE *stream, const YIRFunction *functions,
                       size_t function_count, char *error,
                       size_t error_capacity)
{
    if (error && error_capacity) error[0] = '\0';
    if (!stream)
        return emit_fail(error, error_capacity,
                         "C emitter requires a valid output stream");
    if (!yir_verify_module(functions, function_count, error, error_capacity))
        return false;

    const YIRFunction *entry = NULL;
    size_t entry_count = 0;
    for (size_t fi = 0; fi < function_count; ++fi) {
        if (!supported_function_type(&functions[fi]) ||
            !supported_instruction_types(&functions[fi]))
            return emit_fail(error, error_capacity,
                "C emitter currently supports only void, bool, i64, and f64 scalar signatures and values");
        if (strcmp(functions[fi].name, "main") == 0) {
            entry = &functions[fi];
            entry_count++;
        }
    }
    if (entry_count > 1)
        return emit_fail(error, error_capacity,
                         "C emitter found more than one source main function");
    if (entry && (entry->return_type != YIR_TYPE_I64 ||
                  entry->parameter_count != 0))
        return emit_fail(error, error_capacity,
                         "C emitter requires main() to have signature fn main() -> int");

    (void)fputs(
        "#include <stdbool.h>\n"
        "#include <stdint.h>\n"
        "#include <inttypes.h>\n"
        "#include <limits.h>\n"
        "#include <stdio.h>\n"
        "#include <stdlib.h>\n\n", stream);
    bool need_add = false, need_sub = false, need_mul = false;
    bool need_div = false, need_mod = false, need_neg = false;
    bool need_f64_div = false;
    for (size_t fi = 0; fi < function_count; ++fi) {
        for (size_t bi = 0; bi < functions[fi].block_count; ++bi) {
            const YIRBlock *block = &functions[fi].blocks[bi];
            for (size_t ii = 0; ii < block->instruction_count; ++ii) {
                switch (block->instructions[ii].opcode) {
                    case YIR_OP_ADD_I64: need_add = true; break;
                    case YIR_OP_SUB_I64: need_sub = true; break;
                    case YIR_OP_MUL_I64: need_mul = true; break;
                    case YIR_OP_DIV_I64: need_div = true; break;
                    case YIR_OP_MOD_I64: need_mod = true; break;
                    case YIR_OP_NEG_I64: need_neg = true; break;
                    case YIR_OP_DIV_F64: need_f64_div = true; break;
                    default: break;
                }
            }
        }
    }
    emit_runtime_helpers(stream, need_add, need_sub, need_mul,
                         need_div, need_mod, need_neg, need_f64_div);

    for (size_t fi = 0; fi < function_count; ++fi) {
        emit_signature(stream, &functions[fi], true);
    }
    (void)fputc('\n', stream);

    for (size_t fi = 0; fi < function_count; ++fi) {
        if (!emit_function(stream, &functions[fi], functions,
                           function_count, error, error_capacity))
            return false;
    }

    if (entry) {
        (void)fputs("int main(void) {\n    return (int)", stream);
        emit_symbol(stream, function_symbol(entry));
        (void)fputs("();\n}\n", stream);
    }

    if (fflush(stream) != 0 || ferror(stream))
        return emit_fail(error, error_capacity,
                         "failed while writing generated C source");
    return true;
}
