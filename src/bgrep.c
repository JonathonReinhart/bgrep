/**
 * bgrep
 * (C) 2013 Jonathon Reinhart
 */

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>


#define APP_NAME         "bgrep"

static bool o_string_input = false;


#define LOWNIB_MASK     0x0F
#define HIGHNIB_MASK    0xF0

typedef enum {
    LITERAL,    // Must match this literal byte
    ANY,        // Can be any byte
    LOWNIB,     // Must match the low nibble of this byte
    HIGHNIB,    // Must match the high nibble of this byte
} bytetype_t;

typedef struct {
    uint8_t     byte;
    bytetype_t  type;
} patbyte_t;

typedef struct {
    patbyte_t*  pattern;
    int         length;
} pattern_t;



static bool
read_file(FILE *f, void **buf, size_t *len)
{
    void *_buf;
    size_t _len;

    if (fseek(f, 0, SEEK_END)) {
        perror("Error seeking file");
        return false;
    }

    _len = ftell(f);

    if (fseek(f, 0, SEEK_SET)) {
        perror("Error seeking file");
        return false;
    }

    if ((_buf = malloc(_len)) == NULL) {
        fprintf(stderr, "Error allocating buffer of %zd bytes\n", _len);
        return false;
    }
    
    if (fread(_buf, 1, _len, f) != _len) {
        fprintf(stderr, "Error reading file\n");
        free(_buf);
        return false;
    }

    *buf = _buf;
    *len = _len;
    return true;
}



// Returns the index or -1 if not found
static ssize_t
find_pattern(const void *buffer, size_t len, size_t offset, const pattern_t *pattern)
{
    const uint8_t *buf = buffer;
    size_t b;   // buffer index
    size_t p;   // pattern index

    if (pattern->length > (len - offset))
        return -1;

    // while there is pattern left to be found,
    // and there is more buf left to search than there is pattern to find
    // 
    // Note: This only works because pattern->length is the number of bytes.
    // If we add variable-length pattern pieces, then this conditional will change.
    for (b = offset, p = 0;
         (p < pattern->length) && (len - b >= pattern->length - p);
         b++)
    {
        const patbyte_t *pb = &pattern->pattern[p];

        switch (pb->type) {
            case LITERAL:
                if (pb->byte == buf[b])
                    p++;
                else
                    p = 0;
                break;

            case ANY:
                p++;
                break;

            case LOWNIB:
                if ((pb->byte & LOWNIB_MASK) == (buf[b] & LOWNIB_MASK))
                    p++;
                else
                    p = 0;
                break;

            case HIGHNIB:
                if ((pb->byte & HIGHNIB_MASK) == (buf[b] & HIGHNIB_MASK))
                    p++;
                else
                    p = 0;
                break;

            default:
                fprintf(stderr, "invalid pb type\n");
                abort();
        }
    }

    if (p == pattern->length)
        return b - p;

    return -1;
}


#define COLOR(c)    "\033[" #c "m"
#define ENDC        COLOR(0)
#define PURPLE      COLOR(35)
#define LTBLUE      COLOR(36)
#define GREEN       COLOR(92)

static bool m_color_enabled = false;

static void
print_match(const char *filename, size_t offset)
{
    if (m_color_enabled)
        printf(PURPLE "%s" LTBLUE ":" GREEN "0x%zX" ENDC "\n", filename, offset);
    else
        printf("%s:0x%zX\n", filename, offset);
}

static bool
bgrep(const char *filename, FILE *f, const pattern_t *pattern)
{
    void *buf;
    size_t len;
    ssize_t offset = 0;
    int matches = 0;

    if (!read_file(f, &buf, &len))
        return false;

    while (true) {
        offset = find_pattern(buf, len, offset, pattern);
        if (offset == -1)
            break;

        matches++;
        print_match(filename, offset);

        offset += pattern->length;  // No overlapping matches
    }

    free(buf);
    return true;
}

static bool
handle_file(const char *filename, const pattern_t *pattern)
{
    FILE *f;
    if (strcmp(filename, "-") == 0) {
        f = stdin;
        filename = "[stdin]";
    }
    else {
        if ((f = fopen(filename, "rb")) == NULL) {
            fprintf(stderr, "Error opening %s: %m\n", filename);
            return false;
        }
    }

    bgrep(filename, f, pattern);

    if (f != stdin) {
        fclose(f);
        f = NULL;
    }
    return true;
}

static void
usage(void)
{
    fprintf(stderr, "Usage: " APP_NAME " pattern file1 ...\n");
    fprintf(stderr, "   pattern:    A hex string like 1234..ABCD.F where . is a wildcard nibble\n");
}

static bool
get_hex_nibble(char letter, uint8_t *byte)
{
    char buf[2];

    if (!isxdigit(letter))
        return false;

    buf[0] = letter;
    buf[1] = '\0';

    *byte = (uint8_t)strtoul(buf, NULL, 16);
    return true;
}

static bool
get_hex_byte(const char *str, uint8_t *byte)
{
    char buf[3];

    if (!isxdigit(str[0]) || !isxdigit(str[1]))
        return false;

    buf[0] = str[0];
    buf[1] = str[1];
    buf[2] = '\0';

    *byte = (uint8_t)strtoul(buf, NULL, 16);
    return true;
}

static void
get_pattern_string(const char *str, pattern_t *pattern)
{
    int len = strlen(str);

    patbyte_t *patdata = calloc(len, sizeof(*patdata));
    if (patdata == NULL) {
        fprintf(stderr, "Failed to allocate patdata of length %d\n", len);
        exit(2);
    }

    int i;
    for (i = 0; i < len; i++) {
        patdata[i].byte = str[i];
        patdata[i].type = LITERAL;
    }

    pattern->pattern = patdata;
    pattern->length = len;
}

static void
get_pattern_normal(const char *str, pattern_t *pattern)
{
    int len = strlen(str);
    if (len % 2 != 0) {
        fprintf(stderr, "Error: pattern must be an even number of characters\n");
        usage();
        exit(1);
    }
    len /= 2;

    patbyte_t *patdata = calloc(len, sizeof(*patdata));
    if (patdata == NULL) {
        fprintf(stderr, "Failed to allocate patdata of length %d\n", len);
        exit(2);
    }

    int i;
    for (i = 0; i < len; i++) {

        if (str[2*i] == '.') {
            // Either '..' (ANY) or '.X' (LOWNIB)

            if (str[2*i + 1] == '.') {
                // '..' ANY
                patdata[i].byte = '\xFF';
                patdata[i].type = ANY;
            }
            else {
                // '.X' LOWNIB
                uint8_t b;
                if (!get_hex_nibble(str[2*i + 1], &b)) {
                    fprintf(stderr, "Error: invalid pattern string\n");
                    usage();
                    exit(1);
                }
                patdata[i].byte = b;
                patdata[i].type = LOWNIB;
            }
        }
        else {
            // Either 'XX' (LITERAL) or 'X.' (HIGHNIB)

            if (str[2*i + 1] == '.') {
                // 'X.' HIGHNIB
                uint8_t b;
                if (!get_hex_nibble(str[2*i], &b)) {
                    fprintf(stderr, "Error: invalid pattern string\n");
                    usage();
                    exit(1);
                }
                patdata[i].byte = b << 4;
                patdata[i].type = HIGHNIB;
            }
            else {
                // 'XX' LITERAL
                uint8_t b;
                if (!get_hex_byte(str+(2*i), &b)) {
                    fprintf(stderr, "Error: invalid pattern string\n");
                    usage();
                    exit(1);
                }
                patdata[i].byte = b;
                patdata[i].type = LITERAL;
            }
        }
    }

    pattern->pattern = patdata;
    pattern->length = len;
}

static void
dump_pattern(const pattern_t *pattern)
{
    int i;
    for (i = 0; i < pattern->length; i++) {
        const patbyte_t *pb = &pattern->pattern[i];

        switch (pb->type)
        {
            case LITERAL:
                fprintf(stderr, "%02X ", pb->byte);
                break;

            case ANY:
                fprintf(stderr, "?? ");
                break;

            case LOWNIB:
                fprintf(stderr, "LN(%02X) ", pb->byte);
                break;

            case HIGHNIB:
                fprintf(stderr, "HN(%02X) ", pb->byte);
                break;

            default:
                fprintf(stderr, "invalid pb type\n");
                abort();
        }
    }
    fprintf(stderr, "\n");
}
               

static void
parse_options(int *argc, char ***argv)
{
    int opt;

    while ((opt = getopt(*argc, *argv, "s")) != -1) {
        switch (opt) {
            case 's':
                o_string_input = true;
                break;
            default: /* '?' */
                usage();
                exit(1);
        }
    }

    *argc -= optind;
    *argv += optind;
}


int main(int argc, char **argv)
{
    pattern_t pattern;

    if (isatty(STDIN_FILENO)) {
        m_color_enabled = true;
    }

    parse_options(&argc, &argv);
    /* argv[0] is now first arg */

    /**
     * NOTE: I initially intended on being able to bgrep stdin, but
     * the current implementation reads the entire file into a buffer,
     * using fseek() to get the length. That's not possible with stdin,
     * so for now we'll just disable this feature.
     */

    if (argc < 2) {
        fprintf(stderr, APP_NAME ": not enough arguments\n");
        usage();
        exit(1);
    }

    if (o_string_input) {
        get_pattern_string(argv[0], &pattern);
    }
    else {
        get_pattern_normal(argv[0], &pattern);
    }

    if (0) {
        dump_pattern(&pattern);
    }

    if (argc == 1) {
        // No filenames given; use stdin
        handle_file("-", &pattern);
    }
    else {
        int a;
        for (a = 1; a < argc; a++) {
            handle_file(argv[a], &pattern);
        }
    }

    return 0;
}

