
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

/* PNG PLTE [https://www.w3.org/TR/PNG/#11PLTE] */
#pragma pack(push,1)
struct minipng_PLTE_color {
    uint8_t         red,green,blue;         /* +0x00,+0x01,+0x02 */
};                                          /* =0x03 */
#pragma pack(pop)

struct minipng_reader {
    char*                   err_msg;
    off_t                   chunk_data_offset;
    off_t                   next_chunk_start;
    struct minipng_chunk    chunk_data_header;
    int                     fd;

    struct minipng_IHDR     ihdr;

    struct minipng_PLTE_color*  plte;
    unsigned int                plte_count;
};

inline uint32_t minipng_byteswap32(uint32_t t) {
    t = ((t & 0xFF00FF00UL) >>  8ul) + ((t & 0x00FF00FFUL) <<  8ul);
    t = ((t & 0xFFFF0000UL) >> 16ul) + ((t & 0x0000FFFFUL) << 16ul);
    return t;
}

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

int minipng_reader_rewind(struct minipng_reader *rdr) {
    if (rdr == NULL) return -1;
    if (rdr->fd < 0) return -1;

    rdr->chunk_data_offset = -1;
    rdr->next_chunk_start = 8;

    return 0;
}

int minipng_reader_next_chunk(struct minipng_reader *rdr) {
    unsigned char tmp[8]; /* keep this small, you never know if we're going to end up in a 16-bit DOS app with a very small stack */

    if (rdr == NULL) return -1;
    if (rdr->fd < 0) return -1;
    if (rdr->next_chunk_start < (off_t)-1) return -1;

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
        if ((*rdr)->plte != NULL) free((*rdr)->plte);
        if ((*rdr)->fd >= 0) close((*rdr)->fd);
        free(*rdr);
        *rdr = NULL;
    }
}

/* assume caller just opened or called rewind.
 * don't expect this to work if you call this twice in a row or after the IDAT. */
int minipng_reader_parse_head(struct minipng_reader *rdr) {
    while (minipng_reader_next_chunk(rdr) == 0) {
        if (rdr->chunk_data_header.type == minipng_chunk_fourcc('I','H','D','R')) {
            if (rdr->chunk_data_header.length >= 0x0D) {
                read(rdr->fd,&rdr->ihdr,0x0D);
                /* byte swap fields */
                rdr->ihdr.width =       minipng_byteswap32(rdr->ihdr.width);
                rdr->ihdr.height =      minipng_byteswap32(rdr->ihdr.height);
            }
        }
        else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('P','L','T','E')) {
            if (rdr->plte == NULL && rdr->chunk_data_header.length >= 3ul) {
                rdr->plte_count = rdr->chunk_data_header.length / 3ul;
                if (rdr->plte_count <= 256) {
                    rdr->plte = malloc(3 * rdr->plte_count);
                    if (rdr->plte != NULL) read(rdr->fd,rdr->plte,3 * rdr->plte_count);
                }
            }
        }
        else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('I','D','A','T')) {
            /* it's the body of the PNG. stop now */
            break;
        }
    }

    /* IHDR required */
    if (rdr->ihdr.bit_depth == 0 || rdr->ihdr.width == 0 || rdr->ihdr.height == 0)
        return -1;

    return 0;
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
    while (minipng_reader_next_chunk(rdr) == 0) {
        char tmp[5];

        tmp[4] = 0;
        *((uint32_t*)(tmp+0)) = rdr->chunk_data_header.type;
        fprintf(stderr,"PNG chunk '%s' data=%ld size=%ld\n",tmp,
            (long)rdr->chunk_data_offset,
            (long)rdr->chunk_data_header.length);
    }
    minipng_reader_rewind(rdr);
    if (minipng_reader_parse_head(rdr)) {
        fprintf(stderr,"PNG head parse failed\n");
        return 1;
    }
    fprintf(stderr,"PNG header: %lu x %lu, depth=%u ctype=0x%02x cmpmet=%u flmet=%u ilmet=%u\n",
        rdr->ihdr.width,
        rdr->ihdr.height,
        rdr->ihdr.bit_depth,
        rdr->ihdr.color_type,
        rdr->ihdr.compression_method,
        rdr->ihdr.filter_method,
        rdr->ihdr.interlace_method);
    if (rdr->plte != NULL) {
        unsigned int i;

        fprintf(stderr,"PNG palette: count=%u\n",rdr->plte_count);
        for (i=0;i < rdr->plte_count;i++) {
            fprintf(stderr,"%02x%02x%02x ",
                rdr->plte[i].red,
                rdr->plte[i].green,
                rdr->plte[i].blue);
        }
        fprintf(stderr,"\n");
    }
    if (rdr->ihdr.color_type != 3) {
        fprintf(stderr,"Only indexed color is supported\n");
        return 1;
    }
    getch();

    probe_dos();
    if (!probe_vga()) {
        printf("VGA probe failed\n");
        return 1;
    }
    int10_setmode(19);
    update_state_from_vga();

    vga_palette_lseek(0);
    {
        unsigned int i;
        for (i=0;i < rdr->plte_count;i++) {
            vga_palette_write(rdr->plte[i].red >> 2u,rdr->plte[i].green >> 2u,rdr->plte[i].blue >> 2u);
        }
    }

    getch();

    int10_setmode(3);
    minipng_reader_close(&rdr);
    return 0;
}

