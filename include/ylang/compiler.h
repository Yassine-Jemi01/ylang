#ifndef YLANG_COMPILER_H
#define YLANG_COMPILER_H

#include "ylang/lexer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct Compiler Compiler;
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct VarDecl VarDecl;
typedef struct Function Function;
typedef struct Scope Scope;

typedef enum {
    TYPE_ERROR,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_VOID,
    TYPE_INT_ARRAY,
    TYPE_FLOAT_ARRAY,
    TYPE_BOOL_ARRAY,
    TYPE_CHAR_ARRAY,
    TYPE_STRING_ARRAY
} YType;

typedef enum {
    EXPR_ERROR,
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_BOOL,
    EXPR_CHAR,
    EXPR_STRING,
    EXPR_FSTRING,
    EXPR_NAME,
    EXPR_BORROW,
    EXPR_ARRAY_LITERAL,
    EXPR_INDEX,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_CALL
} ExprKind;

typedef struct FPart {
    char *text;
    Expr *expression;
} FPart;

struct Expr {
    ExprKind kind;
    Token token;
    YType type;
    bool is_min_int;
    union {
        struct { Expr *right; Token op; } unary;
        struct { Expr *left; Expr *right; Token op; } binary;
        struct { Expr *target; Expr *right; VarDecl *variable; } assign;
        struct { char *name; VarDecl *variable; } name;
        struct { Expr *target; VarDecl *variable; bool is_mut; } borrow;
        struct { Expr **items; size_t count; } array_literal;
        struct { Expr *array; Expr *index; VarDecl *variable; } index;
        struct { char *name; Expr **args; size_t count; Function *function; } call;
        struct { FPart *parts; size_t count; } fstring;
    } as;
};

typedef enum {
    STMT_BLOCK,
    STMT_VAR,
    STMT_PRINT,
    STMT_IF,
    STMT_LOOP,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_RETURN,
    STMT_EXPR,
    STMT_FOR
} StmtKind;

struct VarDecl {
    Token token;
    char *name;
    char *c_name;
    YType type;
    bool is_const;
    bool initialized;
    bool is_global;
    bool is_borrowed;
    bool is_mut_borrow;
    bool moved;
    Expr *initializer;
};

struct Stmt {
    StmtKind kind;
    Token token;
    union {
        struct { Stmt **items; size_t count; } block;
        VarDecl *variable;
        struct { Expr **args; size_t count; } print;
        struct { Expr *condition; Stmt *then_branch; Stmt *else_branch; } if_stmt;
        Stmt *loop_body;
        Expr *return_value;
        Expr *expression;
        struct { Stmt *initializer; Expr *condition; Expr *increment; Stmt *body; } for_stmt;
    } as;
};

struct Function {
    Token token;
    char *name;
    char *c_name;
    VarDecl **params;
    size_t param_count;
    YType return_type;
    Stmt *body;
};

typedef struct {
    VarDecl **globals;
    size_t global_count;
    Function **functions;
    size_t function_count;
} Program;

bool ylang_type_is_array(YType type);
YType ylang_array_element_type(YType type);
YType ylang_array_type_for(YType element_type);
bool ylang_type_is_owned(YType type);

/* Returns 0 on success and nonzero if checking/building failed. */
int ylang_run(const char *command, const char *input_path,
              const char *output_path, const char *cc);

#endif
