/* NTS: Based on "W4DECOMP.C" from the book "Windows Undocumented File Formats",
 *      with some changes to range-check pointers and make sure we're reading
 *      correctly. So far, this seems to read VMM32.VXD in my Windows 95 VM
 *      perfectly fine. */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
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

uint8_t             file_temp[8192]; // also decompression output
uint8_t             src_temp[8192]; // decompression input

void LoadMiniBuffer(uint32_t *pMiniBuffer,unsigned char **psrc,unsigned char *srcfence,uint16_t *pBitsUsed,uint16_t *pBitCount) {
    while ((*pBitsUsed) != 0) {
        (*pBitsUsed)--;
        *pMiniBuffer >>= 1;
        if (--(*pBitCount) == 0) {
            if (*psrc < srcfence) {
                if (*pMiniBuffer & 0xFF000000UL)
                    fprintf(stderr,"minibuffer upper 8 bits != 0\n");

                *pMiniBuffer += (uint32_t)(**psrc) << (uint32_t)24U;
                (*psrc) += 1U;
            }
            *pBitCount += 8U;
        }
    }
}

uint32_t W4Decompress(unsigned char *dst,size_t dstmax,unsigned char *src,size_t srclen) {
    unsigned char *srcfence = src + srclen;
    unsigned char *dstfence = dst + dstmax;
    unsigned char *dstbase = dst;
    uint32_t minibuffer = 0;
    uint16_t nCount, nDepth;
    uint16_t nBitsUsed = 0;
    uint16_t nBitCount;
    size_t nIndex = 0;

    nDepth = 1;
    nBitCount = 8;
    for (nIndex=0;nIndex <= 3;nIndex++)
        minibuffer = (minibuffer >> (uint32_t)8) + (((uint32_t)(*src++)) << (uint32_t)24);

    while (nDepth != 0) {
        LoadMiniBuffer(&minibuffer,&src,srcfence,&nBitsUsed,&nBitCount);

        if ((minibuffer & 3) == 1 ||
            (minibuffer & 3) == 2) {
            if (dst >= dstfence) break;
            *dst++ = (unsigned char)(((minibuffer & 0x1FCU) >> 2U) + ((minibuffer & 1U) << 7U));
            nBitsUsed = 9;
        }
        else {
            // 0-63
            if ((minibuffer & 3U) == 0U) {
                nDepth = (minibuffer & 0xFCU) >> 2U;
                nBitsUsed = 8;
            }
            // 64-319
            else if ((minibuffer & 7U) == 3U) {
                nDepth = ((minibuffer & 0x7F8U) >> 3U) + 0x40U;
                nBitsUsed = 11;
            }
            // 320-4414
            else if ((minibuffer & 7U) == 7U) {
                nDepth = ((minibuffer & 0x7FF8U) >> 3U) + 0x140U;
                nBitsUsed = 15;
            }
            else {
                fprintf(stderr,"Invalid depth data\n");
                break;
            }

            // if not zero and not CheckBuffer
            if (nDepth != 0 && nDepth != 0x113FU) { // 0x113FU == (4415 - 320)
                LoadMiniBuffer(&minibuffer,&src,srcfence,&nBitsUsed,&nBitCount);

                if ((minibuffer & 1) == 1) { // 2
                    nCount = 2;
                    nBitsUsed = 1;
                }
                else if ((minibuffer & 3) == 2) { // 3-4
                    nCount = ((minibuffer & 4U) >> 2U) + 3U;
                    nBitsUsed = 3;
                }
                else if ((minibuffer & 7) == 4) { // 5-8
                    nCount = ((minibuffer & 0x18U) >> 3U) + 5U;
                    nBitsUsed = 5;
                }
                else if ((minibuffer & 0x0FU) == 8) { // 9-16
                    nCount = ((minibuffer & 0x70U) >> 4U) + 9U;
                    nBitsUsed = 7;
                }
                else if ((minibuffer & 0x1FU) == 16) { // 17-32
                    nCount = ((minibuffer & 0x1E0U) >> 5U) + 17U;
                    nBitsUsed = 9;
                }
                else if ((minibuffer & 0x3FU) == 32) { // 33-64
                    nCount = ((minibuffer & 0x7C0U) >> 6U) + 33U;
                    nBitsUsed = 11;
                }
                else if ((minibuffer & 0x7FU) == 64) { // 65-128
                    nCount = ((minibuffer & 0x1F80U) >> 7U) + 65U;
                    nBitsUsed = 13;
                }
                else if ((minibuffer & 0xFFU) == 128) { // 129-256
                    nCount = ((minibuffer & 0x7F00U) >> 8U) + 129U;
                    nBitsUsed = 15;
                }
                else if ((minibuffer & 0x1FFU) == 256) { // 257-512
                    nCount = ((minibuffer & 0x1FE00U) >> 9U) + 257U;
                    nBitsUsed = 17;
                }
                else {
                    fprintf(stderr,"Unexpected compressed code=0x%08lx\n",(unsigned long)minibuffer);
                    break;
                }

                {
                    unsigned char *sp = dst - nDepth;

                    if (sp < dstbase) {
                        fprintf(stderr,"Unexpected nDepth too large, reaches back too far\n");
                        break;
                    }
                    if ((dst+nCount) > dstfence) {
                        fprintf(stderr,"Unexpected nCount too large, reaches too far forward into dest\n");
                        break;
                    }

                    assert(nCount != 0);

                    do {  *dst++ = *sp++;
                    } while (--nCount != 0);

                    assert(sp >= dstbase);
                    assert(sp <= dstfence);
                    assert(dst <= dstfence);
                }
            }
            else if (nDepth == 0x113FU && src == srcfence) {
                break;
            }
        }
    }

    if (src < srcfence)
        fprintf(stderr,"Warning: %u bytes left\n",(unsigned int)(srcfence - src));

    return (uint32_t)(dst - dstbase);
}

int main(int argc,char **argv) {
    unsigned char tmp[16];
    uint16_t chunk_size;
    uint16_t num_chunks;
    uint32_t le_offset;
    uint32_t file_size;
    uint32_t start,end;
    size_t i,chunk;

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

    // okay, copy source to dest up to W4 header
    lseek(src_fd,0,SEEK_SET);
    lseek(dst_fd,0,SEEK_SET);
    end = le_offset;
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

    for (chunk=0;chunk < num_chunks;chunk++) {
        start = chunkTable[chunk];
        if ((chunk+1) == num_chunks)
            end = file_size;
        else
            end = chunkTable[chunk+1];

        if (start >= end)
            return 1;
        if ((start+chunk_size) < end)
            return 1;

        fprintf(stderr,"Decompressing chunk %lu/%lu: src sz=%lu\n",
            (unsigned long)chunk,(unsigned long)num_chunks - 1UL,(unsigned long)(end-start));

        if ((uint32_t)lseek(src_fd,start,SEEK_SET) != start)
            return 1;
        if ((size_t)read(src_fd,src_temp,(size_t)(end-start)) != (size_t)(end-start))
            return 1;

        if ((start+chunk_size) != end) {
            uint32_t dstsz;

            memset(file_temp,0xE5,chunk_size);
            dstsz = W4Decompress(file_temp,chunk_size,src_temp,(size_t)(end-start));
            fprintf(stderr,"  Output: %lu\n",(unsigned long)dstsz);
            if (dstsz == 0) return 1;

            write(dst_fd,file_temp,dstsz);
        }
        else {
            /* TODO: When does this happen? */
            write(dst_fd,src_temp,chunk_size);
        }
    }

    free(chunkTable);
    close(dst_fd);
    close(src_fd);
    return 0;
}

