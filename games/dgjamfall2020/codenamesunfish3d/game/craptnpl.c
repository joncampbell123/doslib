
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

#include <hw/8042/8042.h>

void load_def_pal(void) {
    unsigned int i;
    int fd;

    fd = open("cr52pal.pal",O_RDONLY|O_BINARY);
    if (fd >= 0) {
        read(fd,common_tmp_small,216*3);
        close(fd);

        vga_palette_lseek(0);
        for (i=0;i < 216;i++) vga_palette_write(common_tmp_small[i*3+0]>>2u,common_tmp_small[i*3+1]>>2u,common_tmp_small[i*3+2]>>2u);
    }

    /* border color (overscan) */
    vga_write_AC(0x11 | VGA_AC_ENABLE,215);
}

