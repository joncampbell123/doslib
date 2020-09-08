
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

unsigned int font_bmp_draw_chardef_vga8u(struct font_bmp *fnt,unsigned int cdef,unsigned int x,unsigned int y,unsigned char color) {
    unsigned int nx = x;

    if (fnt != NULL) {
        if (fnt->chardef != NULL && cdef < fnt->chardef_count) {
            const struct font_bmp_chardef *cdent = fnt->chardef + cdef;
            x += (unsigned int)((int)cdent->xoffset);
            y += (unsigned int)((int)cdent->yoffset);
            nx += (unsigned int)((int)cdent->xadvance);
            if (x < 640 && y < 400) {
                const unsigned char *sp = fnt->fontbmp + (fnt->stride * cdent->y) + (cdent->x >> 3u);
                unsigned char *dp = vga_state.vga_graphics_ram + (80u * y) + (x >> 2u);
                unsigned char sm = 0x80u >> (cdent->x & 7u);
                unsigned char dm = 1 << (x & 3u);
                unsigned int sw = cdent->w;

                while (sw-- > 0) {
                    unsigned int sh = cdent->h;
                    const unsigned char *rsp = sp;
                    unsigned char *rdp = dp;

                    vga_write_sequencer(0x02/*map mask*/,dm);
                    while (sh-- > 0) {
                        if ((*rsp) & sm) *rdp = color;
                        rsp += fnt->stride;
                        rdp += 80u;
                    }

                    if ((dm <<= 1u) & 0xF0) {
                        dm = 1u;
                        dp++;
                    }
                    if ((sm >>= 1u) == 0) {
                        sm = 0x80;
                        sp++;
                    }
                }
            }
        }
    }

    return nx;
}

