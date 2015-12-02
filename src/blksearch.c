#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#define BUFSIZE 128

static void
hexdump(const void* buf, size_t len)
{
    const unsigned char *b = buf;
    size_t i;
    size_t rowoff = 0;

    printf("          0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (rowoff = 0; rowoff < len; rowoff += 16) {
        printf("%08zX  ", rowoff);

        for (i = 0; i < 16 && rowoff + i < len; i++)
            printf("%02X ", b[rowoff + i]);
        for ( ; i < 16; i++)
            printf("   ");

        printf("    ");
        for (i = 0; i < 16 && rowoff + i < len; i++) {
            char c = b[rowoff + i];
            if (!isprint(c) || (isspace(c) && c != ' '))
                c = '.';
            putchar(c);
        }

        putchar('\n');
    }
}


int
file_search(FILE *f, const char *needle, size_t needle_size, int (*fcn)(off_t))
{
    char haystack[BUFSIZE];

    int occur = 0;
    size_t read;

    /* File offset of the data at the beginning of the haystack buffer */
    off_t f_off = 0;


    /**
     *  Prime the buffer with 'needle_size' bytes of data:
     *
     *  |@@@@@@                             |
     *   ^----^
     */
    read = fread(haystack, 1, needle_size, f);
    if (read < needle_size) {
        fprintf(stderr, "Warning: Input data shorter than needle.\n");
        return -1;
    }
    fprintf(stderr, "Primed haystack with 0x%zX bytes.\n", needle_size);


    for (;;) {
        /* If short read, might be less than sizeof(haystack) */

        fprintf(stderr, "File offset = 0x%zX\n", f_off);

        /**
         *  Read new data in, after the copied data:
         *
         *  |xxxxxx@@@@@@@@@@@@@@@@@@@@@@@@@|
         *         ^----    New Data   ----^
         */
        read = fread(haystack + needle_size, 1, sizeof(haystack) - needle_size, f);
        if (read == 0)
            break;

        size_t haystack_size = needle_size + read;
        fprintf(stderr, "Read in 0x%zX bytes at offset 0x%zX into the haystack.\n",
                read, needle_size);
        fprintf(stderr, "Haystack is now 0x%zX bytes.\n", haystack_size);

        /**
         *  Do the serach, up to the last needle_size bytes:
         *
         *  |xxxxxxxxxxxxxxxxxxxxxxxxxxzzzzz|
         *   ^----      Search    ----^
         */
        size_t search_len = haystack_size - needle_size;
        fprintf(stderr, "Search length is 0x%zX - 0x%zX = 0x%zX\n",
                haystack_size, needle_size, search_len);

        fprintf(stderr, "Buffer:\n");
        hexdump(haystack, haystack_size);

        size_t i;
        for (i = 0; i < search_len; i++) {
            if (memcmp(haystack + i, needle, needle_size) == 0) {
                occur++;
                if (fcn(f_off + i) == 0)
                    break;
            }
        }

        /**
         *  Nothing found. Move the last 'needle_size' bytes of the haystack
         *  (which we couldn't search) to the beginning of the buffer.
         *
         *  |                          zzzzz|
         *
         *      ------------------------/
         *     /
         *    v  
         *
         *  |zzzzz                          |
         *
         */
        memmove(haystack, haystack + search_len, needle_size);
        f_off += search_len;

        fprintf(stderr, "Copied 0x%zX bytes from offset 0x%zX to the "
                        "beginning of haystack.\n\n", needle_size, search_len);
    }


    return occur;
}


static int
callback(off_t off)
{
    printf("\n---> Found at offset 0x%zX\n", off);
    return 1;   /* keep searching */
}


int
main(int argc, char **argv)
{
    const char needle[] = "Jonathon";
    size_t needle_len = sizeof(needle) - 1;

    int result = file_search(stdin, needle, needle_len, callback);

    fprintf(stderr, "\n\nResult = 0x%X\n\n", result);

    return 0;
}
