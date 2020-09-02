
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

/* WARNING: This function will expand bytes to a multiple of 8 pixels rounded up. Allocate your buffer accordingly. */
void minipng_expand1to8(unsigned char *buf,unsigned int pixels) {
    if (pixels > 0) {
        unsigned int bytes = (pixels + 7u) / 8u;
        unsigned char *w = buf + (bytes * 8u);
        buf += bytes;

        do {
            unsigned char pb = *--buf;
            w -= 8;
            w[0] = (pb & 0x80) ? 1 : 0;
            w[1] = (pb & 0x40) ? 1 : 0;
            w[2] = (pb & 0x20) ? 1 : 0;
            w[3] = (pb & 0x10) ? 1 : 0;
            w[4] = (pb & 0x08) ? 1 : 0;
            w[5] = (pb & 0x04) ? 1 : 0;
            w[6] = (pb & 0x02) ? 1 : 0;
            w[7] = (pb & 0x01) ? 1 : 0;
        } while (--bytes != 0u);
    }
}

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

    unsigned char*              trns;
    size_t                      trns_size;

    uint32_t                    idat_rem;
    unsigned char*              compr;
    size_t                      compr_size;
    z_stream                    compr_zlib;

    unsigned int                ungetch;
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
        else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('t','R','N','S')) {
            if (rdr->trns == NULL && rdr->chunk_data_header.length != 0) {
                rdr->trns_size = rdr->chunk_data_header.length;
                if (rdr->chunk_data_header.length <= 1024) {
                    rdr->trns = malloc(rdr->trns_size);
                    if (rdr->trns != NULL) read(rdr->fd,rdr->trns,rdr->trns_size);
                }
            }
        }
        else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('I','D','A','T')) {
            /* it's the body of the PNG. stop now */
            rdr->ungetch++;
            break;
        }
    }

    /* IHDR required */
    if (rdr->ihdr.bit_depth == 0 || rdr->ihdr.width == 0 || rdr->ihdr.height == 0)
        return -1;

    return 0;
}

/* WARNING: Normalize far pointer dst before calling this */
int minipng_reader_read_idat(struct minipng_reader *rdr,unsigned char FAR *dst,size_t count) {
    int err;

    if (rdr == NULL) return -1;
    if (rdr->fd < 0) return -1;

    if (rdr->compr == NULL) {
        rdr->compr_size = 1024;
        rdr->compr = malloc(rdr->compr_size);
        if (rdr->compr == NULL) return -1;

        memset(&(rdr->compr_zlib),0,sizeof(rdr->compr_zlib));
        rdr->compr_zlib.next_in = rdr->compr;
        rdr->compr_zlib.avail_in = 0;
        rdr->compr_zlib.next_out = dst;
        rdr->compr_zlib.avail_out = 0;
        rdr->idat_rem = 0;

        if (inflateInit2(&(rdr->compr_zlib),15/*max window size 32KB*/) != Z_OK) {
            memset(&(rdr->compr_zlib),0,sizeof(rdr->compr_zlib));
            free(rdr->compr);
            rdr->compr = NULL;
            return -1;
        }
    }

    rdr->compr_zlib.next_out = dst;
    rdr->compr_zlib.avail_out = count;
    while (rdr->compr_zlib.avail_out != 0) {
        if (rdr->compr_zlib.avail_in == 0) {
            if (rdr->idat_rem == 0) {
                if (minipng_reader_next_chunk(rdr)) break;
                if (rdr->chunk_data_header.type == minipng_chunk_fourcc('I','D','A','T')) {
                    rdr->idat_rem = rdr->chunk_data_header.length;
                    if (rdr->idat_rem == 0) continue;
                }
                else {
                    rdr->ungetch++;
                    break;
                }
            }

            /* assume idat_rem != 0 */
            {
                size_t icount = (rdr->idat_rem < rdr->compr_size) ? rdr->idat_rem : rdr->compr_size; /* lesser of the two */

                read(rdr->fd,rdr->compr,icount);
                rdr->idat_rem -= icount;
                rdr->compr_zlib.next_in = rdr->compr;
                rdr->compr_zlib.avail_in = icount;
            }
        }

        if ((err=inflate(&(rdr->compr_zlib),Z_NO_FLUSH)) != Z_OK) {
            break;
        }
        if (rdr->compr_zlib.avail_out == count) {
            if ((err=inflate(&(rdr->compr_zlib),Z_SYNC_FLUSH)) != Z_OK) {
                break;
            }
        }
    }

    return (count - rdr->compr_zlib.avail_out);
}

size_t minipng_rowsize_bytes(struct minipng_reader *rdr) {
    if (rdr == NULL) return 0;
    if (rdr->fd < 0) return 0;

    switch (rdr->ihdr.bit_depth) {
        case 1:
        case 2:
        case 4:
        case 8:
            return ((size_t)((((unsigned long)rdr->ihdr.width*(unsigned long)rdr->ihdr.bit_depth)+7ul)/8ul)) + 1u;     /* one pad byte at the beginning? */
        default:
            break;
    }

    return 0;
}

int main(int argc,char **argv) {
    struct minipng_reader *rdr;
    size_t rowsize;

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
    if (rdr->trns != NULL) {
        unsigned int i;

        fprintf(stderr,"PNG trns: bytelength=%u\n",rdr->trns_size);
        for (i=0;i < rdr->trns_size;i++) fprintf(stderr,"%02x ",rdr->trns[i]);
        fprintf(stderr,"\n");
    }
    rowsize = minipng_rowsize_bytes(rdr);
    fprintf(stderr,"Row size in bytes: %zu\n",rowsize);
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

    {
        unsigned int i;
        unsigned char *row;

        if (rdr->ihdr.bit_depth == 1) {
            row = malloc(rdr->ihdr.width + 8/*extra byte*/ + 8/*expandpad*/); /* NTS: For some reason, PNGs have an extra byte per row [https://github.com/glennrp/libpng/blob/libpng16/pngread.c#L543] at the beginning */
            if (row != NULL) {
                for (i=0;i < (rdr->ihdr.height < 200 ? rdr->ihdr.height : 200);i++) {
                    if (minipng_reader_read_idat(rdr,row,rowsize) <= 0) break; /* NTS: rowsize = ((width+7)/8)+1 */
                    minipng_expand1to8(row,rowsize * 8u);

#if TARGET_MSDOS == 32
                    memcpy(vga_state.vga_graphics_ram + (i * 320),(unsigned char*)row + 8/*extra pad byte*/,rdr->ihdr.width);
#else
                    _fmemcpy(vga_state.vga_graphics_ram + (i * 320),(unsigned char far*)row + 8/*extra pad byte*/,rdr->ihdr.width);
#endif
                }
                free(row);
            }
        }
        else if (rdr->ihdr.bit_depth == 8) {
            row = malloc(rdr->ihdr.width + 1); /* NTS: For some reason, PNGs have an extra byte per row [https://github.com/glennrp/libpng/blob/libpng16/pngread.c#L543] at the beginning */
            if (row != NULL) {
                for (i=0;i < (rdr->ihdr.height < 200 ? rdr->ihdr.height : 200);i++) {
                    if (minipng_reader_read_idat(rdr,row,rowsize) <= 0) break;

#if TARGET_MSDOS == 32
                    memcpy(vga_state.vga_graphics_ram + (i * 320),(unsigned char*)row + 1/*extra pad byte*/,rdr->ihdr.width);
#else
                    _fmemcpy(vga_state.vga_graphics_ram + (i * 320),(unsigned char far*)row + 1/*extra pad byte*/,rdr->ihdr.width);
#endif
                }
                free(row);
            }
        }
    }

    getch();

    int10_setmode(3);
    minipng_reader_close(&rdr);
    return 0;
}

