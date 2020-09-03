
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <ext/zlib/zlib.h>

#include "minipng.h"

/* WARNING: This function will expand bytes to a multiple of 8 pixels rounded up. Allocate your buffer accordingly. */
void minipng_expand1to8(unsigned char *buf,unsigned int pixels) {
    if (pixels > 0) {
        unsigned int bytes = (pixels + 7u) / 8u;
        unsigned char *w = buf + (bytes * 8u);
        buf += bytes;

        do {
            unsigned char pb = *--buf;
            w -= 8;
            w[0] = (pb & 0x80) ? 1 : 0;
            w[1] = (pb & 0x40) ? 1 : 0;
            w[2] = (pb & 0x20) ? 1 : 0;
            w[3] = (pb & 0x10) ? 1 : 0;
            w[4] = (pb & 0x08) ? 1 : 0;
            w[5] = (pb & 0x04) ? 1 : 0;
            w[6] = (pb & 0x02) ? 1 : 0;
            w[7] = (pb & 0x01) ? 1 : 0;
        } while (--bytes != 0u);
    }
}

