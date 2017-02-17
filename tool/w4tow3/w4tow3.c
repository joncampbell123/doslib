
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

char*               src_file = NULL;
char*               dst_file = NULL;

int                 src_fd = -1;
int                 dst_fd = -1;

int parse_argv(int argc,char **argv) {
    char *a;
    int i;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                src_file = argv[i++];
                if (src_file == NULL) return 1;
            }
            else if (!strcmp(a,"o")) {
                dst_file = argv[i++];
                if (dst_file == NULL) return 1;
            }
            else {
                fprintf(stderr,"Unknown switch '%s'\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected arg '%s'\n",a);
            return 1;
        }
    }

    if (src_file == NULL) {
        fprintf(stderr,"Need source file, -i switch\n");
        return 1;
    }
    if (dst_file == NULL) {
        fprintf(stderr,"Need output file, -o switch\n");
        return 1;
    }

    return 0;
}

uint32_t*           chunkTable = NULL;
uint32_t            chunkTableEntries = 0;

uint8_t             file_temp[4096];

int main(int argc,char **argv) {
    unsigned char tmp[16];
    uint16_t chunk_size;
    uint16_t num_chunks;
    uint32_t le_offset;
    uint32_t file_size;
    uint32_t start,end;
    size_t i;

    if (parse_argv(argc,argv))
        return 1;

    if ((src_fd=open(src_file,O_RDONLY|O_BINARY)) < 0) {
        fprintf(stderr,"Cannot open source file %s, %s\n",src_file,strerror(errno));
        return 1;
    }
    if ((dst_fd=open(dst_file,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0644)) < 0) {
        fprintf(stderr,"Cannot open dest file %s, %s\n",dst_file,strerror(errno));
        return 1;
    }

    if (lseek(src_fd,0x3C,SEEK_SET) != 0x3C || read(src_fd,&le_offset,4) != 4) {
        fprintf(stderr,"Cannot read\n");
        return 1;
    }
    fprintf(stderr,"W4 offset: %lu\n",(unsigned long)le_offset);

    if (lseek(src_fd,le_offset,SEEK_SET) != le_offset || read(src_fd,tmp,16) != 16) {
        fprintf(stderr,"Cannot read\n");
        return 1;
    }
    if (memcmp(tmp,"W4",2) != 0 || !(*((uint16_t*)(tmp+2)) == 0 || tmp[3] == 0x04)) {
        fprintf(stderr,"Not a W4 file\n");
        return 1;
    }
    if (memcmp(tmp+8,"DS",2) != 0) {
        fprintf(stderr,"Not DoubleSpace compressed\n");
        return 1;
    }
    chunk_size = *((uint16_t*)(tmp+4));
    num_chunks = *((uint16_t*)(tmp+6));
    fprintf(stderr,"Chunk size: %u\n",(unsigned int)chunk_size);
    fprintf(stderr,"Num chunks: %u\n",(unsigned int)num_chunks);

    if (chunk_size != 8192) {
        fprintf(stderr,"Unsupported chunk size\n");
        return 1;
    }
    if (num_chunks == 0 || num_chunks > 1024) {
        fprintf(stderr,"Not the right number of chunks\n");
        return 1;
    }

    // then the chunk table follows
    chunkTableEntries = num_chunks;
    chunkTable = (uint32_t*)malloc(sizeof(uint32_t) * chunkTableEntries);
    if (chunkTable == NULL) {
        fprintf(stderr,"Cannot alloc chunk table\n");
        return 1;
    }
    if ((size_t)read(src_fd,chunkTable,sizeof(uint32_t) * chunkTableEntries) !=
        (sizeof(uint32_t) * chunkTableEntries)) {
        fprintf(stderr,"Unable to read chunk table\n");
        return 1;
    }

    file_size = (uint32_t)lseek(src_fd,0,SEEK_END);

    fprintf(stderr,"Chunk table[%lu] = {",(unsigned long)chunkTableEntries);
    for (i=0;i < (size_t)chunkTableEntries;i++) {
        fprintf(stderr,"%lu",(unsigned long)chunkTable[i]);
        if ((i+1) != chunkTableEntries) {
            fprintf(stderr,", ");
            if ((i&15) == 15) fprintf(stderr,"\n");
        }
    }
    fprintf(stderr,"}\n");
    fprintf(stderr,"File size (and end of last chunk): %lu\n",(unsigned long)file_size);

    // okay, copy source to dest up to and including W4 header.
    lseek(src_fd,0,SEEK_SET);
    lseek(dst_fd,0,SEEK_SET);
    end = le_offset + (size_t)16;
    start = 0;

    while (start < end) {
        size_t cpy = (size_t)end - (size_t)start;
        if (cpy > sizeof(file_temp)) cpy = sizeof(file_temp);

        if ((size_t)read(src_fd,file_temp,cpy) != cpy)
            return 1;
        if ((size_t)write(dst_fd,file_temp,cpy) != cpy)
            return 1;

        start += cpy;
    }

    free(chunkTable);
    close(dst_fd);
    close(src_fd);
    return 0;
}

