#include "internal.h"
#include "ylang/ir.h"
#include "ylang/llvm.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t **continuation_ids;
    size_t *exit_continuation;
    size_t continuation_count;
    bool needs_overflow;
} LLVMFunctionLayout;

static void llvm_error(FILE *diagnostics, const char *message)
{
    fprintf(diagnostics ? diagnostics : stderr, "YLang LLVM backend: %s\n", message);
}

static const char *llvm_type(YLIRType type)
{
    switch (type) {
        case YLIR_TYPE_VOID: return "void";
        case YLIR_TYPE_BOOL: return "i1";
        case YLIR_TYPE_I8: return "i8";
        case YLIR_TYPE_I64: return "i64";
        case YLIR_TYPE_F64: return "double";
        case YLIR_TYPE_PTR: return "ptr";
        case YLIR_TYPE_INVALID: break;
    }
    return NULL;
}

static bool valid_ir_identifier(const char *name)
{
    if (!name || !*name) return false;
    unsigned char first = (unsigned char)name[0];
    if (!((first >= 'A' && first <= 'Z') ||
          (first >= 'a' && first <= 'z') || first == '_')) return false;
    for (size_t i = 1; name[i]; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (!((ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '_')) return false;
    }
    return true;
}

static bool is_checked_instruction(YLIRInstructionKind kind)
{
    return kind == YLIR_OP_ADD_I64 || kind == YLIR_OP_SUB_I64 ||
           kind == YLIR_OP_MUL_I64 || kind == YLIR_OP_SDIV_I64 ||
           kind == YLIR_OP_SREM_I64 || kind == YLIR_OP_NEG_I64;
}

static void free_layout(LLVMFunctionLayout *layout, size_t block_count)
{
    if (!layout) return;
    if (layout->continuation_ids) {
        for (size_t i = 0; i < block_count; i++) free(layout->continuation_ids[i]);
    }
    free(layout->continuation_ids);
    free(layout->exit_continuation);
    memset(layout, 0, sizeof(*layout));
}

static bool build_layout(const YLIRFunction *function, LLVMFunctionLayout *layout)
{
    memset(layout, 0, sizeof(*layout));
    size_t count = function->block_count;
    layout->continuation_ids = calloc(count, sizeof(*layout->continuation_ids));
    layout->exit_continuation = malloc(count * sizeof(*layout->exit_continuation));
    if (!layout->continuation_ids || !layout->exit_continuation) {
        free_layout(layout, count);
        return false;
    }
    for (size_t b = 0; b < count; b++) {
        const YLIRBasicBlock *block = function->blocks[b];
        layout->exit_continuation[b] = SIZE_MAX;
        if (block->instruction_count) {
            if (block->instruction_count > SIZE_MAX / sizeof(size_t)) {
                free_layout(layout, count);
                return false;
            }
            layout->continuation_ids[b] = malloc(
                block->instruction_count * sizeof(*layout->continuation_ids[b]));
            if (!layout->continuation_ids[b]) {
                free_layout(layout, count);
                return false;
            }
        }
        for (size_t i = 0; i < block->instruction_count; i++) {
            if (is_checked_instruction(block->instructions[i].kind)) {
                size_t id = layout->continuation_count++;
                layout->continuation_ids[b][i] = id;
                layout->exit_continuation[b] = id;
                layout->needs_overflow = true;
            } else {
                layout->continuation_ids[b][i] = SIZE_MAX;
            }
        }
    }
    return true;
}

static void emit_value(FILE *out, YLIRValue value)
{
    fprintf(out, "%%v%" PRIu32, value);
}

static void emit_block_label(FILE *out, const YLIRFunction *function,
                             const LLVMFunctionLayout *layout, size_t block_index)
{
    if (layout->exit_continuation[block_index] == SIZE_MAX)
        fprintf(out, "%%%s", function->blocks[block_index]->name);
    else
        fprintf(out, "%%yl_cont_%zu", layout->exit_continuation[block_index]);
}

static bool emit_checked_integer(FILE *out, const YLIRInstruction *inst,
                                 size_t continuation)
{
    const char *intrinsic = NULL;
    switch (inst->kind) {
        case YLIR_OP_ADD_I64: intrinsic = "llvm.sadd.with.overflow.i64"; break;
        case YLIR_OP_SUB_I64: intrinsic = "llvm.ssub.with.overflow.i64"; break;
        case YLIR_OP_MUL_I64: intrinsic = "llvm.smul.with.overflow.i64"; break;
        case YLIR_OP_NEG_I64: intrinsic = "llvm.ssub.with.overflow.i64"; break;
        case YLIR_OP_SDIV_I64:
        case YLIR_OP_SREM_I64: {
            fprintf(out, "  %%t%zu_divzero = icmp eq i64 ", continuation);
            emit_value(out, inst->right);
            fputs(", 0\n", out);
            fprintf(out, "  %%t%zu_ismin = icmp eq i64 ", continuation);
            emit_value(out, inst->left);
            fputs(", -9223372036854775808\n", out);
            fprintf(out, "  %%t%zu_isnegone = icmp eq i64 ", continuation);
            emit_value(out, inst->right);
            fputs(", -1\n", out);
            fprintf(out, "  %%t%zu_divoverflow = and i1 %%t%zu_ismin, %%t%zu_isnegone\n",
                    continuation, continuation, continuation);
            fprintf(out, "  %%t%zu_invalid = or i1 %%t%zu_divzero, %%t%zu_divoverflow\n",
                    continuation, continuation, continuation);
            fprintf(out, "  br i1 %%t%zu_invalid, label %%yl_overflow, label %%yl_cont_%zu\n",
                    continuation, continuation);
            fprintf(out, "yl_cont_%zu:\n", continuation);
            fputs("  ", out); emit_value(out, inst->result);
            fprintf(out, " = %s i64 ", inst->kind == YLIR_OP_SDIV_I64 ? "sdiv" : "srem");
            emit_value(out, inst->left);
            fputs(", ", out); emit_value(out, inst->right); fputc('\n', out);
            return true;
        }
        default: return false;
    }

    fprintf(out, "  %%t%zu_pair = call { i64, i1 } @%s(i64 ",
            continuation, intrinsic);
    if (inst->kind == YLIR_OP_NEG_I64) {
        fputs("0, ", out);
        emit_value(out, inst->left);
    } else {
        emit_value(out, inst->left);
        fputs(", ", out);
        emit_value(out, inst->right);
    }
    fputs(")\n  ", out);
    emit_value(out, inst->result);
    fprintf(out, " = extractvalue { i64, i1 } %%t%zu_pair, 0\n", continuation);
    fprintf(out, "  %%t%zu_overflow = extractvalue { i64, i1 } %%t%zu_pair, 1\n",
            continuation, continuation);
    fprintf(out, "  br i1 %%t%zu_overflow, label %%yl_overflow, label %%yl_cont_%zu\n",
            continuation, continuation);
    fprintf(out, "yl_cont_%zu:\n", continuation);
    return true;
}

static bool emit_instruction(FILE *out, const YLIRModule *module,
                             const YLIRFunction *function,
                             const LLVMFunctionLayout *layout,
                             const YLIRInstruction *inst, size_t continuation,
                             size_t block_index, size_t instruction_index)
{
    const YLIRFunction *callee;
    const char *ty;
    if (is_checked_instruction(inst->kind))
        return emit_checked_integer(out, inst, continuation);

    switch (inst->kind) {
        case YLIR_OP_CONST_BOOL:
            fputs("  ", out); emit_value(out, inst->result);
            fprintf(out, " = add i1 0, %" PRIu64 "\n", inst->immediate);
            return true;
        case YLIR_OP_CONST_I64:
            fputs("  ", out); emit_value(out, inst->result);
            fprintf(out, " = add i64 0, %" PRId64 "\n", (int64_t)inst->immediate);
            return true;
        case YLIR_OP_CONST_F64:
            fputs("  ", out); emit_value(out, inst->result);
            fprintf(out, " = fadd double 0x%016" PRIX64 ", 0x0000000000000000\n",
                    inst->immediate);
            return true;
        case YLIR_OP_EQ_I64: case YLIR_OP_NE_I64:
        case YLIR_OP_SLT_I64: case YLIR_OP_SLE_I64:
        case YLIR_OP_SGT_I64: case YLIR_OP_SGE_I64: {
            const char *predicate = "eq";
            switch (inst->kind) {
                case YLIR_OP_EQ_I64: predicate = "eq"; break;
                case YLIR_OP_NE_I64: predicate = "ne"; break;
                case YLIR_OP_SLT_I64: predicate = "slt"; break;
                case YLIR_OP_SLE_I64: predicate = "sle"; break;
                case YLIR_OP_SGT_I64: predicate = "sgt"; break;
                case YLIR_OP_SGE_I64: predicate = "sge"; break;
                default: break;
            }
            fputs("  ", out); emit_value(out, inst->result);
            fprintf(out, " = icmp %s i64 ", predicate);
            emit_value(out, inst->left);
            fputs(", ", out); emit_value(out, inst->right); fputc('\n', out);
            return true;
        }
        case YLIR_OP_NOT_BOOL:
            fputs("  ", out); emit_value(out, inst->result);
            fputs(" = xor i1 ", out); emit_value(out, inst->left);
            fputs(", true\n", out);
            return true;
        case YLIR_OP_ADD_F64: case YLIR_OP_SUB_F64:
        case YLIR_OP_MUL_F64: case YLIR_OP_DIV_F64: {
            const char *opcode = inst->kind == YLIR_OP_ADD_F64 ? "fadd" :
                inst->kind == YLIR_OP_SUB_F64 ? "fsub" :
                inst->kind == YLIR_OP_MUL_F64 ? "fmul" : "fdiv";
            fputs("  ", out); emit_value(out, inst->result);
            fprintf(out, " = %s double ", opcode);
            emit_value(out, inst->left);
            fputs(", ", out); emit_value(out, inst->right); fputc('\n', out);
            return true;
        }
        case YLIR_OP_NEG_F64:
            fputs("  ", out); emit_value(out, inst->result);
            fputs(" = fsub double 0x0000000000000000, ", out);
            emit_value(out, inst->left); fputc('\n', out);
            return true;
        case YLIR_OP_PHI:
            ty = llvm_type(inst->type);
            if (!ty) return false;
            fputs("  ", out); emit_value(out, inst->result);
            fprintf(out, " = phi %s ", ty);
            for (size_t i = 0; i < inst->incoming_count; i++) {
                const YLIRPhiIncoming *incoming = &inst->incoming[i];
                if (i) fputs(", ", out);
                fputs("[ ", out); emit_value(out, incoming->value);
                fputs(", ", out);
                emit_block_label(out, function, layout, incoming->predecessor);
                fputs(" ]", out);
            }
            fputc('\n', out);
            return true;
        case YLIR_OP_PRINT_I64:
            fprintf(out, "  %%yl_printf_%zu_%zu = call i32 (ptr, ...) @printf(ptr @.yl_format_i64, i64 ", block_index, instruction_index);
            emit_value(out, inst->left);
            fputs(")\n", out);
            return true;
        case YLIR_OP_PRINT_F64:
            fprintf(out, "  %%yl_printf_%zu_%zu = call i32 (ptr, ...) @printf(ptr @.yl_format_f64, double ", block_index, instruction_index);
            emit_value(out, inst->left);
            fputs(")\n", out);
            return true;
        case YLIR_OP_PRINT_BOOL:
            fprintf(out, "  %%yl_print_bool_%zu_%zu = select i1 ", block_index, instruction_index);
            emit_value(out, inst->left);
            fputs(", ptr @.yl_true, ptr @.yl_false\n", out);
            fprintf(out, "  %%yl_printf_%zu_%zu = call i32 (ptr, ...) @printf(ptr @.yl_format_bool, ptr %%yl_print_bool_%zu_%zu)\n",
                    block_index, instruction_index, block_index, instruction_index);
            return true;
        case YLIR_OP_PRINT_SPACE:
            fprintf(out, "  %%yl_printf_%zu_%zu = call i32 (ptr, ...) @printf(ptr @.yl_space)\n",
                    block_index, instruction_index);
            return true;
        case YLIR_OP_PRINT_NEWLINE:
            fprintf(out, "  %%yl_printf_%zu_%zu = call i32 (ptr, ...) @printf(ptr @.yl_newline)\n",
                    block_index, instruction_index);
            return true;
        case YLIR_OP_LOAD_GLOBAL: {
            if (inst->immediate >= module->global_count) return false;
            const YLIRGlobal *global = &module->globals[inst->immediate];
            ty = llvm_type(global->type);
            if (!ty || !valid_ir_identifier(global->name)) return false;
            fputs("  ", out);
            emit_value(out, inst->result);
            fprintf(out, " = load %s, ptr @%s, align 8\n", ty, global->name);
            return true;
        }
        case YLIR_OP_STORE_GLOBAL: {
            if (inst->immediate >= module->global_count) return false;
            const YLIRGlobal *global = &module->globals[inst->immediate];
            ty = llvm_type(global->type);
            if (!ty || !global->is_mutable || !valid_ir_identifier(global->name))
                return false;
            fprintf(out, "  store %s ", ty);
            emit_value(out, inst->left);
            fprintf(out, ", ptr @%s, align 8\n", global->name);
            return true;
        }
        case YLIR_OP_CALL:
            if (inst->callee_index >= module->function_count) return false;
            callee = module->functions[inst->callee_index];
            ty = llvm_type(callee->return_type);
            if (!ty || !valid_ir_identifier(callee->name)) return false;
            fputs("  ", out);
            if (callee->return_type != YLIR_TYPE_VOID) {
                emit_value(out, inst->result);
                fputs(" = ", out);
            }
            fprintf(out, "call %s @%s(", ty, callee->name);
            for (size_t i = 0; i < inst->argument_count; i++) {
                if (i) fputs(", ", out);
                fprintf(out, "%s ", llvm_type(callee->parameter_types[i]));
                emit_value(out, inst->arguments[i]);
            }
            fputs(")\n", out);
            return true;
        default: return false;
    }
}

static bool emit_function(FILE *out, const YLIRModule *module,
                          const YLIRFunction *function)
{
    if (!valid_ir_identifier(function->name)) return false;
    LLVMFunctionLayout layout;
    if (!build_layout(function, &layout)) return false;
    const char *return_type = llvm_type(function->return_type);
    if (!return_type) { free_layout(&layout, function->block_count); return false; }

    fprintf(out, "define %s @%s(", return_type, function->name);
    for (size_t i = 0; i < function->parameter_count; i++) {
        if (i) fputs(", ", out);
        const char *param_type = llvm_type(function->parameter_types[i]);
        if (!param_type) { free_layout(&layout, function->block_count); return false; }
        fprintf(out, "%s %%v%zu", param_type, i);
    }
    fputs(") {\n", out);

    for (size_t b = 0; b < function->block_count; b++) {
        const YLIRBasicBlock *block = function->blocks[b];
        if (!valid_ir_identifier(block->name)) {
            free_layout(&layout, function->block_count);
            return false;
        }
        fprintf(out, "%s:\n", block->name);
        bool saw_non_phi = false;
        for (size_t i = 0; i < block->instruction_count; i++) {
            const YLIRInstruction *inst = &block->instructions[i];
            if (inst->kind == YLIR_OP_PHI && saw_non_phi) {
                free_layout(&layout, function->block_count);
                return false;
            }
            if (inst->kind != YLIR_OP_PHI) saw_non_phi = true;
            if (!emit_instruction(out, module, function, &layout, inst,
                                  layout.continuation_ids[b][i], b, i)) {
                free_layout(&layout, function->block_count);
                return false;
            }
        }

        const YLIRTerminator *term = &block->terminator;
        switch (term->kind) {
            case YLIR_TERM_RETURN:
                fprintf(out, "  ret %s ", return_type);
                emit_value(out, term->value);
                fputc('\n', out);
                break;
            case YLIR_TERM_RETURN_VOID: fputs("  ret void\n", out); break;
            case YLIR_TERM_BRANCH:
                if (term->target >= function->block_count) {
                    free_layout(&layout, function->block_count);
                    return false;
                }
                fprintf(out, "  br label %%%s\n", function->blocks[term->target]->name);
                break;
            case YLIR_TERM_COND_BRANCH:
                if (term->target >= function->block_count ||
                    term->false_target >= function->block_count) {
                    free_layout(&layout, function->block_count);
                    return false;
                }
                fputs("  br i1 ", out); emit_value(out, term->value);
                fprintf(out, ", label %%%s, label %%%s\n",
                        function->blocks[term->target]->name,
                        function->blocks[term->false_target]->name);
                break;
            case YLIR_TERM_UNREACHABLE: fputs("  unreachable\n", out); break;
            case YLIR_TERM_NONE:
            default:
                free_layout(&layout, function->block_count);
                return false;
        }
    }
    if (layout.needs_overflow) {
        fputs("yl_overflow:\n  call void @ylang_runtime_failure()\n  unreachable\n", out);
    }
    fputs("}\n\n", out);
    free_layout(&layout, function->block_count);
    return !ferror(out);
}

bool ylang_llvm_write_ir(const YLIRModule *module, FILE *output,
                         FILE *diagnostics)
{
    if (!module || !output || !ylang_ir_verify(module, diagnostics)) {
        llvm_error(diagnostics, "module verification failed before LLVM emission");
        return false;
    }
    const YLIRFunction *main_function = NULL;
    for (size_t i = 0; i < module->function_count; i++)
        if (strcmp(module->functions[i]->name, "yl_fn_main") == 0) {
            main_function = module->functions[i];
            break;
        }
    if (!main_function || main_function->return_type != YLIR_TYPE_I64 ||
        main_function->parameter_count != 0U) {
        llvm_error(diagnostics, "LLVM executable entry requires fn main() -> int");
        return false;
    }

    fputs(
        "; Generated by YLang's experimental typed-IR LLVM backend.\n"
        "source_filename = \"YLang\"\n\n"
        "@.ylang_runtime_error = private unnamed_addr constant [20 x i8] c\"YLang runtime error\\0A\", align 1\n\n"
        "@.yl_format_i64 = private unnamed_addr constant [5 x i8] c\"%lld\\00\", align 1\n"
        "@.yl_format_f64 = private unnamed_addr constant [3 x i8] c\"%g\\00\", align 1\n"
        "@.yl_format_bool = private unnamed_addr constant [3 x i8] c\"%s\\00\", align 1\n"
        "@.yl_true = private unnamed_addr constant [5 x i8] c\"true\\00\", align 1\n"
        "@.yl_false = private unnamed_addr constant [6 x i8] c\"false\\00\", align 1\n"
        "@.yl_space = private unnamed_addr constant [2 x i8] c\" \\00\", align 1\n"
        "@.yl_newline = private unnamed_addr constant [2 x i8] c\"\\0A\\00\", align 1\n"
        "declare i32 @printf(ptr, ...)\n"
        "declare i64 @write(i32, ptr, i64)\n"
        "declare void @exit(i32) noreturn\n"
        "declare { i64, i1 } @llvm.sadd.with.overflow.i64(i64, i64)\n"
        "declare { i64, i1 } @llvm.ssub.with.overflow.i64(i64, i64)\n"
        "declare { i64, i1 } @llvm.smul.with.overflow.i64(i64, i64)\n\n"
        "define internal void @ylang_runtime_failure() noreturn {\n"
        "entry:\n"
        "  %ignored = call i64 @write(i32 2, ptr @.ylang_runtime_error, i64 20)\n"
        "  call void @exit(i32 70)\n"
        "  unreachable\n"
        "}\n\n",
        output);

    for (size_t i = 0; i < module->global_count; i++) {
        const YLIRGlobal *global = &module->globals[i];
        const char *type = llvm_type(global->type);
        if (!type || !valid_ir_identifier(global->name)) {
            llvm_error(diagnostics, "global has an unsupported type or symbol name");
            return false;
        }
        fprintf(output, "@%s = %s %s ", global->name,
                global->is_mutable ? "global" : "constant", type);
        switch (global->type) {
            case YLIR_TYPE_BOOL:
                fputs(global->initial_bits ? "true" : "false", output);
                break;
            case YLIR_TYPE_I64:
                fprintf(output, "%" PRId64, (int64_t)global->initial_bits);
                break;
            case YLIR_TYPE_F64:
                fprintf(output, "0x%016" PRIX64, global->initial_bits);
                break;
            default:
                llvm_error(diagnostics, "global storage type is unsupported by the scalar LLVM backend");
                return false;
        }
        fputs(", align 8\n", output);
    }
    if (module->global_count) fputc('\n', output);

    for (size_t i = 0; i < module->function_count; i++) {
        if (!emit_function(output, module, module->functions[i])) {
            llvm_error(diagnostics, "could not emit a verified function or supported instruction");
            return false;
        }
    }
    fputs(
        "define i32 @main() {\n"
        "entry:\n"
        "  %yl_result = call i64 @yl_fn_main()\n"
        "  %yl_status = trunc i64 %yl_result to i32\n"
        "  ret i32 %yl_status\n"
        "}\n",
        output);
    return !ferror(output);
}

bool generate_llvm_ir(Compiler *compiler, const char *path)
{
    if (!compiler || !compiler->program || !path) return false;
    YLIRModule module;
    if (!ylang_ir_lower_program(compiler->program, &module, stderr)) return false;
    FILE *output = fopen(path, "wb");
    if (!output) {
        fprintf(stderr, "ylang: cannot write LLVM IR '%s'.\n", path);
        ylang_ir_module_destroy(&module);
        return false;
    }
    bool okay = ylang_llvm_write_ir(&module, output, stderr);
    if (fclose(output) != 0) okay = false;
    ylang_ir_module_destroy(&module);
    if (!okay) {
        (void)remove(path);
        fprintf(stderr, "ylang: LLVM IR emission failed for '%s'.\n", path);
    }
    return okay;
}
