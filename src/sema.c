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
    VarDecl *variable;
    bool is_mut;
    Token token;
} ActiveBorrow;

typedef struct {
    Compiler *compiler;
    Scope *scope;
    Function *function;
    unsigned loop_depth;
    Expr *allowed_assignment;
    bool allow_borrow_expr;
    bool resolving_borrow_target;
    ActiveBorrow *active_borrows;
    size_t active_borrow_count;
    size_t active_borrow_capacity;
} Checker;

static bool type_is_array(YType type)
{
    return type == TYPE_INT_ARRAY || type == TYPE_FLOAT_ARRAY ||
           type == TYPE_BOOL_ARRAY || type == TYPE_CHAR_ARRAY ||
           type == TYPE_STRING_ARRAY;
}

static bool type_is_owned(YType type)
{
    return type == TYPE_STRING || type_is_array(type);
}

static YType array_type_for(YType element)
{
    switch (element) {
        case TYPE_INT: return TYPE_INT_ARRAY;
        case TYPE_FLOAT: return TYPE_FLOAT_ARRAY;
        case TYPE_BOOL: return TYPE_BOOL_ARRAY;
        case TYPE_CHAR: return TYPE_CHAR_ARRAY;
        case TYPE_STRING: return TYPE_STRING_ARRAY;
        default: return TYPE_ERROR;
    }
}

static YType array_element_type(YType type)
{
    switch (type) {
        case TYPE_INT_ARRAY: return TYPE_INT;
        case TYPE_FLOAT_ARRAY: return TYPE_FLOAT;
        case TYPE_BOOL_ARRAY: return TYPE_BOOL;
        case TYPE_CHAR_ARRAY: return TYPE_CHAR;
        case TYPE_STRING_ARRAY: return TYPE_STRING;
        default: return TYPE_ERROR;
    }
}

static bool type_is_borrowable_scalar(YType type)
{
    return type == TYPE_INT || type == TYPE_FLOAT ||
           type == TYPE_BOOL || type == TYPE_CHAR;
}

static void push_active_borrow(Checker *checker, VarDecl *variable,
                               bool is_mut, Token token)
{
    if (checker->active_borrow_count == checker->active_borrow_capacity) {
        size_t capacity = checker->active_borrow_capacity
            ? checker->active_borrow_capacity * 2 : 8;
        ActiveBorrow *grown = arena_alloc(&checker->compiler->arena,
                                         capacity * sizeof(*grown));
        if (checker->active_borrow_count) {
            memcpy(grown, checker->active_borrows,
                   checker->active_borrow_count * sizeof(*grown));
        }
        checker->active_borrows = grown;
        checker->active_borrow_capacity = capacity;
    }
    checker->active_borrows[checker->active_borrow_count++] =
        (ActiveBorrow){ .variable = variable, .is_mut = is_mut, .token = token };
}

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

/* The checker enforces source-level ownership. Heap strings are still tracked
 * until process exit by the bootstrap runtime; scope cleanup is a later stage. */
static void consume_owned_value(Checker *checker, Expr *expr)
{
    if (!expr || !type_is_owned(expr->type) || expr->kind != EXPR_NAME ||
        !expr->as.name.variable) return;
    VarDecl *var = expr->as.name.variable;
    if (var->is_global) {
        diagnostic(checker->compiler, expr->token, "error", "E2071",
                   "A global owned value cannot be moved out of global storage.",
                   "Use clone(global_name) to create an independently owned local copy.");
        return;
    }
    var->is_moved = true;
}

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
                    if (type_is_array(type)) {
                        diagnostic(c, part->expression->token, "error", "E207A",
                                   "An array cannot be inserted directly into an f-string.",
                                   "Format an individual element or print the array to inspect its length.");
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
            if (type_is_owned(var->type) && var->is_moved) {
                diagnostic(c, expr->token, "error", "E2070",
                           "This owned value was moved and can no longer be used.",
                           "Use the new owner, or clone the value before moving it.");
            }
            if (!var->initialized) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                               "Assign a value to '%s' before reading it.", var->name);
                diagnostic(c, expr->token, "error", "E2021",
                           "This variable may be used before it is initialized.", suggestion);
            }
            if (!checker->resolving_borrow_target) {
                for (size_t i = 0; i < checker->active_borrow_count; i++) {
                    ActiveBorrow *borrow = &checker->active_borrows[i];
                    if (borrow->variable == var && borrow->is_mut) {
                        diagnostic(c, expr->token, "error", "E2067",
                                   "Cannot read a variable while it is mutably borrowed by another argument.",
                                   "Avoid using the same variable in other arguments during a mutable borrow.");
                        break;
                    }
                }
            }
            expr->type = var->type;
            return expr->type;
        }
        case EXPR_BORROW: {
            if (!checker->allow_borrow_expr) {
                diagnostic(c, expr->token, "error", "E2063",
                           "A borrow expression can only be passed directly to a matching borrowed function parameter.",
                           "Declare a parameter as '&int value' or '&mut int value', then pass '&name' or '&mut name'.");
            }
            bool previous_resolving = checker->resolving_borrow_target;
            checker->resolving_borrow_target = true;
            YType target_type = check_expr(checker, expr->as.borrow.target);
            checker->resolving_borrow_target = previous_resolving;
            VarDecl *var = expr->as.borrow.target
                ? expr->as.borrow.target->as.name.variable : NULL;
            expr->as.borrow.variable = var;
            if (!var || target_type == TYPE_ERROR) {
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            if (!type_is_borrowable_scalar(var->type)) {
                diagnostic(c, expr->token, "error", "E2064",
                           "Borrowing currently supports int, float, bool, and char only.",
                           "Pass string values normally; string and array borrowing require the owning-memory model.");
            }
            if (var->is_const && expr->as.borrow.is_mut) {
                diagnostic(c, expr->token, "error", "E2066",
                           "Cannot mutably borrow a const variable.",
                           "Remove 'const' only when mutation is intended and safe.");
            }
            if (var->is_global && !var->is_const) {
                diagnostic(c, expr->token, "error", "E2065",
                           "Mutable global variables cannot be borrowed in this initial implementation.",
                           "Copy the value to a local variable, or make the global const.");
            }
            for (size_t i = 0; i < checker->active_borrow_count; i++) {
                ActiveBorrow *active = &checker->active_borrows[i];
                if (active->variable == var &&
                    (active->is_mut || expr->as.borrow.is_mut)) {
                    diagnostic(c, expr->token, "error", "E2045",
                               "This function call creates overlapping borrows of the same variable.",
                               "Use multiple shared borrows, or pass the variable to only one mutable borrow parameter.");
                    break;
                }
            }
            if (checker->allow_borrow_expr) {
                push_active_borrow(checker, var, expr->as.borrow.is_mut, expr->token);
            }
            expr->type = target_type;
            return expr->type;
        }
        case EXPR_ASSIGN: {
            if (checker->allowed_assignment != expr) {
                diagnostic(c, expr->token, "error", "E2023",
                           "Assignment is a statement in YLang 1.0, not a nested value expression.",
                           "Move the assignment to its own statement, then use the variable.");
            }
            Expr *target = expr->as.assign.target;
            if (target && target->kind == EXPR_INDEX) {
                YType target_type = check_expr(checker, target);
                YType right_type = check_expr(checker, expr->as.assign.right);
                VarDecl *array_var = target->as.index.array &&
                    target->as.index.array->kind == EXPR_NAME
                    ? target->as.index.array->as.name.variable : NULL;
                if (array_var && array_var->is_const) {
                    diagnostic(c, expr->token, "error", "E2022",
                               "Cannot modify an element of a const array.",
                               "Remove const only if the array contents should be mutable.");
                }
                if (right_type != TYPE_ERROR && target_type != TYPE_ERROR &&
                    right_type != target_type) {
                    diagnostic(c, expr->token, "error", "E2001",
                               "Array element assignment type mismatch.",
                               "Assign a value with the same type as the array element.");
                }
                if (target_type == TYPE_STRING && right_type == TYPE_STRING)
                    consume_owned_value(checker, expr->as.assign.right);
                expr->type = target_type;
                return expr->type;
            }
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
            if (target && target->kind == EXPR_NAME) {
                target->as.name.variable = var;
                target->as.name.name = var->name;
            }
            for (size_t i = 0; i < checker->active_borrow_count; i++) {
                if (checker->active_borrows[i].variable == var) {
                    diagnostic(c, expr->token, "error", "E2068",
                               "Cannot assign to a variable while it is borrowed by another argument.",
                               "Do not mutate a variable while a borrow of it is active in the current call.");
                    break;
                }
            }
            YType right = check_expr(checker, expr->as.assign.right);
            if (var->is_const) {
                diagnostic(c, expr->token, "error", "E2022",
                           "Cannot assign to a const variable.",
                           "Keep 'const' and remove this assignment, or remove 'const' if the variable should be mutable.");
            }
            if (type_is_owned(right) && right == var->type &&
                expr->as.assign.right && expr->as.assign.right->kind == EXPR_NAME &&
                expr->as.assign.right->as.name.variable == var) {
                diagnostic(c, expr->token, "error", "E2072",
                           "A string cannot be moved into itself.",
                           "Assign from a different string value or use clone(name).");
            } else if (type_is_owned(right) && right == var->type) {
                consume_owned_value(checker, expr->as.assign.right);
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
            var->is_moved = false;
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
                if (left != right || left == TYPE_VOID || type_is_array(left)) {
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
        case EXPR_ARRAY: {
            if (expr->as.array.count == 0) {
                diagnostic(c, expr->token, "error", "E2073",
                           "An array literal must contain at least one element in this version.",
                           "Add an initial element, such as [1], or initialize the array before use.");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            YType element = check_expr(checker, expr->as.array.items[0]);
            bool valid = true;
            if (type_is_array(element) || element == TYPE_VOID || element == TYPE_ERROR) {
                diagnostic(c, expr->as.array.items[0]->token, "error", "E2074",
                           "Array elements must be scalar values of one matching type.",
                           "Use int, float, bool, char, or string elements; nested arrays are not supported yet.");
                valid = false;
            }
            for (size_t i = 1; i < expr->as.array.count; i++) {
                YType item = check_expr(checker, expr->as.array.items[i]);
                if (item != element && item != TYPE_ERROR) {
                    diagnostic(c, expr->as.array.items[i]->token, "error", "E2075",
                               "All elements in an array literal must have the same type.",
                               "Convert the value explicitly or use a separate array for the other type.");
                    valid = false;
                }
                if (item == TYPE_ERROR) valid = false;
            }
            expr->as.array.element_type = element;
            expr->type = valid ? array_type_for(element) : TYPE_ERROR;
            return expr->type;
        }
        case EXPR_INDEX: {
            YType array = check_expr(checker, expr->as.index.array);
            YType index = check_expr(checker, expr->as.index.index);
            YType element = array_element_type(array);
            if (element == TYPE_ERROR && array != TYPE_ERROR) {
                diagnostic(c, expr->as.index.array->token, "error", "E2076",
                           "Indexing requires an array value.",
                           "Declare an array such as int[] values = [1, 2].");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            if (index != TYPE_INT && index != TYPE_ERROR) {
                diagnostic(c, expr->as.index.index->token, "error", "E2077",
                           "Array indexes must be int values.",
                           "Use an integer index such as values[0].");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            expr->as.index.element_type = element;
            expr->type = element;
            return expr->type;
        }
        case EXPR_CALL: {
            size_t borrow_base = checker->active_borrow_count;
            Function *function = find_function(c, expr->as.call.name);
            if (!function && strcmp(expr->as.call.name, "read_line") == 0) {
                if (expr->as.call.count != 0) {
                    diagnostic(c, expr->token, "error", "E2041",
                               "read_line() does not accept arguments.",
                               "Call read_line() with empty parentheses.");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                } else {
                    expr->type = TYPE_STRING;
                }
                checker->active_borrow_count = borrow_base;
                return expr->type;
            }
            if (!function &&
                (strcmp(expr->as.call.name, "parse_int") == 0 ||
                 strcmp(expr->as.call.name, "parse_float") == 0)) {
                bool wants_int = strcmp(expr->as.call.name, "parse_int") == 0;
                if (expr->as.call.count != 1) {
                    diagnostic(c, expr->token, "error", "E2041",
                               "parse_int() and parse_float() expect one string argument.",
                               "Pass the text returned by read_line(), for example parse_int(read_line()).");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                } else {
                    YType arg_type = check_expr(checker, expr->as.call.args[0]);
                    if (arg_type != TYPE_STRING && arg_type != TYPE_ERROR) {
                        diagnostic(c, expr->as.call.args[0]->token, "error", "E2042",
                                   "Numeric parsing requires a string argument.",
                                   "Use parse_int(text) or parse_float(text) with a string value.");
                        expr->type = TYPE_ERROR;
                    } else {
                        expr->type = arg_type == TYPE_ERROR ? TYPE_ERROR :
                                     (wants_int ? TYPE_INT : TYPE_FLOAT);
                    }
                }
                checker->active_borrow_count = borrow_base;
                return expr->type;
            }
            if (!function && strcmp(expr->as.call.name, "clone") == 0) {
                if (expr->as.call.count != 1) {
                    diagnostic(c, expr->token, "error", "E2041",
                               "clone() expects exactly one string argument.",
                               "Use clone(text) to create an independently owned copy.");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                } else {
                    YType arg_type = check_expr(checker, expr->as.call.args[0]);
                    if (!type_is_owned(arg_type) && arg_type != TYPE_ERROR) {
                        diagnostic(c, expr->as.call.args[0]->token, "error", "E2042",
                                   "clone() accepts an owned string or array value.",
                                   "Pass a string or array value.");
                        expr->type = TYPE_ERROR;
                    } else {
                        expr->type = arg_type;
                    }
                }
                checker->active_borrow_count = borrow_base;
                return expr->type;
            }
            if (!function && strcmp(expr->as.call.name, "append") == 0) {
                if (expr->as.call.count != 2) {
                    diagnostic(c, expr->token, "error", "E2041",
                               "append() expects an array and one value.",
                               "Use append(values, item) to add one item to an array.");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                } else {
                    Expr *array_expr = expr->as.call.args[0];
                    YType array_type = check_expr(checker, array_expr);
                    YType value_type = check_expr(checker, expr->as.call.args[1]);
                    YType element_type = array_element_type(array_type);
                    if (element_type == TYPE_ERROR && array_type != TYPE_ERROR) {
                        diagnostic(c, array_expr->token, "error", "E2076",
                                   "append() requires an array as its first argument.",
                                   "Pass a mutable array variable.");
                        expr->type = TYPE_ERROR;
                    } else if (!array_expr || array_expr->kind != EXPR_NAME ||
                               !array_expr->as.name.variable) {
                        diagnostic(c, array_expr ? array_expr->token : expr->token,
                                   "error", "E2078",
                                   "append() requires a named array variable.",
                                   "Store the array in a variable before appending.");
                        expr->type = TYPE_ERROR;
                    } else {
                        VarDecl *array_var = array_expr->as.name.variable;
                        if (array_var->is_const) {
                            diagnostic(c, array_expr->token, "error", "E2079",
                                       "Cannot append to a const array.",
                                       "Use a mutable array variable.");
                            expr->type = TYPE_ERROR;
                        } else if (value_type != TYPE_ERROR && element_type != TYPE_ERROR &&
                                   value_type != element_type) {
                            diagnostic(c, expr->as.call.args[1]->token, "error", "E2001",
                                       "append() value type does not match the array element type.",
                                       "Append a value with the array's element type.");
                            expr->type = TYPE_ERROR;
                        } else {
                            if (element_type == TYPE_STRING && value_type == TYPE_STRING)
                                consume_owned_value(checker, expr->as.call.args[1]);
                            expr->type = TYPE_VOID;
                        }
                    }
                }
                checker->active_borrow_count = borrow_base;
                return expr->type;
            }
            if (!function && strcmp(expr->as.call.name, "len") == 0) {
                if (expr->as.call.count != 1) {
                    diagnostic(c, expr->token, "error", "E2041",
                               "len() expects exactly one string argument.",
                               "Use len(text) to get the UTF-8 byte length of a string.");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                } else {
                    YType arg_type = check_expr(checker, expr->as.call.args[0]);
                    if (arg_type != TYPE_STRING && !type_is_array(arg_type) && arg_type != TYPE_ERROR) {
                        diagnostic(c, expr->as.call.args[0]->token, "error", "E2042",
                                   "len() accepts a string or array value.",
                                   "Pass a string or array.");
                        expr->type = TYPE_ERROR;
                    } else {
                        expr->type = arg_type == TYPE_ERROR ? TYPE_ERROR : TYPE_INT;
                    }
                }
                checker->active_borrow_count = borrow_base;
                return expr->type;
            }
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
                checker->active_borrow_count = borrow_base;
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
                Expr *arg = expr->as.call.args[i];
                bool is_borrow_expr = arg && arg->kind == EXPR_BORROW;
                bool previous_allow = checker->allow_borrow_expr;
                checker->allow_borrow_expr = is_borrow_expr;
                YType arg_type = check_expr(checker, arg);
                checker->allow_borrow_expr = previous_allow;

                if (i >= shared) continue;
                VarDecl *param = function->params[i];
                if (param->is_borrowed != is_borrow_expr) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "Argument %zu of '%s' %s a borrow expression.",
                        i + 1, function->name,
                        param->is_borrowed ? "must be" : "must not be");
                    diagnostic(c, arg->token, "error", "E2042",
                               "Function argument passing mode mismatch.", suggestion);
                    continue;
                }
                if (param->is_borrowed && is_borrow_expr &&
                    param->is_mut_borrow != arg->as.borrow.is_mut) {
                    diagnostic(c, arg->token, "error", "E2044",
                               "The argument's borrow mode does not match the parameter.",
                               param->is_mut_borrow
                                 ? "Pass '&mut name' to a '&mut T' parameter."
                                 : "Pass '&name' to a shared '&T' parameter.");
                }
                if (type_is_owned(arg_type) && param->type == arg_type &&
                    !param->is_borrowed) {
                    consume_owned_value(checker, arg);
                }
                if (arg_type != TYPE_ERROR && arg_type != param->type) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "Argument %zu of '%s' expects %s, but received %s.",
                        i + 1, function->name, type_name(param->type),
                        type_name(arg_type));
                    diagnostic(c, arg->token, "error", "E2042",
                               "Function argument type mismatch.", suggestion);
                }
            }
            checker->active_borrow_count = borrow_base;
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
                if (type_is_owned(init_type) && var->type == init_type) {
                    consume_owned_value(checker, var->initializer);
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
            bool *moved_before = arena_alloc(&c->arena, n * sizeof(bool));
            bool *moved_after_then = arena_alloc(&c->arena, n * sizeof(bool));
            for (size_t i = 0; i < n; i++) {
                before[i] = c->all_vars[i]->initialized;
                moved_before[i] = c->all_vars[i]->is_moved;
            }
            check_stmt(checker, stmt->as.if_stmt.then_branch);
            for (size_t i = 0; i < n; i++) {
                after_then[i] = c->all_vars[i]->initialized;
                moved_after_then[i] = c->all_vars[i]->is_moved;
                c->all_vars[i]->initialized = before[i];
                c->all_vars[i]->is_moved = moved_before[i];
            }
            if (stmt->as.if_stmt.else_branch) {
                check_stmt(checker, stmt->as.if_stmt.else_branch);
                for (size_t i = 0; i < n; i++) {
                    c->all_vars[i]->initialized = after_then[i] && c->all_vars[i]->initialized;
                    c->all_vars[i]->is_moved = moved_after_then[i] || c->all_vars[i]->is_moved;
                }
            } else {
                for (size_t i = 0; i < n; i++) {
                    c->all_vars[i]->initialized = before[i];
                    c->all_vars[i]->is_moved = moved_before[i] || moved_after_then[i];
                }
            }
            break;
        }
        case STMT_LOOP: {
            size_t n = c->all_var_count;
            bool *before = arena_alloc(&c->arena, n * sizeof(bool));
            bool *moved_before = arena_alloc(&c->arena, n * sizeof(bool));
            for (size_t i = 0; i < n; i++) {
                before[i] = c->all_vars[i]->initialized;
                moved_before[i] = c->all_vars[i]->is_moved;
            }
            checker->loop_depth++;
            check_stmt(checker, stmt->as.loop_body);
            checker->loop_depth--;
            for (size_t i = 0; i < n; i++) {
                c->all_vars[i]->initialized = before[i];
                c->all_vars[i]->is_moved = moved_before[i] || c->all_vars[i]->is_moved;
            }
            break;
        }
        case STMT_FOR_EACH: {
            Expr *array_expr = stmt->as.for_each.array;
            YType array_type = check_expr(checker, array_expr);
            YType element_type = array_element_type(array_type);
            VarDecl *variable = stmt->as.for_each.variable;
            if (element_type == TYPE_ERROR && array_type != TYPE_ERROR) {
                diagnostic(c, array_expr->token, "error", "E2076",
                           "A for-each loop requires an array value.",
                           "Iterate over a typed array such as int[] values.");
            } else if (element_type == TYPE_STRING) {
                diagnostic(c, array_expr->token, "error", "E2081",
                           "For-each over string[] is not supported in this initial version.",
                           "Use an indexed loop or iterate over an array of scalar values.");
            } else if (element_type != TYPE_ERROR && variable->type != element_type) {
                char suggestion[192];
                (void)snprintf(suggestion, sizeof(suggestion),
                               "Loop variable '%s' has type %s, but the array element type is %s.",
                               variable->name, type_name(variable->type), type_name(element_type));
                diagnostic(c, variable->token, "error", "E2080",
                           "For-each loop variable type mismatch.", suggestion);
            }
            if (!array_expr || array_expr->kind != EXPR_NAME) {
                diagnostic(c, array_expr ? array_expr->token : stmt->token,
                           "error", "E2082",
                           "For-each currently requires a named array variable.",
                           "Store the array in a variable before iterating over it.");
            }
            Scope *previous_scope = checker->scope;
            Scope *loop_scope = scope_new(c, previous_scope);
            checker->scope = loop_scope;
            variable->initialized = true;
            (void)scope_add(checker, loop_scope, variable);
            size_t n = c->all_var_count;
            bool *before = arena_alloc(&c->arena, n * sizeof(bool));
            bool *moved_before = arena_alloc(&c->arena, n * sizeof(bool));
            for (size_t i = 0; i < n; i++) {
                before[i] = c->all_vars[i]->initialized;
                moved_before[i] = c->all_vars[i]->is_moved;
            }
            checker->loop_depth++;
            check_stmt(checker, stmt->as.for_each.body);
            checker->loop_depth--;
            for (size_t i = 0; i < n; i++) {
                c->all_vars[i]->initialized = before[i];
                c->all_vars[i]->is_moved = moved_before[i] || c->all_vars[i]->is_moved;
            }
            checker->scope = previous_scope;
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
                } else if (type_is_owned(actual) && expected == actual) {
                    consume_owned_value(checker, stmt->as.return_value);
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
            if (function->params[j]->is_borrowed) {
                function->params[j]->is_const = !function->params[j]->is_mut_borrow;
                if (!type_is_borrowable_scalar(function->params[j]->type)) {
                    diagnostic(c, function->params[j]->token, "error", "E2064",
                               "Borrowed parameters currently support int, float, bool, and char only.",
                               "String and array borrowing will follow after ownership and lifetime rules are implemented.");
                }
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

