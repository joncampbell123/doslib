
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
uint16_t            buffer_string_count = 0;        /* number of strings */
uint16_t            buffer_string_start = 0;        /* first string ID */
uint16_t*           buffer_string_offsets = NULL;   /* offsets of each string. array is count + 1 long, with [count] the size of the buffer */
uint32_t            buffer_codepage = 0;

unsigned int buffer_size(void) {
    if (buffer_string_offsets != NULL)
        return buffer_string_offsets[buffer_string_count];

    return 0;
}

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

size_t get_string_rel(char **p,unsigned int i) {
    if (i < buffer_string_count) {
        *p = (char*)buffer + buffer_string_offsets[i];
        return buffer_string_offsets[i+1] - buffer_string_offsets[i];
    }

    *p = (char*)buffer;
    return 0;
}

char out_tmp[4096];

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
        unsigned long ofs;
        unsigned int i;
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

        len -= (off_t)12;
        buffer_codepage = read32le(tmp+4);
        buffer_string_start = read16le(tmp+8);
        buffer_string_count = read16le(tmp+10);

        fprintf(stderr,"Codepage: %lu\n",(unsigned long)buffer_codepage);
        fprintf(stderr,"First string: #%u\n",(unsigned int)buffer_string_start);
        fprintf(stderr,"Number of strings: %u\n",(unsigned int)buffer_string_count);

        if (buffer_string_start == 0) {
            fprintf(stderr,"First string cannot be zero\n");
            close(fd);
            return 1;
        }
        if (buffer_string_count == 0) {
            fprintf(stderr,"Count cannot be zero\n");
            close(fd);
            return 1;
        }
        if (((uint32_t)buffer_string_count + (uint32_t)buffer_string_start) > (uint32_t)0x1FFFul) {
            fprintf(stderr,"Start+count exceeds 8k\n");
            close(fd);
            return 1;
        }

        /* 8K string count limit also prevents integer overflow when multiplied by 2 for alloc. */
        /* One extra element is allocated so the last contains the total size and to simplify lookup code. */

        buffer_string_offsets = (uint16_t*)malloc((buffer_string_count + 1u) * 2u);
        if (buffer_string_offsets == NULL) {
            fprintf(stderr,"Cannot alloc buffer string offsets\n");
            close(fd);
            return 1;
        }
        len -= (off_t)(buffer_string_count * 2u);
        if (len < 0) {
            fprintf(stderr,"Cannot read string buffer sizes (len < 0)\n");
            close(fd);
            return 1;
        }
        if (read(fd,buffer_string_offsets,buffer_string_count * 2u) != (buffer_string_count * 2u)) {
            fprintf(stderr,"Cannot read string buffer sizes\n");
            close(fd);
            return 1;
        }

        ofs = 0;
        for (i=0;i < buffer_string_count;i++) {
            uint16_t count = read16le((unsigned char*)(&buffer_string_offsets[i]));
            buffer_string_offsets[i] = (uint16_t)ofs;
            ofs += count;
            if (ofs >= 0xFFF0ul) {
                fprintf(stderr,"String count exceeds 64KB\n");
                close(fd);
                return 1;
            }
        }
        assert(i == buffer_string_count);
        buffer_string_offsets[i] = (uint16_t)ofs;

        len -= (off_t)ofs;
        if (len < 0) {
            fprintf(stderr,"String total exceeds file\n");
            close(fd);
            return 1;
        }
        if (len > 0) {
            fprintf(stderr,"WARNING: %lu extra bytes\n",len);
        }

        fprintf(stderr,"Buffer size: %u\n",buffer_size());
    }

    {
        unsigned int sz = buffer_size();

        if (sz != 0)
            buffer = malloc(sz+1);
        else
            buffer = malloc(1);

        if (buffer == NULL) {
            fprintf(stderr,"Cannot alloc buffer\n");
            close(fd);
            return 1;
        }

        buffer[sz] = 0;
        if (sz != 0) {
            if (read(fd,buffer,sz) != sz) {
                fprintf(stderr,"Failed to read to buffer\n");
                close(fd);
                return 1;
            }
        }
    }
    close(fd);

    {
        unsigned int i;
        size_t len;
        char *p;

        for (i=0;i < buffer_string_count;i++) {
            len = get_string_rel(&p,i);

            printf("#%d: ",i+buffer_string_start);
            fwrite(p,len,1,stdout);
            printf("\n");
        }
    }

    return 0;
}

