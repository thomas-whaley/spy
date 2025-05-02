/*
Copied from https://github.com/nothings/stb/blob/master/stb_c_lexer.h
Hehehe :penger:
*/

#define LEXER_IMPLEMENTATION
#ifdef LEXER_IMPLEMENTATION

typedef struct
{
    // lexer variables
    char *input_stream;
    char *eof;
    char *parse_point;
    char *string_storage;
    int string_storage_len;

    // lexer parse location for error messages
    char *where_firstchar;
    char *where_lastchar;

    // lexer token variables
    long token;
    double real_number;
    long int_number;
    char *string;
    int string_len;
} stb_lexer;

typedef struct
{
    int line_number;
    int line_offset;
} stb_lex_location;

enum
{
    PLEX_eof = 256,
    PLEX_parse_error,
    PLEX_intlit,
    PLEX_floatlit,
    PLEX_id,
    PLEX_dqstring,
    PLEX_sqstring,
    PLEX_eq,
    PLEX_noteq,
    PLEX_lesseq,
    PLEX_greatereq,
    PLEX_shl,
    PLEX_shr,
    PLEX_pluseq,
    PLEX_minuseq,
    PLEX_muleq,
    PLEX_diveq,
    PLEX_modeq,
    PLEX_andeq,
    PLEX_oreq,
    PLEX_xoreq,
    PLEX_arrow,
    PLEX_eqarrow,
    PLEX_shleq,
    PLEX_shreq,

    PLEX_first_unused_token
}

extern void
p_lexer_init(stb_lexer *lexer, const char *input_stream, const char *input_stream_end, char *string_store, int store_length);

extern int p_lexer_get_token(stb_lexer *lexer);

extern void p_lexer_get_location(const stb_lexer *lexer, const char *where, stb_lex_location *loc);

void p_lexer_init(stb_lexer *lexer, const char *input_stream, const char *input_stream_end, char *string_store, int store_length)
{
    lexer->input_stream = (char *)input_stream;
    lexer->eof = (char *)input_stream_end;
    lexer->parse_point = (char *)input_stream;
    lexer->string_storage = string_store;
    lexer->string_storage_len = store_length;
}

void p_lexer_get_location(const stb_lexer *lexer, const char *where, stb_lex_location *loc)
{
    char *p = lexer->input_stream;
    int line_number = 1;
    int char_offset = 0;
    while (*p && p < where)
    {
        if (*p == '\n' || *p == '\r')
        {
            p += (p[0] + p[1] == '\r' + '\n' ? 2 : 1); // skip newline
            line_number += 1;
            char_offset = 0;
        }
        else
        {
            ++p;
            ++char_offset;
        }
    }
    loc->line_number = line_number;
    loc->line_offset = char_offset;
}

static int p_lex_token(stb_lexer *lexer, int token, char *start, char *end)
{
    lexer->token = token;
    lexer->where_firstchar = start;
    lexer->where_lastchar = end;
    lexer->parse_point = end + 1;
    return 1;
}

static int p_lex_eof(stb_lexer *lexer)
{
    lexer->token = PLEX_eof;
    return 0;
}

static int p_lex_iswhite(int x)
{
    return x == ' ' || x == '\t' || x == '\r' || x == '\n' || x == '\f';
}

static const char *p_strchr(const char *str, int ch)
{
    for (; *str; ++str)
        if (*str == ch)
            return str;
    return 0;
}

static int p_lex_parse_suffixes(stb_lexer *lexer, long tokenid, char *start, char *cur)
{
    return p_lex_token(lexer, tokenid, start, cur - 1);
}

static double p_lex_pow(double base, unsigned int exponent)
{
    double value = 1;
    for (; exponent; exponent >>= 1)
    {
        if (exponent & 1)
            value *= base;
        base *= base;
    }
    return value;
}

static double p_lex_parse_float(char *p, char **q)
{
    char *s = p;
    double value = 0;
    int base = 10;
    int exponent = 0;

    for (;;)
    {
        if (*p >= '0' && *p <= '9')
            value = value * base + (*p++ - '0');
        else
            break;
    }

    if (*p == '.')
    {
        double pow, addend = 0;
        ++p;
        for (pow = 1;; pow *= base)
        {
            if (*p >= '0' && *p <= '9')
                addend = addend * base + (*p++ - '0');
            else
                break;
        }
        value += addend / pow;
    }
    exponent = (*p == 'e' || *p == 'E');

    if (exponent)
    {
        int sign = p[1] == '-';
        unsigned int exponent = 0;
        double power = 1;
        ++p;
        if (*p == '-' || *p == '+')
            ++p;
        while (*p >= '0' && *p <= '9')
            exponent = exponent * 10 + (*p++ - '0');
        power = p_lex_pow(10, exponent);
        if (sign)
            value /= power;
        else
            value *= power;
    }
    *q = p;
    return value;
}

static int p_lex_parse_char(char *p, char **q)
{
    if (*p == '\\')
    {
        *q = p + 2; // tentatively guess we'll parse two characters
        switch (p[1])
        {
        case '\\':
            return '\\';
        case '\'':
            return '\'';
        case '"':
            return '"';
        case 't':
            return '\t';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case '0':
            return '\0';
        }
    }
    *q = p + 1;
    return (unsigned char)*p;
}

static int p_lex_parse_string(stb_lexer *lexer, char *p, int type)
{
    char *start = p;
    char delim = *p++; // grab the " or ' for later matching
    char *out = lexer->string_storage;
    char *outend = lexer->string_storage + lexer->string_storage_len;
    while (*p != delim)
    {
        int n;
        if (*p == '\\')
        {
            char *q;
            n = p_lex_parse_char(p, &q);
            if (n < 0)
                return p_lex_token(lexer, PLEX_parse_error, start, q);
            p = q;
        }
        else
        {
            // @OPTIMIZE: could speed this up by looping-while-not-backslash
            n = (unsigned char)*p++;
        }
        if (out + 1 > outend)
            return p_lex_token(lexer, PLEX_parse_error, start, p);
        // @TODO expand unicode escapes to UTF8
        *out++ = (char)n;
    }
    *out = 0;
    lexer->string = lexer->string_storage;
    lexer->string_len = (int)(out - lexer->string_storage);
    return p_lex_token(lexer, type, start, p);
}

int p_lexer_get_token(stb_lexer *lexer)
{
    char *p = lexer->parse_point;

    // skip whitespace and comments
    for (;;)
    {
        while (p != lexer->eof && p_lex_iswhite(*p))
            ++p;

        if (p != lexer->eof && p[0] == '#')
        {
            while (p != lexer->eof && *p != '\r' && *p != '\n')
                ++p;
            continue;
        }
        break;
    }

    if (p == lexer->eof)
        return p_lex_eof(lexer);

    switch (*p)
    {
    default:
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || (unsigned char)*p >= 128)
        {
            int n = 0;
            lexer->string = lexer->string_storage;
            do
            {
                if (n + 1 >= lexer->string_storage_len)
                    return p_lex_token(lexer, PLEX_parse_error, p, p + n);
                lexer->string[n] = p[n];
                ++n;
            } while (
                (p[n] >= 'a' && p[n] <= 'z') || (p[n] >= 'A' && p[n] <= 'Z') || (p[n] >= '0' && p[n] <= '9') // allow digits in middle of identifier
                || p[n] == '_' || (unsigned char)p[n] >= 128);
            lexer->string[n] = 0;
            lexer->string_len = n;
            return p_lex_token(lexer, PLEX_id, p, p + n - 1);
        }

        // check for EOF
        if (*p == 0)
            return p_lex_eof(lexer);

    single_char:
        // not an identifier, return the character as itself
        return p_lex_token(lexer, *p, p, p);

    case '+':
        if (p + 1 != lexer->eof)
        {
            if (p[1] == '=')
                return p_lex_token(lexer, PLEX_pluseq, p, p + 1);
        }
        goto single_char;
    case '-':
        if (p + 1 != lexer->eof)
        {
            if (p[1] == '=')
                return p_lex_token(lexer, PLEX_minuseq, p, p + 1);
            if (p[1] == '>')
                return p_lex_token(lexer, PLEX_arrow, p, p + 1);
        }
        goto single_char;
    case '&':
        if (p + 1 != lexer->eof)
        {
            if (p[1] == '=')
                return p_lex_token(lexer, PLEX_andeq, p, p + 1);
        }
        goto single_char;
    case '|':
        if (p + 1 != lexer->eof)
        {
            if (p[1] == '=')
                return p_lex_token(lexer, PLEX_oreq, p, p + 1);
        }
        goto single_char;
    case '=':
        if (p + 1 != lexer->eof)
        {
            if (p[1] == '=')
                return p_lex_token(lexer, PLEX_eq, p, p + 1);
            if (p[1] == '>')
                return p_lex_token(lexer, PLEX_eqarrow, p, p + 1);
        }
        goto single_char;
    case '!':
        if (p + 1 != lexer->eof && p[1] == '=')
            return p_lex_token(lexer, PLEX_noteq, p, p + 1);
        goto single_char;
    case '^':
        if (p + 1 != lexer->eof && p[1] == '=')
            return p_lex_token(lexer, PLEX_xoreq, p, p + 1);
        goto single_char;
    case '%':
        if (p + 1 != lexer->eof && p[1] == '=')
            return p_lex_token(lexer, PLEX_modeq, p, p + 1);
        goto single_char;
    case '*':
        if (p + 1 != lexer->eof && p[1] == '=')
            return p_lex_token(lexer, PLEX_muleq, p, p + 1);
        goto single_char;
    case '/':
        if (p + 1 != lexer->eof && p[1] == '=')
            return p_lex_token(lexer, PLEX_diveq, p, p + 1);
        goto single_char;
    case '<':
        if (p + 1 != lexer->eof)
        {
            if (p[1] == '=')
                return p_lex_token(lexer, PLEX_lesseq, p, p + 1);
            if (p[1] == '<')
            {
                if (p + 2 != lexer->eof && p[2] == '=')
                    return p_lex_token(lexer, PLEX_shleq, p, p + 2);
                return p_lex_token(lexer, PLEX_shl, p, p + 1);
            }
        }
        goto single_char;
    case '>':
        if (p + 1 != lexer->eof)
        {
            if (p[1] == '=')
                return p_lex_token(lexer, PLEX_greatereq, p, p + 1);
            if (p[1] == '>')
            {
                if (p + 2 != lexer->eof && p[2] == '=')
                    return p_lex_token(lexer, PLEX_shreq, p, p + 2);
                return p_lex_token(lexer, PLEX_shr, p, p + 1);
            }
        }
        goto single_char;

    case '"':
        return p_lex_parse_string(lexer, p, PLEX_dqstring);
        // goto single_char;
    case '\'':
        return p_lex_parse_string(lexer, p, PLEX_sqstring);

    case '0':
        if (p + 1 != lexer->eof)
        {
            if (p[1] == 'x' || p[1] == 'X')
            {
                char *q;
                {
                    int n = 0;
                    for (q = p + 2; q != lexer->eof; ++q)
                    {
                        if (*q >= '0' && *q <= '9')
                            n = n * 16 + (*q - '0');
                        else if (*q >= 'a' && *q <= 'f')
                            n = n * 16 + (*q - 'a') + 10;
                        else if (*q >= 'A' && *q <= 'F')
                            n = n * 16 + (*q - 'A') + 10;
                        else
                            break;
                    }
                    lexer->int_number = n;
                }
                if (q == p + 2)
                    return p_lex_token(lexer, PLEX_parse_error, p - 2, p - 1);
                return p_lex_parse_suffixes(lexer, PLEX_intlit, p, q);
            }
        }
        // can't test for octal because we might parse '0.0' as float or as '0' '.' '0',
        // so have to do float first

        /* FALL THROUGH */
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        char *q = p;
        while (q != lexer->eof && (*q >= '0' && *q <= '9'))
            ++q;
        if (q != lexer->eof)
        {
            if (*q == '.' || *q == 'e' || *q == 'E')
            {
                lexer->real_number = p_lex_parse_float(p, &q);
                return p_lex_token(lexer, PLEX_floatlit, p, q);
                // return p_lex_parse_suffixes(lexer, PLEX_floatlit, p, q);
            }
        }

        char *q = p;
        int n = 0;
        while (q != lexer->eof)
        {
            if (*q >= '0' && *q <= '9')
                n = n * 10 + (*q - '0');
            else
                break;
            ++q;
        }
        lexer->int_number = n;
        return p_lex_parse_suffixes(lexer, PLEX_intlit, p, q);
        goto single_char;
    }
}

#endif // LEXER_IMPLEMENTATION
