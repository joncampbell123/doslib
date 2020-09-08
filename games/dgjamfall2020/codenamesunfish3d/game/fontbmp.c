
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "vrlimg.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "fzlibdec.h"
#include "fataexit.h"

/*---------------------------------------------------------------------------*/
/* font handling                                                             */
/*---------------------------------------------------------------------------*/

struct font_bmp *font_bmp_load(const char *path) {
    struct font_bmp *fnt = calloc(1,sizeof(struct font_bmp));
    struct minipng_reader *rdr = NULL;

    if (fnt != NULL) {
        if ((rdr=minipng_reader_open(path)) == NULL)
            goto fail;
        if (minipng_reader_parse_head(rdr))
            goto fail;
        if (rdr->ihdr.width > 512 || rdr->ihdr.width < 8 || rdr->ihdr.height > 512 || rdr->ihdr.height < 8)
            goto fail;

        fnt->stride = (rdr->ihdr.width + 7u) / 8u;
        fnt->height = rdr->ihdr.height;

        if ((fnt->fontbmp=malloc(fnt->stride*fnt->height)) == NULL)
            goto fail;

        {
            unsigned int i;
            unsigned char *imgptr = fnt->fontbmp;
            for (i=0;i < fnt->height;i++) {
                minipng_reader_read_idat(rdr,imgptr,1); /* pad byte */
                minipng_reader_read_idat(rdr,imgptr,fnt->stride); /* row */
                imgptr += fnt->stride;
            }

            minipng_reader_reset_idat(rdr);
        }

        {
            uint16_t count;

            /* look for chardef and kerndef structures */
            while (minipng_reader_next_chunk(rdr) == 0) {
                if (rdr->chunk_data_header.type == minipng_chunk_fourcc('c','D','E','Z')) {
                    if (fnt->chardef == NULL) {
                        if (read(rdr->fd,&count,2) != 2) goto fail;
                        fnt->chardef_count = count;
                        if (count != 0 && count < (0xFF00 / sizeof(struct font_bmp_chardef))) {
                            if ((fnt->chardef = malloc(count * sizeof(struct font_bmp_chardef))) == NULL) goto fail;
                            if (file_zlib_decompress(rdr->fd,(unsigned char*)(fnt->chardef),count * sizeof(struct font_bmp_chardef),rdr->chunk_data_header.length-2ul)) goto fail;
                        }
                    }
                }
                else if (rdr->chunk_data_header.type == minipng_chunk_fourcc('k','D','E','Z')) {
                    if (fnt->kerndef == NULL) {
                        if (read(rdr->fd,&count,2) != 2) goto fail;
                        fnt->kerndef_count = count;
                        if (count != 0 && count < (0xFF00u / sizeof(struct font_bmp_kerndef))) {
                            if ((fnt->kerndef = malloc(count * sizeof(struct font_bmp_kerndef))) == NULL) goto fail;
                            if (file_zlib_decompress(rdr->fd,(unsigned char*)(fnt->kerndef),count * sizeof(struct font_bmp_kerndef),rdr->chunk_data_header.length-2ul)) goto fail;
                        }
                    }
                }
            }
        }

        minipng_reader_close(&rdr);
    }

    return fnt;
fail:
    minipng_reader_close(&rdr);
    font_bmp_free(&fnt);
    return NULL;
}

void font_bmp_free(struct font_bmp **fnt) {
    if (*fnt != NULL) {
        if ((*fnt)->fontbmp) free((*fnt)->fontbmp);
        (*fnt)->fontbmp = NULL;

        if ((*fnt)->chardef) free((*fnt)->chardef);
        (*fnt)->chardef = NULL;

        if ((*fnt)->kerndef) free((*fnt)->kerndef);
        (*fnt)->kerndef = NULL;

        free(*fnt);
        *fnt = NULL;
    }
}

