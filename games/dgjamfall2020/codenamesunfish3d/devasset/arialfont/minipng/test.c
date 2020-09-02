
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <ext/zlib/zlib.h>

/* PNG signature [https://www.w3.org/TR/PNG/#5PNG-file-signature] */
const uint8_t minipng_sig[8] = { 137u, 80u, 78u, 71u, 13u, 10u, 26u, 10u };

/* PNG chunk structure [https://www.w3.org/TR/PNG/#5DataRep]. This structure only holds the first two fields */
/* [length]                 4 bytes big endian
 * [chunk type]             4 bytes
 * [chunk data]             length bytes
 * [crc]                    4 bytes */
struct minipng_chunk {
    uint32_t                length;
    uint32_t                type;           /* compare against minipng_chunk_fourcc() */
};

/* this code is intended for DOS applications and little endian platforms */
#define minipng_chunk_fourcc_shf(a,s)       ( ((uint32_t)((unsigned char)(a))) << ((uint32_t)(s)) )
#define minipng_chunk_fourcc(a,b,c,d)       ( minipng_chunk_fourcc_shf(a,0) + minipng_chunk_fourcc_shf(b,8) + minipng_chunk_fourcc_shf(c,16) + minipng_chunk_fourcc_shf(d,24) )

/* PNG IHDR [https://www.w3.org/TR/PNG/#11IHDR]. Fields are big endian. */
#pragma pack(push,1)
struct minipng_IHDR {
    uint32_t        width;                  /* +0x00 */
    uint32_t        height;                 /* +0x04 */
    uint8_t         bit_depth;              /* +0x08 */
    uint8_t         color_type;             /* +0x09 */
    uint8_t         compression_method;     /* +0x0A */
    uint8_t         filter_method;          /* +0x0B */
    uint8_t         interlace_method;       /* +0x0C */
};                                          /* =0x0D */
#pragma pack(pop)

struct minipng_reader {
    off_t                   chunk_data_offset;
    off_t                   next_chunk_start;
    struct minipng_chunk    chunk_data_header;
    int                     fd;
};

struct minipng_reader *minipng_reader_open(const char *path) {
    struct minipng_reader *rdr = NULL;
    unsigned char tmp[32]; /* keep this small, you never know if we're going to end up in a 16-bit DOS app with a very small stack */
    int fd = -1;

    /* open the file */
    fd = open(path,O_RDONLY | O_BINARY);
    if (fd < 0) goto fail;

    /* check PNG signature */
    if (read(fd,tmp,8) != 8) goto fail;
    if (memcmp(tmp,minipng_sig,8)) goto fail;

    /* good. Point at the first one (usually IHDR) */
    rdr = malloc(sizeof(*rdr));
    if (rdr == NULL) goto fail;
    rdr->fd = fd;
    rdr->chunk_data_offset = -1;
    rdr->next_chunk_start = 8;

/* success */
    return rdr;

fail:
    if (rdr != NULL) free(rdr);
    if (fd >= 0) close(fd);
    return NULL;
}

void minipng_reader_close(struct minipng_reader **rdr) {
    if (*rdr != NULL) {
        if ((*rdr)->fd >= 0) close((*rdr)->fd);
        free(*rdr);
        *rdr = NULL;
    }
}

int main(int argc,char **argv) {
    struct minipng_reader *rdr;

    if (argc < 2) {
        fprintf(stderr,"Name the 8-bit paletted PNG file\n");
        return 1;
    }

    if ((rdr=minipng_reader_open(argv[1])) == NULL) {
        fprintf(stderr,"PNG open failed\n");
        return 1;
    }

    probe_dos();
    if (!probe_vga()) {
        printf("VGA probe failed\n");
        return 1;
    }
    int10_setmode(19);
    update_state_from_vga();

    getch();

    int10_setmode(3);
    minipng_reader_close(&rdr);
    return 0;
}

