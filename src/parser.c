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
    size_t offset = block ? (block->used + alignment - 1U) & ~(alignment - 1U) : 0;
    if (!block || offset + size > block->capacity) {
        size_t capacity = size > 8192 ? size + alignment : 8192;
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
    if (sb->length + extra + 1 <= sb->capacity) return;
    size_t capacity = sb->capacity ? sb->capacity : 128;
    while (capacity < sb->length + extra + 1) capacity *= 2;
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
    fprintf(stderr, "%s[%s]: %s\n", level, code, message);
    fprintf(stderr, "  --> %s:%zu:%zu\n", c->filename, line, column);

    const char *line_start = c->source;
    size_t current_line = 1;
    while (current_line < line && *line_start) {
        if (*line_start++ == '\n') current_line++;
    }
    const char *line_end = line_start;
    while (*line_end && *line_end != '\n' && *line_end != '\r') line_end++;
    if (*line_start && current_line == line) {
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
    var->c_name = arena_alloc(&c->arena, 96);
    (void)snprintf(var->c_name, 96, "yl_v_%zu_%s", ++c->next_var_id, name);
    VarDecl **grown = arena_alloc(&c->arena, (c->all_var_count + 1) * sizeof(*grown));
    if (c->all_var_count) memcpy(grown, c->all_vars, c->all_var_count * sizeof(*grown));
    grown[c->all_var_count++] = var;
    c->all_vars = grown;
    return var;
}

static void append_ptr(Compiler *c, void ***items, size_t *count, void *item)
{
    void **grown = arena_alloc(&c->arena, (*count + 1) * sizeof(*grown));
    if (*count) memcpy(grown, *items, *count * sizeof(*grown));
    grown[(*count)++] = item;
    *items = grown;
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
    diagnostic(p->compiler, token, "error", "E2002", "Unknown type name.",
               "Available YLang 1.0 types: int, float, bool, char, string, void.");
    return TYPE_ERROR;
}

const char *type_name(YType type)
{
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
            FPart *grown = arena_alloc(&p->compiler->arena, (expr->as.fstring.count + 1) * sizeof(*grown));
            if (expr->as.fstring.count) memcpy(grown, expr->as.fstring.parts, expr->as.fstring.count * sizeof(*grown));
            grown[expr->as.fstring.count++] = part;
            expr->as.fstring.parts = grown;
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
            FPart *grown = arena_alloc(&p->compiler->arena, (expr->as.fstring.count + 1) * sizeof(*grown));
            if (expr->as.fstring.count) memcpy(grown, expr->as.fstring.parts, expr->as.fstring.count * sizeof(*grown));
            grown[expr->as.fstring.count++] = part;
            expr->as.fstring.parts = grown;
        }
        pos++; /* closing brace */
    }
    if (text.length > 0 || expr->as.fstring.count == 0) {
        FPart part = { .text = arena_strndup(&p->compiler->arena,
                       text.data ? text.data : "", text.length), .expression = NULL };
        FPart *grown = arena_alloc(&p->compiler->arena, (expr->as.fstring.count + 1) * sizeof(*grown));
        if (expr->as.fstring.count) memcpy(grown, expr->as.fstring.parts, expr->as.fstring.count * sizeof(*grown));
        grown[expr->as.fstring.count++] = part;
        expr->as.fstring.parts = grown;
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
            memchr(token.start, '.', token.length) ? EXPR_FLOAT : EXPR_INT, token);
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
                token_is(&token, "io")) {
                StringBuilder qualified;
                sb_init(&qualified);
                sb_append_n(&qualified, token.start, token.length);
                sb_append(&qualified, ".");
                sb_append_n(&qualified, member.start, member.length);
                name = arena_strndup(&p->compiler->arena, qualified.data, qualified.length);
                sb_destroy(&qualified);
            } else {
                diagnostic(p->compiler, token, "error", "E1016",
                           "Unknown standard-library namespace.",
                           "Use a supported namespace such as math, string, or io.");
                name = arena_strndup(&p->compiler->arena, "", 0);
            }
        }
        expr->as.name.name = name;
        if (match(p, TOKEN_LEFT_PAREN)) {
            Expr *call = new_expr(p->compiler, EXPR_CALL, token);
            call->as.call.name = expr->as.name.name;
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
        case TOKEN_EQUAL: return 1;
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
        int prec = precedence(p->current.type);
        if (prec == 0 || prec < min_precedence) break;
        op = p->current;
        advance_parser(p);
        int next_min = prec + (op.type == TOKEN_EQUAL ? 0 : 1);
        Expr *right = parse_precedence(p, next_min);
        if (op.type == TOKEN_EQUAL) {
            if (left->kind != EXPR_NAME && left->kind != EXPR_INDEX) {
                diagnostic(p->compiler, op, "error", "E1004",
                           "The left side of an assignment must be a variable or array element.",
                           "Write an assignment such as 'count = count + 1;' or 'values[0] = 1;'.");
                left = new_expr(p->compiler, EXPR_ERROR, op);
            } else {
                Expr *assign = new_expr(p->compiler, EXPR_ASSIGN, op);
                assign->as.assign.target = left;
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

static Stmt *parse_statement(Parser *p)
{
    Token token = p->current;
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
                       "Only global declarations and function definitions are allowed at file scope.",
                       "Move executable statements into function main().");
            synchronize(&p);
        }
        if (p.current.start == before.start && p.current.type == before.type &&
            p.current.type != TOKEN_EOF) advance_parser(&p);
    }
    c->program = program;
    return program;
}

