#include "ylang/ir.h"
#include "internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void lowers_integer_arithmetic_return(void)
{
    Expr left = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="20",.length=2,.line=1,.column=12},
        .type=TYPE_INT
    };
    Expr right = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="22",.length=2,.line=1,.column=17},
        .type=TYPE_INT
    };
    Expr sum = {
        .kind=EXPR_BINARY,
        .type=TYPE_INT,
        .as.binary={
            .left=&left,
            .right=&right,
            .op={.type=TOKEN_PLUS,.start="+",.length=1,.line=1,.column=15}
        }
    };
    Stmt ret = {.kind=STMT_RETURN,.token={.line=2,.column=3},.as.return_value=&sum};
    Stmt *items[] = {&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {
        .name="main",.return_type=TYPE_INT,.body=&body
    };

    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.block_count==1);
    assert(lowered.blocks[0].instruction_count==4);
    assert(lowered.blocks[0].instructions[2].opcode==YIR_OP_ADD_I64);
    assert(lowered.blocks[0].instructions[2].source_line==1);
    assert(lowered.blocks[0].instructions[2].source_column==15);
    assert(lowered.blocks[0].instructions[3].source_line==2);
    assert(lowered.blocks[0].instructions[3].opcode==YIR_OP_RETURN);
    assert(yir_verify_function(&lowered,error,sizeof(error)));
    yir_destroy_function(&lowered);
}

static void lowers_unary_negative(void)
{
    Expr literal = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="7",.length=1},
        .type=TYPE_INT
    };
    Expr negative = {
        .kind=EXPR_UNARY,
        .type=TYPE_INT,
        .as.unary={
            .right=&literal,
            .op={.type=TOKEN_MINUS,.start="-",.length=1}
        }
    };
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&negative};
    Stmt *items[] = {&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {.name="negative",.return_type=TYPE_INT,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.blocks[0].instructions[0].opcode==YIR_OP_CONST_I64);
    assert(lowered.blocks[0].instructions[0].immediate==-7);
    yir_destroy_function(&lowered);
}

static void lowers_empty_void_function(void)
{
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=NULL,.count=0}};
    Function function = {.name="empty",.return_type=TYPE_VOID,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.return_type==YIR_TYPE_VOID);
    assert(lowered.blocks[0].instruction_count==1);
    assert(lowered.blocks[0].instructions[0].opcode==YIR_OP_RETURN);
    yir_destroy_function(&lowered);
}

static void rejects_unsupported_function_body(void)
{
    Stmt print_stmt = {.kind=STMT_PRINT};
    Stmt *items[] = {&print_stmt};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {.name="unsupported",.return_type=TYPE_VOID,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(!yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(strstr(error,"current scalar IR lowering subset")!=NULL);
}


static void lowers_if_else_returns(void)
{
    Expr left = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="5",.length=1},
        .type=TYPE_INT
    };
    Expr right = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="9",.length=1},
        .type=TYPE_INT
    };
    Expr condition = {
        .kind=EXPR_BINARY,
        .type=TYPE_BOOL,
        .as.binary={
            .left=&left,
            .right=&right,
            .op={.type=TOKEN_LESS,.start="<",.length=1}
        }
    };
    Expr yes = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="1",.length=1},
        .type=TYPE_INT
    };
    Expr no = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="0",.length=1},
        .type=TYPE_INT
    };
    VarDecl branch_local = {
        .name="branch_value",.type=TYPE_INT,.initializer=&yes
    };
    Stmt branch_declaration = {
        .kind=STMT_VAR,.as.variable=&branch_local
    };
    Expr branch_value = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="branch_value",.variable=&branch_local}
    };
    Stmt then_return = {.kind=STMT_RETURN,.as.return_value=&branch_value};
    Stmt *then_items[] = {&branch_declaration,&then_return};
    Stmt then_body = {
        .kind=STMT_BLOCK,.as.block={.items=then_items,.count=2}
    };
    Stmt else_return = {.kind=STMT_RETURN,.as.return_value=&no};
    Stmt conditional = {
        .kind=STMT_IF,
        .as.if_stmt={
            .condition=&condition,
            .then_branch=&then_body,
            .else_branch=&else_return
        }
    };
    Stmt *items[] = {&conditional};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {.name="choose",.return_type=TYPE_INT,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.block_count==3);
    assert(lowered.blocks[0].instructions[2].opcode==YIR_OP_LT_I64);
    assert(lowered.blocks[0].instructions[3].opcode==YIR_OP_COND_BRANCH);
    assert(yir_verify_function(&lowered,error,sizeof(error)));
    yir_destroy_function(&lowered);
}

static void lowers_if_without_else(void)
{
    Expr condition = {
        .kind=EXPR_BOOL,
        .token={.type=TOKEN_TRUE,.start="true",.length=4},
        .type=TYPE_BOOL
    };
    Stmt ret = {.kind=STMT_RETURN};
    Stmt conditional = {
        .kind=STMT_IF,
        .as.if_stmt={.condition=&condition,.then_branch=&ret,.else_branch=NULL}
    };
    Stmt *items[] = {&conditional};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {.name="missing_else",.return_type=TYPE_VOID,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.block_count==3);
    assert(yir_verify_function(&lowered,error,sizeof(error)));
    yir_destroy_function(&lowered);
}


static void lowers_integer_function_parameters(void)
{
    VarDecl left_parameter = {.name="left",.type=TYPE_INT};
    VarDecl right_parameter = {.name="right",.type=TYPE_INT};
    VarDecl *parameters[] = {&left_parameter,&right_parameter};
    Expr left = {
        .kind=EXPR_NAME,
        .type=TYPE_INT,
        .as.name={.name="left",.variable=&left_parameter}
    };
    Expr right = {
        .kind=EXPR_NAME,
        .type=TYPE_INT,
        .as.name={.name="right",.variable=&right_parameter}
    };
    Expr sum = {
        .kind=EXPR_BINARY,
        .type=TYPE_INT,
        .as.binary={
            .left=&left,
            .right=&right,
            .op={.type=TOKEN_PLUS,.start="+",.length=1}
        }
    };
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&sum};
    Stmt *items[] = {&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {
        .name="sum",
        .params=parameters,
        .param_count=2,
        .return_type=TYPE_INT,
        .body=&body
    };
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.parameter_count==2);
    assert(lowered.parameters[0].id==1);
    assert(lowered.parameters[1].id==2);
    assert(lowered.blocks[0].instructions[0].opcode==YIR_OP_ADD_I64);
    assert(lowered.blocks[0].instructions[0].lhs==1);
    assert(lowered.blocks[0].instructions[0].rhs==2);
    assert(yir_verify_function(&lowered,error,sizeof(error)));
    yir_destroy_function(&lowered);
}


static void lowers_boolean_literal_condition(void)
{
    Expr condition = {
        .kind=EXPR_BOOL,
        .token={.type=TOKEN_TRUE,.start="true",.length=4},
        .type=TYPE_BOOL
    };
    Expr yes = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="3",.length=1},
        .type=TYPE_INT
    };
    Expr no = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="4",.length=1},
        .type=TYPE_INT
    };
    Stmt then_return = {.kind=STMT_RETURN,.as.return_value=&yes};
    Stmt else_return = {.kind=STMT_RETURN,.as.return_value=&no};
    Stmt conditional = {
        .kind=STMT_IF,
        .as.if_stmt={
            .condition=&condition,
            .then_branch=&then_return,
            .else_branch=&else_return
        }
    };
    Stmt *items[] = {&conditional};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {.name="bool_condition",.return_type=TYPE_INT,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.blocks[0].instructions[0].opcode==YIR_OP_CONST_BOOL);
    assert(lowered.blocks[0].instructions[0].immediate==1);
    yir_destroy_function(&lowered);
}


static void lowers_minimum_signed_integer(void)
{
    Expr magnitude = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="9223372036854775808",
               .length=19,.line=1,.column=2},
        .type=TYPE_INT
    };
    Expr minimum = {
        .kind=EXPR_UNARY,
        .token={.type=TOKEN_MINUS,.start="-",.length=1,.line=1,.column=1},
        .type=TYPE_INT,
        .is_min_int=true,
        .as.unary={.right=&magnitude,.op={.type=TOKEN_MINUS,.start="-",
                                         .length=1,.line=1,.column=1}}
    };
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&minimum};
    Stmt *items[] = {&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {.name="min",.return_type=TYPE_INT,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.blocks[0].instructions[0].immediate==INT64_MIN);
    yir_destroy_function(&lowered);
}


static void lowers_boolean_function_parameter_and_return(void)
{
    VarDecl parameter = {.name="enabled",.type=TYPE_BOOL};
    VarDecl *parameters[] = {&parameter};
    Expr value = {
        .kind=EXPR_NAME,
        .type=TYPE_BOOL,
        .as.name={.name="enabled",.variable=&parameter}
    };
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&value};
    Stmt *items[] = {&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {
        .name="identity_bool",
        .params=parameters,
        .param_count=1,
        .return_type=TYPE_BOOL,
        .body=&body
    };
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.return_type==YIR_TYPE_BOOL);
    assert(lowered.parameters[0].type==YIR_TYPE_BOOL);
    assert(lowered.blocks[0].instructions[0].opcode==YIR_OP_RETURN);
    assert(lowered.blocks[0].instructions[0].lhs==1);
    yir_destroy_function(&lowered);
}


static void lowers_local_integer_and_assignment(void)
{
    Expr initial = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="40",.length=2,.line=1,.column=13},
        .type=TYPE_INT
    };
    VarDecl local = {.name="answer",.type=TYPE_INT,.initializer=&initial};
    Stmt declaration = {.kind=STMT_VAR,.as.variable=&local};

    Expr current = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="answer",.variable=&local}
    };
    Expr increment = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="2",.length=1,.line=2,.column=17},
        .type=TYPE_INT
    };
    Expr sum = {
        .kind=EXPR_BINARY,.type=TYPE_INT,
        .as.binary={
            .left=&current,.right=&increment,
            .op={.type=TOKEN_PLUS,.start="+",.length=1,.line=2,.column=15}
        }
    };
    Expr target = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="answer",.variable=&local}
    };
    Expr assignment = {
        .kind=EXPR_ASSIGN,.type=TYPE_INT,
        .as.assign={
            .target=&target,.right=&sum,.variable=&local,.is_compound=false
        }
    };
    Stmt assign_statement = {
        .kind=STMT_EXPR,.as.expression=&assignment
    };
    Expr returned = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="answer",.variable=&local}
    };
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&returned};
    Stmt *items[] = {&declaration,&assign_statement,&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=3}};
    Function function = {.name="main",.return_type=TYPE_INT,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.blocks[0].instruction_count==4);
    assert(lowered.blocks[0].instructions[2].opcode==YIR_OP_ADD_I64);
    assert(lowered.blocks[0].instructions[3].opcode==YIR_OP_RETURN);
    assert(lowered.blocks[0].instructions[3].lhs==3);
    yir_destroy_function(&lowered);
}


static void lowers_local_boolean(void)
{
    Expr initial = {
        .kind=EXPR_BOOL,
        .token={.type=TOKEN_TRUE,.start="true",.length=4},
        .type=TYPE_BOOL
    };
    VarDecl local = {.name="ready",.type=TYPE_BOOL,.initializer=&initial};
    Stmt declaration = {.kind=STMT_VAR,.as.variable=&local};
    Expr returned = {
        .kind=EXPR_NAME,.type=TYPE_BOOL,
        .as.name={.name="ready",.variable=&local}
    };
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&returned};
    Stmt *items[] = {&declaration,&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=2}};
    Function function = {.name="ready",.return_type=TYPE_BOOL,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.blocks[0].instructions[0].opcode==YIR_OP_CONST_BOOL);
    assert(lowered.blocks[0].instructions[1].opcode==YIR_OP_RETURN);
    assert(lowered.blocks[0].instructions[1].lhs==1);
    yir_destroy_function(&lowered);
}

static void rejects_assignment_to_const_local(void)
{
    Expr initial = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="1",.length=1},
        .type=TYPE_INT
    };
    VarDecl local = {
        .name="fixed",.type=TYPE_INT,.is_const=true,.initializer=&initial
    };
    Stmt declaration = {.kind=STMT_VAR,.as.variable=&local};
    Expr value = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="2",.length=1},
        .type=TYPE_INT
    };
    Expr target = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="fixed",.variable=&local}
    };
    Expr assignment = {
        .kind=EXPR_ASSIGN,.type=TYPE_INT,
        .as.assign={.target=&target,.right=&value,.variable=&local}
    };
    Stmt assign_statement = {.kind=STMT_EXPR,.as.expression=&assignment};
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&target};
    Stmt *items[] = {&declaration,&assign_statement,&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=3}};
    Function function = {.name="const_local",.return_type=TYPE_INT,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(!yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(strstr(error,"mutable scalar")!=NULL);
}


static void lowers_assignment_to_parameter_as_new_ssa_value(void)
{
    VarDecl parameter = {.name="value",.type=TYPE_INT};
    VarDecl *parameters[] = {&parameter};
    Expr initial = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="value",.variable=&parameter}
    };
    Expr one = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="1",.length=1},
        .type=TYPE_INT
    };
    Expr sum = {
        .kind=EXPR_BINARY,.type=TYPE_INT,
        .as.binary={
            .left=&initial,.right=&one,
            .op={.type=TOKEN_PLUS,.start="+",.length=1}
        }
    };
    Expr target = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="value",.variable=&parameter}
    };
    Expr assignment = {
        .kind=EXPR_ASSIGN,.type=TYPE_INT,
        .as.assign={.target=&target,.right=&sum,.variable=&parameter}
    };
    Stmt assign_statement = {.kind=STMT_EXPR,.as.expression=&assignment};
    Expr returned = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="value",.variable=&parameter}
    };
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&returned};
    Stmt *items[] = {&assign_statement,&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=2}};
    Function function = {
        .name="increment",
        .params=parameters,
        .param_count=1,
        .return_type=TYPE_INT,
        .body=&body
    };
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.blocks[0].instructions[1].opcode==YIR_OP_ADD_I64);
    assert(lowered.blocks[0].instructions[2].opcode==YIR_OP_RETURN);
    assert(lowered.blocks[0].instructions[2].lhs==3);
    yir_destroy_function(&lowered);
}


static void rejects_global_assignment_as_local_ssa(void)
{
    VarDecl global = {.name="counter",.type=TYPE_INT,.is_global=true};
    Expr value = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="2",.length=1},
        .type=TYPE_INT
    };
    Expr target = {
        .kind=EXPR_NAME,.type=TYPE_INT,
        .as.name={.name="counter",.variable=&global}
    };
    Expr assignment = {
        .kind=EXPR_ASSIGN,.type=TYPE_INT,
        .as.assign={.target=&target,.right=&value,.variable=&global}
    };
    Stmt assign_statement = {.kind=STMT_EXPR,.as.expression=&assignment};
    Stmt ret = {.kind=STMT_RETURN,.as.return_value=&value};
    Stmt *items[] = {&assign_statement,&ret};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=2}};
    Function function = {.name="global_write",.return_type=TYPE_INT,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(!yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(strstr(error,"prior local declaration")!=NULL);
}


static void lowers_real_checked_frontend_function(void)
{
    const char *source =
        "fn add(int left, int right) -> int { return left + right; }\n"
        "fn main() -> int { return add(20, 22); }\n";
    Compiler compiler = {0};
    compiler.filename = "ir-lower-integration.yl";
    compiler.source = (char *)source;
    compiler.source_length = strlen(source);
    compiler.quiet_diagnostics = true;
    compiler.program = parse_program(&compiler);
    assert(compiler.program != NULL);
    assert(!compiler.had_error);
    assert(check_program(&compiler));

    Function *add_function = find_function(&compiler, "add");
    assert(add_function != NULL);
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(add_function,&lowered,error,sizeof(error)));
    assert(lowered.parameter_count==2);
    assert(lowered.blocks[0].instructions[0].opcode==YIR_OP_ADD_I64);
    assert(lowered.blocks[0].instructions[1].opcode==YIR_OP_RETURN);
    yir_destroy_function(&lowered);
    arena_destroy(&compiler.arena);
}


static void lowers_boolean_negation_condition(void)
{
    Expr false_literal = {
        .kind=EXPR_BOOL,
        .token={.type=TOKEN_FALSE,.start="false",.length=5},
        .type=TYPE_BOOL
    };
    Expr negated = {
        .kind=EXPR_UNARY,
        .type=TYPE_BOOL,
        .as.unary={
            .right=&false_literal,
            .op={.type=TOKEN_BANG,.start="!",.length=1}
        }
    };
    Expr yes = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="1",.length=1},
        .type=TYPE_INT
    };
    Expr no = {
        .kind=EXPR_INT,
        .token={.type=TOKEN_NUMBER,.start="0",.length=1},
        .type=TYPE_INT
    };
    Stmt then_return = {.kind=STMT_RETURN,.as.return_value=&yes};
    Stmt else_return = {.kind=STMT_RETURN,.as.return_value=&no};
    Stmt conditional = {
        .kind=STMT_IF,
        .as.if_stmt={
            .condition=&negated,
            .then_branch=&then_return,
            .else_branch=&else_return
        }
    };
    Stmt *items[] = {&conditional};
    Stmt body = {.kind=STMT_BLOCK,.as.block={.items=items,.count=1}};
    Function function = {.name="not_condition",.return_type=TYPE_INT,.body=&body};
    YIRFunction lowered;
    char error[192];
    assert(yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(lowered.blocks[0].instructions[0].opcode==YIR_OP_CONST_BOOL);
    assert(lowered.blocks[0].instructions[1].opcode==YIR_OP_NOT_BOOL);
    assert(lowered.blocks[0].instructions[2].opcode==YIR_OP_COND_BRANCH);
    yir_destroy_function(&lowered);
}

static YIRFunction lower_checked_source(Compiler *compiler,
                                         const char *source,
                                         const char *function_name)
{
    memset(compiler, 0, sizeof(*compiler));
    compiler->filename = "ir-control-flow-test.yl";
    compiler->source = (char *)source;
    compiler->source_length = strlen(source);
    compiler->quiet_diagnostics = true;
    compiler->program = parse_program(compiler);
    assert(compiler->program != NULL);
    assert(!compiler->had_error);
    assert(check_program(compiler));
    Function *function = find_function(compiler, function_name);
    assert(function != NULL);

    YIRFunction lowered = {0};
    char error[256];
    if (!yir_lower_function(function, &lowered, error, sizeof(error))) {
        fprintf(stderr, "IR lowering failed: %s\n", error);
        assert(false);
    }
    assert(yir_verify_function(&lowered, error, sizeof(error)));
    return lowered;
}

static void lowers_nested_if_with_ssa_merge(void)
{
    static const char source[] =
        "fn main() -> int {\n"
        "    let mut int value = 0;\n"
        "    if (true) {\n"
        "        if (value < 10) { value = 1; } else { value = 2; }\n"
        "    }\n"
        "    return value;\n"
        "}\n";
    Compiler compiler;
    YIRFunction lowered = lower_checked_source(&compiler, source, "main");
    bool saw_merge_parameter = false;
    for (size_t i = 0; i < lowered.block_count; ++i)
        if (lowered.blocks[i].parameter_count != 0)
            saw_merge_parameter = true;
    assert(lowered.block_count >= 6);
    assert(saw_merge_parameter);
    yir_destroy_function(&lowered);
    arena_destroy(&compiler.arena);
}

static void lowers_while_with_continue_and_loop_carried_value(void)
{
    static const char source[] =
        "fn main() -> int {\n"
        "    let mut int index = 0;\n"
        "    while (index < 6) {\n"
        "        index = index + 1;\n"
        "        if (index == 3) { continue; }\n"
        "    }\n"
        "    return index;\n"
        "}\n";
    Compiler compiler;
    YIRFunction lowered = lower_checked_source(&compiler, source, "main");
    bool saw_backedge = false;
    bool saw_conditional_branch = false;
    for (size_t bi = 0; bi < lowered.block_count; ++bi) {
        const YIRBlock *block = &lowered.blocks[bi];
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *instruction = &block->instructions[ii];
            if (instruction->opcode == YIR_OP_BRANCH &&
                instruction->target <= block->id)
                saw_backedge = true;
            if (instruction->opcode == YIR_OP_COND_BRANCH)
                saw_conditional_branch = true;
        }
    }
    assert(saw_backedge);
    assert(saw_conditional_branch);
    yir_destroy_function(&lowered);
    arena_destroy(&compiler.arena);
}

static void lowers_for_with_break_and_increment_block(void)
{
    static const char source[] =
        "fn main() -> int {\n"
        "    let mut int index = 0;\n"
        "    for (; index < 7; index = index + 1) {\n"
        "        if (index == 4) { break; }\n"
        "    }\n"
        "    return index;\n"
        "}\n";
    Compiler compiler;
    YIRFunction lowered = lower_checked_source(&compiler, source, "main");
    bool saw_exit_parameters = false;
    bool saw_increment_block = false;
    for (size_t bi = 0; bi < lowered.block_count; ++bi) {
        const YIRBlock *block = &lowered.blocks[bi];
        if (block->parameter_count != 0 && block->id != 0)
            saw_exit_parameters = true;
        for (size_t ii = 0; ii < block->instruction_count; ++ii) {
            const YIRInstruction *instruction = &block->instructions[ii];
            if (instruction->opcode == YIR_OP_ADD_I64 &&
                instruction->source_line >= 3)
                saw_increment_block = true;
        }
    }
    assert(saw_exit_parameters);
    assert(saw_increment_block);
    yir_destroy_function(&lowered);
    arena_destroy(&compiler.arena);
}

static void lowers_nested_loops_and_inner_break_scope(void)
{
    static const char source[] =
        "fn main() -> int {\n"
        "    let mut int outer = 0;\n"
        "    while (outer < 2) {\n"
        "        let mut int inner = 0;\n"
        "        while (inner < 3) {\n"
        "            inner = inner + 1;\n"
        "            break;\n"
        "        }\n"
        "        outer = outer + 1;\n"
        "    }\n"
        "    return outer;\n"
        "}\n";
    Compiler compiler;
    YIRFunction lowered = lower_checked_source(&compiler, source, "main");
    size_t conditional_blocks = 0;
    for (size_t bi = 0; bi < lowered.block_count; ++bi) {
        for (size_t ii = 0; ii < lowered.blocks[bi].instruction_count; ++ii)
            if (lowered.blocks[bi].instructions[ii].opcode == YIR_OP_COND_BRANCH)
                conditional_blocks++;
    }
    assert(conditional_blocks >= 2);
    yir_destroy_function(&lowered);
    arena_destroy(&compiler.arena);
}

static void lowers_for_initializer_scope(void)
{
    static const char source[] =
        "fn main() -> int {\n"
        "    for (let mut int index = 0; index < 3; index = index + 1) { }\n"
        "    return 42;\n"
        "}\n";
    Compiler compiler;
    YIRFunction lowered = lower_checked_source(&compiler, source, "main");
    assert(lowered.block_count >= 4);
    assert(lowered.blocks[lowered.block_count - 1].instruction_count != 0);
    yir_destroy_function(&lowered);
    arena_destroy(&compiler.arena);
}

static void lowers_loop_with_break_and_continue(void)
{
    static const char source[] =
        "fn main() -> int {\n"
        "    let mut int index = 0;\n"
        "    loop() {\n"
        "        index = index + 1;\n"
        "        if (index < 3) { continue; } else { break; }\n"
        "    }\n"
        "    return index;\n"
        "}\n";
    Compiler compiler;
    YIRFunction lowered = lower_checked_source(&compiler, source, "main");
    assert(lowered.block_count >= 5);
    yir_destroy_function(&lowered);
    arena_destroy(&compiler.arena);
}

int main(void)
{
    lowers_integer_arithmetic_return();
    lowers_unary_negative();
    lowers_minimum_signed_integer();
    lowers_empty_void_function();
    rejects_unsupported_function_body();
    lowers_if_else_returns();
    lowers_if_without_else();
    lowers_integer_function_parameters();
    lowers_boolean_literal_condition();
    lowers_boolean_function_parameter_and_return();
    lowers_local_integer_and_assignment();
    lowers_local_boolean();
    rejects_assignment_to_const_local();
    lowers_assignment_to_parameter_as_new_ssa_value();
    rejects_global_assignment_as_local_ssa();
    lowers_real_checked_frontend_function();
    lowers_boolean_negation_condition();
    lowers_nested_if_with_ssa_merge();
    lowers_while_with_continue_and_loop_carried_value();
    lowers_for_with_break_and_increment_block();
    lowers_loop_with_break_and_continue();
    lowers_nested_loops_and_inner_break_scope();
    lowers_for_initializer_scope();
    puts("YLang IR lowering tests passed");
    return 0;
}
