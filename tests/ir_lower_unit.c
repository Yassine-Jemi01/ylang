#include "ylang/ir.h"
#include "ylang/compiler.h"

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
    assert(strstr(error,"one return statement")!=NULL);
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

static void rejects_if_without_else(void)
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
    assert(!yir_lower_function(&function,&lowered,error,sizeof(error)));
    assert(strstr(error,"else branch")!=NULL);
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

int main(void)
{
    lowers_integer_arithmetic_return();
    lowers_unary_negative();
    lowers_minimum_signed_integer();
    lowers_empty_void_function();
    rejects_unsupported_function_body();
    lowers_if_else_returns();
    rejects_if_without_else();
    lowers_integer_function_parameters();
    lowers_boolean_literal_condition();
    lowers_boolean_function_parameter_and_return();
    lowers_local_integer_and_assignment();
    puts("YLang IR lowering tests passed");
    return 0;
}
