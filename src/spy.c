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
    expect_plex(lexer, file_path, PLEX_id);
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

    Nob_String_Builder sb = {0};

    nob_read_entire_file(*file_path, &sb);

    for (size_t i = 0; i < sb.count; i++)
    {
        printf("%c", sb.items[i]);
    }
    printf("\n");

    stb_lexer lexer = {0};
    char string_store[1024];

    p_lexer_init(&lexer, sb.items, sb.items + sb.count, string_store, sizeof string_store);

    while (true)
    {
        p_lexer_get_token(&lexer);
        if (lexer.token == PLEX_eof)
        {
            break;
        }
        printf("    .globl _main\n");

        // def
        expect_id(&lexer, *file_path, "def");

        // function_name
        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, PLEX_id);
        printf("_%s:\n", lexer.string);
        printf("    push %%rbp\n");

        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, '(');
        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, ')');
        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, PLEX_arrow);
        p_lexer_get_token(&lexer);
        expect_id(&lexer, *file_path, "None");
        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, '{');

        p_lexer_get_token(&lexer);
        expect_id(&lexer, *file_path, "print");
        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, '(');
        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, PLEX_intlit);
        printf("    movl $%ld, %%edi\n", lexer.int_number);
        printf("    call _putchar\n");

        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, ')');
        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, ';');

        p_lexer_get_token(&lexer);
        expect_plex(&lexer, *file_path, '}');

        printf("    mov $0, %%eax\n");
        printf("    pop %%rbp\n");
        printf("    ret\n");
        return 0;
    }
}