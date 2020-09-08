
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
#include "fonts.h"
#include "vrlimg.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"

/* atomic playboy background 256x256 */
int rotozoomerpngload(unsigned rotozoomerimgseg,const char *path) {
    struct minipng_reader *rdr;

    /* WARNING: Code assumes 16-bit large memory model! */

    if ((rdr=minipng_reader_open(path)) == NULL)
        return -1;

    if (minipng_reader_parse_head(rdr) || rdr->plte == NULL || rdr->plte_count == 0) {
        minipng_reader_close(&rdr);
        return -1;
    }

    {
        unsigned int i;

        for (i=0;i < 256;i++) {
            unsigned char far *imgptr = (unsigned char far*)MK_FP(rotozoomerimgseg + (i * 16u/*paragraphs=256b*/),0);
            minipng_reader_read_idat(rdr,imgptr,1); /* pad byte */
            minipng_reader_read_idat(rdr,imgptr,256); /* row */
        }

        {
            const unsigned char *p = (const unsigned char*)(rdr->plte);
            vga_palette_lseek(ATOMPB_PAL_OFFSET);
            for (i=0;i < rdr->plte_count;i++)
                vga_palette_write(p[(i*3)+0]>>2,p[(i*3)+1]>>2,p[(i*3)+2]>>2);
        }
    }

    minipng_reader_close(&rdr);
    return 0;
}

