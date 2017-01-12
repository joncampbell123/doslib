/* This doesn't work.
 * The "32-bit CRC" mentioned is not a standard CRC-32 algorithm anyone knows.. */

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>

/**
 * \file x.h
 * Functions and types for CRC checks.
 *
 * Generated on Thu Jan 12 02:23:26 2017,
 * by pycrc v0.9, https://pycrc.org
 * using the configuration:
 *    Width         = 32
 *    Poly          = 0x04c11db7
 *    Xor_In        = 0xffffffff
 *    ReflectIn     = True
 *    Xor_Out       = 0xffffffff
 *    ReflectOut    = True
 *    Algorithm     = bit-by-bit
 *****************************************************************************/
#ifndef __X_H__
#define __X_H__

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * The definition of the used algorithm.
 *
 * This is not used anywhere in the generated code, but it may be used by the
 * application code to call algoritm-specific code, is desired.
 *****************************************************************************/
#define CRC_ALGO_BIT_BY_BIT 1


/**
 * The type of the CRC values.
 *
 * This type must be big enough to contain at least 32 bits.
 *****************************************************************************/
typedef uint_fast32_t crc_t;


/**
 * Calculate the initial crc value.
 *
 * \return     The initial crc value.
 *****************************************************************************/
static inline crc_t crc_init(void)
{
    return 0x46af6449;
}


/**
 * Update the crc value with new data.
 *
 * \param crc      The current crc value.
 * \param data     Pointer to a buffer of \a data_len bytes.
 * \param data_len Number of bytes in the \a data buffer.
 * \return         The updated crc value.
 *****************************************************************************/
crc_t crc_update(crc_t crc, const void *data, size_t data_len);


/**
 * Calculate the final crc value.
 *
 * \param crc  The current crc value.
 * \return     The final crc value.
 *****************************************************************************/
crc_t crc_finalize(crc_t crc);


#ifdef __cplusplus
}           /* closing brace for extern "C" */
#endif

#endif      /* __X_H__ */
/**
 * \file x.c
 * Functions and types for CRC checks.
 *
 * Generated on Thu Jan 12 02:23:24 2017,
 * by pycrc v0.9, https://pycrc.org
 * using the configuration:
 *    Width         = 32
 *    Poly          = 0x04c11db7
 *    Xor_In        = 0xffffffff
 *    ReflectIn     = True
 *    Xor_Out       = 0xffffffff
 *    ReflectOut    = True
 *    Algorithm     = bit-by-bit
 *****************************************************************************/
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

static crc_t crc_reflect(crc_t data, size_t data_len);

/**
 * Reflect all bits of a \a data word of \a data_len bytes.
 *
 * \param data         The data word to be reflected.
 * \param data_len     The width of \a data expressed in number of bits.
 * \return             The reflected data.
 *****************************************************************************/
crc_t crc_reflect(crc_t data, size_t data_len)
{
    unsigned int i;
    crc_t ret;

    ret = data & 0x01;
    for (i = 1; i < data_len; i++) {
        data >>= 1;
        ret = (ret << 1) | (data & 0x01);
    }
    return ret;
}


/**
 * Update the crc value with new data.
 *
 * \param crc      The current crc value.
 * \param data     Pointer to a buffer of \a data_len bytes.
 * \param data_len Number of bytes in the \a data buffer.
 * \return         The updated crc value.
 *****************************************************************************/
crc_t crc_update(crc_t crc, const void *data, size_t data_len)
{
    const unsigned char *d = (const unsigned char *)data;
    unsigned int i;
    bool bit;
    unsigned char c;

    while (data_len--) {
        c = crc_reflect(*d++, 8);
        for (i = 0; i < 8; i++) {
            bit = crc & 0x80000000;
            crc = (crc << 1) | ((c >> (7 - i)) & 0x01);
            if (bit) {
                crc ^= 0x04c11db7;
            }
        }
        crc &= 0xffffffff;
    }
    return crc & 0xffffffff;
}


/**
 * Calculate the final crc value.
 *
 * \param crc  The current crc value.
 * \return     The final crc value.
 *****************************************************************************/
crc_t crc_finalize(crc_t crc)
{
    unsigned int i;
    bool bit;

    for (i = 0; i < 32; i++) {
        bit = crc & 0x80000000;
        crc = (crc << 1) | 0x00;
        if (bit) {
            crc ^= 0x04c11db7;
        }
    }
    crc = crc_reflect(crc, 32);
    return (crc ^ 0xffffffff) & 0xffffffff;
}

#ifndef O_BINARY
#define O_BINARY (0)
#endif

int main(int argc,char **argv) {
    unsigned long orig_crc;
    unsigned char tmp[512];
    unsigned long ne_ofs;
    unsigned long count;
    crc_t crcval;
    int file_fd;
    int out_fd;
    int rd;

    if (argc < 2) {
        fprintf(stderr,"Please enter a file path argv[1]\n");
        return 1;
    }

    file_fd = open(argv[1],O_RDWR | O_BINARY);
    if (file_fd < 0) {
        fprintf(stderr,"Failed to open file %s\n",argv[1]);
        return 1;
    }

    /* locate NE header */
    if (lseek(file_fd,0x3C,SEEK_SET) != 0x3C ||
        read(file_fd,tmp,4) != 4) {
        fprintf(stderr,"Cannot read 0x1C\n");
        return 1;
    }
    ne_ofs = *((uint32_t*)tmp);
    if ((unsigned long)lseek(file_fd,ne_ofs,SEEK_SET) != ne_ofs ||
        read(file_fd,tmp,2) != 2 || memcmp(tmp,"NE",2) != 0) {
        fprintf(stderr,"Cannot find NE header\n");
        return 1;
    }
    fprintf(stderr,"NE header offset: %lu\n",ne_ofs);

    /* run through the file, CRC-32 it, compare and update checksum */
    if (lseek(file_fd,0,SEEK_SET) != 0)
        return 1;

    out_fd = open("x.bmp",O_WRONLY|O_CREAT|O_TRUNC,0644);

    count = 0;
    crcval = crc_init();

    while (count < ne_ofs) {
        int todo;

        if ((ne_ofs - count) > sizeof(tmp))
            todo = (int)sizeof(tmp);
        else
            todo = (int)(ne_ofs - count);

        if ((rd=read(file_fd,tmp,todo)) != todo)
            return 1;

        write(out_fd,tmp,rd); /*DEBUG*/

        crcval = crc_update(crcval,tmp,(size_t)rd);
        count += (unsigned long)rd;
    }

    assert(count == ne_ofs);

    /* blanked CRC field */
    {
        if ((rd=read(file_fd,tmp,16)) != 16)
            return 1;

        orig_crc = *((uint32_t*)(tmp+8));
        memset(tmp+8,0,4); /* zero out CRC field (NE header + 8, 4 bytes) */

        write(out_fd,tmp,rd); /*DEBUG*/

        crcval = crc_update(crcval,tmp,(size_t)rd);
        count += (unsigned long)rd;
    }

    while (1) {
        if ((rd=read(file_fd,tmp,sizeof(tmp))) <= 0)
            break;

        write(out_fd,tmp,rd); /*DEBUG*/

        crcval = crc_update(crcval,tmp,(size_t)rd);
        count += (unsigned long)rd;
    }

    crcval = crc_finalize(crcval);

    fprintf(stderr,"CRC-32: orig=0x%08lx new=0x%08lx\n",
        (unsigned long)orig_crc,
        (unsigned long)crcval);

    close(file_fd);
    return 0;
}

