#define _POSIX_C_SOURCE 200809L
#include "internal.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------- semantic analysis --------------------------- */
struct Scope {
    Scope *parent;
    VarDecl **vars;
    size_t count;
};

typedef struct {
    Compiler *compiler;
    Scope *scope;
    Function *function;
    unsigned loop_depth;
    Expr *allowed_assignment;
} Checker;

static Scope *scope_new(Compiler *c, Scope *parent)
{
    Scope *scope = arena_alloc(&c->arena, sizeof(*scope));
    scope->parent = parent;
    return scope;
}

static VarDecl *scope_lookup(Scope *scope, const char *name)
{
    for (Scope *at = scope; at; at = at->parent) {
        for (size_t i = 0; i < at->count; i++) {
            if (strcmp(at->vars[i]->name, name) == 0) return at->vars[i];
        }
    }
    return NULL;
}

static bool scope_add(Checker *checker, Scope *scope, VarDecl *var)
{
    for (size_t i = 0; i < scope->count; i++) {
        if (strcmp(scope->vars[i]->name, var->name) == 0) {
            diagnostic(checker->compiler, var->token, "error", "E2004",
                       "This name is already declared in the current scope.",
                       "Choose a different name or remove the duplicate declaration.");
            return false;
        }
    }
    VarDecl **grown = arena_alloc(&checker->compiler->arena,
                                  (scope->count + 1) * sizeof(*grown));
    if (scope->count) memcpy(grown, scope->vars, scope->count * sizeof(*grown));
    grown[scope->count++] = var;
    scope->vars = grown;
    return true;
}

Function *find_function(Compiler *c, const char *name)
{
    for (size_t i = 0; i < c->program->function_count; i++) {
        Function *function = c->program->functions[i];
        if (strcmp(function->name, name) == 0) return function;
    }
    return NULL;
}

static unsigned edit_distance(const char *a, const char *b)
{
    size_t na = strlen(a), nb = strlen(b);
    if (na > 128 || nb > 128) return 1000;
    unsigned previous[129], current[129];
    for (size_t j = 0; j <= nb; j++) previous[j] = (unsigned)j;
    for (size_t i = 1; i <= na; i++) {
        current[0] = (unsigned)i;
        for (size_t j = 1; j <= nb; j++) {
            unsigned deletion = previous[j] + 1;
            unsigned insertion = current[j - 1] + 1;
            unsigned substitution = previous[j - 1] + (a[i - 1] == b[j - 1] ? 0U : 1U);
            unsigned best = deletion < insertion ? deletion : insertion;
            current[j] = best < substitution ? best : substitution;
        }
        memcpy(previous, current, (nb + 1) * sizeof(*previous));
    }
    return previous[nb];
}

static const char *nearest_variable(Checker *checker, const char *name)
{
    const char *best_name = NULL;
    unsigned best = 4;
    for (Scope *scope = checker->scope; scope; scope = scope->parent) {
        for (size_t i = 0; i < scope->count; i++) {
            const char *candidate = scope->vars[i]->name;
            unsigned distance = edit_distance(name, candidate);
            if (distance < best) {
                best = distance;
                best_name = candidate;
            }
        }
    }
    return best_name;
}

static const char *nearest_function(Compiler *c, const char *name)
{
    const char *best_name = NULL;
    unsigned best = 4;
    for (size_t i = 0; i < c->program->function_count; i++) {
        const char *candidate = c->program->functions[i]->name;
        unsigned distance = edit_distance(name, candidate);
        if (distance < best) { best = distance; best_name = candidate; }
    }
    return best_name;
}

static bool same_numeric_type(YType a, YType b)
{
    return (a == TYPE_INT && b == TYPE_INT) ||
           (a == TYPE_FLOAT && b == TYPE_FLOAT);
}

static bool expr_contains_call_or_assignment(Expr *expr)
{
    if (!expr) return false;
    switch (expr->kind) {
        case EXPR_CALL: case EXPR_ASSIGN: return true;
        case EXPR_UNARY:
            return expr_contains_call_or_assignment(expr->as.unary.right);
        case EXPR_BINARY:
            return expr_contains_call_or_assignment(expr->as.binary.left) ||
                   expr_contains_call_or_assignment(expr->as.binary.right);
        case EXPR_FSTRING:
            for (size_t i = 0; i < expr->as.fstring.count; i++) {
                if (expr_contains_call_or_assignment(expr->as.fstring.parts[i].expression)) return true;
            }
            return false;
        default: return false;
    }
}

static YType check_expr(Checker *checker, Expr *expr);
static void check_stmt(Checker *checker, Stmt *stmt);

static YType check_expr(Checker *checker, Expr *expr)
{
    if (!expr) return TYPE_VOID;
    Compiler *c = checker->compiler;
    switch (expr->kind) {
        case EXPR_ERROR: expr->type = TYPE_ERROR; return expr->type;
        case EXPR_INT: {
            char *text = token_copy(c, expr->token);
            errno = 0;
            char *end = NULL;
            (void)strtoll(text, &end, 10);
            if (errno == ERANGE || !end || *end != '\0') {
                diagnostic(c, expr->token, "error", "E2010",
                           "Integer literal is outside the signed 64-bit range.",
                           "Use a value between -9223372036854775808 and 9223372036854775807.");
                expr->type = TYPE_ERROR;
            } else expr->type = TYPE_INT;
            return expr->type;
        }
        case EXPR_FLOAT: {
            char *text = token_copy(c, expr->token);
            errno = 0;
            char *end = NULL;
            double value = strtod(text, &end);
            if (errno == ERANGE || !end || *end != '\0' || !isfinite(value)) {
                diagnostic(c, expr->token, "error", "E2011",
                           "Floating-point literal is invalid or out of range.",
                           "Use a finite numeric literal such as 3.14.");
                expr->type = TYPE_ERROR;
            } else expr->type = TYPE_FLOAT;
            return expr->type;
        }
        case EXPR_BOOL: expr->type = TYPE_BOOL; return expr->type;
        case EXPR_STRING: expr->type = TYPE_STRING; return expr->type;
        case EXPR_CHAR:
            if (expr->token.length < 3 || expr->token.length > 4) {
                diagnostic(c, expr->token, "error", "E2012",
                           "YLang 1.0 char literals must contain one byte.",
                           "Use a single ASCII character such as 'A' or an escape such as '\\n'.");
                expr->type = TYPE_ERROR;
            } else expr->type = TYPE_CHAR;
            return expr->type;
        case EXPR_FSTRING:
            for (size_t i = 0; i < expr->as.fstring.count; i++) {
                FPart *part = &expr->as.fstring.parts[i];
                if (part->expression) {
                    YType type = check_expr(checker, part->expression);
                    if (type == TYPE_VOID) {
                        diagnostic(c, part->expression->token, "error", "E2013",
                                   "A void expression cannot be inserted into an f-string.",
                                   "Use a value-returning expression inside '{...}'.");
                    }
                    if (expr_contains_call_or_assignment(part->expression)) {
                        diagnostic(c, part->expression->token, "error", "E2014",
                                   "Function calls and assignments are not allowed inside f-string interpolations in YLang 1.0.",
                                   "Compute the value in a separate statement, then interpolate the variable.");
                    }
                }
            }
            expr->type = TYPE_STRING;
            return expr->type;
        case EXPR_NAME: {
            VarDecl *var = scope_lookup(checker->scope, expr->as.name.name);
            if (!var) {
                const char *near = nearest_variable(checker, expr->as.name.name);
                if (near) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                                   "Did you mean '%s'? Check the spelling and declaration.", near);
                    diagnostic(c, expr->token, "error", "E2020",
                               "Unknown variable name.", suggestion);
                } else {
                    diagnostic(c, expr->token, "error", "E2020",
                               "Unknown variable name.",
                               "Declare the variable with 'let type name' before using it.");
                }
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            expr->as.name.name = var->name;
            expr->as.name.variable = var;
            if (!var->initialized) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                               "Assign a value to '%s' before reading it.", var->name);
                diagnostic(c, expr->token, "error", "E2021",
                           "This variable may be used before it is initialized.", suggestion);
            }
            expr->type = var->type;
            return expr->type;
        }
        case EXPR_ASSIGN: {
            if (checker->allowed_assignment != expr) {
                diagnostic(c, expr->token, "error", "E2023",
                           "Assignment is a statement in YLang 1.0, not a nested value expression.",
                           "Move the assignment to its own statement, then use the variable.");
            }
            Expr *target = expr->as.assign.target;
            const char *name = target && target->kind == EXPR_NAME ? target->as.name.name : "";
            VarDecl *var = scope_lookup(checker->scope, name);
            if (!var) {
                diagnostic(c, expr->token, "error", "E2020",
                           "Cannot assign to an unknown variable.",
                           "Declare the variable before assigning to it.");
                (void)check_expr(checker, expr->as.assign.right);
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            expr->as.assign.variable = var;
            YType right = check_expr(checker, expr->as.assign.right);
            if (var->is_const) {
                diagnostic(c, expr->token, "error", "E2022",
                           "Cannot assign to a const variable.",
                           "Keep 'const' and remove this assignment, or remove 'const' if the variable should be mutable.");
            }
            if (right != TYPE_ERROR && right != var->type) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                    "'%s' has type %s, but the assigned expression has type %s.",
                    var->name, type_name(var->type), type_name(right));
                diagnostic(c, expr->token, "error", "E2001",
                           "Assignment type mismatch.", suggestion);
            }
            var->initialized = true;
            expr->type = var->type;
            return expr->type;
        }
        case EXPR_UNARY: {
            TokenType op = expr->as.unary.op.type;
            Expr *operand = expr->as.unary.right;
            if (op == TOKEN_MINUS && operand && operand->kind == EXPR_INT) {
                const char *digits = operand->token.start;
                size_t length = operand->token.length;
                while (length > 1 && *digits == '0') { digits++; length--; }
                if (length == 19 && memcmp(digits, "9223372036854775808", 19) == 0) {
                    expr->is_min_int = true;
                    operand->type = TYPE_INT;
                    expr->type = TYPE_INT;
                    return expr->type;
                }
            }
            YType right = check_expr(checker, operand);
            if (op == TOKEN_MINUS) {
                if (right != TYPE_INT && right != TYPE_FLOAT && right != TYPE_ERROR) {
                    diagnostic(c, expr->token, "error", "E2030",
                               "Unary '-' requires a numeric value.",
                               "Use int or float, or remove the minus sign.");
                    expr->type = TYPE_ERROR;
                } else expr->type = right;
            } else {
                if (right != TYPE_BOOL && right != TYPE_ERROR) {
                    diagnostic(c, expr->token, "error", "E2031",
                               "Logical negation requires a bool value.",
                               "Use a comparison such as 'age >= 18' to produce bool.");
                    expr->type = TYPE_ERROR;
                } else expr->type = TYPE_BOOL;
            }
            return expr->type;
        }
        case EXPR_BINARY: {
            YType left = check_expr(checker, expr->as.binary.left);
            YType right = check_expr(checker, expr->as.binary.right);
            TokenType op = expr->as.binary.op.type;
            if (left == TYPE_ERROR || right == TYPE_ERROR) {
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            if (op == TOKEN_AND || op == TOKEN_OR) {
                if (left != TYPE_BOOL || right != TYPE_BOOL) {
                    diagnostic(c, expr->token, "error", "E2032",
                               "Logical operators 'and' and 'or' require bool operands.",
                               "Compare values first, for example 'age >= 18 and enabled'.");
                    expr->type = TYPE_ERROR;
                } else expr->type = TYPE_BOOL;
                return expr->type;
            }
            if (op == TOKEN_PLUS || op == TOKEN_MINUS || op == TOKEN_STAR ||
                op == TOKEN_SLASH || op == TOKEN_PERCENT) {
                if (op == TOKEN_PERCENT) {
                    if (left != TYPE_INT || right != TYPE_INT) {
                        diagnostic(c, expr->token, "error", "E2033",
                                   "The '%' operator requires two int operands.",
                                   "Use int values on both sides of '%'.");
                        expr->type = TYPE_ERROR;
                    } else expr->type = TYPE_INT;
                } else if (!same_numeric_type(left, right)) {
                    diagnostic(c, expr->token, "error", "E2034",
                               "Arithmetic operands must have the same numeric type.",
                               "Use two int values or two float values; convert explicitly when needed.");
                    expr->type = TYPE_ERROR;
                } else expr->type = left;
                return expr->type;
            }
            if (op == TOKEN_LESS || op == TOKEN_LESS_EQUAL ||
                op == TOKEN_GREATER || op == TOKEN_GREATER_EQUAL) {
                if (!(same_numeric_type(left, right) ||
                      (left == TYPE_CHAR && right == TYPE_CHAR))) {
                    diagnostic(c, expr->token, "error", "E2035",
                               "Ordered comparisons require matching numeric types or two chars.",
                               "Make both operands the same compatible type.");
                    expr->type = TYPE_ERROR;
                } else expr->type = TYPE_BOOL;
                return expr->type;
            }
            if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL) {
                if (left != right || left == TYPE_VOID) {
                    diagnostic(c, expr->token, "error", "E2036",
                               "Equality comparisons require matching non-void types.",
                               "Compare values of the same type.");
                    expr->type = TYPE_ERROR;
                } else expr->type = TYPE_BOOL;
                return expr->type;
            }
            expr->type = TYPE_ERROR;
            return expr->type;
        }
        case EXPR_CALL: {
            Function *function = find_function(c, expr->as.call.name);
            if (!function) {
                const char *near = nearest_function(c, expr->as.call.name);
                if (edit_distance(expr->as.call.name, "print") <= 2) {
                    diagnostic(c, expr->token, "error", "E2040",
                               "Unknown function name; 'print' is a built-in statement, not a function.",
                               "Did you mean 'print(value);'? Use print as a statement.");
                } else if (near) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                                   "Did you mean function '%s'?", near);
                    diagnostic(c, expr->token, "error", "E2040",
                               "Call to an unknown function.", suggestion);
                } else {
                    diagnostic(c, expr->token, "error", "E2040",
                               "Call to an unknown function.",
                               "Define the function with the 'function' keyword.");
                }
                for (size_t i = 0; i < expr->as.call.count; i++)
                    (void)check_expr(checker, expr->as.call.args[i]);
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            expr->as.call.function = function;
            if (expr->as.call.count != function->param_count) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                               "Function '%s' expects %zu argument(s), but received %zu.",
                               function->name, function->param_count, expr->as.call.count);
                diagnostic(c, expr->token, "error", "E2041",
                           "Incorrect number of function arguments.", suggestion);
            }
            size_t shared = expr->as.call.count < function->param_count ?
                            expr->as.call.count : function->param_count;
            for (size_t i = 0; i < expr->as.call.count; i++) {
                YType arg_type = check_expr(checker, expr->as.call.args[i]);
                if (i < shared && arg_type != TYPE_ERROR &&
                    arg_type != function->params[i]->type) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "Argument %zu of '%s' expects %s, but received %s.",
                        i + 1, function->name, type_name(function->params[i]->type),
                        type_name(arg_type));
                    diagnostic(c, expr->as.call.args[i]->token, "error", "E2042",
                               "Function argument type mismatch.", suggestion);
                }
            }
            expr->type = function->return_type;
            return expr->type;
        }
    }
    expr->type = TYPE_ERROR;
    return expr->type;
}

static bool stmt_guarantees_return(Stmt *stmt)
{
    if (!stmt) return false;
    if (stmt->kind == STMT_RETURN) return true;
    if (stmt->kind == STMT_BLOCK) {
        for (size_t i = 0; i < stmt->as.block.count; i++) {
            if (stmt_guarantees_return(stmt->as.block.items[i])) return true;
        }
    }
    if (stmt->kind == STMT_IF) {
        return stmt->as.if_stmt.else_branch &&
               stmt_guarantees_return(stmt->as.if_stmt.then_branch) &&
               stmt_guarantees_return(stmt->as.if_stmt.else_branch);
    }
    return false;
}

static bool global_initializer_is_constant(Expr *expr)
{
    if (!expr) return true;
    switch (expr->kind) {
        case EXPR_INT: case EXPR_FLOAT: case EXPR_BOOL: case EXPR_CHAR: case EXPR_STRING:
            return true;
        case EXPR_UNARY:
            return (expr->as.unary.op.type == TOKEN_MINUS) &&
                   global_initializer_is_constant(expr->as.unary.right);
        default: return false;
    }
}

static void check_stmt(Checker *checker, Stmt *stmt)
{
    if (!stmt) return;
    Compiler *c = checker->compiler;
    switch (stmt->kind) {
        case STMT_BLOCK: {
            Scope *outer = checker->scope;
            checker->scope = scope_new(c, outer);
            for (size_t i = 0; i < stmt->as.block.count; i++)
                check_stmt(checker, stmt->as.block.items[i]);
            checker->scope = outer;
            break;
        }
        case STMT_VAR: {
            VarDecl *var = stmt->as.variable;
            if (var->type == TYPE_VOID) {
                diagnostic(c, var->token, "error", "E2005",
                           "A variable cannot have type void.",
                           "Use a value type such as int, float, bool, char, or string.");
            }
            if (var->initializer) {
                YType init_type = check_expr(checker, var->initializer);
                if (init_type != TYPE_ERROR && init_type != var->type) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "'%s' has type %s, but its initializer has type %s.",
                        var->name, type_name(var->type), type_name(init_type));
                    diagnostic(c, var->initializer->token, "error", "E2001",
                               "Variable initializer type mismatch.", suggestion);
                }
                var->initialized = true;
            }
            (void)scope_add(checker, checker->scope, var);
            break;
        }
        case STMT_PRINT:
            for (size_t i = 0; i < stmt->as.print.count; i++) {
                YType type = check_expr(checker, stmt->as.print.args[i]);
                if (type == TYPE_VOID) {
                    diagnostic(c, stmt->as.print.args[i]->token, "error", "E2050",
                               "print() cannot print a void expression.",
                               "Print a value or change the function to return one.");
                }
            }
            break;
        case STMT_IF: {
            YType cond = check_expr(checker, stmt->as.if_stmt.condition);
            if (cond != TYPE_BOOL && cond != TYPE_ERROR) {
                diagnostic(c, stmt->as.if_stmt.condition->token, "error", "E2051",
                           "if condition must have type bool.",
                           "Write a comparison such as 'if (age >= 18)'.");
            }
            size_t n = c->all_var_count;
            bool *before = arena_alloc(&c->arena, n * sizeof(bool));
            bool *after_then = arena_alloc(&c->arena, n * sizeof(bool));
            for (size_t i = 0; i < n; i++) before[i] = c->all_vars[i]->initialized;
            check_stmt(checker, stmt->as.if_stmt.then_branch);
            for (size_t i = 0; i < n; i++) after_then[i] = c->all_vars[i]->initialized;
            for (size_t i = 0; i < n; i++) c->all_vars[i]->initialized = before[i];
            if (stmt->as.if_stmt.else_branch) {
                check_stmt(checker, stmt->as.if_stmt.else_branch);
                for (size_t i = 0; i < n; i++)
                    c->all_vars[i]->initialized = after_then[i] && c->all_vars[i]->initialized;
            } else {
                for (size_t i = 0; i < n; i++) c->all_vars[i]->initialized = before[i];
            }
            break;
        }
        case STMT_LOOP: {
            size_t n = c->all_var_count;
            bool *before = arena_alloc(&c->arena, n * sizeof(bool));
            for (size_t i = 0; i < n; i++) before[i] = c->all_vars[i]->initialized;
            checker->loop_depth++;
            check_stmt(checker, stmt->as.loop_body);
            checker->loop_depth--;
            for (size_t i = 0; i < n; i++) c->all_vars[i]->initialized = before[i];
            break;
        }
        case STMT_BREAK:
        case STMT_CONTINUE:
            if (checker->loop_depth == 0) {
                diagnostic(c, stmt->token, "error", "E2052",
                           stmt->kind == STMT_BREAK ? "break is only valid inside loop()." :
                                                      "continue is only valid inside loop().",
                           "Place this statement inside a loop() block.");
            }
            break;
        case STMT_RETURN: {
            YType expected = checker->function->return_type;
            if (!stmt->as.return_value) {
                if (expected != TYPE_VOID) {
                    char suggestion[192];
                    (void)snprintf(suggestion, sizeof(suggestion),
                                   "This function returns %s, so return a value.", type_name(expected));
                    diagnostic(c, stmt->token, "error", "E2053",
                               "Missing return value.", suggestion);
                }
            } else {
                YType actual = check_expr(checker, stmt->as.return_value);
                if (expected == TYPE_VOID) {
                    diagnostic(c, stmt->token, "error", "E2054",
                               "A void function cannot return a value.",
                               "Use 'return;' or change the function return type.");
                } else if (actual != TYPE_ERROR && actual != expected) {
                    char suggestion[192];
                    (void)snprintf(suggestion, sizeof(suggestion),
                                   "Expected a %s return value, but received %s.",
                                   type_name(expected), type_name(actual));
                    diagnostic(c, stmt->token, "error", "E2055",
                               "Return type mismatch.", suggestion);
                }
            }
            break;
        }
        case STMT_EXPR: {
            Expr *previous_assignment = checker->allowed_assignment;
            checker->allowed_assignment = stmt->as.expression &&
                stmt->as.expression->kind == EXPR_ASSIGN ? stmt->as.expression : NULL;
            (void)check_expr(checker, stmt->as.expression);
            checker->allowed_assignment = previous_assignment;
            break;
        }
    }
}

bool check_program(Compiler *c)
{
    Checker checker = { .compiler = c, .scope = NULL, .function = NULL, .loop_depth = 0 };
    Scope *globals = scope_new(c, NULL);
    checker.scope = globals;

    for (size_t i = 0; i < c->program->global_count; i++) {
        VarDecl *var = c->program->globals[i];
        if (var->type == TYPE_VOID) {
            diagnostic(c, var->token, "error", "E2005",
                       "A variable cannot have type void.",
                       "Use a value type such as int, float, bool, char, or string.");
        }
        (void)scope_add(&checker, globals, var);
    }
    for (size_t i = 0; i < c->program->global_count; i++) {
        VarDecl *var = c->program->globals[i];
        if (var->initializer) {
            YType actual = check_expr(&checker, var->initializer);
            if (actual != TYPE_ERROR && actual != var->type) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                    "Global '%s' has type %s, but its initializer has type %s.",
                    var->name, type_name(var->type), type_name(actual));
                diagnostic(c, var->initializer->token, "error", "E2001",
                           "Global initializer type mismatch.", suggestion);
            }
            if (!global_initializer_is_constant(var->initializer)) {
                diagnostic(c, var->token, "error", "E2056",
                           "Global initializers must be constant literals in YLang 1.0.",
                           "Move computed initialization into function main().");
            }
        }
    }

    for (size_t i = 0; i < c->program->function_count; i++) {
        for (size_t j = i + 1; j < c->program->function_count; j++) {
            if (strcmp(c->program->functions[i]->name,
                       c->program->functions[j]->name) == 0) {
                diagnostic(c, c->program->functions[j]->token, "error", "E2043",
                           "This function is already defined.",
                           "YLang 1.0 does not support function overloading; rename one definition.");
            }
        }
    }

    Function *main_function = find_function(c, "main");
    if (!main_function) {
        Token token = { .type = TOKEN_EOF, .start = c->source + c->source_length,
                        .length = 0, .line = 1, .column = 1, .message = NULL };
        diagnostic(c, token, "error", "E2060",
                   "Program has no function main().",
                   "Add 'function main() -> int { return 0; }'.");
    } else if (main_function->param_count != 0 ||
               (main_function->return_type != TYPE_INT &&
                main_function->return_type != TYPE_VOID)) {
        diagnostic(c, main_function->token, "error", "E2061",
                   "main() must have no parameters and return int or void.",
                   "Use 'function main() -> int' or 'function main() -> void'.");
    }

    for (size_t i = 0; i < c->program->function_count; i++) {
        Function *function = c->program->functions[i];
        checker.function = function;
        checker.loop_depth = 0;
        checker.scope = scope_new(c, globals);
        for (size_t j = 0; j < function->param_count; j++) {
            function->params[j]->initialized = true;
            if (function->params[j]->type == TYPE_VOID) {
                diagnostic(c, function->params[j]->token, "error", "E2005",
                           "A function parameter cannot have type void.",
                           "Use a value type such as int, float, bool, char, or string.");
            }
            (void)scope_add(&checker, checker.scope, function->params[j]);
        }
        check_stmt(&checker, function->body);
        if (function->return_type != TYPE_VOID &&
            !stmt_guarantees_return(function->body)) {
            diagnostic(c, function->token, "error", "E2062",
                       "Not every path in this function returns a value.",
                       "Add a return statement, or return in every if/else branch.");
        }
    }

    return !c->had_error;
}

