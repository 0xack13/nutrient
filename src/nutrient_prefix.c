#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>

#include "nutrient.h"

void usage()
{
    fprintf(stderr, "usage: nutrient_prefix prefix-file db-file\n"); 
    _exit(1);
}

void error(const char * message)
{
    fprintf(stderr, "nutrient_prefix error: %s\n", message);
    _exit(1);
}

int main(int argc, char * argv[])
{
    if (argc != 3)
        usage();

    struct nutrient_tree * tree = nutrient_open(argv[2]);
    if (tree == NULL)
    {
        usage();
    }

    if (strcmp(argv[1], "-") == 0) {
        argv[1] = "/dev/stdin";
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        usage();
    }

    char c;
    uint8_t * key_data = NULL; 

    for (;;) {
        uint32_t key_len = 0;

        /* Read the key length */
        do {
            int r = read(fd, &c, 1);
            if (r == 0) {
                break;
            }
            if (r != 1) {
                error("Couldn't read key length");
            }

            if (!isdigit(c)) {
                if (c != ':') {
                    error("Malformed key/value pair");
                }
        
                break;
            }

            key_len *= 10;
            key_len += c - '0';
        } while (1);

        if (key_len == 0) {
            break;
        }

        /* Read the key data */
        key_data = realloc(key_data, key_len);
        if (key_data == NULL) {
            error("Out of memory");
        }
        if (read(fd, key_data, key_len) < key_len)
        {
            error("Couldn't read key data");
        }

        /* Read the EOL '->' */
        if (read(fd, &c, 1) < 1)
        {
            error("Couldn't read EOL");
        }
        if (c != '\n') 
        {
            error("Malformed key line");
        }

        /* Search for the data */
        const uint8_t * value_data;
        uint32_t value_len;
        const uint8_t * prefix;
        uint32_t prefix_len;
    
        int found = nutrient_find_longest_prefix(tree, key_data, key_len, &prefix, &prefix_len, &value_data, &value_len);
        if (found != 0)
        {
            value_len = 0;
        }

        printf("+%" PRIu32 ",%" PRIu32 ":", key_len, value_len);
        fwrite(key_data, key_len, 1, stdout);
        printf("->");
        fwrite(value_data, value_len, 1, stdout);
        printf("\n");
    }

    nutrient_sync(tree);
    nutrient_close(tree);

    return 0;
}


