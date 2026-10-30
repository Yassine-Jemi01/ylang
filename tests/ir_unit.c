#include "ylang/ir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_integer_function(void)
{
    YLIRModule m; YLIRFunction *fn = NULL; YLIRBasicBlock *entry = NULL;
    YLIRValue a, b, sum;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "main", YLIR_TYPE_I64, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_emit_const_i64(entry, 40, 1U, 1U, &a));
    assert(ylang_ir_emit_const_i64(entry, 2, 1U, 10U, &b));
    assert(ylang_ir_emit_op(entry, YLIR_OP_ADD_I64, a, b, 1U, 15U, &sum));
    assert(ylang_ir_return(entry, sum, 2U, 1U));
    assert(ylang_ir_verify(&m, stderr));
    ylang_ir_module_destroy(&m);
}

static void test_params_and_branches(void)
{
    YLIRModule m; YLIRFunction *fn = NULL;
    YLIRBasicBlock *entry = NULL, *yes = NULL, *no = NULL;
    const YLIRType params[] = { YLIR_TYPE_BOOL, YLIR_TYPE_I64 };
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "choose", YLIR_TYPE_I64, params, 2U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_add_block(fn, "yes", &yes));
    assert(ylang_ir_add_block(fn, "no", &no));
    assert(ylang_ir_cond_branch(entry, 0U, 1U, 2U, 1U, 1U));
    assert(ylang_ir_return(yes, 1U, 2U, 1U));
    assert(ylang_ir_return(no, 1U, 3U, 1U));
    assert(ylang_ir_verify(&m, stderr));
    ylang_ir_module_destroy(&m);
}

static void test_f64(void)
{
    YLIRModule m; YLIRFunction *fn = NULL; YLIRBasicBlock *entry = NULL;
    YLIRValue a, b, sum;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "ratio", YLIR_TYPE_F64, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_emit_const_f64(entry, 1.5, 1U, 1U, &a));
    assert(ylang_ir_emit_const_f64(entry, 2.5, 1U, 8U, &b));
    assert(ylang_ir_emit_op(entry, YLIR_OP_ADD_F64, a, b, 1U, 15U, &sum));
    assert(ylang_ir_return(entry, sum, 2U, 1U));
    assert(ylang_ir_verify(&m, stderr));
    ylang_ir_module_destroy(&m);
}

static void test_stable_addresses(void)
{
    YLIRModule m; YLIRFunction *first = NULL, *helper = NULL;
    YLIRBasicBlock *entry = NULL, *last = NULL; YLIRValue v;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "first", YLIR_TYPE_I64, NULL, 0U, &first));
    assert(ylang_ir_add_block(first, "entry", &entry));
    for (size_t i = 0; i < 24U; i++) {
        char name[32]; (void)snprintf(name, sizeof(name), "block_%zu", i);
        assert(ylang_ir_add_function(&m, name, YLIR_TYPE_VOID, NULL, 0U, &helper));
        assert(ylang_ir_add_block(first, name, &last));
    }
    assert(ylang_ir_emit_const_i64(entry, 9, 1U, 1U, &v));
    assert(ylang_ir_return(entry, v, 1U, 3U));
    assert(ylang_ir_return_void(last, 1U, 1U));
    assert(ylang_ir_verify(&m, stderr));
    ylang_ir_module_destroy(&m);
}

static void test_missing_terminator(void)
{
    YLIRModule m; YLIRFunction *fn = NULL; YLIRBasicBlock *entry = NULL;
    FILE *out = tmpfile(); char text[256] = {0};
    assert(out); assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "bad", YLIR_TYPE_VOID, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(!ylang_ir_verify(&m, out));
    rewind(out); assert(fread(text, 1U, sizeof(text)-1U, out) > 0U);
    assert(strstr(text, "no terminator") != NULL);
    fclose(out); ylang_ir_module_destroy(&m);
}

static void test_bad_operand_type(void)
{
    YLIRModule m; YLIRFunction *fn = NULL; YLIRBasicBlock *entry = NULL;
    YLIRValue flag, number, result; FILE *out = tmpfile();
    assert(out); assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "bad_type", YLIR_TYPE_I64, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_emit_const_bool(entry, true, 1U, 1U, &flag));
    assert(ylang_ir_emit_const_i64(entry, 7, 1U, 5U, &number));
    assert(ylang_ir_emit_op(entry, YLIR_OP_ADD_I64, flag, number, 1U, 9U, &result));
    assert(ylang_ir_return(entry, result, 2U, 1U));
    assert(!ylang_ir_verify(&m, out));
    fclose(out); ylang_ir_module_destroy(&m);
}

static void test_bad_return_and_branch(void)
{
    YLIRModule m; YLIRFunction *fn = NULL; YLIRBasicBlock *entry = NULL;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "bad", YLIR_TYPE_I64, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_return_void(entry, 1U, 1U));
    assert(!ylang_ir_verify(&m, NULL));
    ylang_ir_module_destroy(&m);

    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "branch", YLIR_TYPE_VOID, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_branch(entry, 9U, 1U, 1U));
    assert(!ylang_ir_verify(&m, NULL));
    ylang_ir_module_destroy(&m);
}

static void test_terminator_is_final(void)
{
    YLIRModule m; YLIRFunction *fn = NULL; YLIRBasicBlock *entry = NULL;
    YLIRValue ignored;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "done", YLIR_TYPE_VOID, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_return_void(entry, 1U, 1U));
    assert(!ylang_ir_emit_const_i64(entry, 1, 2U, 1U, &ignored));
    assert(!ylang_ir_branch(entry, 0U, 2U, 1U));
    ylang_ir_module_destroy(&m);
}

static void test_cross_block_value_requires_phi(void)
{
    YLIRModule m; YLIRFunction *fn = NULL;
    YLIRBasicBlock *entry = NULL, *later = NULL; YLIRValue v;
    FILE *out = tmpfile(); assert(out);
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "cross", YLIR_TYPE_I64, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_add_block(fn, "later", &later));
    assert(ylang_ir_emit_const_i64(entry, 5, 1U, 1U, &v));
    assert(ylang_ir_branch(entry, 1U, 1U, 5U));
    assert(ylang_ir_return(later, v, 2U, 1U));
    assert(!ylang_ir_verify(&m, out));
    fclose(out); ylang_ir_module_destroy(&m);
}

int main(void)
{
    test_integer_function();
    test_params_and_branches();
    test_f64();
    test_stable_addresses();
    test_missing_terminator();
    test_bad_operand_type();
    test_bad_return_and_branch();
    test_terminator_is_final();
    test_cross_block_value_requires_phi();
    puts("YLang typed IR tests passed");
    return 0;
}
