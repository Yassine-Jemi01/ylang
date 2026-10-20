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
typedef struct Module Module;
typedef struct ImportName ImportName;
typedef struct ImportDecl ImportDecl;
typedef struct Program Program;
typedef struct EnumDecl EnumDecl;
typedef struct EnumVariant EnumVariant;
typedef struct MatchArm MatchArm;
typedef struct StructDecl StructDecl;
typedef struct StructField StructField;

typedef enum {
    TYPE_ERROR,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_VOID
} YType;

/* User-defined enum and struct types occupy disjoint identity ranges. */
#define TYPE_ENUM_BASE 1000
#define TYPE_STRUCT_BASE 2000

typedef enum {
    EXPR_ERROR,
    EXPR_ENUM_VALUE,
    EXPR_STRUCT_INIT,
    EXPR_MEMBER,
    EXPR_INT,
    EXPR_FLOAT,
    EXPR_BOOL,
    EXPR_CHAR,
    EXPR_STRING,
    EXPR_FSTRING,
    EXPR_NAME,
    EXPR_UNARY,
    EXPR_BINARY,
    EXPR_ASSIGN,
    EXPR_CALL,
    EXPR_ARRAY,
    EXPR_INDEX
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
    /* Deterministic lowering id used by the C backend's sequencing temporaries. */
    size_t codegen_id;
    union {
        struct { Expr *right; Token op; } unary;
        struct { Expr *left; Expr *right; Token op; } binary;
        struct { Expr *target; Expr *right; VarDecl *variable; bool is_compound; } assign;
        struct { char *name; VarDecl *variable; } name;
        struct { EnumDecl *enumeration; EnumVariant *variant; } enum_value;
        struct { StructDecl *structure; Expr **args; size_t count; } struct_init;
        struct { Expr *target; char *name; StructField *field; } member;
        struct { char *name; Expr **args; size_t count; Function *function; } call;
        struct { Expr **items; size_t count; } array;
        struct { Expr *target; Expr *index; VarDecl *variable; } index;
        struct { FPart *parts; size_t count; } fstring;
    } as;
};

typedef enum {
    STMT_BLOCK,
    STMT_VAR,
    STMT_PRINT,
    STMT_IF,
    STMT_LOOP,
    STMT_WHILE,
    STMT_FOR,
    STMT_BREAK,
    STMT_CONTINUE,
    STMT_RETURN,
    STMT_MATCH,
    STMT_EXPR
} StmtKind;

struct VarDecl {
    Token token;
    char *name;
    char *c_name;
    YType type;
    bool is_const;
    bool initialized;
    bool is_global;
    bool is_array;
    size_t array_length;
    Expr *initializer;
    Module *module;
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
        struct { Expr *condition; Stmt *body; } while_stmt;
        struct { Stmt *initializer; Expr *condition; Expr *increment; Stmt *body; } for_stmt;
        struct { Expr *value; MatchArm **arms; size_t arm_count; } match_stmt;
        Expr *return_value;
        Expr *expression;
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
    Module *module;
};

struct EnumVariant {
    Token token;
    char *name;
    int64_t value;
    EnumDecl *enumeration;
};

struct EnumDecl {
    Token token;
    char *name;
    YType type;
    EnumVariant **variants;
    size_t variant_count;
    Module *module;
};

struct MatchArm {
    Token token;
    EnumVariant *variant;
    Stmt *body;
};

struct StructField {
    Token token;
    char *name;
    char *c_name;
    YType type;
    size_t index;
};

struct StructDecl {
    Token token;
    char *name;
    char *c_name;
    YType type;
    StructField **fields;
    size_t field_count;
    Module *module;
};

struct ImportName {
    Token token;
    char *name;
    Function *function;
    VarDecl *variable;
};

struct ImportDecl {
    Token token;
    char *path;
    ImportName **names;
    size_t name_count;
    Module *target;
};

struct Program {
    VarDecl **globals;
    size_t global_count;
    Function **functions;
    size_t function_count;
    ImportDecl **imports;
    size_t import_count;
    Module *module;
};

struct Module {
    char *path;
    char *canonical_path;
    char *source;
    size_t source_length;
    Program *program;
    Scope *global_scope;
    size_t id;
    bool is_root;
    bool loading;
    bool loaded;
};

/* Returns 0 on success and nonzero if checking/building failed. */
int ylang_run(const char *command, const char *input_path,
              const char *output_path, const char *cc);

#endif
