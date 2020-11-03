
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8237/8237.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/sndsb/sndsb.h>
#include <hw/adlib/adlib.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "seqcomm.h"
#include "keyboard.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"
#include "cutscene.h"
#include "craptn52.h"
#include "ldwavsn.h"

struct minipng_reader *woo_title_load_png(unsigned char *buf,unsigned int w,unsigned int h,const char *path) {
    struct minipng_reader *rdr;
    unsigned int y;

    if ((rdr=minipng_reader_open(path)) == NULL)
        return NULL;

    if (minipng_reader_parse_head(rdr) || rdr->plte == NULL || rdr->plte_count == 0 || rdr->ihdr.width != w || rdr->ihdr.height != h) {
        minipng_reader_close(&rdr);
        return NULL;
    }

    for (y=0;y < h;y++) {
        unsigned char *imgptr = buf + (y * w);

        if (minipng_reader_read_idat(rdr,imgptr,1) != 1) { /* pad byte */
            minipng_reader_close(&rdr);
            return NULL;
        }

        if (rdr->ihdr.bit_depth == 8) {
            if (minipng_reader_read_idat(rdr,imgptr,w) != w) { /* row */
                minipng_reader_close(&rdr);
                return NULL;
            }
        }
        else if (rdr->ihdr.bit_depth == 4) {
            if (minipng_reader_read_idat(rdr,imgptr,(w+1u)>>1u) != ((w+1u)>>1u)) { /* row */
                minipng_reader_close(&rdr);
                return NULL;
            }
            minipng_expand4to8(imgptr,w);
        }
    }

    return rdr;
}

