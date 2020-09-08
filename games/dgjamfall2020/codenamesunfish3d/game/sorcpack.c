
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

struct dumbpack *sorc_pack = NULL;
/* PACK contents:

    sorcwoo.pal             // 0 (0x00)
    sorcwoo.sin
    sorcwoo1.vrl
    sorcwoo2.vrl
    sorcwoo3.vrl
    sorcwoo4.vrl            // 5 (0x05)
    sorcwoo5.vrl
    sorcwoo6.vrl
    sorcwoo7.vrl
    sorcwoo8.vrl
    sorcwoo9.vrl            // 10 (0x0A)
    sorcuhhh.vrl
    sorcbwo1.vrl
    sorcbwo2.vrl
    sorcbwo3.vrl
    sorcbwo4.vrl            // 15 (0x0F)
    sorcbwo5.vrl
    sorcbwo6.vrl
    sorcbwo7.vrl
    sorcbwo8.vrl
    sorcbwo9.vrl            // 20 (0x14)
                            //=21 (0x15)
 */

