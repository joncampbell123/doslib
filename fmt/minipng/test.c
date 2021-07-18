
#include <stdio.h>
#if defined(TARGET_MSDOS)
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <dos.h>
#endif
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>

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
        (unsigned long)rdr->ihdr.width,
        (unsigned long)rdr->ihdr.height,
        (unsigned int)rdr->ihdr.bit_depth,
        (unsigned int)rdr->ihdr.color_type,
        (unsigned int)rdr->ihdr.compression_method,
        (unsigned int)rdr->ihdr.filter_method,
        (unsigned int)rdr->ihdr.interlace_method);
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

        fprintf(stderr,"PNG trns: bytelength=%u\n",(unsigned int)rdr->trns_size);
        for (i=0;i < rdr->trns_size;i++) fprintf(stderr,"%02x ",(unsigned int)rdr->trns[i]);
        fprintf(stderr,"\n");
    }
    rowsize = minipng_rowsize_bytes(rdr);
    fprintf(stderr,"Row size in bytes: %zu\n",rowsize);
    if (rdr->ihdr.color_type != 3) {
        fprintf(stderr,"Only indexed color is supported\n");
        return 1;
    }
#if defined(TARGET_MSDOS)
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
#else
    {
            unsigned int i;
            unsigned char *row;
            int fd;

            fd = open("rawpng.bin",O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
            if (fd < 0) return 1;

            if (rdr->ihdr.bit_depth == 8) {
                    row = malloc(rdr->ihdr.width + 1); /* NTS: For some reason, PNGs have an extra byte per row [https://github.com/glennrp/libpng/blob/libpng16/pngread.c#L543] at the beginning */
                    if (row != NULL) {
                            for (i=0;i < (rdr->ihdr.height < 200 ? rdr->ihdr.height : 200);i++) {
                                    if (minipng_reader_read_idat(rdr,row,rowsize) <= 0) break;
                                    write(fd,row+1/*pad byte*/,rdr->ihdr.width);
                            }
                            free(row);
                    }
            }

            close(fd);
    }
#endif
    minipng_reader_close(&rdr);
    return 0;
}

