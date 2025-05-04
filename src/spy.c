#define LEXER_IMPLEMENTATION
#include "strict_python_lexer.h"
#define FLAG_IMPLEMENTATION
#include "flag.h"

#define NOB_IMPLEMENTATION
#include "nob.h"

#include <stdio.h>

void usage(void)
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n", flag_program_name());
    flag_print_options(stderr);
}

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

void str_cpy(char *from, char *to, size_t n)
{
    for (size_t i = 0; i <= n; i++)
    {
        to[i] = from[i];
        if (from[i] == '\0')
            return;
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

enum spy_op_type
{
    SPY_OP_assign,
    SPY_OP_func_call,
};

enum spy_op_expression_type
{
    SPY_OP_EXPR_intlit,
    SPY_OP_EXPR_var,
};

typedef struct
{
    enum spy_op_expression_type type;
    union
    {
        size_t rhs_index;
        long intlit;
    } data;
} spy_op_expression;

typedef struct
{
    size_t var_index;
    spy_op_expression expr;
} spy_op_assign;

typedef struct
{
    char *name;
    size_t var_index;
} spy_op_func_call;

typedef struct
{
    enum spy_op_type type;
    union
    {
        spy_op_assign assign;
        spy_op_func_call func_call;
    } data;
} spy_op_stmt;

typedef struct
{
    spy_op_stmt *items;
    size_t count;
    size_t capacity;
    char *name;
} spy_ops_function;

typedef struct
{
    spy_ops_function *items;
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

bool parse_expression(stb_lexer *lexer, char *file_path, spy_vars *vars, spy_op_expression *expression)
{
    if (lexer->token == PLEX_intlit)
    {
        expression->type = SPY_OP_EXPR_intlit;
        expression->data.intlit = lexer->int_number;
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
        expression->type = SPY_OP_EXPR_var;
        expression->data.rhs_index = var_check->index;
    }
    else
    {
        print_loc(lexer, file_path, lexer->where_firstchar);
        fprintf(stderr, ": ERROR: Expression only supports integers and other variables.\n");
        print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
        return false;
    }
    return true;
}

bool parse_statement(stb_lexer *lexer, char *file_path, spy_vars *vars, size_t *local_variables_count, spy_ops_function *ops)
{
    if (lexer->token == PLEX_id)
    {
        // const char *old_parse_point = lexer.parse_point;
        char *id = strdup(lexer->string);
        char *id_where = lexer->where_firstchar;
        char *id_where_last = lexer->where_lastchar;
        str_cpy(lexer->string, id, lexer->string_len);
        p_lexer_get_token(lexer);
        if (lexer->token == '(')
        {
            // Function call
            if (!str_eq(id, "print"))
            {
                print_loc(lexer, file_path, lexer->where_firstchar);
                fprintf(stderr, ": ERROR: Function calls other than print are currently unsupported.\n");
                print_line(lexer, lexer->where_firstchar, lexer->where_lastchar);
                return false;
            }
            // Function args
            (*local_variables_count)++;
            p_lexer_get_token(lexer);
            spy_op_expression expression = {0};
            if (!parse_expression(lexer, file_path, vars, &expression))
                return false;
            spy_op_assign assign = {
                .var_index = *local_variables_count,
                .expr = expression,
            };
            spy_op_stmt op = {
                .type = SPY_OP_assign,
                .data.assign = assign,
            };
            nob_da_append(ops, op);
            p_lexer_get_token(lexer);
            // End of function call
            if (!expect_plex(lexer, file_path, ')'))
                return false;
            spy_op_func_call func_call = {
                .name = id,
                .var_index = *local_variables_count,
            };
            op.type = SPY_OP_func_call;
            op.data.func_call = func_call;
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
            (*local_variables_count)++;
            // Expression
            p_lexer_get_token(lexer);
            spy_op_expression expression = {0};
            if (!parse_expression(lexer, file_path, vars, &expression))
                return false;
            // TODO: Check variable name is not a keyword
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
            spy_var var = {
                .name = id,
                .where = id_where,
                .where_last = id_where_last,
                .index = *local_variables_count,
            };
            nob_da_append(vars, var);
            spy_op_assign op_assign = {
                .var_index = *local_variables_count,
                .expr = expression,
            };
            spy_op_stmt op = {
                .type = SPY_OP_assign,
                .data.assign = op_assign,
            };
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

bool parse_function(stb_lexer *lexer, char *file_path, spy_vars *vars, spy_ops_function *ops)
{
    // def
    if (!expect_id(lexer, file_path, "def"))
        return false;

    // function_name
    p_lexer_get_token(lexer);
    if (!expect_plex(lexer, file_path, PLEX_id))
        return false;
    ops->name = strdup(lexer->string);

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
        if (!parse_statement(lexer, file_path, vars, &local_variables_count, ops))
            return false;
        p_lexer_get_token(lexer);
    }

    // End of statement
    if (!expect_plex(lexer, file_path, '}'))
        return false;
    return true;
}

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
    "dump-ir",
};

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

bool compile_dump_ir(spy_ops *ops, Nob_String_Builder *output)
{
    nob_sb_appendf(output, "OPS (count: %zu)\n", ops->count);
    for (size_t i = 0; i < ops->count; i++)
    {
        spy_ops_function op_function = ops->items[i];
        nob_sb_appendf(output, "FUNCTION `%s` (count: %zu) {\n", op_function.name, op_function.count);
        for (size_t j = 0; j < op_function.count; j++)
        {
            spy_op_stmt op_stmt = op_function.items[j];
            switch (op_stmt.type)
            {
            case SPY_OP_assign:
            {
                spy_op_assign *assign = &op_stmt.data.assign;
                spy_op_expression *expr = &assign->expr;
                if (expr->type == SPY_OP_EXPR_intlit)
                {
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_int_lit [ var_%ld = %ld ]\n", assign->var_index, expr->data.intlit);
                }
                else if (expr->type == SPY_OP_EXPR_var)
                {
                    nob_sb_appendf(output, "    OP_STATEMENT_ASSIGN_var [ var_%ld = var_%ld ]\n", assign->var_index, expr->data.rhs_index);
                }
                break;
            }
            case SPY_OP_func_call:
            {
                spy_op_func_call func_call = op_stmt.data.func_call;
                nob_sb_appendf(output, "    OP_STATEMENT_FUNCTION_CALL [ %s(var_%ld) ] \n", func_call.name, func_call.var_index);
                break;
            }
            }
        }
        nob_sb_appendf(output, "}\n");
    }
    return true;
}

bool compile_python311(spy_ops *ops, Nob_String_Builder *output)
{
    for (size_t i = 0; i < ops->count; i++)
    {
        spy_ops_function op_function = ops->items[i];
        nob_sb_appendf(output, "def %s() -> None:\n", op_function.name);
        for (size_t j = 0; j < op_function.count; j++)
        {
            spy_op_stmt op_stmt = op_function.items[j];
            switch (op_stmt.type)
            {
            case SPY_OP_assign:
            {
                spy_op_assign *assign = &op_stmt.data.assign;
                spy_op_expression *expr = &assign->expr;
                if (expr->type == SPY_OP_EXPR_intlit)
                {
                    nob_sb_appendf(output, "    var_%ld: int = %ld\n", assign->var_index, expr->data.intlit);
                }
                else if (expr->type == SPY_OP_EXPR_var)
                {
                    nob_sb_appendf(output, "    var_%ld: int = var_%ld\n", assign->var_index, expr->data.rhs_index);
                }
                break;
            }
            case SPY_OP_func_call:
            {
                spy_op_func_call func_call = op_stmt.data.func_call;
                nob_sb_appendf(output, "    %s(var_%ld) \n", func_call.name, func_call.var_index);
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
    {
        spy_op_assign *assign = &op->data.assign;
        spy_op_expression *expr = &assign->expr;
        if (expr->type == SPY_OP_EXPR_intlit)
        {
            nob_sb_appendf(output, "    movl $%ld, -%ld(%%rbp)\n", expr->data.intlit, assign->var_index * 4);
        }
        else if (expr->type == SPY_OP_EXPR_var)
        {
            nob_sb_appendf(output, "    movl -%ld(%%rbp), %%eax\n", expr->data.rhs_index * 4);
            nob_sb_appendf(output, "    movl %%eax, -%ld(%%rbp)\n", assign->var_index * 4);
        }
        break;
    }
    case SPY_OP_func_call:
    {
        spy_op_func_call *func_call = &op->data.func_call;
        nob_sb_appendf(output, "    movl -%ld(%%rbp), %%edi\n", func_call->var_index * 4);
        nob_sb_appendf(output, "    call _putchar\n");
        break;
    }
    }
    return true;
}

bool compile_x86_64_macos_function_body(spy_ops_function *ops, Nob_String_Builder *output)
{
    nob_sb_appendf(output, "_%s:\n", ops->name);
    nob_sb_appendf(output, "    push %%rbp\n");
    for (size_t i = 0; i < ops->count; i++)
    {
        spy_op_stmt *op = ops->items + i;
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

int main(int argc, char **argv)
{
    char **file_path = flag_str("path", NULL, "Path to the input spy file (MANDATORY)");
    char **output_path = flag_str("o", NULL, "Path to the output file (MANDATORY)");
    char **output_target = flag_str("target", NULL, "Target compilation output");

    if (!flag_parse(argc, argv))
    {
        usage();
        flag_print_error(stderr);
        return 1;
    }

    if (*file_path == NULL)
    {
        usage();
        fprintf(stderr, "ERROR: No -%s was provided\n", flag_name(file_path));
        return 1;
    }

    if (*output_path == NULL)
    {
        usage();
        fprintf(stderr, "ERROR: No -%s was provided\n", flag_name(output_path));
        return 1;
    }

    if (*output_target == NULL)
    {
        output_target = &TARGET_STRINGS[0];
    }

    enum spy_output_target target = get_target(*output_target);
    if (target < 0)
        return 1;

    Nob_String_Builder sb = {0};

    nob_read_entire_file(*file_path, &sb);

    stb_lexer lexer = {0};
    char string_store[1024];

    p_lexer_init(&lexer, sb.items, sb.items + sb.count, string_store, sizeof string_store);

    spy_vars vars = {0};

    spy_ops ops = {0};

    p_lexer_get_token(&lexer);
    // TODO: turn into while loop to parse multiple files
    spy_ops_function op_function = {0};
    if (!parse_function(&lexer, *file_path, &vars, &op_function))
        return 1;
    nob_da_append(&ops, op_function);

    Nob_String_Builder output = {0};

    compile(&ops, &output, target);

    if (!nob_write_entire_file(*output_path, output.items, output.count))
    {
        fprintf(stderr, "ERROR: Unable to write to %s\n", *output_path);
        return 1;
    }

    nob_sb_free(sb);
    nob_sb_free(output);
    nob_da_free(vars);
    nob_da_free(ops);
    return 0;
}