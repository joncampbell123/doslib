
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

unsigned char*      buffer = NULL;                  /* string data */
size_t              buffer_sz = 0;
uint16_t            buffer_string_count = 0;        /* number of strings */
uint16_t            buffer_string_start = 0;        /* first string ID */
uint16_t*           buffer_string_offsets = NULL;   /* offsets of each string. array is count + 1 long, with [count] the size of the buffer */
uint32_t            buffer_codepage = 0;

uint16_t read16le(const unsigned char *x) {
    return      ((uint16_t)x[0]) +
                ((uint16_t)x[1] << (uint32_t)8u);
}

uint32_t read32le(const unsigned char *x) {
    return      ((uint32_t)x[0]) +
                ((uint32_t)x[1] << (uint32_t)8u) +
                ((uint32_t)x[2] << (uint32_t)16u) +
                ((uint32_t)x[3] << (uint32_t)24u);
}

int main(int argc,char **argv) {
    int fd;

    if (argc < 2) {
        fprintf(stderr,"gstdmp <file>\n");
        return 1;
    }

    fd = open(argv[1],O_RDONLY|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Cannot open %s\n",argv[1]);
        return 1;
    }

    {
        unsigned char tmp[12];
        off_t len;

        len = lseek(fd,0,SEEK_END);
        if (len < (off_t)12 || len > (off_t)0xFFF0UL) {
            fprintf(stderr,"File wrong size\n");
            close(fd);
            return 1;
        }
        if (lseek(fd,0,SEEK_SET) != 0 || read(fd,tmp,12) != 12) {
            fprintf(stderr,"Failed to read header\n");
            close(fd);
            return 1;
        }
        if (memcmp(tmp,"ST0U",4)) {
            fprintf(stderr,"Header wrong\n");
            close(fd);
            return 1;
        }

        buffer_codepage = read32le(tmp+4);
        buffer_string_start = read16le(tmp+8);
        buffer_string_count = read16le(tmp+10);

        fprintf(stderr,"Codepage: %lu\n",(unsigned long)buffer_codepage);
        fprintf(stderr,"First string: #%u\n",(unsigned int)buffer_string_start);
        fprintf(stderr,"Number of strings: %u\n",(unsigned int)buffer_string_count);
    }

    close(fd);

    return 0;
}

