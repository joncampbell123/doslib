
#include <sys/types.h>
#include <sys/stat.h>

#include <time.h>
#include <stdint.h>
#include <endian.h>
#include <dirent.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include "zlib.h"
#include "iconv.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

char*           in_file = NULL;
char*           out_file = NULL;
char*           out_codepage = NULL;

static void help(void) {
    fprintf(stderr,"DOSLIB game string table compiler\n");
    fprintf(stderr,"gstcc -i <input> -o <output> [-e codepage]\n");
    fprintf(stderr,"To get a complete list of code pages see iconv --list\n");
}

void clear_string(char **a) {
    if (*a != NULL) {
        free(*a);
        *a = NULL;
    }
}

char *set_string(char **a,const char *s) {
    clear_string(a);

    if (s != NULL) {
        assert(*a == NULL);
        *a = strdup(s);
    }

    return *a;
}

static int parse(int argc,char **argv) {
    struct stat st;
    char *a;
    int i;

    if (argc <= 1) {
        help();
        return 1;
    }

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"i")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&in_file,a);
            }
            else if (!strcmp(a,"o")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&out_file,a);
            }
            else if (!strcmp(a,"e")) {
                a = argv[i++];
                if (a == NULL) return 1;
                set_string(&out_codepage,a);
            }
            else {
                fprintf(stderr,"Unknown switch %s\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected arg %s\n",a);
            return 1;
        }
    }

    if (in_file == NULL) {
        fprintf(stderr,"Input file -i required\n");
        return 1;
    }
    if (out_file == NULL) {
        fprintf(stderr,"Output file -o required\n");
        return 1;
    }

    if (out_codepage == NULL)
        set_string(&out_codepage,"CP437");

    return 0;
}

int main(int argc,char **argv) {
    if (parse(argc,argv))
        return 1;

    return 0;
}

