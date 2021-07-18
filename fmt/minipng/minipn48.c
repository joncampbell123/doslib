
#include <stdio.h>
#if defined(TARGET_MSDOS)
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#if defined(TARGET_MSDOS)
#include <dos.h>
#endif

#if defined(TARGET_MSDOS)
#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#endif

#if defined(TARGET_MSDOS)
#include <ext/zlib/zlib.h>
#else
#include <zlib.h>
#endif

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

