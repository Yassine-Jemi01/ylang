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
    YLIRBasicBlock *entry = NULL, *last = NULL;
    const YLIRType params[] = { YLIR_TYPE_I64 };
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "first", YLIR_TYPE_I64,
                                 params, 1U, &first));
    assert(ylang_ir_add_block(first, "entry", &entry));
    for (size_t i = 0; i < 24U; i++) {
        char name[32]; (void)snprintf(name, sizeof(name), "block_%zu", i);
        assert(ylang_ir_add_function(&m, name, YLIR_TYPE_VOID, NULL, 0U, &helper));
        assert(ylang_ir_add_block(first, name, &last));
        assert(ylang_ir_return(last, 0U, 1U, 1U));
    }
    assert(ylang_ir_return(entry, 0U, 1U, 3U));
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

static void test_phi_join(void)
{
    YLIRModule m; YLIRFunction *fn = NULL;
    YLIRBasicBlock *entry = NULL, *yes = NULL, *no = NULL, *join = NULL;
    const YLIRType params[] = { YLIR_TYPE_BOOL };
    YLIRValue yes_value, no_value, selected;
    const YLIRPhiIncoming incoming[] = {
        { .predecessor = 1U, .value = 1U },
        { .predecessor = 2U, .value = 2U }
    };
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "select", YLIR_TYPE_I64, params, 1U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_add_block(fn, "yes", &yes));
    assert(ylang_ir_add_block(fn, "no", &no));
    assert(ylang_ir_add_block(fn, "join", &join));
    assert(ylang_ir_cond_branch(entry, 0U, 1U, 2U, 1U, 1U));
    assert(ylang_ir_emit_const_i64(yes, 10, 2U, 1U, &yes_value));
    assert(ylang_ir_branch(yes, 3U, 2U, 5U));
    assert(ylang_ir_emit_const_i64(no, 20, 3U, 1U, &no_value));
    assert(ylang_ir_branch(no, 3U, 3U, 5U));
    assert(yes_value == 1U && no_value == 2U);
    assert(ylang_ir_emit_phi(join, YLIR_TYPE_I64, incoming, 2U, 4U, 1U, &selected));
    assert(ylang_ir_return(join, selected, 4U, 12U));
    assert(ylang_ir_verify(&m, stderr));
    ylang_ir_module_destroy(&m);
}

static void test_phi_requires_every_predecessor(void)
{
    YLIRModule m; YLIRFunction *fn = NULL;
    YLIRBasicBlock *entry = NULL, *yes = NULL, *no = NULL, *join = NULL;
    const YLIRType params[] = { YLIR_TYPE_BOOL, YLIR_TYPE_I64 };
    const YLIRPhiIncoming incoming[] = {
        { .predecessor = 1U, .value = 1U }
    };
    YLIRValue result;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "bad_phi", YLIR_TYPE_I64, params, 2U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_add_block(fn, "yes", &yes));
    assert(ylang_ir_add_block(fn, "no", &no));
    assert(ylang_ir_add_block(fn, "join", &join));
    assert(ylang_ir_cond_branch(entry, 0U, 1U, 2U, 1U, 1U));
    assert(ylang_ir_branch(yes, 3U, 2U, 1U));
    assert(ylang_ir_branch(no, 3U, 3U, 1U));
    assert(ylang_ir_emit_phi(join, YLIR_TYPE_I64, incoming, 1U, 4U, 1U, &result));
    assert(ylang_ir_return(join, result, 4U, 8U));
    assert(!ylang_ir_verify(&m, NULL));
    ylang_ir_module_destroy(&m);
}

static void test_loop_phi(void)
{
    YLIRModule m; YLIRFunction *fn = NULL;
    YLIRBasicBlock *entry = NULL, *header = NULL, *body = NULL, *exit = NULL;
    YLIRValue zero, phi, limit, condition, one, next;
    const YLIRPhiIncoming incoming[] = {
        { .predecessor = 0U, .value = 0U },
        { .predecessor = 2U, .value = 5U }
    };
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "loop_phi", YLIR_TYPE_I64, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_add_block(fn, "header", &header));
    assert(ylang_ir_add_block(fn, "body", &body));
    assert(ylang_ir_add_block(fn, "exit", &exit));

    assert(ylang_ir_emit_const_i64(entry, 0, 1U, 1U, &zero));
    assert(zero == 0U);
    assert(ylang_ir_branch(entry, 1U, 1U, 5U));

    assert(ylang_ir_emit_phi(header, YLIR_TYPE_I64, incoming, 2U, 2U, 1U, &phi));
    assert(phi == 1U);
    assert(ylang_ir_emit_const_i64(header, 5, 2U, 8U, &limit));
    assert(ylang_ir_emit_op(header, YLIR_OP_SLT_I64, phi, limit,
                            2U, 14U, &condition));
    assert(ylang_ir_cond_branch(header, condition, 2U, 3U, 2U, 20U));

    assert(ylang_ir_emit_const_i64(body, 1, 3U, 1U, &one));
    assert(ylang_ir_emit_op(body, YLIR_OP_ADD_I64, phi, one,
                            3U, 8U, &next));
    assert(next == 5U);
    assert(ylang_ir_branch(body, 1U, 3U, 15U));

    assert(ylang_ir_return(exit, phi, 4U, 1U));
    assert(ylang_ir_verify(&m, stderr));
    ylang_ir_module_destroy(&m);
}

static void test_non_dominating_cross_block_use_is_rejected(void)
{
    YLIRModule m; YLIRFunction *fn = NULL;
    YLIRBasicBlock *entry = NULL, *yes = NULL, *no = NULL, *join = NULL;
    const YLIRType params[] = { YLIR_TYPE_BOOL };
    YLIRValue value;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "bad_dominance", YLIR_TYPE_I64, params, 1U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_add_block(fn, "yes", &yes));
    assert(ylang_ir_add_block(fn, "no", &no));
    assert(ylang_ir_add_block(fn, "join", &join));
    assert(ylang_ir_cond_branch(entry, 0U, 1U, 2U, 1U, 1U));
    assert(ylang_ir_emit_const_i64(yes, 10, 2U, 1U, &value));
    assert(ylang_ir_branch(yes, 3U, 2U, 5U));
    assert(ylang_ir_branch(no, 3U, 3U, 5U));
    assert(ylang_ir_return(join, value, 4U, 1U));
    assert(!ylang_ir_verify(&m, NULL));
    ylang_ir_module_destroy(&m);
}

static void test_cross_block_dominated_value(void)
{
    YLIRModule m; YLIRFunction *fn = NULL;
    YLIRBasicBlock *entry = NULL, *later = NULL; YLIRValue v;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "cross", YLIR_TYPE_I64, NULL, 0U, &fn));
    assert(ylang_ir_add_block(fn, "entry", &entry));
    assert(ylang_ir_add_block(fn, "later", &later));
    assert(ylang_ir_emit_const_i64(entry, 5, 1U, 1U, &v));
    assert(ylang_ir_branch(entry, 1U, 1U, 5U));
    assert(ylang_ir_return(later, v, 2U, 1U));
    assert(ylang_ir_verify(&m, stderr));
    ylang_ir_module_destroy(&m);
}

static void test_calls_and_signatures(void)
{
    YLIRModule m;
    YLIRFunction *add = NULL, *main_fn = NULL, *consume = NULL;
    YLIRBasicBlock *add_entry = NULL, *main_entry = NULL, *consume_entry = NULL;
    const YLIRType add_params[] = { YLIR_TYPE_I64, YLIR_TYPE_I64 };
    const YLIRType consume_params[] = { YLIR_TYPE_I64 };
    YLIRValue sum, result, a, b, void_out = 0U;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "add", YLIR_TYPE_I64, add_params, 2U, &add));
    assert(ylang_ir_add_function(&m, "main", YLIR_TYPE_I64, NULL, 0U, &main_fn));
    assert(ylang_ir_add_function(&m, "consume", YLIR_TYPE_VOID,
                                 consume_params, 1U, &consume));
    assert(ylang_ir_add_block(add, "entry", &add_entry));
    assert(ylang_ir_add_block(main_fn, "entry", &main_entry));
    assert(ylang_ir_add_block(consume, "entry", &consume_entry));
    assert(ylang_ir_emit_op(add_entry, YLIR_OP_ADD_I64, 0U, 1U, 1U, 1U, &sum));
    assert(ylang_ir_return(add_entry, sum, 1U, 10U));
    assert(ylang_ir_emit_const_i64(main_entry, 20, 1U, 1U, &a));
    assert(ylang_ir_emit_const_i64(main_entry, 22, 1U, 5U, &b));
    const YLIRValue args[] = { a, b };
    assert(ylang_ir_emit_call(main_entry, 0U, args, 2U, 1U, 10U, &result));
    const YLIRValue consume_args[] = { result };
    assert(ylang_ir_emit_call(consume_entry, 2U, consume_args, 1U, 1U, 1U, &void_out));
    assert(void_out == YLIR_NO_VALUE);
    assert(ylang_ir_return_void(consume_entry, 1U, 10U));
    assert(ylang_ir_return(main_entry, result, 2U, 1U));
    assert(ylang_ir_verify(&m, stderr));
    ylang_ir_module_destroy(&m);
}

static void test_call_argument_type_is_checked(void)
{
    YLIRModule m;
    YLIRFunction *callee = NULL, *caller = NULL;
    YLIRBasicBlock *callee_entry = NULL, *caller_entry = NULL;
    const YLIRType param[] = { YLIR_TYPE_I64 };
    YLIRValue flag, result;
    assert(ylang_ir_module_init(&m));
    assert(ylang_ir_add_function(&m, "callee", YLIR_TYPE_I64, param, 1U, &callee));
    assert(ylang_ir_add_function(&m, "caller", YLIR_TYPE_I64, NULL, 0U, &caller));
    assert(ylang_ir_add_block(callee, "entry", &callee_entry));
    assert(ylang_ir_add_block(caller, "entry", &caller_entry));
    assert(ylang_ir_return(callee_entry, 0U, 1U, 1U));
    assert(ylang_ir_emit_const_bool(caller_entry, true, 1U, 1U, &flag));
    const YLIRValue args[] = { flag };
    assert(ylang_ir_emit_call(caller_entry, 0U, args, 1U, 1U, 5U, &result));
    assert(ylang_ir_return(caller_entry, result, 2U, 1U));
    assert(!ylang_ir_verify(&m, NULL));
    ylang_ir_module_destroy(&m);
}

int main(void)
{
    test_integer_function();
    test_calls_and_signatures();
    test_call_argument_type_is_checked();
    test_params_and_branches();
    test_f64();
    test_stable_addresses();
    test_missing_terminator();
    test_bad_operand_type();
    test_bad_return_and_branch();
    test_terminator_is_final();
    test_phi_join();
    test_phi_requires_every_predecessor();
    test_loop_phi();
    test_non_dominating_cross_block_use_is_rejected();
    test_cross_block_dominated_value();
    puts("YLang typed IR tests passed");
    return 0;
}
