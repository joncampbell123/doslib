
#include <stdio.h>
#if defined(TARGET_MSDOS)
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#if defined(TARGET_MSDOS)
#include <dos.h>
#endif

#if defined(TARGET_MSDOS)
#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#endif

#if defined(TARGET_MSDOS)
#include <ext/zlib/zlib.h>
#else
#include <zlib.h>
#endif

#ifndef O_BINARY
#define O_BINARY (0)
#endif

#include <fmt/minipng/minipng.h>

/* PNG signature [https://www.w3.org/TR/PNG/#5PNG-file-signature] */
const uint8_t minipng_sig[8] = { 137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u };

struct minipng_reader *minipng_reader_open(const char *path) {
    struct minipng_reader *rdr = NULL;
    unsigned char tmp[8]; /* keep this small, you never know if we're going to end up in a 16-bit DOS app with a very small stack */
    int fd = -1;

    /* open the file */
    fd = open(path,O_RDONLY | O_BINARY);
    if (fd < 0) goto fail;

    /* check PNG signature */
    if (read(fd,tmp,8) != 8) goto fail;
    if (memcmp(tmp,minipng_sig,8)) goto fail;

    /* good. Point at the first one (usually IHDR) */
    rdr = calloc(1,sizeof(*rdr)); /* calloc() zeros the memory */
    if (rdr == NULL) goto fail;
    rdr->fd = fd;
    rdr->err_msg = NULL;
    rdr->chunk_data_offset = -1;
    rdr->next_chunk_start = 8;

    /* success */
    return rdr;

fail:
    if (rdr != NULL) free(rdr);
    if (fd >= 0) close(fd);
    return NULL;
}

int minipng_reader_next_chunk(struct minipng_reader *rdr) {
    unsigned char tmp[8]; /* keep this small, you never know if we're going to end up in a 16-bit DOS app with a very small stack */

    if (rdr == NULL) return -1;
    if (rdr->fd < 0) return -1;
    if (rdr->next_chunk_start < (off_t)-1) return -1;
    if (rdr->ungetch > 0) {
        rdr->ungetch--;
        return 0;
    }

    if (lseek(rdr->fd,rdr->next_chunk_start,SEEK_SET) != rdr->next_chunk_start || read(rdr->fd,tmp,8) != 8) {
        rdr->next_chunk_start = -1;
        return -1;
    }

    rdr->chunk_data_header.type = *((uint32_t*)(tmp + 4));
    rdr->chunk_data_offset = rdr->next_chunk_start + (off_t)8ul;
    rdr->chunk_data_header.length = minipng_byteswap32( *((uint32_t*)(tmp + 0)) );
    rdr->next_chunk_start = rdr->chunk_data_offset + (off_t)rdr->chunk_data_header.length + (off_t)4ul/*CRC field*/;

    /* file pointer left at chunk data for caller if interested */

    return 0;
}

void minipng_reader_close(struct minipng_reader **rdr) {
    if (*rdr != NULL) {
        if ((*rdr)->compr_zlib.next_in != NULL) inflateEnd(&((*rdr)->compr_zlib));
        if ((*rdr)->compr != NULL) free((*rdr)->compr);
        if ((*rdr)->trns != NULL) free((*rdr)->trns);
        if ((*rdr)->plte != NULL) free((*rdr)->plte);
        if ((*rdr)->fd >= 0) close((*rdr)->fd);
        free(*rdr);
        *rdr = NULL;
    }
}

