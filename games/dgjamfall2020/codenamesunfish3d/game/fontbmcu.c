
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

/* yes, we use unicode (UTF-8) strings here in this DOS application! */
int font_bmp_unicode_to_chardef(struct font_bmp *fnt,uint32_t c) {
    if (fnt != NULL && c < 0x10000ul) {
        if (fnt->chardef != NULL) {
            unsigned int i;

            /* NTS: I know, this is very inefficient. Later revisions will add a faster method */
            for (i=0;i < fnt->chardef_count;i++) {
                if (fnt->chardef[i].id == (uint16_t)c)
                    return i;
            }
        }
    }

    return -1;
}

