
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

int font_bmp_do_load(struct font_bmp **fnt,const char *path) {
    if (*fnt == NULL) {
        if ((*fnt = font_bmp_load(path)) == NULL)
            return -1;
    }

    return 0;
}

