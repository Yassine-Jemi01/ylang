#include "ylang/ir.h"

#include <inttypes.h>
#include <stdio.h>

static const char *ir_type_name(YIRType type)
{
    switch (type) {
        case YIR_TYPE_VOID: return "void";
        case YIR_TYPE_BOOL: return "bool";
        case YIR_TYPE_I64: return "i64";
        case YIR_TYPE_F64: return "f64";
        case YIR_TYPE_PTR: return "ptr";
    }
    return "<invalid-type>";
}

static void print_value(FILE *stream, uint32_t id)
{
    (void)fprintf(stream, "%%%u", id);
}

static void print_edge(FILE *stream, uint32_t target,
                       const uint32_t *arguments, size_t argument_count)
{
    (void)fprintf(stream, "bb%u(", target);
    for (size_t i = 0; i < argument_count; ++i) {
        if (i != 0) (void)fputs(", ", stream);
        if (arguments) print_value(stream, arguments[i]);
        else (void)fputs("<missing>", stream);
    }
    (void)fputc(')', stream);
}

static void print_instruction(FILE *stream, const YIRInstruction *instruction)
{
    if (instruction->result_id != 0) {
        print_value(stream, instruction->result_id);
        (void)fputs(" = ", stream);
    }

    switch (instruction->opcode) {
        case YIR_OP_CONST_I64:
            (void)fprintf(stream, "const.i64 %" PRId64, instruction->immediate);
            break;
        case YIR_OP_ADD_I64:
            (void)fputs("add.i64 ", stream);
            print_value(stream, instruction->lhs);
            (void)fputs(", ", stream);
            print_value(stream, instruction->rhs);
            break;
        case YIR_OP_SUB_I64:
            (void)fputs("sub.i64 ", stream);
            print_value(stream, instruction->lhs);
            (void)fputs(", ", stream);
            print_value(stream, instruction->rhs);
            break;
        case YIR_OP_MUL_I64:
            (void)fputs("mul.i64 ", stream);
            print_value(stream, instruction->lhs);
            (void)fputs(", ", stream);
            print_value(stream, instruction->rhs);
            break;
        case YIR_OP_EQ_I64:
        case YIR_OP_NE_I64:
        case YIR_OP_LT_I64:
        case YIR_OP_LE_I64:
        case YIR_OP_GT_I64:
        case YIR_OP_GE_I64: {
            const char *name = "eq.i64";
            switch (instruction->opcode) {
                case YIR_OP_NE_I64: name = "ne.i64"; break;
                case YIR_OP_LT_I64: name = "lt.i64"; break;
                case YIR_OP_LE_I64: name = "le.i64"; break;
                case YIR_OP_GT_I64: name = "gt.i64"; break;
                case YIR_OP_GE_I64: name = "ge.i64"; break;
                default: break;
            }
            (void)fprintf(stream, "%s ", name);
            print_value(stream, instruction->lhs);
            (void)fputs(", ", stream);
            print_value(stream, instruction->rhs);
            break;
        }
        case YIR_OP_CONST_BOOL:
            (void)fprintf(stream, "const.bool %s",
                          instruction->immediate != 0 ? "true" : "false");
            break;
        case YIR_OP_NOT_BOOL:
            (void)fputs("not.bool ", stream);
            print_value(stream, instruction->lhs);
            break;
        case YIR_OP_BRANCH:
            (void)fputs("br ", stream);
            print_edge(stream, instruction->target,
                       instruction->arguments, instruction->argument_count);
            break;
        case YIR_OP_COND_BRANCH:
            (void)fputs("cond_br ", stream);
            print_value(stream, instruction->lhs);
            (void)fputs(", ", stream);
            print_edge(stream, instruction->target,
                       instruction->arguments, instruction->argument_count);
            (void)fputs(", ", stream);
            print_edge(stream, instruction->false_target,
                       instruction->false_arguments,
                       instruction->false_argument_count);
            break;
        case YIR_OP_CALL:
            (void)fprintf(stream, "call @%s(", instruction->callee ?
                          instruction->callee : "<missing-callee>");
            for (size_t i = 0; i < instruction->argument_count; ++i) {
                if (i != 0) (void)fputs(", ", stream);
                if (instruction->arguments)
                    print_value(stream, instruction->arguments[i]);
                else
                    (void)fputs("<missing-argument>", stream);
            }
            (void)fputc(')', stream);
            break;
        case YIR_OP_RETURN:
            (void)fputs("return", stream);
            if (instruction->lhs != 0) {
                (void)fputc(' ', stream);
                print_value(stream, instruction->lhs);
            }
            break;
        default:
            (void)fputs("<invalid-opcode>", stream);
            break;
    }

    if (instruction->source_line != 0 && instruction->source_column != 0) {
        (void)fprintf(stream, "  ; @%zu:%zu",
                      instruction->source_line, instruction->source_column);
    }
    (void)fputc('\n', stream);
}

void yir_print_function(FILE *stream, const YIRFunction *function)
{
    if (!stream || !function) return;

    (void)fprintf(stream, "fn %s(", function->name ? function->name : "<anonymous>");
    for (size_t i = 0; i < function->parameter_count; ++i) {
        const YIRFunctionParameter *parameter =
            function->parameters ? &function->parameters[i] : NULL;
        if (i != 0) (void)fputs(", ", stream);
        if (!parameter) {
            (void)fputs("<missing-parameter>", stream);
            continue;
        }
        print_value(stream, parameter->id);
        (void)fprintf(stream, ": %s", ir_type_name(parameter->type));
    }
    (void)fprintf(stream, ") -> %s {\n", ir_type_name(function->return_type));

    for (size_t bi = 0; bi < function->block_count; ++bi) {
        const YIRBlock *block = function->blocks ? &function->blocks[bi] : NULL;
        if (!block) break;

        (void)fprintf(stream, "bb%u", block->id);
        if (block->parameter_count != 0) {
            (void)fputc('(', stream);
            for (size_t pi = 0; pi < block->parameter_count; ++pi) {
                const YIRBlockParameter *parameter =
                    block->parameters ? &block->parameters[pi] : NULL;
                if (pi != 0) (void)fputs(", ", stream);
                if (!parameter) {
                    (void)fputs("<missing-parameter>", stream);
                    continue;
                }
                print_value(stream, parameter->id);
                (void)fprintf(stream, ": %s", ir_type_name(parameter->type));
            }
            (void)fputc(')', stream);
        }
        (void)fputs(":\n", stream);

        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            if (!block->instructions) break;
            (void)fputs("  ", stream);
            print_instruction(stream, &block->instructions[ii]);
        }
    }

    (void)fputs("}\n", stream);
}
