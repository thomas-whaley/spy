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

bool parse_statement(stb_lexer *lexer, char *file_path, spy_vars *vars, size_t *local_variables_count, Nob_String_Builder *output)
{
    if (lexer->token == PLEX_id)
    {
        // const char *old_parse_point = lexer.parse_point;
        char *id = strdup(lexer->string);
        char *id_where = lexer->where_firstchar;
        char *id_where_last = lexer->where_firstchar;
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
            p_lexer_get_token(lexer);
            if (!expect_plex(lexer, file_path, PLEX_intlit))
                return false;
            nob_sb_appendf(output, "    movl $%ld, %%edi\n", lexer->int_number);
            nob_sb_appendf(output, "    call _putchar\n");

            p_lexer_get_token(lexer);
            // End of function call
            if (!expect_plex(lexer, file_path, ')'))
                return false;
        }
        else if (lexer->token == ':')
        {
            // Variable assignment
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
            if (!expect_plex(lexer, file_path, PLEX_intlit))
                return false;
            // TODO: Put (id: local_variables_count) into map to lookup later
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
            (*local_variables_count)++;
            nob_sb_appendf(output, "    movl $%ld, -%zu(%%rbp)\n", lexer->int_number, *local_variables_count * 4);
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

bool parse_function(stb_lexer *lexer, char *file_path, spy_vars *vars, Nob_String_Builder *output)
{
    // def
    if (!expect_id(lexer, file_path, "def"))
        return false;

    // function_name
    p_lexer_get_token(lexer);
    if (!expect_plex(lexer, file_path, PLEX_id))
        return false;
    nob_sb_appendf(output, "_%s:\n", lexer->string);
    nob_sb_appendf(output, "    push %%rbp\n");

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
        if (!parse_statement(lexer, file_path, vars, &local_variables_count, output))
            return false;
        p_lexer_get_token(lexer);
    }

    // End of statement
    if (!expect_plex(lexer, file_path, '}'))
        return false;

    nob_sb_appendf(output, "    mov $0, %%eax\n");
    nob_sb_appendf(output, "    pop %%rbp\n");
    nob_sb_appendf(output, "    ret\n");
    return true;
}

int main(int argc, char **argv)
{
    char **file_path = flag_str("path", NULL, "Path to the input spy file (MANDATORY)");
    char **output_path = flag_str("o", NULL, "Path to the output file (MANDATORY)");

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

    Nob_String_Builder sb = {0};

    nob_read_entire_file(*file_path, &sb);

    stb_lexer lexer = {0};
    char string_store[1024];

    p_lexer_init(&lexer, sb.items, sb.items + sb.count, string_store, sizeof string_store);

    Nob_String_Builder output = {0};

    spy_vars vars = {0};

    p_lexer_get_token(&lexer);
    nob_sb_appendf(&output, "    .globl _main\n");

    if (!parse_function(&lexer, *file_path, &vars, &output))
        return 1;

    if (!nob_write_entire_file(*output_path, output.items, output.count))
    {
        fprintf(stderr, "ERROR: Unable to write to %s\n", *output_path);
        return 1;
    }

    return 0;
}