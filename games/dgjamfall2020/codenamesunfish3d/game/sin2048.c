
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
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"

uint16_t *sin2048fps16_table = NULL;         // 2048-sample sin(x) quarter wave lookup table (0....0x7FFF)

/* 0x0000 -> sin(0) */
/* 0x0800 -> sin(pi*0.5)    0x0800 = 2048
 * 0x1000 -> sin(pi)        0x1000 = 4096
 * 0x1800 -> sin(pi*1.5)    0x1800 = 6144
 * 0x2000 -> sin(pi*2.0)    0x2000 = 8192 */
int sin2048fps16_lookup(unsigned int a) {
    int r;

    /* sin(0) to sin(pi) is symmetrical across sin(pi/2) */
    if (a & 0x800u)
        r = sin2048fps16_table[0x7FFu - (a & 0x7FFu)];
    else
        r = sin2048fps16_table[a & 0x7FFu];

    /* sin(pi) to sin(pi*2) is just the first half with the opposite sign */
    if (a & 0x1000u)
        r = -r;

    return r;
}

void sin2048fps16_free(void) {
    if (sin2048fps16_table != NULL) {
        free(sin2048fps16_table);
        sin2048fps16_table = NULL;
    }
}

int sin2048fps16_open(void) {
    if (sin2048fps16_table == NULL) {
        uint32_t ofs = dumbpack_ent_offset(sorc_pack,1);
        uint32_t sz = dumbpack_ent_size(sorc_pack,1);
        if (ofs == 0ul || sz < (sizeof(uint16_t) * 2048)) return -1;
        if (lseek(sorc_pack->fd,ofs,SEEK_SET) != ofs) return -1;
        if ((sin2048fps16_table=malloc(sizeof(uint16_t) * 2048)) == NULL) return -1;
        read(sorc_pack->fd,sin2048fps16_table,sizeof(uint16_t) * 2048);
    }

    return 0;
}

