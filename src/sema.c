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
    bool allow_array_name;
    bool allow_owned_move;
    bool allow_owned_value;
    bool allow_mut_borrow;
    bool checking_borrow_owner;
    bool checking_move_owner;
    Module *module;
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

static bool is_borrow_type(YType type)
{
    return type == TYPE_BORROW_STRING || type == TYPE_MUT_BORROW_STRING;
}

static void checker_active_borrows(Checker *checker, VarDecl *owner,
                                   size_t *shared_count, bool *exclusive)
{
    if (shared_count) *shared_count = 0U;
    if (exclusive) *exclusive = false;
    if (!checker || !owner) return;
    for (Scope *scope = checker->scope; scope; scope = scope->parent) {
        for (size_t i = 0; i < scope->count; i++) {
            VarDecl *candidate = scope->vars[i];
            if (candidate->borrowed_from != owner || !candidate->initialized) continue;
            if (candidate->type == TYPE_MUT_BORROW_STRING) {
                if (exclusive) *exclusive = true;
            } else if (candidate->type == TYPE_BORROW_STRING) {
                if (shared_count && *shared_count < SIZE_MAX) (*shared_count)++;
            }
        }
    }
}

static bool borrow_initializer_shape(const Expr *expr, YType target_type)
{
    if (!expr || expr->kind != EXPR_CALL || expr->as.call.count != 1U ||
        !expr->as.call.args[0] || expr->as.call.args[0]->kind != EXPR_NAME) return false;
    if (target_type == TYPE_BORROW_STRING)
        return strcmp(expr->as.call.name, "borrow") == 0;
    if (target_type == TYPE_MUT_BORROW_STRING)
        return strcmp(expr->as.call.name, "borrow_mut") == 0;
    return false;
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
    scope->vars = arena_vector_append(&checker->compiler->arena,
                                      scope->vars, scope->count,
                                      sizeof(*scope->vars), &var);
    scope->count++;
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

static EnumDecl *enum_for_type(Compiler *compiler, YType type)
{
    if (type < TYPE_ENUM_BASE || type >= TYPE_STRUCT_BASE) return NULL;
    for (size_t i = 0; i < compiler->enum_count; i++) {
        if (compiler->enums[i]->type == type) return compiler->enums[i];
    }
    return NULL;
}

static StructDecl *struct_for_type(Compiler *compiler, YType type)
{
    if (type < TYPE_STRUCT_BASE) return NULL;
    for (size_t i = 0; i < compiler->struct_count; i++) {
        if (compiler->structs[i]->type == type) return compiler->structs[i];
    }
    return NULL;
}

static const char *semantic_type_name(Compiler *compiler, YType type)
{
    EnumDecl *enumeration = enum_for_type(compiler, type);
    if (enumeration) return enumeration->name;
    StructDecl *structure = struct_for_type(compiler, type);
    if (structure) return structure->name;
    return type_name(type);
}

static StructField *sema_struct_field(StructDecl *structure, const char *name)
{
    if (!structure) return NULL;
    for (size_t i = 0; i < structure->field_count; i++) {
        if (strcmp(structure->fields[i]->name, name) == 0) return structure->fields[i];
    }
    return NULL;
}

/* Find the named struct variable that owns a possibly chained field target. */
static VarDecl *sema_member_root_variable(Expr *expr)
{
    while (expr && expr->kind == EXPR_MEMBER) {
        expr = expr->as.member.target;
    }
    if (!expr || expr->kind != EXPR_NAME) return NULL;
    return expr->as.name.variable;
}

static Function *module_local_function(Module *module, const char *name)
{
    if (!module || !module->program) return NULL;
    for (size_t i = 0; i < module->program->function_count; i++) {
        Function *function = module->program->functions[i];
        if (strcmp(function->name, name) == 0) return function;
    }
    return NULL;
}

static ImportName *module_imported_name(Module *module, const char *name)
{
    if (!module || !module->program) return NULL;
    for (size_t i = 0; i < module->program->import_count; i++) {
        ImportDecl *import = module->program->imports[i];
        for (size_t j = 0; j < import->name_count; j++) {
            ImportName *symbol = import->names[j];
            if (strcmp(symbol->name, name) == 0) return symbol;
        }
    }
    return NULL;
}

static Function *resolve_function(Checker *checker, const char *name)
{
    if (!checker->module) return find_function(checker->compiler, name);
    Function *local = module_local_function(checker->module, name);
    if (local) return local;
    ImportName *symbol = module_imported_name(checker->module, name);
    return symbol ? symbol->function : NULL;
}

static const char *nearest_function(Checker *checker, const char *name)
{
    const char *best_name = NULL;
    unsigned best = 4;
    Module *module = checker->module;
    if (!module || !module->program) return NULL;
    for (size_t i = 0; i < module->program->function_count; i++) {
        const char *candidate = module->program->functions[i]->name;
        unsigned distance = edit_distance(name, candidate);
        if (distance < best) { best = distance; best_name = candidate; }
    }
    for (size_t i = 0; i < module->program->import_count; i++) {
        ImportDecl *import = module->program->imports[i];
        for (size_t j = 0; j < import->name_count; j++) {
            ImportName *symbol = import->names[j];
            if (!symbol->function) continue;
            unsigned distance = edit_distance(name, symbol->name);
            if (distance < best) { best = distance; best_name = symbol->name; }
        }
    }
    return best_name;
}

static bool same_numeric_type(YType a, YType b)
{
    return (a == TYPE_INT && b == TYPE_INT) ||
           (a == TYPE_FLOAT && b == TYPE_FLOAT);
}

static bool string_like_type(YType type)
{
    return type == TYPE_STRING || type == TYPE_OWNED_STRING ||
           type == TYPE_BORROW_STRING || type == TYPE_MUT_BORROW_STRING;
}

static bool owned_string_initializer_shape(Checker *checker, const Expr *expr)
{
    if (!expr || expr->kind != EXPR_CALL) return false;
    if (strcmp(expr->as.call.name, "string.own") == 0)
        return expr->as.call.count == 1U;
    if (strcmp(expr->as.call.name, "move") == 0 &&
        !resolve_function(checker, "move")) {
        return expr->as.call.count == 1U && expr->as.call.args[0] &&
               expr->as.call.args[0]->kind == EXPR_NAME;
    }
    Function *function = resolve_function(checker, expr->as.call.name);
    return function && function->return_type == TYPE_OWNED_STRING;
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

typedef struct {
    const char *name;
    size_t arity;
} MathBuiltin;

static const MathBuiltin math_builtins[] = {
    {"math.sqrt", 1}, {"math.sin", 1}, {"math.cos", 1}, {"math.tan", 1},
    {"math.asin", 1}, {"math.acos", 1}, {"math.atan", 1}, {"math.exp", 1},
    {"math.log", 1}, {"math.log10", 1}, {"math.floor", 1}, {"math.ceil", 1},
    {"math.round", 1}, {"math.abs", 1}, {"math.sinh", 1}, {"math.cosh", 1},
    {"math.tanh", 1}, {"math.pow", 2}, {"math.atan2", 2}, {"math.min", 2},
    {"math.max", 2}, {"math.hypot", 2}, {"math.clamp", 3}
};

static const MathBuiltin *find_math_builtin(const char *name)
{
    for (size_t i = 0; i < sizeof(math_builtins) / sizeof(math_builtins[0]); i++) {
        if (strcmp(name, math_builtins[i].name) == 0) return &math_builtins[i];
    }
    return NULL;
}

typedef struct {
    const char *name;
    size_t arity;
    YType result;
} StringBuiltin;

static const StringBuiltin string_builtins[] = {
    {"string.length", 1, TYPE_INT},
    {"string.contains", 2, TYPE_BOOL},
    {"string.starts_with", 2, TYPE_BOOL},
    {"string.ends_with", 2, TYPE_BOOL},
    {"string.concat", 2, TYPE_STRING},
    {"string.replace", 3, TYPE_STRING},
    {"string.upper", 1, TYPE_STRING},
    {"string.lower", 1, TYPE_STRING},
    {"string.parse_int", 1, TYPE_INT},
    {"string.is_int", 1, TYPE_BOOL},
    {"io.read_line", 0, TYPE_STRING},
    {"io.read_file", 1, TYPE_STRING},
    {"io.write_file", 2, TYPE_BOOL},
    {"io.file_exists", 1, TYPE_BOOL},
    {"path.exists", 1, TYPE_BOOL},
    {"path.basename", 1, TYPE_STRING},
    {"path.extension", 1, TYPE_STRING},
    {"image.open", 1, TYPE_BOOL}
};

static const StringBuiltin *find_string_builtin(const char *name)
{
    for (size_t i = 0; i < sizeof(string_builtins) / sizeof(string_builtins[0]); i++) {
        if (strcmp(name, string_builtins[i].name) == 0) return &string_builtins[i];
    }
    return NULL;
}

typedef struct {
    const char *name;
    size_t arity;
    YType result;
    YType args[7];
} GuiBuiltin;

static const GuiBuiltin gui_builtins[] = {
    {"gui.init", 0, TYPE_BOOL, {TYPE_VOID}},
    {"gui.create_window", 3, TYPE_BOOL, {TYPE_STRING, TYPE_INT, TYPE_INT}},
    {"gui.poll", 0, TYPE_BOOL, {TYPE_VOID}},
    {"gui.key_down", 1, TYPE_BOOL, {TYPE_STRING}},
    {"gui.mouse_x", 0, TYPE_INT, {TYPE_VOID}},
    {"gui.mouse_y", 0, TYPE_INT, {TYPE_VOID}},
    {"gui.mouse_button_down", 1, TYPE_BOOL, {TYPE_INT}},
    {"gui.ticks_ms", 0, TYPE_INT, {TYPE_VOID}},
    {"gui.clear", 3, TYPE_BOOL, {TYPE_INT, TYPE_INT, TYPE_INT}},
    {"gui.draw_rect", 7, TYPE_BOOL, {TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT, TYPE_INT}},
    {"gui.present", 0, TYPE_BOOL, {TYPE_VOID}},
    {"gui.last_error", 0, TYPE_STRING, {TYPE_VOID}},
    {"gui.close", 0, TYPE_VOID, {TYPE_VOID}},
    {"gui.shutdown", 0, TYPE_VOID, {TYPE_VOID}}
};

static const GuiBuiltin *find_gui_builtin(const char *name)
{
    for (size_t i = 0; i < sizeof(gui_builtins) / sizeof(gui_builtins[0]); i++) {
        if (strcmp(name, gui_builtins[i].name) == 0) return &gui_builtins[i];
    }
    return NULL;
}

static YType check_expr(Checker *checker, Expr *expr)
{
    if (!expr) return TYPE_VOID;
    Compiler *c = checker->compiler;
    switch (expr->kind) {
        case EXPR_ERROR: expr->type = TYPE_ERROR; return expr->type;
        case EXPR_ENUM_VALUE:
            expr->type = expr->as.enum_value.enumeration->type;
            return expr->type;
        case EXPR_STRUCT_INIT: {
            StructDecl *structure = expr->as.struct_init.structure;
            if (!structure) {
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            if (expr->as.struct_init.count != structure->field_count) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                    "Struct %s expects %zu field value(s), but received %zu.",
                    structure->name, structure->field_count, expr->as.struct_init.count);
                diagnostic(c, expr->token, "error", "E2074",
                           "Incorrect number of struct constructor arguments.", suggestion);
            }
            for (size_t i = 0; i < expr->as.struct_init.count; i++) {
                YType actual = check_expr(checker, expr->as.struct_init.args[i]);
                if (i < structure->field_count && actual != TYPE_ERROR &&
                    actual != structure->fields[i]->type) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "Field '%s' expects %s, but received %s.",
                        structure->fields[i]->name, semantic_type_name(c, structure->fields[i]->type),
                        semantic_type_name(c, actual));
                    diagnostic(c, expr->as.struct_init.args[i]->token, "error", "E2075",
                               "Struct field initializer type mismatch.", suggestion);
                }
            }
            expr->type = structure->type;
            return expr->type;
        }
        case EXPR_MEMBER: {
            YType target_type = check_expr(checker, expr->as.member.target);
            StructDecl *structure = struct_for_type(c, target_type);
            if (!structure) {
                diagnostic(c, expr->token, "error", "E2076",
                           "Field access requires a struct value.",
                           "Use field access only on a variable or expression whose type is a declared struct.");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            StructField *field = sema_struct_field(structure, expr->as.member.name);
            if (!field) {
                diagnostic(c, expr->token, "error", "E2077",
                           "This struct has no field with that name.",
                           "Check the field spelling against the struct declaration.");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            expr->as.member.field = field;
            expr->type = field->type;
            return expr->type;
        }
        case EXPR_INT: {
            char *text = token_copy(c, expr->token);
            errno = 0;
            char *end = NULL;
            (void)strtoll(text, &end,
                text[0] == '0' && (text[1] == 'x' || text[1] == 'X') ? 16 : 10);
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
        case EXPR_CHAR: {
            unsigned char *bytes = NULL;
            size_t length = 0;
            const bool valid = decode_quoted_literal(expr->token, &bytes, &length) &&
                               length == 1U;
            free(bytes);
            if (!valid) {
                diagnostic(c, expr->token, "error", "E2012",
                           "Character literals must decode to exactly one byte.",
                           "Use a character such as 'A', an escape such as '\\n', or a two-digit hex escape such as '\\x41'.");
                expr->type = TYPE_ERROR;
            } else {
                expr->type = TYPE_CHAR;
            }
            return expr->type;
        }
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
                                   "Function calls and assignments are not allowed inside f-string interpolations yet.",
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
            if (var->moved) {
                diagnostic(c, expr->token, "error", "E2110",
                           "This value was already moved and cannot be used again.",
                           "Use the new owner, or assign a fresh value before reading this variable.");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            if (!is_borrow_type(var->type) &&
                !checker->checking_borrow_owner &&
                !checker->checking_move_owner) {
                size_t shared_count = 0U;
                bool exclusive_active = false;
                checker_active_borrows(checker, var, &shared_count, &exclusive_active);
                if (exclusive_active) {
                    diagnostic(c, expr->token, "error", "E2138",
                               "The owner cannot be read directly while an exclusive borrow is active.",
                               "Use the borrow_mut binding until its scope ends.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
            }
            if (var->is_array && !checker->allow_array_name) {
                diagnostic(c, expr->token, "error", "E2029",
                           "An array must be indexed before it can be used as a value.",
                           "Use an element such as values[0], or add array support to the function you want to call.");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
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
                           "Assignment is a statement, not a nested value expression.",
                           "Move the assignment to its own statement, then use the variable.");
            }
            Expr *target = expr->as.assign.target;
            VarDecl *var = NULL;
            YType target_type = TYPE_ERROR;
            if (target && target->kind == EXPR_NAME) {
                var = scope_lookup(checker->scope, target->as.name.name);
                if (var) {
                    target->as.name.variable = var;
                    target->as.name.name = var->name;
                    target->type = var->type;
                    target_type = var->type;
                }
            } else if (target && target->kind == EXPR_INDEX) {
                (void)check_expr(checker, target);
                var = target->as.index.variable;
                target_type = target->type;
            } else if (target && target->kind == EXPR_MEMBER) {
                target_type = check_expr(checker, target);
                var = sema_member_root_variable(target);
                if (!var || !struct_for_type(c, var->type)) {
                    diagnostic(c, expr->token, "error", "E2078",
                               "Only a field path rooted at a named struct variable can be assigned.",
                               "Store the struct in a mutable variable, then assign to its field.");
                }
            }
            if (!var) {
                diagnostic(c, expr->token, "error", "E2092",
                           "Cannot assign to an unknown variable, invalid array element, or unsupported field target.",
                           "Declare the variable first and use a valid variable, array element, or struct field.");
                (void)check_expr(checker, expr->as.assign.right);
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            expr->as.assign.variable = var;
            if (expr->as.assign.is_compound &&
                target_type != TYPE_INT && target_type != TYPE_FLOAT) {
                diagnostic(c, expr->token, "error", "E2095",
                           "The '+=' operator requires an int or float target.",
                           "Use '+=' only with matching int or float values.");
            }
            if (var->is_array && target && target->kind == EXPR_NAME) {
                diagnostic(c, expr->token, "error", "E2037",
                           "Whole-array assignment is not supported.",
                           "Assign to a specific element, for example values[0] = 5;.");
                (void)check_expr(checker, expr->as.assign.right);
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            Expr *right_expr = expr->as.assign.right;
            bool self_move = target && target->kind == EXPR_NAME &&
                right_expr && right_expr->kind == EXPR_CALL &&
                strcmp(right_expr->as.call.name, "move") == 0 &&
                !resolve_function(checker, "move") &&
                right_expr->as.call.count == 1U &&
                right_expr->as.call.args[0]->kind == EXPR_NAME &&
                strcmp(target->as.name.name,
                       right_expr->as.call.args[0]->as.name.name) == 0;
            bool previous_move_context = checker->allow_owned_move;
            bool previous_owned_context = checker->allow_owned_value;
            checker->allow_owned_move = var->type == TYPE_OWNED_STRING &&
                right_expr && right_expr->kind == EXPR_CALL &&
                strcmp(right_expr->as.call.name, "move") == 0 &&
                !resolve_function(checker, "move") &&
                right_expr->as.call.count == 1U;
            checker->allow_owned_value = var->type == TYPE_OWNED_STRING;
            YType right = check_expr(checker, right_expr);
            checker->allow_owned_move = previous_move_context;
            checker->allow_owned_value = previous_owned_context;
            if (self_move) {
                diagnostic(c, expr->token, "error", "E2114",
                           "A binding cannot be moved into itself.",
                           "Move into a different destination variable.");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            if (is_borrow_type(var->type)) {
                diagnostic(c, expr->token, "error", "E2141",
                           "Borrow bindings cannot be reassigned.",
                           "Create a new borrow binding inside the scope where it is needed.");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            if (target && target->kind == EXPR_NAME &&
                (var->type == TYPE_STRING || var->type == TYPE_OWNED_STRING)) {
                size_t shared_count = 0U;
                bool exclusive_active = false;
                checker_active_borrows(checker, var, &shared_count, &exclusive_active);
                if (shared_count != 0U || exclusive_active) {
                    diagnostic(c, expr->token, "error", "E2142",
                               "An owner cannot be replaced while it is borrowed.",
                               "End the borrow before assigning a new value to the owner.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
            }
            if (var->type == TYPE_OWNED_STRING &&
                right != TYPE_ERROR && !owned_string_initializer_shape(checker, right_expr)) {
                diagnostic(c, right_expr->token, "error", "E2120",
                           "Owned strings cannot be copied by assignment.",
                           "Use string.own(value) to clone, or move(owner) to transfer ownership.");
            }
            if (var->is_const) {
                diagnostic(c, expr->token, "error", "E2022",
                           "Cannot assign to a const variable.",
                           "Keep 'const' and remove this assignment, or remove 'const' if the variable should be mutable.");
            }
            if (right != TYPE_ERROR && target_type != TYPE_ERROR && right != target_type) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                    "The assignment target has type %s, but the assigned expression has type %s.",
                    semantic_type_name(c, target_type), semantic_type_name(c, right));
                diagnostic(c, expr->token, "error", "E2001",
                           "Assignment type mismatch.", suggestion);
            }
            var->initialized = true;
            var->moved = false;
            expr->type = target_type;
            return expr->type;
        }
        case EXPR_UNARY: {
            TokenType op = expr->as.unary.op.type;
            Expr *operand = expr->as.unary.right;
            if (op == TOKEN_MINUS && operand && operand->kind == EXPR_INT) {
                char *literal = token_copy(c, operand->token);
                errno = 0;
                char *end = NULL;
                int base = literal[0] == '0' &&
                    (literal[1] == 'x' || literal[1] == 'X') ? 16 : 10;
                unsigned long long magnitude = strtoull(literal, &end, base);
                if (errno != ERANGE && end != literal && *end == '\0' &&
                    magnitude == (unsigned long long)INT64_MAX + 1ULL) {
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
                      (left == TYPE_CHAR && right == TYPE_CHAR) ||
                      (string_like_type(left) && string_like_type(right)))) {
                    diagnostic(c, expr->token, "error", "E2035",
                               "Ordered comparisons require matching numeric types, two chars, or two strings.",
                               "Make both operands the same compatible type.");
                    expr->type = TYPE_ERROR;
                } else expr->type = TYPE_BOOL;
                return expr->type;
            }
            if (op == TOKEN_EQUAL_EQUAL || op == TOKEN_BANG_EQUAL) {
                bool comparable_strings = string_like_type(left) && string_like_type(right);
                if ((!comparable_strings && left != right) || left == TYPE_VOID || left >= TYPE_STRUCT_BASE) {
                    diagnostic(c, expr->token, "error", "E2036",
                               left >= TYPE_STRUCT_BASE && right >= TYPE_STRUCT_BASE && left == right
                                   ? "Struct values do not support equality comparisons yet."
                                   : "Equality comparisons require matching comparable non-void types.",
                               left >= TYPE_STRUCT_BASE && right >= TYPE_STRUCT_BASE && left == right
                                   ? "Compare the struct's fields explicitly, for example 'point.x == other.x'."
                                   : "Compare values of the same comparable type.");
                    expr->type = TYPE_ERROR;
                } else expr->type = TYPE_BOOL;
                return expr->type;
            }
            expr->type = TYPE_ERROR;
            return expr->type;
        }
        case EXPR_ARRAY: {
            if (expr->as.array.count == 0) {
                diagnostic(c, expr->token, "error", "E2024",
                           "An array literal must contain at least one element.",
                           "Initialize the array with one or more values.");
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            YType element_type = check_expr(checker, expr->as.array.items[0]);
            for (size_t i = 1; i < expr->as.array.count; i++) {
                YType item_type = check_expr(checker, expr->as.array.items[i]);
                if (item_type != TYPE_ERROR && element_type != TYPE_ERROR &&
                    item_type != element_type) {
                    diagnostic(c, expr->as.array.items[i]->token, "error", "E2025",
                               "All array elements must have the same type.",
                               "Use one element type throughout the array literal.");
                    element_type = TYPE_ERROR;
                }
            }
            expr->type = element_type;
            return expr->type;
        }
        case EXPR_INDEX: {
            Expr *target = expr->as.index.target;
            if (!target || target->kind != EXPR_NAME) {
                diagnostic(c, expr->token, "error", "E2026",
                           "Only named fixed-size arrays can be indexed.",
                           "Index an array variable such as values[0].");
                (void)check_expr(checker, expr->as.index.index);
                expr->type = TYPE_ERROR;
                return expr->type;
            }
            bool previous_array_access = checker->allow_array_name;
            checker->allow_array_name = true;
            (void)check_expr(checker, target);
            checker->allow_array_name = previous_array_access;
            VarDecl *var = target->as.name.variable;
            expr->as.index.variable = var;
            YType index_type = check_expr(checker, expr->as.index.index);
            if (!var || !var->is_array) {
                diagnostic(c, expr->token, "error", "E2027",
                           "Indexing requires an array variable.",
                           "Declare an array with syntax such as 'let int values[] = [1, 2, 3];'.");
                expr->type = TYPE_ERROR;
            } else if (index_type != TYPE_INT && index_type != TYPE_ERROR) {
                diagnostic(c, expr->as.index.index->token, "error", "E2028",
                           "Array indices must have type int.",
                           "Use an integer index such as values[0].");
                expr->type = TYPE_ERROR;
            } else {
                expr->type = var->type;
            }
            return expr->type;
        }
        case EXPR_CALL: {
            if ((strcmp(expr->as.call.name, "borrow") == 0 ||
                 strcmp(expr->as.call.name, "borrow_mut") == 0) &&
                !resolve_function(checker, expr->as.call.name)) {
                bool exclusive_request = strcmp(expr->as.call.name, "borrow_mut") == 0;
                if (exclusive_request && !checker->allow_mut_borrow) {
                    diagnostic(c, expr->token, "error", "E2143",
                               "borrow_mut() must initialize a local '&mut string' binding.",
                               "Bind the exclusive borrow in a local block so its lifetime is visible to the checker.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if (expr->as.call.count != 1U) {
                    diagnostic(c, expr->token, "error", "E2130",
                               "borrow() and borrow_mut() expect one named string variable.",
                               "Use borrow(owner) or borrow_mut(owner).");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                Expr *argument = expr->as.call.args[0];
                if (argument->kind != EXPR_NAME) {
                    (void)check_expr(checker, argument);
                    diagnostic(c, argument->token, "error", "E2131",
                               "A borrow must refer directly to a named binding.",
                               "First bind the value to a variable, then borrow that variable.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                bool previous_owner_context = checker->checking_borrow_owner;
                checker->checking_borrow_owner = true;
                YType actual = check_expr(checker, argument);
                checker->checking_borrow_owner = previous_owner_context;
                VarDecl *owner = argument->as.name.variable;
                if (actual == TYPE_ERROR || !owner) {
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if (exclusive_request ? actual != TYPE_OWNED_STRING :
                    (actual != TYPE_STRING && actual != TYPE_OWNED_STRING)) {
                    diagnostic(c, argument->token, "error", "E2132",
                               exclusive_request
                                   ? "borrow_mut() requires a non-const owned string."
                                   : "borrow() requires a string or owned string.",
                               exclusive_request
                                   ? "Use borrow_mut(name) on a mutable owned string binding."
                                   : "Use borrow(name) on a string binding.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if (exclusive_request && owner->is_const) {
                    diagnostic(c, argument->token, "error", "E2133",
                               "A const owned string cannot be exclusively borrowed.",
                               "Remove const from the owner before creating borrow_mut(owner).");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                size_t shared_count = 0U;
                bool exclusive_active = false;
                checker_active_borrows(checker, owner, &shared_count, &exclusive_active);
                if (exclusive_request && (shared_count != 0U || exclusive_active)) {
                    diagnostic(c, argument->token, "error", "E2134",
                               "An exclusive borrow conflicts with an active borrow.",
                               "End all existing borrows before calling borrow_mut(owner).");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if (!exclusive_request && exclusive_active) {
                    diagnostic(c, argument->token, "error", "E2135",
                               "A shared borrow conflicts with an active exclusive borrow.",
                               "End the exclusive borrow before calling borrow(owner).");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                expr->type = exclusive_request ? TYPE_MUT_BORROW_STRING : TYPE_BORROW_STRING;
                return expr->type;
            }
            if (strcmp(expr->as.call.name, "string.own") == 0) {
                if (expr->as.call.count != 1U) {
                    diagnostic(c, expr->token, "error", "E2116",
                               "string.own() expects exactly one string argument.",
                               "Use string.own(value) to make an independent owned copy.");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                YType actual = check_expr(checker, expr->as.call.args[0]);
                if (!checker->allow_owned_value) {
                    diagnostic(c, expr->token, "error", "E2144",
                               "string.own() must initialize, assign, pass, or return an owned value.",
                               "Bind the result to an owned string, pass it to an owned string parameter, or return it from an owned-string function.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if (actual != TYPE_ERROR && !string_like_type(actual)) {
                    diagnostic(c, expr->as.call.args[0]->token, "error", "E2117",
                               "string.own() accepts only string values.",
                               "Pass a string or owned string, not a scalar or struct.");
                    expr->type = TYPE_ERROR;
                } else {
                    expr->type = TYPE_OWNED_STRING;
                }
                return expr->type;
            }
            if (strcmp(expr->as.call.name, "move") == 0 &&
                !resolve_function(checker, "move")) {
                if (expr->as.call.count != 1U) {
                    diagnostic(c, expr->token, "error", "E2111",
                               "move() expects exactly one local string variable.",
                               "Use move(name) to transfer a string value to a new owner.");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                Expr *argument = expr->as.call.args[0];
                if (argument->kind != EXPR_NAME) {
                    (void)check_expr(checker, argument);
                    diagnostic(c, argument->token, "error", "E2112",
                               "move() currently requires a named string variable.",
                               "Move a local binding directly, for example move(message).");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                bool previous_owner_context = checker->checking_move_owner;
                checker->checking_move_owner = true;
                YType argument_type = check_expr(checker, argument);
                checker->checking_move_owner = previous_owner_context;
                VarDecl *source = argument->as.name.variable;
                if (argument_type == TYPE_ERROR || !source) {
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if ((argument_type != TYPE_STRING && argument_type != TYPE_OWNED_STRING) ||
                    source->is_array) {
                    diagnostic(c, argument->token, "error", "E2112",
                               "Only scalar string or owned string variables can be moved.",
                               "Use move(name) with a local string or owned string variable.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                size_t active_shared_borrows = 0U;
                bool active_exclusive_borrow = false;
                checker_active_borrows(checker, source, &active_shared_borrows,
                                       &active_exclusive_borrow);
                if (active_shared_borrows != 0U || active_exclusive_borrow) {
                    diagnostic(c, argument->token, "error", "E2136",
                               "A value cannot be moved while it is borrowed.",
                               "End every borrow of this binding before moving it.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if (argument_type == TYPE_OWNED_STRING && !checker->allow_owned_move) {
                    diagnostic(c, argument->token, "error", "E2137",
                               "An owned string move must initialize or replace an owned string binding.",
                               "Use 'let owned string next = move(current);' or assign move(current) to another owned string.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                bool is_parameter = false;
                if (checker->function) {
                    for (size_t i = 0; i < checker->function->param_count; i++) {
                        if (checker->function->params[i] == source) {
                            is_parameter = true;
                            break;
                        }
                    }
                }
                if (source->is_global ||
                    (is_parameter && argument_type != TYPE_OWNED_STRING)) {
                    diagnostic(c, argument->token, "error", "E2113",
                               "move() can transfer only local values, or owned-string parameters.",
                               "Use an owned string parameter or a local owned value; legacy string parameters and globals are not movable.");
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if (!source->initialized) {
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                source->moved = true;
                source->initialized = false;
                expr->type = argument_type;
                return expr->type;
            }
            if (strncmp(expr->as.call.name, "gui.", 4) == 0) {
                c->uses_gui = true;
                const GuiBuiltin *builtin = find_gui_builtin(expr->as.call.name);
                if (!builtin) {
                    diagnostic(c, expr->token, "error", "E2089",
                               "Unknown GUI function.",
                               "Use a documented gui.* function such as gui.init() or gui.create_window(...).");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                if (expr->as.call.count != builtin->arity) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "%s expects %zu argument(s), but received %zu.",
                        builtin->name, builtin->arity, expr->as.call.count);
                    diagnostic(c, expr->token, "error", "E2085",
                               "Incorrect number of GUI function arguments.", suggestion);
                }
                for (size_t i = 0; i < expr->as.call.count; i++) {
                    YType actual = check_expr(checker, expr->as.call.args[i]);
                    if (i < builtin->arity && actual != TYPE_ERROR &&
                        actual != builtin->args[i]) {
                        char suggestion[256];
                        (void)snprintf(suggestion, sizeof(suggestion),
                            "Argument %zu of %s expects %s, but received %s.",
                            i + 1, builtin->name, semantic_type_name(c, builtin->args[i]),
                            semantic_type_name(c, actual));
                        diagnostic(c, expr->as.call.args[i]->token, "error", "E2090",
                                   "GUI argument type mismatch.", suggestion);
                    }
                }
                expr->type = builtin->result;
                return expr->type;
            }
            if (strcmp(expr->as.call.name, "to_float") == 0 ||
                strcmp(expr->as.call.name, "to_int") == 0 ||
                strcmp(expr->as.call.name, "to_string") == 0) {
                const char *name = expr->as.call.name;
                if (expr->as.call.count != 1) {
                    diagnostic(c, expr->token, "error", "E2086",
                               "A conversion function expects exactly one argument.",
                               "Pass one value, for example to_float(42).");
                }
                YType arg_type = expr->as.call.count ? check_expr(checker, expr->as.call.args[0]) : TYPE_ERROR;
                for (size_t i = 1; i < expr->as.call.count; i++) (void)check_expr(checker, expr->as.call.args[i]);
                if (strcmp(name, "to_float") == 0) {
                    if (arg_type != TYPE_INT && arg_type != TYPE_ERROR)
                        diagnostic(c, expr->token, "error", "E2045", "to_float expects an int argument.",
                                   "Use to_float(integer_value); float values do not need conversion.");
                    expr->type = TYPE_FLOAT;
                } else if (strcmp(name, "to_int") == 0) {
                    if (arg_type != TYPE_FLOAT && arg_type != TYPE_ERROR)
                        diagnostic(c, expr->token, "error", "E2045", "to_int expects a float argument.",
                                   "Use to_int(float_value); conversion truncates toward zero and checks range.");
                    expr->type = TYPE_INT;
                } else {
                    if (arg_type != TYPE_INT && arg_type != TYPE_FLOAT && arg_type != TYPE_BOOL &&
                        arg_type != TYPE_CHAR && !string_like_type(arg_type) && arg_type != TYPE_ERROR)
                        diagnostic(c, expr->token, "error", "E2045", "to_string cannot convert this type.",
                                   "Supported types are int, float, bool, char, string, and owned string.");
                    expr->type = TYPE_STRING;
                }
                return expr->type;
            }
            if (strcmp(expr->as.call.name, "length") == 0) {
                if (expr->as.call.count != 1) {
                    diagnostic(c, expr->token, "error", "E2087",
                               "length expects exactly one array argument.",
                               "Use length(values) to get the number of elements.");
                    for (size_t i = 0; i < expr->as.call.count; i++)
                        (void)check_expr(checker, expr->as.call.args[i]);
                    expr->type = TYPE_ERROR;
                    return expr->type;
                }
                Expr *arg = expr->as.call.args[0];
                bool previous = checker->allow_array_name;
                checker->allow_array_name = true;
                (void)check_expr(checker, arg);
                checker->allow_array_name = previous;
                if (arg->kind != EXPR_NAME || !arg->as.name.variable ||
                    !arg->as.name.variable->is_array) {
                    diagnostic(c, arg->token, "error", "E2096",
                               "length requires an array variable.",
                               "Pass a declared array, for example length(values).");
                    expr->type = TYPE_ERROR;
                } else {
                    expr->type = TYPE_INT;
                }
                return expr->type;
            }
            const StringBuiltin *string_builtin = find_string_builtin(expr->as.call.name);
            if (string_builtin) {
                if (expr->as.call.count != string_builtin->arity) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "%s expects %zu argument(s), but received %zu.",
                        string_builtin->name, string_builtin->arity, expr->as.call.count);
                    diagnostic(c, expr->token, "error", "E2088",
                               "Incorrect number of standard-library arguments.", suggestion);
                }
                for (size_t i = 0; i < expr->as.call.count; i++) {
                    YType arg_type = check_expr(checker, expr->as.call.args[i]);
                    if (arg_type != TYPE_ERROR && !string_like_type(arg_type)) {
                        diagnostic(c, expr->as.call.args[i]->token, "error", "E2044",
                                   "This standard-library function requires string arguments.",
                                   "Pass string values or string literals.");
                    }
                }
                expr->type = string_builtin->result;
                return expr->type;
            }
            const MathBuiltin *math_builtin = find_math_builtin(expr->as.call.name);
            if (math_builtin) {
                if (expr->as.call.count != math_builtin->arity) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "%s expects %zu argument(s), but received %zu.",
                        math_builtin->name, math_builtin->arity, expr->as.call.count);
                    diagnostic(c, expr->token, "error", "E2097",
                               "Incorrect number of math function arguments.", suggestion);
                }
                for (size_t i = 0; i < expr->as.call.count; i++) {
                    YType arg_type = check_expr(checker, expr->as.call.args[i]);
                    if (arg_type != TYPE_ERROR && arg_type != TYPE_FLOAT) {
                        diagnostic(c, expr->as.call.args[i]->token, "error", "E2098",
                                   "Math functions require float arguments.",
                                   "Use a float value, for example math.sqrt(9.0). YLang does not implicitly convert int to float.");
                    }
                }
                expr->type = TYPE_FLOAT;
                return expr->type;
            }
            Function *function = resolve_function(checker, expr->as.call.name);
            if (!function) {
                const char *near = nearest_function(checker, expr->as.call.name);
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
                Expr *arg = expr->as.call.args[i];
                bool array_parameter = i < shared && function->params[i]->is_array;
                VarDecl *param = i < shared ? function->params[i] : NULL;
                bool previous_allow_array = checker->allow_array_name;
                bool previous_owned_context = checker->allow_owned_value;
                bool previous_move_context = checker->allow_owned_move;
                checker->allow_array_name = array_parameter;
                checker->allow_owned_value = param && param->type == TYPE_OWNED_STRING;
                checker->allow_owned_move = param && param->type == TYPE_OWNED_STRING &&
                    arg && arg->kind == EXPR_CALL &&
                    strcmp(arg->as.call.name, "move") == 0 &&
                    !resolve_function(checker, "move") &&
                    arg->as.call.count == 1U;
                YType arg_type = check_expr(checker, arg);
                checker->allow_array_name = previous_allow_array;
                checker->allow_owned_value = previous_owned_context;
                checker->allow_owned_move = previous_move_context;
                if (i >= shared || arg_type == TYPE_ERROR) continue;
                if (param->is_array) {
                    if (arg->kind != EXPR_NAME || !arg->as.name.variable ||
                        !arg->as.name.variable->is_array) {
                        diagnostic(c, arg->token, "error", "E2094",
                                   "This parameter expects an array argument.",
                                   "Pass a declared array variable, for example sum(values).");
                    } else if (arg->as.name.variable->type != param->type) {
                        diagnostic(c, arg->token, "error", "E2099",
                                   "Function array element type mismatch.",
                                   "Pass an array whose element type matches the parameter declaration.");
                    }
                } else if (param->type == TYPE_OWNED_STRING &&
                           !owned_string_initializer_shape(checker, arg)) {
                    diagnostic(c, arg->token, "error", "E2120",
                               "An owned-string parameter consumes an owned value; a bare owner cannot be copied.",
                               "Pass move(owner), string.own(owner), or another function's owned-string result.");
                } else if (arg_type != param->type) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "Argument %zu of '%s' expects %s, but received %s.",
                        i + 1, function->name, semantic_type_name(c, param->type),
                        semantic_type_name(c, arg_type));
                    diagnostic(c, arg->token, "error", "E2042",
                               "Function argument type mismatch.", suggestion);
                }
            }
            if (function->return_type == TYPE_OWNED_STRING &&
                !checker->allow_owned_value) {
                diagnostic(c, expr->token, "error", "E2144",
                           "An owned-string result cannot be discarded or used as a borrowed expression.",
                           "Bind it to an owned string, pass it to an owned string parameter, or return it from an owned-string function.");
                expr->type = TYPE_ERROR;
                return expr->type;
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
    if (stmt->kind == STMT_MATCH) {
        if (stmt->as.match_stmt.arm_count == 0) return false;
        for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++) {
            if (!stmt_guarantees_return(stmt->as.match_stmt.arms[i]->body)) return false;
        }
        return true;
    }
    return false;
}

static bool global_initializer_is_constant(Expr *expr)
{
    if (!expr) return true;
    switch (expr->kind) {
        case EXPR_INT: case EXPR_FLOAT: case EXPR_BOOL: case EXPR_CHAR: case EXPR_STRING:
        case EXPR_ENUM_VALUE:
            return true;
        case EXPR_ARRAY:
            for (size_t i = 0; i < expr->as.array.count; i++) {
                if (!global_initializer_is_constant(expr->as.array.items[i])) return false;
            }
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
            if (var->is_array &&
                (var->type == TYPE_OWNED_STRING || is_borrow_type(var->type))) {
                diagnostic(c, var->token, "error",
                           var->type == TYPE_OWNED_STRING ? "E2118" : "E2140",
                           "Arrays of owned or borrowed string values are not supported yet.",
                           "Keep owned strings and borrows in scalar local bindings until array lifetimes are implemented.");
            }
            if (var->is_array &&
                (!var->initializer || var->initializer->kind != EXPR_ARRAY)) {
                diagnostic(c, var->token, "error", "E2038",
                           "Array declarations require an array literal initializer.",
                           "Use syntax such as 'let int values[] = [1, 2, 3];'.");
            }
            if (var->type == TYPE_VOID) {
                diagnostic(c, var->token, "error", "E2005",
                           "A variable cannot have type void.",
                           "Use a value type such as int, float, bool, char, string, or owned string.");
            }
            if (var->type == TYPE_OWNED_STRING) {
                if (var->is_global) {
                    diagnostic(c, var->token, "error", "E2118",
                               "Owned strings are restricted to local variables.",
                               "Create an owned string inside a function scope.");
                }
                if (!var->initializer) {
                    diagnostic(c, var->token, "error", "E2119",
                               "An owned string must be initialized.",
                               "Initialize it with string.own(value) or move(other_owned_string).");
                } else if (!owned_string_initializer_shape(checker, var->initializer)) {
                    diagnostic(c, var->initializer->token, "error", "E2120",
                               "Owned strings cannot be copied by assignment or initialization.",
                               "Use string.own(value) to clone, or move(owner) to transfer ownership.");
                }
            }
            if (is_borrow_type(var->type)) {
                if (var->is_global) {
                    diagnostic(c, var->token, "error", "E2140",
                               "Borrow bindings cannot be global.",
                               "Borrow values must stay inside a local scope nested within the owner's lifetime.");
                }
                if (!borrow_initializer_shape(var->initializer, var->type)) {
                    diagnostic(c, var->token, "error", "E2139",
                               "A borrow binding must be initialized with borrow(name) or borrow_mut(name).",
                               "Use 'let &string view = borrow(owner);' or 'let &mut string view = borrow_mut(owner);'.");
                }
            }
            if (var->initializer) {
                bool previous_move_context = checker->allow_owned_move;
                bool previous_owned_context = checker->allow_owned_value;
                bool previous_mut_borrow_context = checker->allow_mut_borrow;
                checker->allow_owned_move = var->type == TYPE_OWNED_STRING &&
                    var->initializer->kind == EXPR_CALL &&
                    strcmp(var->initializer->as.call.name, "move") == 0 &&
                    !resolve_function(checker, "move") &&
                    var->initializer->as.call.count == 1U;
                checker->allow_owned_value = var->type == TYPE_OWNED_STRING;
                checker->allow_mut_borrow = var->type == TYPE_MUT_BORROW_STRING &&
                    var->initializer->kind == EXPR_CALL &&
                    strcmp(var->initializer->as.call.name, "borrow_mut") == 0 &&
                    !resolve_function(checker, "borrow_mut") &&
                    var->initializer->as.call.count == 1U;
                YType init_type = check_expr(checker, var->initializer);
                checker->allow_owned_move = previous_move_context;
                checker->allow_owned_value = previous_owned_context;
                checker->allow_mut_borrow = previous_mut_borrow_context;
                if (init_type != TYPE_ERROR && init_type != var->type) {
                    char suggestion[256];
                    (void)snprintf(suggestion, sizeof(suggestion),
                        "'%s' has type %s, but its initializer has type %s.",
                        var->name, semantic_type_name(c, var->type), semantic_type_name(c, init_type));
                    diagnostic(c, var->initializer->token, "error", "E2001",
                               "Variable initializer type mismatch.", suggestion);
                }
                var->initialized = true;
                if (is_borrow_type(var->type) &&
                    borrow_initializer_shape(var->initializer, var->type)) {
                    Expr *borrow_arg = var->initializer->as.call.args[0];
                    var->borrowed_from = borrow_arg->as.name.variable;
                }
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
                } else if (type >= TYPE_ENUM_BASE && type < TYPE_STRUCT_BASE) {
                    diagnostic(c, stmt->as.print.args[i]->token, "error", "E2091",
                               "print() does not yet format user-defined enum values.",
                               "Compare the enum against its named variants and print a string label.");
                } else if (type >= TYPE_STRUCT_BASE) {
                    diagnostic(c, stmt->as.print.args[i]->token, "error", "E2079",
                               "print() does not format struct values directly.",
                               "Print individual fields or add an explicit formatting function.");
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
            bool *before_moved = arena_alloc(&c->arena, n * sizeof(bool));
            bool *after_then = arena_alloc(&c->arena, n * sizeof(bool));
            bool *after_then_moved = arena_alloc(&c->arena, n * sizeof(bool));
            for (size_t i = 0; i < n; i++) {
                before[i] = c->all_vars[i]->initialized;
                before_moved[i] = c->all_vars[i]->moved;
            }
            check_stmt(checker, stmt->as.if_stmt.then_branch);
            for (size_t i = 0; i < n; i++) {
                after_then[i] = c->all_vars[i]->initialized;
                after_then_moved[i] = c->all_vars[i]->moved;
            }
            for (size_t i = 0; i < n; i++) {
                c->all_vars[i]->initialized = before[i];
                c->all_vars[i]->moved = before_moved[i];
            }
            if (stmt->as.if_stmt.else_branch) {
                check_stmt(checker, stmt->as.if_stmt.else_branch);
                for (size_t i = 0; i < n; i++) {
                    c->all_vars[i]->initialized =
                        after_then[i] && c->all_vars[i]->initialized;
                    c->all_vars[i]->moved =
                        after_then_moved[i] || c->all_vars[i]->moved;
                }
            } else {
                for (size_t i = 0; i < n; i++) {
                    c->all_vars[i]->initialized = before[i];
                    c->all_vars[i]->moved =
                        before_moved[i] || after_then_moved[i];
                }
            }
            break;
        }
        case STMT_LOOP: {
            size_t n = c->all_var_count;
            bool *before = arena_alloc(&c->arena, n * sizeof(bool));
            bool *before_moved = arena_alloc(&c->arena, n * sizeof(bool));
            for (size_t i = 0; i < n; i++) {
                before[i] = c->all_vars[i]->initialized;
                before_moved[i] = c->all_vars[i]->moved;
            }
            checker->loop_depth++;
            check_stmt(checker, stmt->as.loop_body);
            checker->loop_depth--;
            for (size_t i = 0; i < n; i++) {
                c->all_vars[i]->moved = before_moved[i] || c->all_vars[i]->moved;
                c->all_vars[i]->initialized = before[i] && !c->all_vars[i]->moved;
            }
            break;
        }
        case STMT_WHILE: {
            YType cond = check_expr(checker, stmt->as.while_stmt.condition);
            if (cond != TYPE_BOOL && cond != TYPE_ERROR) {
                diagnostic(c, stmt->as.while_stmt.condition->token, "error", "E2051",
                           "while condition must have type bool.",
                           "Write a comparison such as 'while (count < limit)'.");
            }
            size_t n = c->all_var_count;
            bool *before = arena_alloc(&c->arena, n * sizeof(bool));
            bool *before_moved = arena_alloc(&c->arena, n * sizeof(bool));
            for (size_t i = 0; i < n; i++) {
                before[i] = c->all_vars[i]->initialized;
                before_moved[i] = c->all_vars[i]->moved;
            }
            checker->loop_depth++;
            check_stmt(checker, stmt->as.while_stmt.body);
            checker->loop_depth--;
            for (size_t i = 0; i < n; i++) {
                c->all_vars[i]->moved = before_moved[i] || c->all_vars[i]->moved;
                c->all_vars[i]->initialized = before[i] && !c->all_vars[i]->moved;
            }
            break;
        }
        case STMT_FOR: {
            Scope *outer_scope = checker->scope;
            checker->scope = scope_new(c, outer_scope);
            size_t n_before_init = c->all_var_count;
            bool *before = arena_alloc(&c->arena, n_before_init * sizeof(bool));
            bool *before_moved = arena_alloc(&c->arena, n_before_init * sizeof(bool));
            for (size_t i = 0; i < n_before_init; i++) {
                before[i] = c->all_vars[i]->initialized;
                before_moved[i] = c->all_vars[i]->moved;
            }
            if (stmt->as.for_stmt.initializer)
                check_stmt(checker, stmt->as.for_stmt.initializer);
            if (stmt->as.for_stmt.condition) {
                YType cond = check_expr(checker, stmt->as.for_stmt.condition);
                if (cond != TYPE_BOOL && cond != TYPE_ERROR) {
                    diagnostic(c, stmt->as.for_stmt.condition->token, "error", "E2051",
                               "for condition must have type bool.",
                               "Use a boolean comparison or omit the condition for an infinite loop.");
                }
            }
            checker->loop_depth++;
            check_stmt(checker, stmt->as.for_stmt.body);
            if (stmt->as.for_stmt.increment) {
                Expr *previous_assignment = checker->allowed_assignment;
                checker->allowed_assignment =
                    stmt->as.for_stmt.increment->kind == EXPR_ASSIGN ?
                    stmt->as.for_stmt.increment : NULL;
                (void)check_expr(checker, stmt->as.for_stmt.increment);
                checker->allowed_assignment = previous_assignment;
            }
            checker->loop_depth--;
            for (size_t i = 0; i < n_before_init; i++) {
                c->all_vars[i]->moved = before_moved[i] || c->all_vars[i]->moved;
                c->all_vars[i]->initialized = before[i] && !c->all_vars[i]->moved;
            }
            checker->scope = outer_scope;
            break;
        }
        case STMT_MATCH: {
            if (expr_contains_call_or_assignment(stmt->as.match_stmt.value)) {
                diagnostic(c, stmt->as.match_stmt.value->token, "error", "E2064",
                           "match values must be free of calls and assignments in this milestone.",
                           "Store the enum value in a variable before matching it.");
            }
            YType value_type = check_expr(checker, stmt->as.match_stmt.value);
            EnumDecl *enumeration = enum_for_type(c, value_type);
            if (!enumeration) {
                diagnostic(c, stmt->as.match_stmt.value->token, "error", "E2057",
                           "match currently requires a user-defined enum value.",
                           "Match a value declared with an enum type.");
            }

            size_t variable_count = c->all_var_count;
            bool *before = arena_alloc(&c->arena, variable_count * sizeof(bool));
            bool *before_moved = arena_alloc(&c->arena, variable_count * sizeof(bool));
            bool *moved_in_any_arm = arena_alloc(&c->arena, variable_count * sizeof(bool));
            for (size_t i = 0; i < variable_count; i++) {
                before[i] = c->all_vars[i]->initialized;
                before_moved[i] = c->all_vars[i]->moved;
                moved_in_any_arm[i] = false;
            }

            for (size_t i = 0; i < stmt->as.match_stmt.arm_count; i++) {
                MatchArm *arm = stmt->as.match_stmt.arms[i];
                if (arm->variant && arm->variant->enumeration->type != value_type) {
                    diagnostic(c, arm->token, "error", "E2058",
                               "This match arm belongs to a different enum type.",
                               "Use variants from the same enum as the matched value.");
                }
                for (size_t j = 0; j < i; j++) {
                    if (arm->variant &&
                        arm->variant == stmt->as.match_stmt.arms[j]->variant) {
                        diagnostic(c, arm->token, "error", "E2059",
                                   "This enum variant appears in more than one match arm.",
                                   "Keep exactly one arm for each enum variant.");
                    }
                }
                for (size_t j = 0; j < variable_count; j++) {
                    c->all_vars[j]->initialized = before[j];
                    c->all_vars[j]->moved = before_moved[j];
                }
                check_stmt(checker, arm->body);
                for (size_t j = 0; j < variable_count; j++)
                    moved_in_any_arm[j] = moved_in_any_arm[j] || c->all_vars[j]->moved;
            }

            if (enumeration) {
                for (size_t i = 0; i < enumeration->variant_count; i++) {
                    EnumVariant *variant = enumeration->variants[i];
                    bool found = false;
                    for (size_t j = 0; j < stmt->as.match_stmt.arm_count; j++) {
                        if (stmt->as.match_stmt.arms[j]->variant == variant) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        diagnostic(c, stmt->token, "error", "E2063",
                                   "This match is not exhaustive.",
                                   "Add an arm for every variant of the enum.");
                        break;
                    }
                }
            }
            for (size_t i = 0; i < variable_count; i++) {
                /* Match is exhaustive when valid: every continuation comes from
                   one arm, so the arm states already include the incoming state
                   unless that arm reinitialized a moved binding. */
                c->all_vars[i]->moved = moved_in_any_arm[i];
                c->all_vars[i]->initialized = before[i] && !c->all_vars[i]->moved;
            }
            break;
        }
        case STMT_BREAK:
        case STMT_CONTINUE:
            if (checker->loop_depth == 0) {
                diagnostic(c, stmt->token, "error", "E2052",
                           stmt->kind == STMT_BREAK ? "break is only valid inside a loop." :
                                                      "continue is only valid inside a loop.",
                           "Place this statement inside loop(), while, or for.");
            }
            break;
        case STMT_RETURN: {
            YType expected = checker->function->return_type;
            if (!stmt->as.return_value) {
                if (expected != TYPE_VOID) {
                    char suggestion[192];
                    (void)snprintf(suggestion, sizeof(suggestion),
                                   "This function returns %s, so return a value.", semantic_type_name(c, expected));
                    diagnostic(c, stmt->token, "error", "E2053",
                               "Missing return value.", suggestion);
                }
            } else {
                Expr *return_expr = stmt->as.return_value;
                bool previous_owned_context = checker->allow_owned_value;
                bool previous_move_context = checker->allow_owned_move;
                checker->allow_owned_value = expected == TYPE_OWNED_STRING;
                checker->allow_owned_move = expected == TYPE_OWNED_STRING &&
                    return_expr->kind == EXPR_CALL &&
                    strcmp(return_expr->as.call.name, "move") == 0 &&
                    !resolve_function(checker, "move") &&
                    return_expr->as.call.count == 1U;
                YType actual = check_expr(checker, return_expr);
                checker->allow_owned_value = previous_owned_context;
                checker->allow_owned_move = previous_move_context;
                if (expected == TYPE_OWNED_STRING && actual != TYPE_ERROR &&
                    !owned_string_initializer_shape(checker, return_expr)) {
                    diagnostic(c, return_expr->token, "error", "E2124",
                               "Returning an owned string must transfer or create ownership.",
                               "Use return move(local_owner);, return string.own(value);, or return a function's owned-string result.");
                }
                if (expected == TYPE_VOID) {
                    diagnostic(c, stmt->token, "error", "E2054",
                               "A void function cannot return a value.",
                               "Use 'return;' or change the function return type.");
                } else if (actual != TYPE_ERROR && actual != expected) {
                    char suggestion[192];
                    (void)snprintf(suggestion, sizeof(suggestion),
                                   "Expected a %s return value, but received %s.",
                                   semantic_type_name(c, expected), semantic_type_name(c, actual));
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
    Checker checker = {
        .compiler = c, .scope = NULL, .function = NULL, .loop_depth = 0,
        .allowed_assignment = NULL, .allow_array_name = false,
        .allow_owned_move = false, .allow_owned_value = false, .allow_mut_borrow = false,
        .checking_borrow_owner = false, .checking_move_owner = false, .module = NULL
    };

    for (size_t i = 0; i < c->struct_count; i++) {
        StructDecl *structure = c->structs[i];
        for (size_t j = 0; j < structure->field_count; j++) {
            StructField *field = structure->fields[j];
            if (field->type == TYPE_OWNED_STRING) {
                diagnostic(c, field->token, "error", "E2118",
                           "Owned strings cannot be stored in structs yet.",
                           "Keep owned strings in local bindings until struct destruction is implemented.");
            } else if (is_borrow_type(field->type)) {
                diagnostic(c, field->token, "error", "E2140",
                           "Borrow fields are not supported yet.",
                           "A borrow cannot be stored in a struct until field lifetimes are checked.");
            }
        }
    }

    /* Each source module gets its own global scope. Imported variables are
       added explicitly; private globals remain module-local. */
    for (size_t m = 0; m < c->module_count; m++) {
        Module *module = c->modules[m];
        Scope *globals = scope_new(c, NULL);
        module->global_scope = globals;
        checker.module = module;
        checker.scope = globals;

        for (size_t i = 0; i < module->program->global_count; i++) {
            VarDecl *var = module->program->globals[i];
            if (var->type == TYPE_VOID) {
                diagnostic(c, var->token, "error", "E2005",
                           "A variable cannot have type void.",
                           "Use a value type such as int, float, bool, char, string, or owned string.");
            }
            if (var->type == TYPE_OWNED_STRING) {
                diagnostic(c, var->token, "error", "E2118",
                           "Owned strings are restricted to local variables.",
                           "Create an owned string inside a function scope.");
            } else if (is_borrow_type(var->type)) {
                diagnostic(c, var->token, "error", "E2140",
                           "Borrow bindings cannot be global.",
                           "Borrow values must stay inside a local scope nested within the owner's lifetime.");
            }
            (void)scope_add(&checker, globals, var);
        }
        for (size_t i = 0; i < module->program->import_count; i++) {
            ImportDecl *import = module->program->imports[i];
            for (size_t j = 0; j < import->name_count; j++) {
                ImportName *symbol = import->names[j];
                if (!symbol->variable) continue;
                if (scope_lookup(globals, symbol->name)) {
                    diagnostic(c, symbol->token, "error", "E1025",
                               "An imported global conflicts with a global already visible in this module.",
                               "Rename the local declaration or import the name only once.");
                    continue;
                }
                (void)scope_add(&checker, globals, symbol->variable);
            }
        }
    }

    for (size_t m = 0; m < c->module_count; m++) {
        Module *module = c->modules[m];
        checker.module = module;
        checker.scope = module->global_scope;
        checker.function = NULL;
        for (size_t i = 0; i < module->program->global_count; i++) {
            VarDecl *var = module->program->globals[i];
            if (!var->initializer) continue;
            YType actual = check_expr(&checker, var->initializer);
            if (actual != TYPE_ERROR && actual != var->type) {
                char suggestion[256];
                (void)snprintf(suggestion, sizeof(suggestion),
                    "Global '%s' has type %s, but its initializer has type %s.",
                    var->name, semantic_type_name(c, var->type), semantic_type_name(c, actual));
                diagnostic(c, var->initializer->token, "error", "E2001",
                           "Global initializer type mismatch.", suggestion);
            }
            if (!global_initializer_is_constant(var->initializer)) {
                diagnostic(c, var->token, "error", "E2056",
                           "Global initializers must be constant expressions.",
                           "Move computed initialization into function main().");
            }
        }
    }

    /* Functions are private to their source module unless imported explicitly. */
    for (size_t m = 0; m < c->module_count; m++) {
        Module *module = c->modules[m];
        for (size_t i = 0; i < module->program->function_count; i++) {
            for (size_t j = i + 1; j < module->program->function_count; j++) {
                if (strcmp(module->program->functions[i]->name,
                           module->program->functions[j]->name) == 0) {
                    diagnostic(c, module->program->functions[j]->token,
                               "error", "E2043",
                               "This function is already defined in this module.",
                               "YLang does not support function overloading; rename one definition.");
                }
            }
        }
    }

    Function *main_function = NULL;
    if (c->root_module && c->root_module->program) {
        main_function = module_local_function(c->root_module, "main");
    } else {
        main_function = find_function(c, "main");
    }
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
        checker.module = function->module;
        checker.function = function;
        checker.loop_depth = 0;
        if (is_borrow_type(function->return_type)) {
            diagnostic(c, function->token, "error", "E2140",
                       "Borrow return values are not supported yet.",
                       "Return an owned value or a regular string; borrowed return lifetimes are not implemented.");
        }
        Scope *global_scope = function->module ?
            function->module->global_scope : NULL;
        checker.scope = scope_new(c, global_scope);
        for (size_t j = 0; j < function->param_count; j++) {
            function->params[j]->initialized = true;
            if (is_borrow_type(function->params[j]->type)) {
                diagnostic(c, function->params[j]->token, "error", "E2140",
                           "Borrow parameters are not supported yet.",
                           "Borrowed function parameters need explicit signature and lifetime rules.");
            }
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

