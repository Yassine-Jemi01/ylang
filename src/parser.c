#define _POSIX_C_SOURCE 200809L
#include "internal.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------- arena memory ----------------------------- */
void *arena_alloc(Arena *arena, size_t size)
{
    const size_t alignment = _Alignof(max_align_t);
    if (size == 0) size = 1;
    ArenaBlock *block = arena->head;
    size_t offset = 0;
    if (block) {
        if (block->used > SIZE_MAX - (alignment - 1U)) {
            fputs("ylang: fatal: arena offset overflow\n", stderr);
            exit(70);
        }
        offset = (block->used + alignment - 1U) & ~(alignment - 1U);
    }

    if (!block || offset > block->capacity ||
        size > block->capacity - offset) {
        if (size > SIZE_MAX - alignment) {
            fputs("ylang: fatal: arena allocation size overflow\n", stderr);
            exit(70);
        }
        size_t capacity = size > 8192 ? size + alignment : 8192;
        if (capacity > SIZE_MAX - sizeof(*block)) {
            fputs("ylang: fatal: arena allocation size overflow\n", stderr);
            exit(70);
        }
        ArenaBlock *fresh = malloc(sizeof(*fresh) + capacity);
        if (!fresh) {
            fputs("ylang: fatal: out of memory\n", stderr);
            exit(70);
        }
        fresh->next = arena->head;
        fresh->used = 0;
        fresh->capacity = capacity;
        arena->head = fresh;
        block = fresh;
        offset = 0;
    }

    void *result = block->data + offset;
    block->used = offset + size;
    memset(result, 0, size);
    return result;
}

char *arena_strndup(Arena *arena, const char *text, size_t length)
{
    char *copy = arena_alloc(arena, length + 1);
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

void arena_destroy(Arena *arena)
{
    ArenaBlock *block = arena->head;
    while (block) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
}

typedef union {
    size_t capacity;
    max_align_t alignment;
} ArenaVectorHeader;

void *arena_vector_append(Arena *arena, void *items, size_t count,
                          size_t item_size, const void *item)
{
    if (!arena || item_size == 0 || !item || (count != 0 && !items)) {
        fputs("ylang: fatal: invalid vector state\n", stderr);
        exit(70);
    }

    ArenaVectorHeader *header = items ? ((ArenaVectorHeader *)items - 1) : NULL;
    size_t capacity = header ? header->capacity : 0;
    if (count == SIZE_MAX) {
        fputs("ylang: fatal: vector size overflow\n", stderr);
        exit(70);
    }

    if (count >= capacity) {
        size_t needed = count + 1;
        size_t next_capacity = capacity ? capacity : 8;
        while (next_capacity < needed) {
            if (next_capacity > SIZE_MAX / 2) {
                fputs("ylang: fatal: vector capacity overflow\n", stderr);
                exit(70);
            }
            next_capacity *= 2;
        }
        if (next_capacity > (SIZE_MAX - sizeof(*header)) / item_size) {
            fputs("ylang: fatal: vector allocation size overflow\n", stderr);
            exit(70);
        }

        header = arena_alloc(arena, sizeof(*header) + next_capacity * item_size);
        header->capacity = next_capacity;
        void *grown = header + 1;
        if (count > 0) memcpy(grown, items, count * item_size);
        items = grown;
    }

    memcpy((unsigned char *)items + count * item_size, item, item_size);
    return items;
}

static void append_ptr(Compiler *c, void ***items, size_t *count, void *item)
{
    *items = arena_vector_append(&c->arena, *items, *count, sizeof(**items), &item);
    (*count)++;
}

static void append_fpart(Compiler *c, FPart **items, size_t *count, FPart item)
{
    *items = arena_vector_append(&c->arena, *items, *count, sizeof(**items), &item);
    (*count)++;
}

typedef struct {
    Compiler *compiler;
    Lexer lexer;
    Token current;
    Token previous;
    int base_line;
    int base_column;
} Parser;

void sb_init(StringBuilder *sb)
{
    sb->data = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

static void sb_reserve(StringBuilder *sb, size_t extra)
{
    if (sb->length == SIZE_MAX || extra > SIZE_MAX - sb->length - 1) {
        fputs("ylang: fatal: generated text is too large\n", stderr);
        exit(70);
    }
    size_t required = sb->length + extra + 1;
    if (required <= sb->capacity) return;
    size_t capacity = sb->capacity ? sb->capacity : 128;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    char *data = realloc(sb->data, capacity);
    if (!data) {
        fputs("ylang: fatal: out of memory\n", stderr);
        exit(70);
    }
    sb->data = data;
    sb->capacity = capacity;
}

void sb_append_n(StringBuilder *sb, const char *text, size_t length)
{
    sb_reserve(sb, length);
    memcpy(sb->data + sb->length, text, length);
    sb->length += length;
    sb->data[sb->length] = '\0';
}

void sb_append(StringBuilder *sb, const char *text)
{
    sb_append_n(sb, text, strlen(text));
}

void sb_appendf(StringBuilder *sb, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        return;
    }
    sb_reserve(sb, (size_t)needed);
    (void)vsnprintf(sb->data + sb->length, sb->capacity - sb->length,
                    format, args);
    sb->length += (size_t)needed;
    va_end(args);
}

void sb_destroy(StringBuilder *sb)
{
    free(sb->data);
    sb->data = NULL;
    sb->length = sb->capacity = 0;
}

void diagnostic(Compiler *c, Token token, const char *level,
                       const char *code, const char *message,
                       const char *suggestion)
{
    size_t line = token.line ? token.line : 1;
    size_t column = token.column ? token.column : 1;
    const char *shown_filename = c->filename;
    const char *shown_source = c->source;
    size_t shown_source_length = c->source_length;
    uintptr_t token_address = (uintptr_t)(const void *)token.start;
    for (size_t i = 0; i < c->module_count; i++) {
        Module *module = c->modules[i];
        uintptr_t base = (uintptr_t)(const void *)module->source;
        if (token_address >= base &&
            token_address - base <= module->source_length) {
            shown_filename = module->path;
            shown_source = module->source;
            shown_source_length = module->source_length;
            break;
        }
    }
    fprintf(stderr, "%s[%s]: %s\n", level, code, message);
    fprintf(stderr, "  --> %s:%zu:%zu\n", shown_filename, line, column);

    const char *line_start = shown_source;
    size_t current_line = 1;
    while (current_line < line && *line_start) {
        if (*line_start++ == '\n') current_line++;
    }
    const char *line_end = line_start;
    while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
    if (shown_source && line_start < shown_source + shown_source_length &&
        *line_start && current_line == line) {
        fprintf(stderr, "   |\n%3zu | %.*s\n   | ", line,
                (int)(line_end - line_start), line_start);
        for (size_t i = 1; i < column; i++) fputc(' ', stderr);
        size_t width = token.length > 0 ? token.length : 1;
        if (width > 60) width = 1;
        for (size_t i = 0; i < width; i++) fputc('^', stderr);
        fputc('\n', stderr);
    }
    if (suggestion && *suggestion) {
        fprintf(stderr, "   = help: %s\n", suggestion);
    }
    fputc('\n', stderr);
    if (strcmp(level, "error") == 0) {
        c->had_error = true;
        c->error_count++;
    } else {
        c->warning_count++;
    }
}

static bool token_is(const Token *token, const char *text)
{
    size_t length = strlen(text);
    return token->length == length && memcmp(token->start, text, length) == 0;
}

char *token_copy(Compiler *c, Token token)
{
    return arena_strndup(&c->arena, token.start, token.length);
}

static Token adjusted_token(Parser *p, Token token)
{
    if (p->base_line > 1) token.line += (size_t)p->base_line - 1;
    if (token.line == (size_t)p->base_line) {
        token.column += (size_t)(p->base_column > 0 ? p->base_column - 1 : 0);
    }
    return token;
}

static Token next_non_newline(Parser *p)
{
    Token token;
    do {
        token = lexer_next(&p->lexer);
        token = adjusted_token(p, token);
        if (token.type == TOKEN_ERROR) {
            diagnostic(p->compiler, token, "error", "E1001",
                       token.message ? token.message : "Invalid token.",
                       "Check this character and the surrounding syntax.");
        }
    } while (token.type == TOKEN_NEWLINE);
    return token;
}

static void parser_init(Parser *p, Compiler *c, const char *source,
                        int base_line, int base_column)
{
    memset(p, 0, sizeof(*p));
    p->compiler = c;
    p->lexer = lexer_init(source);
    p->base_line = base_line;
    p->base_column = base_column;
    p->current = next_non_newline(p);
}

static void advance_parser(Parser *p)
{
    p->previous = p->current;
    if (p->current.type != TOKEN_EOF) p->current = next_non_newline(p);
}

static bool match(Parser *p, TokenType type)
{
    if (p->current.type != type) return false;
    advance_parser(p);
    return true;
}

static Token synthetic_token(TokenType type, Token near)
{
    near.type = type;
    near.length = 0;
    return near;
}

static Token consume(Parser *p, TokenType type, const char *expectation,
                    const char *suggestion)
{
    if (p->current.type == type) {
        Token result = p->current;
        advance_parser(p);
        return result;
    }
    diagnostic(p->compiler, p->current, "error", "E1002", expectation, suggestion);
    return synthetic_token(type, p->current);
}

static void synchronize(Parser *p)
{
    while (p->current.type != TOKEN_EOF) {
        if (p->previous.type == TOKEN_SEMICOLON ||
            p->current.type == TOKEN_FUNCTION || p->current.type == TOKEN_LET ||
            p->current.type == TOKEN_IF || p->current.type == TOKEN_RETURN) {
            return;
        }
        advance_parser(p);
    }
}

static Expr *new_expr(Compiler *c, ExprKind kind, Token token)
{
    Expr *expr = arena_alloc(&c->arena, sizeof(*expr));
    expr->kind = kind;
    expr->token = token;
    expr->type = TYPE_ERROR;
    return expr;
}

static Stmt *new_stmt(Compiler *c, StmtKind kind, Token token)
{
    Stmt *stmt = arena_alloc(&c->arena, sizeof(*stmt));
    stmt->kind = kind;
    stmt->token = token;
    return stmt;
}

static VarDecl *new_var(Compiler *c, Token token, char *name, YType type,
                        bool is_const, bool is_global, Expr *initializer)
{
    VarDecl *var = arena_alloc(&c->arena, sizeof(*var));
    var->token = token;
    var->name = name;
    var->type = type;
    var->is_const = is_const;
    var->is_global = is_global;
    var->initializer = initializer;
    var->initialized = initializer != NULL || (is_global && !is_const);
    size_t variable_id = ++c->next_var_id;
    size_t name_length = strlen(name);
    int prefix_length = snprintf(NULL, 0, "yl_v_%zu_", variable_id);
    if (prefix_length < 0 ||
        (size_t)prefix_length > SIZE_MAX - name_length - 1) {
        fputs("ylang: fatal: generated variable name is too large\n", stderr);
        exit(70);
    }
    size_t c_name_size = (size_t)prefix_length + name_length + 1;
    var->c_name = arena_alloc(&c->arena, c_name_size);
    (void)snprintf(var->c_name, c_name_size, "yl_v_%zu_%s", variable_id, name);
    append_ptr(c, (void ***)&c->all_vars, &c->all_var_count, var);
    return var;
}

static EnumDecl *find_enum(Compiler *compiler, const char *name)
{
    for (size_t i = 0; i < compiler->enum_count; i++) {
        EnumDecl *enumeration = compiler->enums[i];
        if ((!compiler->current_module || enumeration->module == compiler->current_module) &&
            strcmp(enumeration->name, name) == 0) return enumeration;
    }
    return NULL;
}

static StructDecl *find_struct(Compiler *compiler, const char *name)
{
    for (size_t i = 0; i < compiler->struct_count; i++) {
        StructDecl *structure = compiler->structs[i];
        if ((!compiler->current_module || structure->module == compiler->current_module) &&
            strcmp(structure->name, name) == 0) return structure;
    }
    return NULL;
}

static StructField *find_struct_field(StructDecl *structure, const char *name)
{
    if (!structure) return NULL;
    for (size_t i = 0; i < structure->field_count; i++) {
        if (strcmp(structure->fields[i]->name, name) == 0)
            return structure->fields[i];
    }
    return NULL;
}

static EnumVariant *find_enum_variant(EnumDecl *enumeration, const char *name)
{
    if (!enumeration) return NULL;
    for (size_t i = 0; i < enumeration->variant_count; i++) {
        if (strcmp(enumeration->variants[i]->name, name) == 0)
            return enumeration->variants[i];
    }
    return NULL;
}

static YType parse_type(Parser *p)
{
    Token token = consume(p, TOKEN_IDENTIFIER,
        "Expected a type name such as 'int', 'float', 'bool', 'char', 'string', or 'void'.",
        "Write the type before the variable or parameter name.");
    if (token_is(&token, "int")) return TYPE_INT;
    if (token_is(&token, "float")) return TYPE_FLOAT;
    if (token_is(&token, "bool")) return TYPE_BOOL;
    if (token_is(&token, "char")) return TYPE_CHAR;
    if (token_is(&token, "string")) return TYPE_STRING;
    if (token_is(&token, "void")) return TYPE_VOID;
    const char *type_text = token_copy(p->compiler, token);
    EnumDecl *enumeration = find_enum(p->compiler, type_text);
    if (enumeration) return enumeration->type;
    StructDecl *structure = find_struct(p->compiler, type_text);
    if (structure) return structure->type;
    diagnostic(p->compiler, token, "error", "E2002", "Unknown type name.",
               "Declare a struct or enum before using it as a type.");
    return TYPE_ERROR;
}

const char *type_name(YType type)
{
    if (type >= TYPE_STRUCT_BASE) return "<struct>";
    if (type >= TYPE_ENUM_BASE) return "<enum>";
    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_STRING: return "string";
        case TYPE_VOID: return "void";
        case TYPE_ERROR: return "<error>";
    }
    return "<unknown>";
}

static Expr *parse_expression(Parser *p);
static Stmt *parse_statement(Parser *p);
static Stmt *parse_block_after_open(Parser *p, Token opening);
static Expr *parse_precedence(Parser *p, int min_precedence);

static Expr *parse_fstring(Parser *p, Token token)
{
    Expr *expr = new_expr(p->compiler, EXPR_FSTRING, token);
    /* token is f"...". Parse interpolations as independent expression sources,
       while keeping every copied fragment in the compiler arena. */
    if (token.length < 3) {
        diagnostic(p->compiler, token, "error", "E1010", "Invalid f-string.",
                   "Use f\"text {expression}\".");
        return expr;
    }
    const char *inside = token.start + 2;
    size_t length = token.length - 3;
    size_t pos = 0;
    StringBuilder text;
    sb_init(&text);
    while (pos < length) {
        char ch = inside[pos];
        if (ch == '\\' && pos + 1 < length) {
            char escaped = inside[pos + 1];
            switch (escaped) {
                case 'n': sb_append_n(&text, "\n", 1); break;
                case 't': sb_append_n(&text, "\t", 1); break;
                case 'r': sb_append_n(&text, "\r", 1); break;
                case '\\': sb_append_n(&text, "\\", 1); break;
                case '"': sb_append_n(&text, "\"", 1); break;
                case '{': sb_append_n(&text, "{", 1); break;
                case '}': sb_append_n(&text, "}", 1); break;
                default: sb_append_n(&text, &ch, 1); sb_append_n(&text, &escaped, 1); break;
            }
            pos += 2;
            continue;
        }
        if (ch == '{' && pos + 1 < length && inside[pos + 1] == '{') {
            sb_append_n(&text, "{", 1); pos += 2; continue;
        }
        if (ch == '}' && pos + 1 < length && inside[pos + 1] == '}') {
            sb_append_n(&text, "}", 1); pos += 2; continue;
        }
        if (ch == '}') {
            diagnostic(p->compiler, token, "error", "E1011",
                       "Unmatched '}' in f-string.",
                       "Use '}}' or \\} to include a literal closing brace.");
            pos++;
            continue;
        }
        if (ch != '{') {
            sb_append_n(&text, &ch, 1);
            pos++;
            continue;
        }
        if (text.length > 0) {
            FPart part = { .text = arena_strndup(&p->compiler->arena, text.data, text.length), .expression = NULL };
            append_fpart(p->compiler, &expr->as.fstring.parts,
                         &expr->as.fstring.count, part);
            text.length = 0;
            if (text.data) text.data[0] = '\0';
        }
        size_t expr_start = ++pos;
        int depth = 1;
        char quote = '\0';
        bool escaped = false;
        while (pos < length && depth > 0) {
            char inner = inside[pos];
            if (quote) {
                if (escaped) escaped = false;
                else if (inner == '\\') escaped = true;
                else if (inner == quote) quote = '\0';
            } else if (inner == '\"' || inner == '\'') {
                quote = inner;
            } else if (inner == '{') {
                depth++;
            } else if (inner == '}') {
                depth--;
                if (depth == 0) break;
            }
            pos++;
        }
        if (depth != 0) {
            diagnostic(p->compiler, token, "error", "E1012",
                       "Unclosed interpolation in f-string.",
                       "Close the expression with '}', for example f\"value: {count}\".");
            break;
        }
        size_t expr_length = pos - expr_start;
        if (expr_length == 0) {
            diagnostic(p->compiler, token, "error", "E1013",
                       "Empty f-string interpolation.",
                       "Place a variable or expression between the braces.");
        } else {
            char *copy = arena_strndup(&p->compiler->arena, inside + expr_start, expr_length);
            Parser sub;
            int base_col = (int)token.column + 2 + (int)expr_start;
            parser_init(&sub, p->compiler, copy, (int)token.line, base_col);
            Expr *embedded = parse_expression(&sub);
            if (sub.current.type != TOKEN_EOF) {
                diagnostic(p->compiler, sub.current, "error", "E1014",
                           "Unexpected token in f-string expression.",
                           "Keep each interpolation to one valid YLang expression.");
            }
            FPart part = { .text = NULL, .expression = embedded };
            append_fpart(p->compiler, &expr->as.fstring.parts,
                         &expr->as.fstring.count, part);
        }
        pos++; /* closing brace */
    }
    if (text.length > 0 || expr->as.fstring.count == 0) {
        FPart part = { .text = arena_strndup(&p->compiler->arena,
                       text.data ? text.data : "", text.length), .expression = NULL };
        append_fpart(p->compiler, &expr->as.fstring.parts,
                     &expr->as.fstring.count, part);
    }
    sb_destroy(&text);
    return expr;
}

static Expr *parse_primary(Parser *p)
{
    Token token = p->current;
    if (p->current.type == TOKEN_ERROR) {
        if (p->current.type != TOKEN_EOF) advance_parser(p);
        return new_expr(p->compiler, EXPR_ERROR, token);
    }
    if (match(p, TOKEN_NUMBER)) {
        Expr *expr = new_expr(p->compiler,
            (memchr(token.start, '.', token.length) ||
             memchr(token.start, 'e', token.length) ||
             memchr(token.start, 'E', token.length)) ? EXPR_FLOAT : EXPR_INT, token);
        return expr;
    }
    if (match(p, TOKEN_STRING)) return new_expr(p->compiler, EXPR_STRING, token);
    if (match(p, TOKEN_CHAR)) return new_expr(p->compiler, EXPR_CHAR, token);
    if (match(p, TOKEN_FSTRING)) return parse_fstring(p, token);
    if (match(p, TOKEN_TRUE) || match(p, TOKEN_FALSE)) return new_expr(p->compiler, EXPR_BOOL, token);
    if (match(p, TOKEN_LEFT_BRACKET)) {
        Expr *array = new_expr(p->compiler, EXPR_ARRAY, token);
        if (p->current.type != TOKEN_RIGHT_BRACKET) {
            do {
                Expr *item = parse_expression(p);
                append_ptr(p->compiler, (void ***)&array->as.array.items,
                           &array->as.array.count, item);
            } while (match(p, TOKEN_COMMA) && p->current.type != TOKEN_RIGHT_BRACKET);
        }
        consume(p, TOKEN_RIGHT_BRACKET, "Expected ']' after array literal.",
                "Close the array literal, for example [1, 2, 3].");
        return array;
    }
    if (match(p, TOKEN_IDENTIFIER)) {
        Expr *expr = new_expr(p->compiler, EXPR_NAME, token);
        char *name = token_copy(p->compiler, token);
        /* Namespaced standard-library calls use the familiar module.function form. */
        if (match(p, TOKEN_DOT)) {
            Token member = consume(p, TOKEN_IDENTIFIER,
                "Expected a member name after '.'.",
                "For example: math.sqrt(value), string.length(value), or io.read_line().");
            if (token_is(&token, "math") || token_is(&token, "string") ||
                token_is(&token, "io") || token_is(&token, "path") ||
                token_is(&token, "image") || token_is(&token, "gui")) {
                StringBuilder qualified;
                sb_init(&qualified);
                sb_append_n(&qualified, token.start, token.length);
                sb_append(&qualified, ".");
                sb_append_n(&qualified, member.start, member.length);
                name = arena_strndup(&p->compiler->arena, qualified.data, qualified.length);
                sb_destroy(&qualified);
            } else {
                EnumDecl *enumeration = find_enum(p->compiler, token_copy(p->compiler, token));
                if (enumeration) {
                    EnumVariant *variant = find_enum_variant(
                        enumeration, token_copy(p->compiler, member));
                    if (!variant) {
                        diagnostic(p->compiler, member, "error", "E2052",
                                   "Unknown variant for this enum type.",
                                   "Use a variant declared inside the enum definition.");
                        return new_expr(p->compiler, EXPR_ERROR, member);
                    }
                    Expr *value = new_expr(p->compiler, EXPR_ENUM_VALUE, token);
                    value->as.enum_value.enumeration = enumeration;
                    value->as.enum_value.variant = variant;
                    value->type = enumeration->type;
                    return value;
                }
                Expr *field = new_expr(p->compiler, EXPR_FIELD, token);
                field->as.field.target = expr;
                field->as.field.name = token_copy(p->compiler, member);
                return field;
            }
        }
        expr->as.name.name = name;
        if (match(p, TOKEN_LEFT_PAREN)) {
            Expr *call = new_expr(p->compiler, EXPR_CALL, token);
            call->as.call.name = expr->as.name.name;
            call->as.call.struct_type = find_struct(p->compiler, call->as.call.name);
            while (p->current.type != TOKEN_RIGHT_PAREN && p->current.type != TOKEN_EOF) {
                Expr *arg = parse_expression(p);
                append_ptr(p->compiler, (void ***)&call->as.call.args,
                           &call->as.call.count, arg);
                if (!match(p, TOKEN_COMMA)) break;
            }
            consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after function arguments.",
                    "Add ')' after the last argument.");
            return call;
        }
        return expr;
    }
    if (match(p, TOKEN_LEFT_PAREN)) {
        Expr *expr = parse_expression(p);
        consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after expression.",
                "Close the expression with ')'.");
        return expr;
    }
    diagnostic(p->compiler, token, "error", "E1003", "Expected an expression.",
               "A value can be a literal, variable, function call, or parenthesized expression.");
    if (p->current.type != TOKEN_EOF) advance_parser(p);
    return new_expr(p->compiler, EXPR_ERROR, token);
}

static int precedence(TokenType type)
{
    switch (type) {
        case TOKEN_EQUAL: case TOKEN_PLUS_EQUAL: return 1;
        case TOKEN_OR: return 2;
        case TOKEN_AND: return 3;
        case TOKEN_EQUAL_EQUAL: case TOKEN_BANG_EQUAL: return 4;
        case TOKEN_LESS: case TOKEN_LESS_EQUAL: case TOKEN_GREATER: case TOKEN_GREATER_EQUAL: return 5;
        case TOKEN_PLUS: case TOKEN_MINUS: return 6;
        case TOKEN_STAR: case TOKEN_SLASH: case TOKEN_PERCENT: return 7;
        default: return 0;
    }
}

static Expr *parse_precedence(Parser *p, int min_precedence)
{
    Expr *left;
    Token op = p->current;
    if (match(p, TOKEN_MINUS) || match(p, TOKEN_NOT) || match(p, TOKEN_BANG)) {
        op = p->previous;
        Expr *right = parse_precedence(p, 8);
        left = new_expr(p->compiler, EXPR_UNARY, op);
        left->as.unary.op = op;
        left->as.unary.right = right;
    } else {
        left = parse_primary(p);
    }

    for (;;) {
        if (p->current.type == TOKEN_LEFT_BRACKET && min_precedence <= 8) {
            Token bracket = p->current;
            advance_parser(p);
            Expr *index = parse_expression(p);
            consume(p, TOKEN_RIGHT_BRACKET, "Expected ']' after array index.",
                    "Close the index expression with ']'.");
            Expr *indexed = new_expr(p->compiler, EXPR_INDEX, bracket);
            indexed->as.index.target = left;
            indexed->as.index.index = index;
            left = indexed;
            continue;
        }
        if (p->current.type == TOKEN_PLUS_PLUS) {
            op = p->current;
            if (min_precedence > 1) break;
            advance_parser(p);
            if (left->kind != EXPR_NAME) {
                diagnostic(p->compiler, op, "error", "E1004",
                           "The '++' operator requires a variable name.",
                           "Use 'count++' for a mutable numeric variable.");
                left = new_expr(p->compiler, EXPR_ERROR, op);
                continue;
            }
            Token one_token = op;
            one_token.start = "1";
            one_token.length = 1;
            Expr *one = new_expr(p->compiler, EXPR_INT, one_token);
            one->is_min_int = false;
            Token plus_token = op;
            plus_token.type = TOKEN_PLUS;
            plus_token.start = "+";
            plus_token.length = 1;
            Expr *sum = new_expr(p->compiler, EXPR_BINARY, plus_token);
            sum->as.binary.left = left;
            sum->as.binary.right = one;
            sum->as.binary.op = plus_token;
            Expr *assign = new_expr(p->compiler, EXPR_ASSIGN, op);
            assign->as.assign.target = left;
            assign->as.assign.right = sum;
            assign->as.assign.variable = NULL;
            left = assign;
            continue;
        }
        int prec = precedence(p->current.type);
        if (prec == 0 || prec < min_precedence) break;
        op = p->current;
        advance_parser(p);
        int next_min = prec + ((op.type == TOKEN_EQUAL || op.type == TOKEN_PLUS_EQUAL) ? 0 : 1);
        Expr *right = parse_precedence(p, next_min);
        if (op.type == TOKEN_EQUAL || op.type == TOKEN_PLUS_EQUAL) {
            if (left->kind != EXPR_NAME &&
                (op.type == TOKEN_EQUAL && left->kind != EXPR_INDEX &&
                 left->kind != EXPR_FIELD)) {
                diagnostic(p->compiler, op, "error", "E1004",
                           "The left side of an assignment must be a variable or array element.",
                           "Write an assignment such as 'count = count + 1;' or 'values[0] = 1;'.");
                left = new_expr(p->compiler, EXPR_ERROR, op);
            } else {
                Expr *assign = new_expr(p->compiler, EXPR_ASSIGN, op);
                assign->as.assign.target = left;
                assign->as.assign.is_compound = op.type == TOKEN_PLUS_EQUAL;
                assign->as.assign.right = right;
                assign->as.assign.variable = NULL;
                left = assign;
            }
        } else {
            Expr *binary = new_expr(p->compiler, EXPR_BINARY, op);
            binary->as.binary.left = left;
            binary->as.binary.right = right;
            binary->as.binary.op = op;
            left = binary;
        }
    }
    return left;
}

static Expr *parse_expression(Parser *p)
{
    return parse_precedence(p, 1);
}

static Stmt *parse_block_after_open(Parser *p, Token opening)
{
    Stmt *block = new_stmt(p->compiler, STMT_BLOCK, opening);
    while (p->current.type != TOKEN_RIGHT_BRACE && p->current.type != TOKEN_EOF) {
        Token before = p->current;
        Stmt *stmt = parse_statement(p);
        if (stmt) append_ptr(p->compiler, (void ***)&block->as.block.items,
                             &block->as.block.count, stmt);
        if (p->current.start == before.start && p->current.type == before.type) {
            if (p->current.type != TOKEN_EOF) advance_parser(p);
            else break;
        }
    }
    consume(p, TOKEN_RIGHT_BRACE, "Expected '}' to close this block.",
            "Add a closing brace matching the opening '{'.");
    return block;
}

static Stmt *parse_variable(Parser *p, bool global)
{
    Token start = p->previous;
    bool is_const = match(p, TOKEN_CONST);
    YType type = parse_type(p);
    Token name_token = consume(p, TOKEN_IDENTIFIER,
        "Expected a variable name after the type.",
        "For example: let int count = 0;");
    bool is_array = false;
    if (match(p, TOKEN_LEFT_BRACKET)) {
        is_array = true;
        consume(p, TOKEN_RIGHT_BRACKET, "Array declarations use empty brackets after the name.",
                "Use a declaration such as 'let int values[] = [1, 2, 3];'.");
        if (is_const) {
            diagnostic(p->compiler, name_token, "error", "E2017",
                       "const arrays are not supported yet.",
                       "Use a mutable array declaration for now.");
        }
    }
    Expr *initializer = NULL;
    if (match(p, TOKEN_EQUAL)) initializer = parse_expression(p);
    if (is_array && !initializer) {
        diagnostic(p->compiler, name_token, "error", "E2018",
                   "An array declaration requires an initializer in this release.",
                   "Initialize it with an array literal, for example [1, 2, 3].");
    }
    if (!is_array && initializer && initializer->kind == EXPR_ARRAY) {
        diagnostic(p->compiler, name_token, "error", "E2019",
                   "An array literal requires an array declaration.",
                   "Declare the variable with empty brackets after its name, such as 'let int values[] = [1, 2, 3];'.");
    }
    if (is_const && !initializer) {
        diagnostic(p->compiler, name_token, "error", "E2003",
                   "A const variable must be initialized at declaration.",
                   "Give it a value, for example 'let const int limit = 10;'.");
    }
    consume(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration.",
            "The declaration may be missing ';' before this token.");
    VarDecl *var = new_var(p->compiler, name_token, token_copy(p->compiler, name_token),
                           type, is_const, global, initializer);
    var->is_array = is_array;
    var->array_length = is_array && initializer && initializer->kind == EXPR_ARRAY ?
                        initializer->as.array.count : 0;
    Stmt *stmt = new_stmt(p->compiler, STMT_VAR, start);
    stmt->as.variable = var;
    return stmt;
}

static Stmt *parse_if_after_keyword(Parser *p, Token token)
{
    Stmt *stmt = new_stmt(p->compiler, STMT_IF, token);
    consume(p, TOKEN_LEFT_PAREN, "Expected '(' after if.",
            "Use if (condition) { ... }.");
    stmt->as.if_stmt.condition = parse_expression(p);
    consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after if condition.",
            "Close the condition before the block.");
    consume(p, TOKEN_LEFT_BRACE, "Expected '{' before the if body.",
            "Start the block with '{'.");
    stmt->as.if_stmt.then_branch = parse_block_after_open(p, p->previous);
    if (match(p, TOKEN_ELSE)) {
        if (match(p, TOKEN_IF)) {
            stmt->as.if_stmt.else_branch = parse_if_after_keyword(p, p->previous);
        } else {
            consume(p, TOKEN_LEFT_BRACE, "Expected '{' before else body.",
                    "Start the else block with '{'.");
            stmt->as.if_stmt.else_branch = parse_block_after_open(p, p->previous);
        }
    }
    return stmt;
}

static Stmt *parse_match_after_keyword(Parser *p, Token token)
{
    Stmt *stmt = new_stmt(p->compiler, STMT_MATCH, token);
    consume(p, TOKEN_LEFT_PAREN, "Expected '(' after match.",
            "Write match (value) { EnumName.Variant -> { ... } }.");
    stmt->as.match_stmt.value = parse_expression(p);
    consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after match value.",
            "Close the matched expression with ')'.");
    consume(p, TOKEN_LEFT_BRACE, "Expected '{' before match arms.",
            "Place one EnumName.Variant -> statement arm inside braces for each variant.");
    while (p->current.type != TOKEN_RIGHT_BRACE &&
           p->current.type != TOKEN_EOF) {
        Token arm_token = p->current;
        Expr *pattern = parse_expression(p);
        MatchArm *arm = arena_alloc(&p->compiler->arena, sizeof(*arm));
        arm->token = arm_token;
        if (pattern->kind != EXPR_ENUM_VALUE) {
            diagnostic(p->compiler, pattern->token, "error", "E2055",
                       "A match arm must name an enum variant.",
                       "Use a pattern such as Direction.North -> { ... }.");
        } else {
            arm->variant = pattern->as.enum_value.variant;
        }
        consume(p, TOKEN_ARROW, "Expected '->' after the match variant.",
                "Use EnumName.Variant -> { ... }.");
        arm->body = parse_statement(p);
        append_ptr(p->compiler, (void ***)&stmt->as.match_stmt.arms,
                   &stmt->as.match_stmt.arm_count, arm);
        if (match(p, TOKEN_COMMA) || match(p, TOKEN_SEMICOLON)) continue;
        if (p->current.type != TOKEN_RIGHT_BRACE &&
            p->current.start == arm_token.start &&
            p->current.type == arm_token.type) {
            advance_parser(p);
        }
    }
    consume(p, TOKEN_RIGHT_BRACE, "Expected '}' after match arms.",
            "Close the match statement with '}'.");
    return stmt;
}

static Stmt *parse_statement(Parser *p)
{
    Token token = p->current;
    if (p->current.type == TOKEN_IDENTIFIER && token_is(&p->current, "match")) {
        advance_parser(p);
        return parse_match_after_keyword(p, token);
    }
    if (match(p, TOKEN_LET)) return parse_variable(p, false);
    if (match(p, TOKEN_LEFT_BRACE)) return parse_block_after_open(p, token);

    if (match(p, TOKEN_PRINT)) {
        Stmt *stmt = new_stmt(p->compiler, STMT_PRINT, token);
        consume(p, TOKEN_LEFT_PAREN, "Expected '(' after print.",
                "Use print(expression);.");
        if (p->current.type != TOKEN_RIGHT_PAREN) {
            do {
                Expr *arg = parse_expression(p);
                append_ptr(p->compiler, (void ***)&stmt->as.print.args,
                           &stmt->as.print.count, arg);
            } while (match(p, TOKEN_COMMA));
        }
        consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after print arguments.",
                "Close the print call with ')'.");
        consume(p, TOKEN_SEMICOLON, "Expected ';' after print statement.",
                "Write print(value);.");
        if (stmt->as.print.count == 0) {
            diagnostic(p->compiler, token, "error", "E1015",
                       "print() needs at least one value.",
                       "For a blank line, use print(\"\");.");
        }
        return stmt;
    }

    if (match(p, TOKEN_IF)) return parse_if_after_keyword(p, token);

    if (match(p, TOKEN_LOOP)) {
        Stmt *stmt = new_stmt(p->compiler, STMT_LOOP, token);
        consume(p, TOKEN_LEFT_PAREN, "Expected '()' after loop.",
                "Write loop() { ... }.");
        consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after loop's opening parenthesis.",
                "Write loop() with empty parentheses.");
        consume(p, TOKEN_LEFT_BRACE, "Expected '{' before loop body.",
                "Start the loop body with '{'.");
        stmt->as.loop_body = parse_block_after_open(p, p->previous);
        return stmt;
    }

    if (match(p, TOKEN_WHILE)) {
        Stmt *stmt = new_stmt(p->compiler, STMT_WHILE, token);
        consume(p, TOKEN_LEFT_PAREN, "Expected '(' after while.",
                "Write while (condition) { ... }.");
        stmt->as.while_stmt.condition = parse_expression(p);
        consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after while condition.",
                "Close the condition with ')'.");
        stmt->as.while_stmt.body = parse_statement(p);
        return stmt;
    }

    if (match(p, TOKEN_FOR)) {
        Stmt *stmt = new_stmt(p->compiler, STMT_FOR, token);
        consume(p, TOKEN_LEFT_PAREN, "Expected '(' after for.",
                "Write for (let int i = 0; i < limit; i = i + 1) { ... }.");
        if (match(p, TOKEN_SEMICOLON)) {
            stmt->as.for_stmt.initializer = NULL;
        } else if (match(p, TOKEN_LET)) {
            stmt->as.for_stmt.initializer = parse_variable(p, false);
        } else {
            Stmt *init = new_stmt(p->compiler, STMT_EXPR, p->current);
            init->as.expression = parse_expression(p);
            consume(p, TOKEN_SEMICOLON, "Expected ';' after for initializer.",
                    "Separate the initializer and condition with ';'.");
            stmt->as.for_stmt.initializer = init;
        }
        if (p->current.type != TOKEN_SEMICOLON)
            stmt->as.for_stmt.condition = parse_expression(p);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after for condition.",
                "Separate the condition and increment with ';'.");
        if (p->current.type != TOKEN_RIGHT_PAREN)
            stmt->as.for_stmt.increment = parse_expression(p);
        consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after for clauses.",
                "Close the for header with ')'.");
        stmt->as.for_stmt.body = parse_statement(p);
        return stmt;
    }

    if (match(p, TOKEN_BREAK)) {
        Stmt *stmt = new_stmt(p->compiler, STMT_BREAK, token);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after break.", "Write break;.");
        return stmt;
    }
    if (match(p, TOKEN_CONTINUE)) {
        Stmt *stmt = new_stmt(p->compiler, STMT_CONTINUE, token);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after continue.", "Write continue;.");
        return stmt;
    }
    if (match(p, TOKEN_RETURN)) {
        Stmt *stmt = new_stmt(p->compiler, STMT_RETURN, token);
        if (p->current.type != TOKEN_SEMICOLON) stmt->as.return_value = parse_expression(p);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after return.", "End the return statement with ';'.");
        return stmt;
    }

    Expr *expr = parse_expression(p);
    Stmt *stmt = new_stmt(p->compiler, STMT_EXPR, token);
    stmt->as.expression = expr;
    consume(p, TOKEN_SEMICOLON, "Expected ';' after expression.",
            "End the statement with ';'.");
    return stmt;
}

static Function *parse_function(Parser *p)
{
    Token start = p->previous;
    Token name_token = consume(p, TOKEN_IDENTIFIER,
        "Expected a function name after 'function'.",
        "For example: function add(int a, int b) -> int { ... }");
    Function *function = arena_alloc(&p->compiler->arena, sizeof(*function));
    function->token = name_token;
    function->name = token_copy(p->compiler, name_token);
    function->c_name = arena_alloc(&p->compiler->arena, strlen(function->name) + 8);
    (void)sprintf(function->c_name, "yl_fn_%s", function->name);
    consume(p, TOKEN_LEFT_PAREN, "Expected '(' after function name.",
            "Write the parameter list inside parentheses.");
    if (p->current.type != TOKEN_RIGHT_PAREN) {
        do {
            Token param_type_token = p->current;
            YType type = parse_type(p);
            Token param_name = consume(p, TOKEN_IDENTIFIER,
                "Expected a parameter name after its type.",
                "For example: function greet(string name) -> void.");
            VarDecl *param = new_var(p->compiler, param_name,
                token_copy(p->compiler, param_name), type, false, false, NULL);
            if (match(p, TOKEN_LEFT_BRACKET)) {
                param->is_array = true;
                consume(p, TOKEN_RIGHT_BRACKET,
                        "Array parameters use empty brackets after the name.",
                        "Use syntax such as function sum(int values[]) -> int.");
            }
            param->initialized = true;
            (void)param_type_token;
            append_ptr(p->compiler, (void ***)&function->params,
                       &function->param_count, param);
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RIGHT_PAREN, "Expected ')' after parameters.",
            "Close the parameter list with ')'.");
    consume(p, TOKEN_ARROW, "Expected '->' before function return type.",
            "For example: function main() -> int { ... }");
    function->return_type = parse_type(p);
    if (function->return_type == TYPE_ERROR) function->return_type = TYPE_VOID;
    consume(p, TOKEN_LEFT_BRACE, "Expected '{' before function body.",
            "Start the function body with '{'.");
    function->body = parse_block_after_open(p, p->previous);
    (void)start;
    return function;
}

static StructDecl *parse_struct(Parser *p, Token struct_token)
{
    Token name_token = consume(p, TOKEN_IDENTIFIER,
        "Expected a struct name after 'struct'.",
        "Use a declaration such as struct Point { int x; int y; }.");
    if (name_token.length == 0) return NULL;
    const char *name = token_copy(p->compiler, name_token);
    for (size_t i = 0; i < p->compiler->struct_count; i++) {
        StructDecl *existing = p->compiler->structs[i];
        if (existing->module == p->compiler->current_module &&
            strcmp(existing->name, name) == 0) {
            diagnostic(p->compiler, name_token, "error", "E2065",
                       "This struct type is already declared in the current module.",
                       "Choose a unique struct type name.");
            break;
        }
    }

    StructDecl *structure = arena_alloc(&p->compiler->arena, sizeof(*structure));
    structure->token = struct_token;
    structure->name = (char *)name;
    structure->type = (YType)(TYPE_STRUCT_BASE + p->compiler->struct_count);
    structure->module = p->compiler->current_module;
    size_t prefix = p->compiler->struct_count;
    int c_name_length = snprintf(NULL, 0, "yl_struct_%zu_%s", prefix, name);
    if (c_name_length < 0) {
        diagnostic(p->compiler, name_token, "error", "E2066",
                   "Could not create an internal struct name.",
                   "Rename this type and try again.");
    } else {
        size_t size = (size_t)c_name_length + 1U;
        structure->c_name = arena_alloc(&p->compiler->arena, size);
        (void)snprintf(structure->c_name, size, "yl_struct_%zu_%s", prefix, name);
    }
    append_ptr(p->compiler, (void ***)&p->compiler->structs,
               &p->compiler->struct_count, structure);

    consume(p, TOKEN_LEFT_BRACE, "Expected '{' after the struct name.",
            "Declare fields inside the struct braces.");
    while (p->current.type != TOKEN_RIGHT_BRACE &&
           p->current.type != TOKEN_EOF) {
        Token field_type_token = p->current;
        YType field_type = parse_type(p);
        Token field_name_token = consume(p, TOKEN_IDENTIFIER,
            "Expected a field name after its type.",
            "Use field declarations such as 'int x;'.");
        if (field_name_token.length == 0) break;
        char *field_name = token_copy(p->compiler, field_name_token);
        if (field_type == TYPE_VOID) {
            diagnostic(p->compiler, field_type_token, "error", "E2067",
                       "A struct field cannot have type void.",
                       "Use a value type for every field.");
        }
        if (field_type == structure->type) {
            diagnostic(p->compiler, field_type_token, "error", "E2068",
                       "A struct cannot contain itself by value.",
                       "Use a different field type; recursive pointer fields are not supported yet.");
        }
        if (find_struct_field(structure, field_name)) {
            diagnostic(p->compiler, field_name_token, "error", "E2069",
                       "This struct field is declared more than once.",
                       "Each field name must be unique within the struct.");
        } else {
            StructField *field = arena_alloc(&p->compiler->arena, sizeof(*field));
            field->token = field_name_token;
            field->name = field_name;
            field->type = field_type;
            field->structure = structure;
            int field_c_len = snprintf(NULL, 0, "yl_f_%zu_%s", prefix, field_name);
            size_t field_c_size = field_c_len < 0 ? 1U : (size_t)field_c_len + 1U;
            field->c_name = arena_alloc(&p->compiler->arena, field_c_size);
            if (field_c_len >= 0)
                (void)snprintf(field->c_name, field_c_size, "yl_f_%zu_%s", prefix, field_name);
            append_ptr(p->compiler, (void ***)&structure->fields,
                       &structure->field_count, field);
        }
        consume(p, TOKEN_SEMICOLON, "Expected ';' after struct field.",
                "End each field declaration with ';'.");
    }
    if (structure->field_count == 0) {
        diagnostic(p->compiler, name_token, "error", "E2070",
                   "A struct must contain at least one field.",
                   "Add one or more typed fields inside the braces.");
    }
    consume(p, TOKEN_RIGHT_BRACE, "Expected '}' after struct fields.",
            "Close the struct declaration with '}'.");
    (void)match(p, TOKEN_SEMICOLON);
    return structure;
}

static EnumDecl *parse_enum(Parser *p, Token enum_token)
{
    Token name_token = consume(p, TOKEN_IDENTIFIER,
        "Expected an enum name after 'enum'.",
        "Use a declaration such as enum Color { Red, Green, Blue }.");
    if (name_token.length == 0) return NULL;

    const char *name = token_copy(p->compiler, name_token);
    for (size_t i = 0; i < p->compiler->enum_count; i++) {
        EnumDecl *existing = p->compiler->enums[i];
        if (existing->module == p->compiler->current_module &&
            strcmp(existing->name, name) == 0) {
            diagnostic(p->compiler, name_token, "error", "E2050",
                       "This enum type is already declared in the current module.",
                       "Choose a unique enum type name.");
            break;
        }
    }

    EnumDecl *enumeration = arena_alloc(&p->compiler->arena, sizeof(*enumeration));
    enumeration->token = enum_token;
    enumeration->name = (char *)name;
    enumeration->type = (YType)(TYPE_ENUM_BASE + p->compiler->enum_count);
    enumeration->module = p->compiler->current_module;
    append_ptr(p->compiler, (void ***)&p->compiler->enums,
               &p->compiler->enum_count, enumeration);

    consume(p, TOKEN_LEFT_BRACE, "Expected '{' after the enum name.",
            "List enum variants inside braces, for example enum Color { Red, Green }.");
    while (p->current.type != TOKEN_RIGHT_BRACE &&
           p->current.type != TOKEN_EOF) {
        Token variant_token = consume(p, TOKEN_IDENTIFIER,
            "Expected an enum variant name.",
            "Use a variant identifier such as Red or Green.");
        if (variant_token.length == 0) break;
        char *variant_name = token_copy(p->compiler, variant_token);
        if (find_enum_variant(enumeration, variant_name)) {
            diagnostic(p->compiler, variant_token, "error", "E2051",
                       "This enum variant is declared more than once.",
                       "Each variant name must be unique within its enum.");
        } else {
            EnumVariant *variant = arena_alloc(&p->compiler->arena, sizeof(*variant));
            variant->token = variant_token;
            variant->name = variant_name;
            variant->value = (int64_t)enumeration->variant_count;
            variant->enumeration = enumeration;
            append_ptr(p->compiler, (void ***)&enumeration->variants,
                       &enumeration->variant_count, variant);
        }
        if (match(p, TOKEN_COMMA)) {
            if (p->current.type == TOKEN_RIGHT_BRACE) break;
        } else {
            break;
        }
    }
    if (enumeration->variant_count == 0) {
        diagnostic(p->compiler, name_token, "error", "E2053",
                   "An enum must declare at least one variant.",
                   "Add one or more variants inside the braces.");
    }
    consume(p, TOKEN_RIGHT_BRACE, "Expected '}' after enum variants.",
            "Close the enum declaration with '}'.");
    (void)match(p, TOKEN_SEMICOLON);
    return enumeration;
}

static char *decode_import_path(Parser *p, Token token)
{
    if (token.length < 2 || token.start[0] != '"' ||
        token.start[token.length - 1] != '"') {
        diagnostic(p->compiler, token, "error", "E1020",
                   "An import path must be a double-quoted string literal.",
                   "Use syntax such as: from \"./module.yl\" import function_name;");
        return arena_strndup(&p->compiler->arena, "", 0);
    }

    StringBuilder decoded;
    sb_init(&decoded);
    size_t end = token.length - 1;
    for (size_t i = 1; i < end; i++) {
        char ch = token.start[i];
        if (ch != '\\') {
            sb_append_n(&decoded, &ch, 1);
            continue;
        }
        if (i + 1 >= end) {
            diagnostic(p->compiler, token, "error", "E1020",
                       "The import path ends with an incomplete escape.",
                       "Use a plain path, or escape a backslash as \\\\.");
            sb_destroy(&decoded);
            return arena_strndup(&p->compiler->arena, "", 0);
        }
        char escaped = token.start[++i];
        switch (escaped) {
            case '\\': sb_append_n(&decoded, "\\", 1); break;
            case '"': sb_append_n(&decoded, "\"", 1); break;
            case '/': sb_append_n(&decoded, "/", 1); break;
            default:
                diagnostic(p->compiler, token, "error", "E1020",
                           "Unsupported escape in import path.",
                           "Use ordinary path characters, \\\\, or \\\".");
                sb_destroy(&decoded);
                return arena_strndup(&p->compiler->arena, "", 0);
        }
    }
    char *path = arena_strndup(&p->compiler->arena,
                               decoded.data ? decoded.data : "",
                               decoded.length);
    sb_destroy(&decoded);
    if (path[0] == '\0') {
        diagnostic(p->compiler, token, "error", "E1020",
                   "An import path cannot be empty.",
                   "Provide a relative or absolute .yl file path.");
    }
    return path;
}

static ImportDecl *parse_import(Parser *p, Token from_token)
{
    ImportDecl *import = arena_alloc(&p->compiler->arena, sizeof(*import));
    Token path_token = p->current;
    if (p->current.type == TOKEN_STRING) {
        advance_parser(p);
        path_token = p->previous;
        import->path = decode_import_path(p, path_token);
    } else if (p->current.type == TOKEN_IDENTIFIER) {
        StringBuilder path;
        sb_init(&path);
        bool expect_segment = true;
        while (p->current.type == TOKEN_IDENTIFIER) {
            Token segment = p->current;
            advance_parser(p);
            sb_append_n(&path, segment.start, segment.length);
            expect_segment = false;
            if (p->current.type != TOKEN_DOT) break;
            sb_append_n(&path, ".", 1);
            advance_parser(p);
            if (p->current.type != TOKEN_IDENTIFIER) {
                diagnostic(p->compiler, p->current, "error", "E1020",
                           "Expected a path segment after '.'.",
                           "Use a dotted filename such as from main.yl import helper;");
                expect_segment = true;
                break;
            }
            expect_segment = true;
        }
        if (expect_segment || path.length == 0U) {
            import->path = arena_strndup(&p->compiler->arena, "", 0);
        } else {
            import->path = arena_strndup(&p->compiler->arena,
                                         path.data ? path.data : "",
                                         path.length);
        }
        sb_destroy(&path);
    } else {
        path_token = consume(p, TOKEN_STRING,
            "Expected a module path after 'from'.",
            "Use a quoted relative path or a dotted filename such as from main.yl import helper;");
        import->path = decode_import_path(p, path_token);
    }
    import->token = path_token.length ? path_token : from_token;

    if (p->current.type != TOKEN_IDENTIFIER ||
        !token_is(&p->current, "import")) {
        diagnostic(p->compiler, p->current, "error", "E1020",
                   "Expected 'import' after the module path.",
                   "Use: from \"./module.yl\" import function_name;");
        synchronize(p);
        return import;
    }
    advance_parser(p);

    if (p->current.type != TOKEN_IDENTIFIER) {
        diagnostic(p->compiler, p->current, "error", "E1020",
                   "Expected at least one imported name.",
                   "List one or more function or global names after 'import'.");
        synchronize(p);
        return import;
    }

    do {
        Token name_token = consume(p, TOKEN_IDENTIFIER,
            "Expected an imported function or global name.",
            "Import a declared name, for example 'import system_data;'");
        if (name_token.length == 0) break;
        ImportName *name = arena_alloc(&p->compiler->arena, sizeof(*name));
        name->token = name_token;
        name->name = token_copy(p->compiler, name_token);
        append_ptr(p->compiler, (void ***)&import->names,
                   &import->name_count, name);
        /* Accept the function-like spelling for compatibility: import helper()
           names a symbol; it does not call it. */
        if (match(p, TOKEN_LEFT_PAREN)) {
            (void)consume(p, TOKEN_RIGHT_PAREN,
                          "Imported names cannot have arguments.",
                          "Use empty parentheses only, for example import system_data().");
        }
        if (p->current.type != TOKEN_COMMA) break;
        advance_parser(p);
        if (p->current.type != TOKEN_IDENTIFIER) {
            diagnostic(p->compiler, p->current, "error", "E1020",
                       "Expected a name after ','.",
                       "Remove the trailing comma or add another imported name.");
            break;
        }
    } while (true);

    (void)consume(p, TOKEN_SEMICOLON,
                  "Expected ';' after the import declaration.",
                  "End the import declaration with ';'.");
    return import;
}

Program *parse_program(Compiler *c)
{
    Parser p;
    parser_init(&p, c, c->source, 1, 1);
    Program *program = arena_alloc(&c->arena, sizeof(*program));
    while (p.current.type != TOKEN_EOF) {
        Token before = p.current;
        if (p.current.type == TOKEN_ERROR) {
            advance_parser(&p);
            continue;
        } else if (p.current.type == TOKEN_IDENTIFIER &&
                   token_is(&p.current, "struct")) {
            Token struct_token = p.current;
            advance_parser(&p);
            (void)parse_struct(&p, struct_token);
        } else if (p.current.type == TOKEN_IDENTIFIER &&
                   token_is(&p.current, "enum")) {
            Token enum_token = p.current;
            advance_parser(&p);
            (void)parse_enum(&p, enum_token);
        } else if (p.current.type == TOKEN_IDENTIFIER &&
                   token_is(&p.current, "from")) {
            Token from_token = p.current;
            advance_parser(&p);
            ImportDecl *import = parse_import(&p, from_token);
            append_ptr(c, (void ***)&program->imports,
                       &program->import_count, import);
        } else if (match(&p, TOKEN_LET)) {
            Stmt *global_stmt = parse_variable(&p, true);
            append_ptr(c, (void ***)&program->globals, &program->global_count,
                       global_stmt->as.variable);
        } else if (match(&p, TOKEN_FUNCTION)) {
            Function *function = parse_function(&p);
            append_ptr(c, (void ***)&program->functions,
                       &program->function_count, function);
        } else {
            diagnostic(c, p.current, "error", "E1005",
                       "Only imports, global declarations, and function definitions are allowed at file scope.",
                       "Move executable statements into function main().");
            synchronize(&p);
        }
        if (p.current.start == before.start && p.current.type == before.type &&
            p.current.type != TOKEN_EOF) advance_parser(&p);
    }
    c->program = program;
    return program;
}

