
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

#include <fmt/minipng/minipng.h>

/* WARNING: This function will expand bytes to a multiple of 2 pixels rounded up. Allocate your buffer accordingly. */
void minipng_expand4to8(unsigned char *buf,unsigned int pixels) {
    if (pixels > 0) {
        unsigned int bytes = (pixels + 1u) / 2u;
        unsigned char *w = buf + (bytes * 2u);
        buf += bytes;

        do {
            unsigned char pb = *--buf;
            w -= 2;
            w[0] = (pb >> 4u) & 0xFu;
            w[1] = (pb >> 0u) & 0xFu;
        } while (--bytes != 0u);
    }
}

