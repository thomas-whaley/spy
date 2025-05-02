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
    return 0;
}
