/**
 * bgrep.c
 *
 * Copyright 2013 Jonathon Reinhart
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef HAS_MMAP
#include <sys/mman.h>
#include <fcntl.h>
#endif


#define APP_NAME            "bgrep"
#define APP_VERSION         "0.4.0"

static void
usage(void)
{
    fprintf(stderr, "Usage: " APP_NAME " [options] pattern file1 ...\n");
    fprintf(stderr, "   pattern:    A hex string like 1234..ABCD.F where . is a wildcard nibble\n"
                    "               unless -s is specified\n");
    fprintf(stderr, "Options:\n"
                    "   -c          show context\n"
                    "   -r          handle directories recursively\n"
                    "   -s          pattern specified using ASCII string instead of hex\n"
                    "   -v          print version and exit\n");
}

static bool o_show_context = false;
static bool o_string_input = false;
static bool o_recursive = false;


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

struct filebuf
{
    const void *buf;
    size_t len;
#ifdef HAS_MMAP
    int fd;
#else
    FILE *f;
#endif
};


static bool
handle_file(const char *filename, const pattern_t *pattern);


#ifdef HAS_MMAP

static void
release_file(struct filebuf *fb)
{
    if (fb->buf) {
        munmap((void*)fb->buf, fb->len);
        fb->buf = NULL;
        fb->len = 0;
    }

    if (fb->fd >= 0) {
        close(fb->fd);
        fb->fd = -1;
    }
}

static bool
read_file(const char *path, struct filebuf *fb)
{
    struct stat st;

    /* Open file */
    if ((fb->fd = open(path, O_RDONLY)) < 0) {
        perror("Error opening file");
        goto fail;
    }

    /* Get file length */
    if (fstat(fb->fd, &st) < 0) {
        perror("Error stating file");
        goto fail;
    }
    fb->len = st.st_size;

    if (fb->len == 0)
        goto fail;

    /* Map file into memory */
    if ((fb->buf = mmap(NULL, fb->len, PROT_READ, MAP_PRIVATE, fb->fd, 0)) == MAP_FAILED) {
        perror("Error mmaping file");
        goto fail;
    }

    return true;

fail:
    release_file(fb);
    return false;
}

#else /* HAS_MMAP */

static void
release_file(struct filebuf *fb)
{
    if (fb->buf) {
        free((void*)fb->buf);
        fb->buf = NULL;
        fb->len = 0;
    }

    if (fb->f) {
        fclose(fb->f);
        fb->f = NULL;
    }
}

static bool
read_file(const char *path, struct filebuf *fb)
{
    FILE *f;
    void *buf;
    size_t len;

    /* Open file */
    if ((f = fopen(path, "rb")) == NULL) {
        perror("Error opening file");
        return false;
    }

    /* Get file length */
    if (fseek(f, 0, SEEK_END)) {
        perror("Error seeking file");
        return false;
    }

    len = ftell(f);

    if (fseek(f, 0, SEEK_SET)) {
        perror("Error seeking file");
        return false;
    }

    /* Allocate buffer */
    if ((buf = malloc(len)) == NULL) {
        fprintf(stderr, "Error allocating buffer of %zd bytes\n", len);
        return false;
    }

    /* Read entire file */
    if (fread(buf, 1, len, f) != len) {
        fprintf(stderr, "Error reading file\n");
        free(buf);
        return false;
    }


    fb->f = f;
    fb->buf = buf;
    fb->len = len;
    return true;
}

#endif /* HAS_MMAP */




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
         /* no increment */ )
    {
        const patbyte_t *pb = &pattern->pattern[p];

        switch (pb->type) {
            case LITERAL:
                if (pb->byte == buf[b])
                    goto matched;
                break;

            case ANY:
                goto matched;

            case LOWNIB:
                if ((pb->byte & LOWNIB_MASK) == (buf[b] & LOWNIB_MASK))
                    goto matched;
                break;

            case HIGHNIB:
                if ((pb->byte & HIGHNIB_MASK) == (buf[b] & HIGHNIB_MASK))
                    goto matched;
                break;

            default:
                fprintf(stderr, "invalid pb type\n");
                abort();
        }

        /* no match */
        if (p > 0) {
            /* restart pattern, and try the same input byte again */
            p = 0;
        }
        else {
            /* move to next input byte */
            b++;
        }
        continue;

    matched:
        /* matched; advance */
        p++;
        b++;
        continue;

    }

    if (p == pattern->length)
        return b - p;

    return -1;
}


#define COLOR(c)    "\033[" #c "m"
#define ENDC        COLOR(0)
#define GREY        COLOR(38;5;238)
#define RED         COLOR(31)
#define PURPLE      COLOR(35)
#define LTBLUE      COLOR(36)
#define GREEN       COLOR(92)

static const char *color_filename = PURPLE;
static const char *color_offset = GREEN;
static const char *color_hexaddr = GREY;
static const char *color_match = RED;
static const char *color_end = ENDC;

static void
disable_color(void)
{
    color_filename = "";
    color_offset = "";
    color_hexaddr = "";
    color_match = "";
    color_end = "";
}

static void
print_match(const char *filename, const struct filebuf *fb, size_t offset, int pat_len)
{
    const unsigned char *buf = fb->buf;
    size_t buf_len = fb->len;

    printf("%s%s%s:%s0x%zX%s",
            color_filename, filename, color_end,
            color_offset, offset, color_end);

    if (o_show_context) {
        size_t start_offset = offset & ~0xF;
        size_t i;
        printf("  %s0x%zX:%s ", color_hexaddr, start_offset, color_end);
        for (i = start_offset; i < buf_len && i < start_offset + 0x10; i++) {
            bool in_match = (i >= offset) && (i < offset + pat_len);
            printf("%s%02hhX%s ",
                    in_match ? color_match : "",
                    buf[i],
                    in_match ? color_end : "");
        }
    }

    printf("\n");
}

static bool
bgrep(const char *filename, const struct filebuf *fb, const pattern_t *pattern)
{
    ssize_t offset = 0;
    unsigned int matches = 0;

    while (true) {
        offset = find_pattern(fb->buf, fb->len, offset, pattern);
        if (offset == -1)
            break;

        matches++;
        print_match(filename, fb, offset, pattern->length);

        offset += pattern->length;  // No overlapping matches
    }

    return matches > 0;
}

static bool
is_dir(const char *filename)
{
    struct stat st;

    if (stat(filename, &st) < 0)
        return false;

    return S_ISDIR(st.st_mode);
}

static bool
copy_str(char **dst, size_t *dstlen, const char *src)
{
    char * const end = *dst + *dstlen;

    while (*src) {
        if (*dst >= end)
            return false;

        *(*dst)++ = *src++;
        (*dstlen)--;
    }

    if (*dst >= end)
        return false;

    **dst = '\0';

    return true;
}

#define PATH_SEP    '/'

static bool
path_join(char *buf, size_t len, const char *path1, const char *path2)
{
    if (!copy_str(&buf, &len, path1))
        return false;

    if ((buf[-1] != PATH_SEP) && (path2[0] != PATH_SEP)) {
        const char sep[] = { PATH_SEP, '\0' };
        if (!copy_str(&buf, &len, sep))
            return false;
    }


    if (!copy_str(&buf, &len, path2))
        return false;

    return true;
}

static bool
handle_directory(const char *path, const pattern_t *pattern)
{
    bool result = false;
    DIR *dp;
    struct dirent *ent;

    if ((dp = opendir(path)) == NULL) {
        fprintf(stderr, "Failed to open directory %s: %m\n", path);
        return false;
    }

    while ((ent = readdir(dp)) != NULL) {
        const char *name = ent->d_name;
        char path2[512];

        if ((name[0] == '.') && (name[1] == '\0' || name[1] == '.'))
            continue;

        if (!path_join(path2, sizeof(path2), path, name)) {
            fprintf(stderr, "Path too long\n");
            continue;
        }


        if (handle_file(path2, pattern))
            result = true;
    }

    closedir(dp);
    return result;
}

static bool
handle_file(const char *filename, const pattern_t *pattern)
{
    bool result;
    struct filebuf fb;

    if (is_dir(filename)) {
        if (o_recursive)
            return handle_directory(filename, pattern);

        fprintf(stderr, "Ignoring directory: %s\n", filename);
        return false;
    }

    if (!read_file(filename, &fb))
        return false;

    result = bgrep(filename, &fb, pattern);

    release_file(&fb);

    return result;
}


static bool
get_hex_nibble(char letter, uint8_t *byte)
{
    char buf[] = { letter, '\0' };

    if (!isxdigit((unsigned char)letter))
        return false;

    *byte = (uint8_t)strtoul(buf, NULL, 16);
    return true;
}

static bool
get_hex_byte(const char *str, uint8_t *byte)
{
    char buf[] = { str[0], str[1], '\0' };

    if (!isxdigit((unsigned char)str[0]) || !isxdigit((unsigned char)str[1]))
        return false;

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
show_pattern_error(const char *str, int err_off, int err_len, const char *err_msg_fmt, ...)
{
    int i;
    va_list ap;

    va_start(ap, err_msg_fmt);
    fprintf(stderr, "Error: invalid pattern: ");
    vfprintf(stderr, err_msg_fmt, ap);
    va_end(ap);
    fputc('\n', stderr);

    fprintf(stderr, "    %s\n", str);

    fprintf(stderr, "    %*s", err_off, "");
    for (i=0; i<err_len; i++)
        fputc('^', stderr);
    fputc('\n', stderr);

    //usage();
    exit(1);
}

static void
get_pattern_normal(const char *str, pattern_t *pattern)
{
    int len = strlen(str);
    if (len % 2 != 0) {
        fprintf(stderr, "Error: pattern must be an even number of characters\n");
        //usage();
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
        const char *s = &str[2*i];

        if (s[0] == '.') {
            // Either '..' (ANY) or '.X' (LOWNIB)

            if (s[1] == '.') {
                // '..' ANY
                patdata[i].byte = '\xFF';
                patdata[i].type = ANY;
            }
            else {
                // '.X' LOWNIB
                uint8_t b;
                if (!get_hex_nibble(s[1], &b)) {
                    show_pattern_error(str, 2*i + 1, 1, "Invalid hex character '%c'", s[1]);
                }
                patdata[i].byte = b;
                patdata[i].type = LOWNIB;
            }
        }
        else {
            // Either 'XX' (LITERAL) or 'X.' (HIGHNIB)

            if (s[1] == '.') {
                // 'X.' HIGHNIB
                uint8_t b;
                if (!get_hex_nibble(s[0], &b)) {
                    show_pattern_error(str, 2*i, 1, "Invalid hex character '%c'", s[0]);
                }
                patdata[i].byte = b << 4;
                patdata[i].type = HIGHNIB;
            }
            else {
                // 'XX' LITERAL
                uint8_t b;
                if (!get_hex_byte(s, &b)) {
                    show_pattern_error(str, 2*i, 2, "Invalid hex byte '%.2s'", s);
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
version(void)
{
    printf(APP_NAME " version " APP_VERSION "\n");
}

static void
parse_options(int *argc, char ***argv)
{
    int opt;

    while ((opt = getopt(*argc, *argv, "crsv")) != -1) {
        switch (opt) {
            case 'c':
                o_show_context = true;
                break;
            case 'r':
                o_recursive = true;
                break;
            case 's':
                o_string_input = true;
                break;
            case 'v':
                version();
                exit(0);
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
    int retcode = 1;
    pattern_t pattern;
    int a;

    if (!isatty(STDOUT_FILENO)) {
        disable_color();
    }

    parse_options(&argc, &argv);
    /* argv[0] is now first arg */

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

    for (a = 1; a < argc; a++) {
        if (handle_file(argv[a], &pattern))
            retcode = 0;
    }

    return retcode;
}

