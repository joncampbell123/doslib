
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

static const uint8_t vrev4[16] = {
    0x0,    // 0000 0000
    0x8,    // 0001 1000
    0x4,    // 0010 0100
    0xC,    // 0011 1100
    0x2,    // 0100 0010
    0xA,    // 0101 1010
    0x6,    // 0110 0110
    0xE,    // 0111 1110
    0x1,    // 1000 0001
    0x9,    // 1001 1001
    0x5,    // 1010 0101
    0xD,    // 1011 1101
    0x3,    // 1100 0011
    0xB,    // 1101 1011
    0x7,    // 1110 0111
    0xF     // 1111 1111
};

void woo_menu_item_draw_char(unsigned int o,unsigned char c,unsigned char color) {
    unsigned char FAR *fbmp = vga_8x8_font_ptr + ((unsigned int)c * 8u);
    unsigned char cb;
    unsigned int i;

    for (i=0;i < 8;i++) {
        cb = fbmp[i];

        vga_write_sequencer(0x02/*map mask*/,vrev4[(cb >> 4u) & 0xFu]);
        vga_state.vga_graphics_ram[o+0] = color;

        vga_write_sequencer(0x02/*map mask*/,vrev4[(cb >> 0u) & 0xFu]);
        vga_state.vga_graphics_ram[o+1] = color;

        o += 80u;
    }
}

void woo_menu_item_drawstr(unsigned int x,unsigned int y,const char *str,unsigned char color) {
    /* 'x' is in units of 4 pixels because unchained 256-color mode */
    unsigned int o;
    char c;

    o = (y * 80u) + x;
    while ((c=(*str++)) != 0) {
        woo_menu_item_draw_char(o,(unsigned char)c,color);
        o += 2u;
    }
}

