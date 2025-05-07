#define LEXER_IMPLEMENTATION
#include "strict_python_lexer.h"
#define FLAG_IMPLEMENTATION
#include "flag.h"
#define NOB_IMPLEMENTATION
#include "nob.h"
#include <stdio.h>

bool str_eq(char *str_a, char *str_b)
{
    for (size_t i = 0;; i++)
    {
        if (str_a[i] != str_b[i])
        {
            return false;
        }
        if (str_a[i] == '\0')
        {
            return true;
        }
    }
}

char *pretty_token(long token)
{
    switch (token)
    {
    case PLEX_eof:
        return "eof";
    case PLEX_parse_error:
        return "parse error";
    case PLEX_intlit:
        return "integer literal";
    case PLEX_floatlit:
        return "float literal";
    case PLEX_id:
        return "identifier";
    case PLEX_dqstring:
        return "\"";
    case PLEX_sqstring:
        return "\'";
    case PLEX_eq:
        return "==";
    case PLEX_noteq:
        return "!=";
    case PLEX_lesseq:
        return "<=";
    case PLEX_greatereq:
        return ">=";
    case PLEX_shl:
        return "<<";
    case PLEX_shr:
        return ">>";
    case PLEX_pluseq:
        return "+=";
    case PLEX_minuseq:
        return "-=";
    case PLEX_muleq:
        return "*=";
    case PLEX_diveq:
        return "/=";
    case PLEX_modeq:
        return "%=";
    case PLEX_andeq:
        return "&=";
    case PLEX_oreq:
        return "|=";
    case PLEX_xoreq:
        return "^=";
    case PLEX_arrow:
        return "->";
    case PLEX_shleq:
        return ">>=";
    case PLEX_shreq:
        return "<<=";
    }
    return nob_temp_sprintf("%c", (char)token);
}

void print_line(stb_lexer *lexer, char *where_start, char *where_end)
{
    // TODO: maybe remove line number, its slow :)
    stb_lex_location loc = {0};
    p_lexer_get_location(lexer, where_start, &loc);
    printf("%d", loc.line_number);

    char *ptr = where_start;
    size_t start_col = 0;
    while (ptr != lexer->input_stream && *(ptr - 1) != '\n')
    {
        ptr--;
        start_col++;
    }
    while (ptr != lexer->eof && *ptr != '\n')
    {
        putchar(*ptr);
        ptr++;
    }
    putchar('\n');
    for (size_t i = 0; i < start_col + 1; i++)
    {
        putchar(' ');
    }
    for (size_t i = 0; i <= (size_t)(where_end - where_start); i++)
    {
        putchar('~');
    }
    putchar('\n');
}

void print_loc(stb_lexer *lexer, char *file_path, const char *where)
{
    stb_lex_location loc = {0};
    p_lexer_get_location(lexer, where, &loc);
    fprintf(stderr, "%s:%d:%d", file_path, loc.line_number, loc.line_offset + 1);
}

/*
    PARSER
*/

bool expect_plex(stb_lexer *lexer, char *file_path, long plex)
{
    if (lexer->token != plex)
    {
        print_loc(lexer, file_path, lexer->where_firstchar);
        fprintf(stderr, ": ERROR: expected %s but got %s\n", pretty_token(plex), pretty_token(lexer->token));
        print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
        return false;
    }
    return true;
}

bool expect_id(stb_lexer *lexer, char *file_path, char *compare_string)
{
    if (!expect_plex(lexer, file_path, PLEX_id))
        return false;
    if (!str_eq(lexer->string, compare_string))
    {
        print_loc(lexer, file_path, lexer->where_firstchar);
        fprintf(stderr, ": ERROR: expected `%s` but got `%s`\n", compare_string, lexer->string);
        print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
        return false;
    }
    return true;
}

char *KEYWORDS[] = {
    "def",
};

typedef struct
{
    char *name;
    size_t index;
    char *where;
    char *where_last;
} spy_var;

typedef struct
{
    spy_var *items;
    size_t count;
    size_t capacity;
} spy_vars;

spy_var *find_var(spy_vars *vars, char *name)
{
    for (size_t i = 0; i < vars->count; i++)
    {
        if (str_eq(vars->items[i].name, name))
        {
            return &vars->items[i];
        }
    }
    return NULL;
}

enum spy_op_stmt_type
{
    SPY_OP_assign,
    SPY_OP_declare_assign,
    SPY_OP_assign_binop,
    SPY_OP_declare_assign_binop,
    SPY_OP_func_call,
};

enum spy_op_term_type
{
    SPY_OP_TERM_intlit,
    SPY_OP_TERM_var,
};

enum spy_op_expr_binop_type
{
    SPY_OP_EXPR_BINOP_add,
    SPY_OP_EXPR_BINOP_sub,
    SPY_OP_EXPR_BINOP_mul,
    SPY_OP_EXPR_BINOP_lt,
    SPY_OP_EXPR_BINOP_gt,
    SPY_OP_EXPR_BINOP_eq,
    SPY_OP_EXPR_BINOP_neq,
};

typedef struct
{
    enum spy_op_term_type type;
    union
    {
        size_t var_index;
        long intlit;
    } data;
} spy_op_term;

typedef struct
{
    size_t var_index;
    spy_op_term term;
} spy_op_assign;

typedef struct
{
    size_t var_index;
    enum spy_op_expr_binop_type type;
    spy_op_term lhs;
    spy_op_term rhs;
} spy_op_assign_binop;

typedef struct
{
    char *name;
    spy_op_term arg;
} spy_op_func_call;

typedef struct
{
    enum spy_op_stmt_type type;
    union
    {
        spy_op_assign assign;
        spy_op_assign_binop assign_binop;
        spy_op_func_call func_call;
    } data;
} spy_op_stmt;

typedef struct
{
    spy_op_stmt *items;
    size_t count;
    size_t capacity;
} spy_op_stmts;

typedef struct
{
    spy_op_stmts stmts;
    char *name;
} spy_op_function;

typedef struct
{
    spy_op_function *items;
    size_t count;
    size_t capacity;
} spy_ops;

bool is_keyword(char *name)
{
    for (size_t i = 0; i < sizeof KEYWORDS / sizeof(char *); i++)
    {
        if (str_eq(KEYWORDS[i], name))
        {
            return true;
        }
    }
    return false;
}

#define NUM_PRECEDENCE_LEVELS 3

bool token_is_at_binop_precedence(long token, size_t precedence_level)
{
    // Get compiler error to add new types here
    spy_op_assign_binop binop = {0};
    switch (binop.type)
    {
    case SPY_OP_EXPR_BINOP_add:
    case SPY_OP_EXPR_BINOP_sub:
    case SPY_OP_EXPR_BINOP_mul:
    case SPY_OP_EXPR_BINOP_lt:
    case SPY_OP_EXPR_BINOP_gt:
    case SPY_OP_EXPR_BINOP_eq:
    case SPY_OP_EXPR_BINOP_neq:
        break;
    }
    switch (precedence_level)
    {
    case 0:
        return token == '<' || token == '>' || token == PLEX_eq || token == PLEX_noteq;
    case 1:
        return token == '+' || token == '-';
    case 2:
        return token == '*';
    }
    fprintf(stderr, "Unreachable. Please update the precedence tables. Trying to grab %s from precedence %zu\n", pretty_token(token), precedence_level);
    exit(1);
}

enum spy_op_expr_binop_type expr_binop_type_from_token_precedence(long token, size_t precedence_level)
{
    // Get compiler error to add new types here
    spy_op_assign_binop binop = {0};
    switch (binop.type)
    {
    case SPY_OP_EXPR_BINOP_add:
    case SPY_OP_EXPR_BINOP_sub:
    case SPY_OP_EXPR_BINOP_mul:
    case SPY_OP_EXPR_BINOP_lt:
    case SPY_OP_EXPR_BINOP_gt:
    case SPY_OP_EXPR_BINOP_eq:
    case SPY_OP_EXPR_BINOP_neq:
        break;
    }
    switch (precedence_level)
    {
    case 0:
    {
        switch (token)
        {
        case '<':
            return SPY_OP_EXPR_BINOP_lt;
        case '>':
            return SPY_OP_EXPR_BINOP_gt;
        case PLEX_eq:
            return SPY_OP_EXPR_BINOP_eq;
        case PLEX_noteq:
            return SPY_OP_EXPR_BINOP_neq;
        }
    }
    case 1:
    {
        switch (token)
        {
        case '+':
            return SPY_OP_EXPR_BINOP_add;
        case '-':
            return SPY_OP_EXPR_BINOP_sub;
        }
    }
    case 2:
    {
        switch (token)
        {
        case '*':
            return SPY_OP_EXPR_BINOP_add;
        }
    }
    }
    fprintf(stderr, "Unreachable. Please update the precedence tables. Trying to grab %s from precedence %zu\n", pretty_token(token), precedence_level);
    exit(1);
}

bool parse_expression(stb_lexer *lexer, char *file_path, spy_vars *vars, size_t *local_variables_count, spy_op_stmts *ops, spy_op_term *term);

bool parse_expression_term(stb_lexer *lexer, char *file_path, spy_vars *vars, size_t *local_variables_count, spy_op_stmts *ops, spy_op_term *term)
{
    // Get compiler error to add new types here
    switch (term->type)
    {
    case SPY_OP_TERM_intlit:
    case SPY_OP_TERM_var:
        break;
    }
    if (lexer->token == PLEX_intlit)
    {
        term->type = SPY_OP_TERM_intlit;
        term->data.intlit = lexer->int_number;
    }
    else if (lexer->token == PLEX_id)
    {
        // Check if not keyword
        spy_var *var_check = find_var(vars, lexer->string);
        if (var_check == NULL)
        {
            print_loc(lexer, file_path, lexer->where_firstchar);
            fprintf(stderr, ": ERROR: Undefined variable.\n");
            print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
            return false;
        }
        term->type = SPY_OP_TERM_var;
        term->data.var_index = var_check->index;
    }
    else if (lexer->token == '(')
    {
        p_lexer_get_token(lexer);
        if (!parse_expression(lexer, file_path, vars, local_variables_count, ops, term))
            return false;
        p_lexer_get_token(lexer);
        if (!expect_plex(lexer, file_path, ')'))
            return false;
    }
    else
    {
        print_loc(lexer, file_path, lexer->where_firstchar);
        fprintf(stderr, ": ERROR: Term only supports integers and other variables.\n");
        print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
        return false;
    }
    return true;
}

bool parse_expression_precedence(stb_lexer *lexer, char *file_path, spy_vars *vars, size_t *local_variables_count, spy_op_stmts *ops, spy_op_term *term, size_t precedence_level)
{
    if (precedence_level >= NUM_PRECEDENCE_LEVELS)
    {
        if (!parse_expression_term(lexer, file_path, vars, local_variables_count, ops, term))
            return false;
        return true;
    }
    spy_op_term lhs = {0};
    if (!parse_expression_precedence(lexer, file_path, vars, local_variables_count, ops, &lhs, precedence_level + 1))
        return false;

    char *old_parse_point = lexer->parse_point;
    p_lexer_get_token(lexer);
    if (token_is_at_binop_precedence(lexer->token, precedence_level))
    {
        (*local_variables_count)++;
        size_t index = *local_variables_count;
        spy_op_term rhs = {0};
        spy_op_assign_binop binop = {0};
        spy_op_stmt stmt = {0};
        size_t count = 0;
        long token = lexer->token;
        while (token_is_at_binop_precedence(lexer->token, precedence_level))
        {
            p_lexer_get_token(lexer);
            if (!parse_expression_precedence(lexer, file_path, vars, local_variables_count, ops, &rhs, precedence_level + 1))
                return false;
            binop.type = expr_binop_type_from_token_precedence(token, precedence_level);
            binop.lhs = lhs;
            binop.rhs = rhs;
            binop.var_index = index;
            stmt.type = count > 0 ? SPY_OP_assign_binop : SPY_OP_declare_assign_binop;
            stmt.data.assign_binop = binop;
            nob_da_append(ops, stmt);
            lhs.type = SPY_OP_TERM_var;
            lhs.data.var_index = index;

            old_parse_point = lexer->parse_point;
            p_lexer_get_token(lexer);
            count++;
        }
    }
    lexer->parse_point = old_parse_point;
    *term = lhs;
    return true;
}

bool parse_expression(stb_lexer *lexer, char *file_path, spy_vars *vars, size_t *local_variables_count, spy_op_stmts *ops, spy_op_term *term)
{
    return parse_expression_precedence(lexer, file_path, vars, local_variables_count, ops, term, 0);
}

bool parse_statement(stb_lexer *lexer, char *file_path, spy_vars *vars, size_t *local_variables_count, spy_op_stmts *ops)
{
    if (lexer->token == PLEX_id)
    {
        // const char *old_parse_point = lexer.parse_point;
        char *id = strdup(lexer->string);
        char *id_where = lexer->where_firstchar;
        char *id_where_last = lexer->where_lastchar;
        p_lexer_get_token(lexer);
        if (lexer->token == '(')
        {
            // Function call
            if (!str_eq(id, "putchar"))
            {
                print_loc(lexer, file_path, lexer->where_firstchar);
                fprintf(stderr, ": ERROR: Function calls other than putchar are currently unsupported.\n");
                print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
                return false;
            }
            // Function args
            p_lexer_get_token(lexer);
            spy_op_term term = {0};
            if (!parse_expression(lexer, file_path, vars, local_variables_count, ops, &term))
                return false;
            p_lexer_get_token(lexer);
            // End of function call
            if (!expect_plex(lexer, file_path, ')'))
                return false;
            spy_op_func_call op_func_call = {
                .name = id,
                .arg = term,
            };
            spy_op_stmt op = {
                .type = SPY_OP_func_call,
                .data.func_call = op_func_call,
            };
            // Get compiler error to add new types here
            switch (op.type)
            {
            case SPY_OP_assign:
            case SPY_OP_declare_assign:
            case SPY_OP_assign_binop:
            case SPY_OP_declare_assign_binop:
            case SPY_OP_func_call:
                break;
            }
            nob_da_append(ops, op);
        }
        else if (lexer->token == ':')
        {
            // Variable assignment
            if (is_keyword(id))
            {
                print_loc(lexer, file_path, id_where);
                fprintf(stderr, ": ERROR: Cannot assign keyword.\n");
                print_line(lexer, id_where, id_where_last);
                return false;
            }
            spy_var *var_check = find_var(vars, id);
            if (var_check != NULL)
            {
                print_loc(lexer, file_path, lexer->where_firstchar);
                fprintf(stderr, ": ERROR: Cannot declare existing variable.\n");
                print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
                print_loc(lexer, file_path, var_check->where);
                fprintf(stderr, ": NOTE: Original definition is here.\n");
                print_line(lexer, var_check->where, var_check->where_last);
                return false;
            }
            p_lexer_get_token(lexer);
            if (!expect_plex(lexer, file_path, PLEX_id))
                return false;
            if (!str_eq(lexer->string, "int"))
            {
                print_loc(lexer, file_path, lexer->where_firstchar);
                fprintf(stderr, ": ERROR: Variable types other than int are currently unsupported.\n");
                print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
                return false;
            }
            p_lexer_get_token(lexer);
            if (!expect_plex(lexer, file_path, '='))
                return false;
            // Expression
            p_lexer_get_token(lexer);
            spy_op_term term = {0};
            if (!parse_expression(lexer, file_path, vars, local_variables_count, ops, &term))
                return false;
            (*local_variables_count)++;
            spy_var var = {
                .name = id,
                .where = id_where,
                .where_last = id_where_last,
                .index = *local_variables_count,
            };
            nob_da_append(vars, var);
            spy_op_assign op_assign = {
                .var_index = *local_variables_count,
                .term = term,
            };
            spy_op_stmt op = {
                .type = SPY_OP_declare_assign,
                .data.assign = op_assign,
            };
            // Get compiler error to add new types here
            switch (op.type)
            {
            case SPY_OP_assign:
            case SPY_OP_assign_binop:
            case SPY_OP_declare_assign:
            case SPY_OP_declare_assign_binop:
            case SPY_OP_func_call:
                break;
            }
            nob_da_append(ops, op);
        }
        else if (lexer->token == '=')
        {
            // Variable assignment
            if (is_keyword(id))
            {
                print_loc(lexer, file_path, id_where);
                fprintf(stderr, ": ERROR: Cannot assign keyword.\n");
                print_line(lexer, id_where, id_where_last);
                return false;
            }
            spy_var *var_check = find_var(vars, id);
            if (var_check == NULL)
            {
                print_loc(lexer, file_path, lexer->where_firstchar);
                fprintf(stderr, ": ERROR: Cannot assign to non-existing variable.\n");
                print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
                return false;
            }
            // Expression
            p_lexer_get_token(lexer);
            spy_op_term term = {0};
            if (!parse_expression(lexer, file_path, vars, local_variables_count, ops, &term))
                return false;
            spy_op_assign op_assign = {
                .var_index = var_check->index,
                .term = term,
            };
            spy_op_stmt op = {
                .type = SPY_OP_assign,
                .data.assign = op_assign,
            };
            // Get compiler error to add new types here
            switch (op.type)
            {
            case SPY_OP_assign:
            case SPY_OP_declare_assign:
            case SPY_OP_assign_binop:
            case SPY_OP_declare_assign_binop:
            case SPY_OP_func_call:
                break;
            }
            nob_da_append(ops, op);
        }
        else
        {
            print_loc(lexer, file_path, lexer->where_firstchar);
            fprintf(stderr, ": ERROR: Invalid statement. Expects function call, or variable assignment\n");
            print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
            return false;
        }
    }
    else
    {
        print_loc(lexer, file_path, lexer->where_firstchar);
        fprintf(stderr, ": ERROR: Invalid statement. Expects function call, or variable assignment\n");
        print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
        return false;
    }

    p_lexer_get_token(lexer);
    if (!expect_plex(lexer, file_path, ';'))
        return false;
    return true;
}

bool parse_function(stb_lexer *lexer, char *file_path, spy_vars *vars, spy_op_function *op_func)
{
    // def
    if (!expect_id(lexer, file_path, "def"))
        return false;

    // function_name
    p_lexer_get_token(lexer);
    if (!expect_plex(lexer, file_path, PLEX_id))
        return false;
    op_func->name = strdup(lexer->string);

    p_lexer_get_token(lexer);
    if (!expect_plex(lexer, file_path, '('))
        return false;
    p_lexer_get_token(lexer);
    if (!expect_plex(lexer, file_path, ')'))
        return false;
    p_lexer_get_token(lexer);
    if (!expect_plex(lexer, file_path, PLEX_arrow))
        return false;
    p_lexer_get_token(lexer);
    if (!expect_id(lexer, file_path, "None"))
        return false;
    p_lexer_get_token(lexer);
    if (!expect_plex(lexer, file_path, '{'))
        return false;

    p_lexer_get_token(lexer);
    size_t local_variables_count = 0;
    while (lexer->token != '}')
    {
        if (!parse_statement(lexer, file_path, vars, &local_variables_count, &op_func->stmts))
            return false;
        p_lexer_get_token(lexer);
    }

    // End of statement
    if (!expect_plex(lexer, file_path, '}'))
        return false;
    return true;
}

/*
    COMPILER (OUTPUT)
*/

enum spy_output_target
{
    SPY_OUTPUT_TARGET_x86_64_macos,
    SPY_OUTPUT_TARGET_aarch64_mac_m1,
    SPY_OUTPUT_TARGET_python311,
    SPY_OUTPUT_TARGET_dump_ir,
};

char *TARGET_STRINGS[] = {
    "x86-64-macos",
    "aarch64-mac-m1",
    "python311",
    "ir",
};

bool compile_dump_ir_term(spy_op_term *term, Nob_String_Builder *output)
{
    switch (term->type)
    {
    case SPY_OP_TERM_intlit:
        nob_sb_appendf(output, "(int literal) %ld", term->data.intlit);
        break;
    case SPY_OP_TERM_var:
        nob_sb_appendf(output, "var_%ld", term->data.var_index);
        break;
    }
    return true;
}

bool compile_dump_ir(spy_ops *ops, Nob_String_Builder *output)
{
    nob_sb_appendf(output, "OPS (count: %zu)\n", ops->count);
    for (size_t i = 0; i < ops->count; i++)
    {
        spy_op_function op_function = ops->items[i];
        spy_op_stmts op_stmts = op_function.stmts;
        nob_sb_appendf(output, "FUNCTION `%s` (count: %zu) {\n", op_function.name, op_stmts.count);
        for (size_t j = 0; j < op_stmts.count; j++)
        {
            spy_op_stmt op_stmt = op_stmts.items[j];
            switch (op_stmt.type)
            {
            case SPY_OP_assign:
            {
                spy_op_assign *assign = &op_stmt.data.assign;
                spy_op_term *term = &assign->term;
                nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN [ var_%ld, ", assign->var_index);
                if (!compile_dump_ir_term(term, output))
                    return false;
                nob_sb_appendf(output, " ]\n");
                break;
            }
            case SPY_OP_declare_assign:
            {
                spy_op_assign *assign = &op_stmt.data.assign;
                spy_op_term *term = &assign->term;
                nob_sb_appendf(output, "    OP_STATEMENT_DECLARE_ASSIGN [ var_%ld, ", assign->var_index);
                if (!compile_dump_ir_term(term, output))
                    return false;
                nob_sb_appendf(output, " ]\n");
                break;
            }
            case SPY_OP_assign_binop:
            {
                switch (op_stmt.data.assign_binop.type)
                {
                case SPY_OP_EXPR_BINOP_add:
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_ADD [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_sub:
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_SUB [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_mul:
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_MUL [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_lt:
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_LT [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_gt:
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_GT [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_eq:
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_EQ [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_neq:
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_NEQ [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                }
                if (!compile_dump_ir_term(&op_stmt.data.assign_binop.lhs, output))
                    return false;
                nob_sb_appendf(output, ", ");
                if (!compile_dump_ir_term(&op_stmt.data.assign_binop.rhs, output))
                    return false;
                nob_sb_appendf(output, " ]\n");
                break;
            }
            case SPY_OP_declare_assign_binop:
            {
                switch (op_stmt.data.assign_binop.type)
                {
                case SPY_OP_EXPR_BINOP_add:
                    nob_sb_appendf(output, "    OP_STATEMENT_DECLARE_ASSIGN_ADD [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_sub:
                    nob_sb_appendf(output, "    OP_STATEMENT_DECLARE_ASSIGN_SUB [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_mul:
                    nob_sb_appendf(output, "    OP_STATEMENT_DECLARE_ASSIGN_MUL [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_lt:
                    nob_sb_appendf(output, "    OP_STATEMENT_DECLARE_ASSIGN_LT [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_gt:
                    nob_sb_appendf(output, "    OP_STATEMENT_DECLARE_ASSIGN_GT [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_eq:
                    nob_sb_appendf(output, "    OP_STATEMENT_DECLARE_ASSIGN_EQ [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                case SPY_OP_EXPR_BINOP_neq:
                    nob_sb_appendf(output, "    OP_STATEMENT_DECLARE_ASSIGN_NEQ [ var_%ld, ", op_stmt.data.assign.var_index);
                    break;
                }
                if (!compile_dump_ir_term(&op_stmt.data.assign_binop.lhs, output))
                    return false;
                nob_sb_appendf(output, ", ");
                if (!compile_dump_ir_term(&op_stmt.data.assign_binop.rhs, output))
                    return false;
                nob_sb_appendf(output, " ]\n");
                break;
            }
            case SPY_OP_func_call:
            {
                spy_op_func_call func_call = op_stmt.data.func_call;
                nob_sb_appendf(output, "    OP_STATEMENT_FUNCTION_CALL [ %s, ", func_call.name);
                if (!compile_dump_ir_term(&func_call.arg, output))
                    return false;
                nob_sb_appendf(output, " ]\n");
                break;
            }
            }
        }
        nob_sb_appendf(output, "}\n");
    }
    return true;
}

bool compile_dump_python311_term(spy_op_term *term, Nob_String_Builder *output)
{
    switch (term->type)
    {
    case SPY_OP_TERM_intlit:
        nob_sb_appendf(output, "%ld", term->data.intlit);
        break;
    case SPY_OP_TERM_var:
        nob_sb_appendf(output, "var_%ld", term->data.var_index);
        break;
    }
    return true;
}

bool compile_python311(spy_ops *ops, Nob_String_Builder *output)
{
    for (size_t i = 0; i < ops->count; i++)
    {
        spy_op_function op_function = ops->items[i];
        spy_op_stmts op_stmts = op_function.stmts;
        nob_sb_appendf(output, "def %s() -> None:\n", op_function.name);
        for (size_t j = 0; j < op_stmts.count; j++)
        {
            spy_op_stmt op_stmt = op_stmts.items[j];
            switch (op_stmt.type)
            {
            case SPY_OP_assign:
            {
                spy_op_assign *assign = &op_stmt.data.assign;
                spy_op_term *term = &assign->term;
                nob_sb_appendf(output, "    var_%ld = ", assign->var_index);
                if (!compile_dump_python311_term(term, output))
                    return false;
                nob_sb_appendf(output, "\n");
                break;
            }
            case SPY_OP_declare_assign:
            {
                spy_op_assign *assign = &op_stmt.data.assign;
                spy_op_term *term = &assign->term;
                nob_sb_appendf(output, "    var_%ld: int = ", assign->var_index);
                if (!compile_dump_python311_term(term, output))
                    return false;
                nob_sb_appendf(output, "\n");
                break;
            }
            case SPY_OP_assign_binop:
            {
                nob_sb_appendf(output, "    var_%ld = ", op_stmt.data.assign.var_index);
                if (!compile_dump_python311_term(&op_stmt.data.assign_binop.lhs, output))
                    return false;
                switch (op_stmt.data.assign_binop.type)
                {
                case SPY_OP_EXPR_BINOP_add:
                    nob_sb_appendf(output, " + ");
                    break;
                case SPY_OP_EXPR_BINOP_sub:
                    nob_sb_appendf(output, " - ");
                    break;
                case SPY_OP_EXPR_BINOP_mul:
                    nob_sb_appendf(output, " * ");
                    break;
                case SPY_OP_EXPR_BINOP_lt:
                    nob_sb_appendf(output, " < ");
                    break;
                case SPY_OP_EXPR_BINOP_gt:
                    nob_sb_appendf(output, " > ");
                    break;
                case SPY_OP_EXPR_BINOP_eq:
                    nob_sb_appendf(output, " == ");
                    break;
                case SPY_OP_EXPR_BINOP_neq:
                    nob_sb_appendf(output, " != ");
                    break;
                }
                if (!compile_dump_python311_term(&op_stmt.data.assign_binop.rhs, output))
                    return false;
                nob_sb_appendf(output, "\n");
                break;
            }
            case SPY_OP_declare_assign_binop:
            {
                nob_sb_appendf(output, "    var_%ld: int = ", op_stmt.data.assign.var_index);
                if (!compile_dump_python311_term(&op_stmt.data.assign_binop.lhs, output))
                    return false;
                switch (op_stmt.data.assign_binop.type)
                {
                case SPY_OP_EXPR_BINOP_add:
                    nob_sb_appendf(output, " + ");
                    break;
                case SPY_OP_EXPR_BINOP_sub:
                    nob_sb_appendf(output, " - ");
                    break;
                case SPY_OP_EXPR_BINOP_mul:
                    nob_sb_appendf(output, " * ");
                    break;
                case SPY_OP_EXPR_BINOP_lt:
                    nob_sb_appendf(output, " < ");
                    break;
                case SPY_OP_EXPR_BINOP_gt:
                    nob_sb_appendf(output, " > ");
                    break;
                case SPY_OP_EXPR_BINOP_eq:
                    nob_sb_appendf(output, " == ");
                    break;
                case SPY_OP_EXPR_BINOP_neq:
                    nob_sb_appendf(output, " != ");
                    break;
                }
                if (!compile_dump_python311_term(&op_stmt.data.assign_binop.rhs, output))
                    return false;
                nob_sb_appendf(output, "\n");
                break;
            }
            case SPY_OP_func_call:
            {
                spy_op_func_call func_call = op_stmt.data.func_call;
                nob_sb_appendf(output, "    %s(", func_call.name);
                compile_dump_python311_term(&func_call.arg, output);
                nob_sb_appendf(output, ")\n");
                break;
            }
            }
        }
        nob_sb_appendf(output, "\n");
    }
    return true;
}

bool compile_x86_64_macos_file_header(Nob_String_Builder *output)
{
    nob_sb_appendf(output, "    .globl _main\n");
    return true;
}

bool compile_x86_64_macos_file_footer()
{
    return true;
}

bool compile_x86_64_macos_statement(spy_op_stmt *op, Nob_String_Builder *output)
{
    switch (op->type)
    {
    case SPY_OP_assign:
    case SPY_OP_declare_assign:
    {
        spy_op_assign *assign = &op->data.assign;
        spy_op_term *term = &assign->term;
        switch (term->type)
        {
        case SPY_OP_TERM_intlit:
            nob_sb_appendf(output, "    movl $%ld, -%ld(%%rbp)\n", term->data.intlit, assign->var_index * 4);
            break;
        case SPY_OP_TERM_var:
            nob_sb_appendf(output, "    movl -%ld(%%rbp), %%eax\n", term->data.var_index * 4);
            nob_sb_appendf(output, "    movl %%eax, -%ld(%%rbp)\n", assign->var_index * 4);
            break;
        }
        break;
    }
    case SPY_OP_assign_binop:
    case SPY_OP_declare_assign_binop:
    {
        spy_op_assign_binop *assign = &op->data.assign_binop;
        // spy_op_term *term = &assign->term;
        switch (assign->lhs.type)
        {
        case SPY_OP_TERM_intlit:
            nob_sb_appendf(output, "    movl $%ld, %%eax\n", assign->lhs.data.intlit);
            break;
        case SPY_OP_TERM_var:
            nob_sb_appendf(output, "    movl -%ld(%%rbp), %%eax\n", assign->lhs.data.var_index * 4);
            break;
        }
        switch (assign->type)
        {
        case SPY_OP_EXPR_BINOP_add:
        {
            switch (assign->rhs.type)
            {
            case SPY_OP_TERM_intlit:
                nob_sb_appendf(output, "    addl $%ld, %%eax\n", assign->rhs.data.intlit);
                break;
            case SPY_OP_TERM_var:
                nob_sb_appendf(output, "    addl -%ld(%%rbp), %%eax\n", assign->rhs.data.var_index * 4);
                break;
            }
            break;
        }
        case SPY_OP_EXPR_BINOP_sub:
        {
            switch (assign->rhs.type)
            {
            case SPY_OP_TERM_intlit:
                nob_sb_appendf(output, "    subl $%ld, %%eax\n", assign->rhs.data.intlit);
                break;
            case SPY_OP_TERM_var:
                nob_sb_appendf(output, "    subl -%ld(%%rbp), %%eax\n", assign->rhs.data.var_index * 4);
                break;
            }
            break;
        }
        case SPY_OP_EXPR_BINOP_mul:
        {
            switch (assign->rhs.type)
            {
            case SPY_OP_TERM_intlit:
                nob_sb_appendf(output, "    imull $%ld, %%eax\n", assign->rhs.data.intlit);
                break;
            case SPY_OP_TERM_var:
                nob_sb_appendf(output, "    imull -%ld(%%rbp), %%eax\n", assign->rhs.data.var_index * 4);
                break;
            }
            break;
        }
        case SPY_OP_EXPR_BINOP_lt:
        case SPY_OP_EXPR_BINOP_gt:
        case SPY_OP_EXPR_BINOP_eq:
        case SPY_OP_EXPR_BINOP_neq:
        {
            // Use ecx as temporary register, not ebx, because ebx causes segfault on my machine.
            nob_sb_appendf(output, "    xor %%ecx,  %%ecx\n");
            switch (assign->rhs.type)
            {
            case SPY_OP_TERM_intlit:
                nob_sb_appendf(output, "    cmp $%ld, %%eax\n", assign->rhs.data.intlit);
                break;
            case SPY_OP_TERM_var:
                nob_sb_appendf(output, "    cmp -%ld(%%rbp), %%eax\n", assign->rhs.data.var_index * 4);
                break;
            }
            switch (assign->type)
            {
            case SPY_OP_EXPR_BINOP_add:
            case SPY_OP_EXPR_BINOP_sub:
            case SPY_OP_EXPR_BINOP_mul:
            case SPY_OP_EXPR_BINOP_lt:
                nob_sb_appendf(output, "    setl %%cl\n");
                break;
            case SPY_OP_EXPR_BINOP_gt:
                nob_sb_appendf(output, "    setg %%cl\n");
                break;
            case SPY_OP_EXPR_BINOP_eq:
                nob_sb_appendf(output, "    sete %%cl\n");
                break;
            case SPY_OP_EXPR_BINOP_neq:
                nob_sb_appendf(output, "    setne %%cl\n");
                break;
            }
            nob_sb_appendf(output, "    movl %%ecx, %%eax\n");
            break;
        }
        }
        nob_sb_appendf(output, "    movl %%eax, -%ld(%%rbp)\n", assign->var_index * 4);
        break;
    }
    case SPY_OP_func_call:
    {
        spy_op_func_call *func_call = &op->data.func_call;
        switch (func_call->arg.type)
        {
        case SPY_OP_TERM_intlit:
            nob_sb_appendf(output, "    movl $%ld, %%edi\n", func_call->arg.data.intlit);
            break;
        case SPY_OP_TERM_var:
            nob_sb_appendf(output, "    movl -%ld(%%rbp), %%edi\n", func_call->arg.data.var_index * 4);
            break;
        }
        nob_sb_appendf(output, "    call _putchar\n");
        break;
    }
    }
    return true;
}

bool compile_x86_64_macos_function_body(spy_op_function *ops, Nob_String_Builder *output)
{
    nob_sb_appendf(output, "_%s:\n", ops->name);
    nob_sb_appendf(output, "    push %%rbp\n");
    for (size_t i = 0; i < ops->stmts.count; i++)
    {
        spy_op_stmt *op = ops->stmts.items + i;
        if (!compile_x86_64_macos_statement(op, output))
            return false;
    }
    // TODO proper return
    nob_sb_appendf(output, "    movl $0, %%eax\n");
    nob_sb_appendf(output, "    pop %%rbp\n");
    nob_sb_appendf(output, "    ret\n");
    return true;
}

bool compile_x86_64_macos(spy_ops *ops, Nob_String_Builder *output)
{
    if (!compile_x86_64_macos_file_header(output))
        return false;
    for (size_t i = 0; i < ops->count; i++)
    {
        if (!compile_x86_64_macos_function_body(&ops->items[i], output))
            return false;
    }
    if (!compile_x86_64_macos_file_footer())
        return false;
    return true;
}

bool compile(spy_ops *ops, Nob_String_Builder *output, enum spy_output_target target)
{
    switch (target)
    {
    case SPY_OUTPUT_TARGET_x86_64_macos:
        return compile_x86_64_macos(ops, output);
    case SPY_OUTPUT_TARGET_dump_ir:
        return compile_dump_ir(ops, output);
    case SPY_OUTPUT_TARGET_aarch64_mac_m1:
        fprintf(stderr, "Target `aarch64-mac-m1` is not supported yet!\n");
        return false;
    case SPY_OUTPUT_TARGET_python311:
        return compile_python311(ops, output);
    }
    return true;
}

/*
    COMMAND LINE ARGS
*/

void usage(void)
{
    fprintf(stderr, "Usage: %s <path> [OPTIONS]\n", flag_program_name());
    flag_print_options(stderr);
}

void default_output_path(char *input_path, enum spy_output_target target, Nob_String_Builder *output)
{
    size_t ptr = 0;
    while (input_path[ptr] != '\0' && input_path[ptr] != '.')
    {
        nob_sb_appendf(output, "%c", input_path[ptr]);
        ptr++;
    }
    switch (target)
    {
    case SPY_OUTPUT_TARGET_x86_64_macos:
        nob_sb_append_cstr(output, ".s");
        break;
    case SPY_OUTPUT_TARGET_dump_ir:
        nob_sb_append_cstr(output, ".txt");
        break;
    case SPY_OUTPUT_TARGET_aarch64_mac_m1:
        nob_sb_append_cstr(output, ".s");
        break;
    case SPY_OUTPUT_TARGET_python311:
        nob_sb_append_cstr(output, ".py");
        break;
    }
    nob_sb_append_null(output);
}

enum spy_output_target get_target(char *target_string)
{
    for (size_t i = 0; i < sizeof TARGET_STRINGS / sizeof(char *); i++)
    {
        if (str_eq(target_string, TARGET_STRINGS[i]))
            return (enum spy_output_target)i;
    }
    fprintf(stderr, "Invalid target `%s`! Supports\n", target_string);
    for (size_t i = 0; i < sizeof TARGET_STRINGS / sizeof(char *); i++)
    {
        fprintf(stderr, "    %s\n", TARGET_STRINGS[i]);
    }
    return -1;
}

/*
    MAIN
*/

int main(int argc, char **argv)
{
    char **output_path = flag_str("o", NULL, "Path to the output file (MANDATORY)");
    char **output_target = flag_str("target", NULL, "Target compilation output");

    char *file_path = NULL;
    while (argc > 0)
    {
        if (!flag_parse(argc, argv))
        {
            usage();
            flag_print_error(stderr);
            return 1;
        }
        argc = flag_rest_argc();
        argv = flag_rest_argv();
        if (argc > 0)
        {
            if (file_path != NULL)
            {
                // TODO: support compiling several files?
                fprintf(stderr, "ERROR: Serveral input files is not supported yet\n");
                return 1;
            }
            file_path = flag_shift_args(&argc, &argv);
        }
    }

    if (file_path == NULL)
    {
        usage();
        fprintf(stderr, "ERROR: No input path was provided\n");
        return 1;
    }
    if (*output_target == NULL)
    {
        output_target = &TARGET_STRINGS[0];
    }

    enum spy_output_target target = get_target(*output_target);
    if (target < 0)
        return 1;

    Nob_String_Builder default_output_path_sb = {0};
    if (*output_path == NULL)
    {
        default_output_path(file_path, target, &default_output_path_sb);
        output_path = &default_output_path_sb.items;
    }

    Nob_String_Builder sb = {0};

    nob_read_entire_file(file_path, &sb);

    stb_lexer lexer = {0};
    char string_store[1024];

    p_lexer_init(&lexer, sb.items, sb.items + sb.count, string_store, sizeof string_store);

    spy_vars vars = {0};

    spy_ops ops = {0};

    p_lexer_get_token(&lexer);
    // TODO: turn into while loop to parse multiple files
    spy_op_function op_function = {0};
    if (!parse_function(&lexer, file_path, &vars, &op_function))
        return 1;
    nob_da_append(&ops, op_function);

    Nob_String_Builder output = {0};

    if (!compile(&ops, &output, target))
        return 1;

    if (!nob_write_entire_file(*output_path, output.items, output.count))
    {
        fprintf(stderr, "ERROR: Unable to write to %s\n", *output_path);
        return 1;
    }

    nob_sb_free(default_output_path_sb);
    nob_sb_free(sb);
    nob_sb_free(output);
    nob_da_free(vars);
    nob_da_free(ops);
    return 0;
}