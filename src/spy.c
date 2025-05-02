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
        fprintf(stderr, ": ERROR: expected %ld but got %ld\n", plex, lexer->token);
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
        return false;
    }
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

    while (true)
    {
        p_lexer_get_token(&lexer);
        if (lexer.token == PLEX_eof)
        {
            break;
        }
        nob_sb_appendf(&output, "    .globl _main\n");

        // def
        if (!expect_id(&lexer, *file_path, "def"))
            return 1;

        // function_name
        p_lexer_get_token(&lexer);
        if (!expect_plex(&lexer, *file_path, PLEX_id))
            return 1;
        nob_sb_appendf(&output, "_%s:\n", lexer.string);
        nob_sb_appendf(&output, "    push %%rbp\n");

        p_lexer_get_token(&lexer);
        if (!expect_plex(&lexer, *file_path, '('))
            return 1;
        p_lexer_get_token(&lexer);
        if (!expect_plex(&lexer, *file_path, ')'))
            return 1;
        p_lexer_get_token(&lexer);
        if (!expect_plex(&lexer, *file_path, PLEX_arrow))
            return 1;
        p_lexer_get_token(&lexer);
        if (!expect_id(&lexer, *file_path, "None"))
            return 1;
        p_lexer_get_token(&lexer);
        if (!expect_plex(&lexer, *file_path, '{'))
            return 1;

        p_lexer_get_token(&lexer);
        while (lexer.token != '}')
        {
            // Statement
            size_t local_variables_count = 0;
            if (lexer.token == PLEX_id)
            {
                // const char *old_parse_point = lexer.parse_point;
                char id[1024];
                str_cpy(lexer.string, id, lexer.string_len);
                p_lexer_get_token(&lexer);
                if (lexer.token == '(')
                {
                    // Function call
                    if (!str_eq(id, "print"))
                    {
                        print_loc(&lexer, *file_path, lexer.where_firstchar);
                        fprintf(stderr, ": ERROR: Function calls other than print are currently unsupported.\n");
                        return 1;
                    }
                    // Function args
                    p_lexer_get_token(&lexer);
                    if (!expect_plex(&lexer, *file_path, PLEX_intlit))
                        return 1;
                    nob_sb_appendf(&output, "    movl $%ld, %%edi\n", lexer.int_number);
                    nob_sb_appendf(&output, "    call _putchar\n");

                    p_lexer_get_token(&lexer);
                    // End of function call
                    if (!expect_plex(&lexer, *file_path, ')'))
                        return 1;
                }
                else if (lexer.token == ':')
                {
                    // Variable assignment
                    p_lexer_get_token(&lexer);
                    if (!expect_plex(&lexer, *file_path, PLEX_id))
                        return 1;
                    if (!str_eq(lexer.string, "int"))
                    {
                        print_loc(&lexer, *file_path, lexer.where_firstchar);
                        fprintf(stderr, ": ERROR: Variable types other than int are currently unsupported.\n");
                        return 1;
                    }
                    p_lexer_get_token(&lexer);
                    if (!expect_plex(&lexer, *file_path, '='))
                        return 1;
                    // Expression
                    p_lexer_get_token(&lexer);
                    if (!expect_plex(&lexer, *file_path, PLEX_intlit))
                        return 1;
                    // TODO: Put (id: local_variables_count) into map to lookup later
                    local_variables_count++;
                    nob_sb_appendf(&output, "    movl $%ld, -%zu(%%rbp)\n", lexer.int_number, local_variables_count * 4);
                }
                else
                {
                    print_loc(&lexer, *file_path, lexer.where_firstchar);
                    fprintf(stderr, ": ERROR: Invalid statement. Expects function call, or variable assignment\n");
                    return 1;
                }
            }
            else
            {
                print_loc(&lexer, *file_path, lexer.where_firstchar);
                fprintf(stderr, ": ERROR: Invalid statement. Expects function call, or variable assignment\n");
                return 1;
            }

            p_lexer_get_token(&lexer);
            if (!expect_plex(&lexer, *file_path, ';'))
                return 1;

            p_lexer_get_token(&lexer);
        }

        // End of statement
        if (!expect_plex(&lexer, *file_path, '}'))
            return 1;

        nob_sb_appendf(&output, "    mov $0, %%eax\n");
        nob_sb_appendf(&output, "    pop %%rbp\n");
        nob_sb_appendf(&output, "    ret\n");
    }

    if (!nob_write_entire_file(*output_path, output.items, output.count))
    {
        fprintf(stderr, "ERROR: Unable to write to %s\n", *output_path);
        return 1;
    }

    return 0;
}