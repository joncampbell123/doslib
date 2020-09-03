
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

size_t minipng_rowsize_bytes(struct minipng_reader *rdr) {
    if (rdr == NULL) return 0;
    if (rdr->fd < 0) return 0;

    switch (rdr->ihdr.bit_depth) {
        case 1:
        case 2:
        case 4:
        case 8:
            return ((size_t)((((unsigned long)rdr->ihdr.width*(unsigned long)rdr->ihdr.bit_depth)+7ul)/8ul)) + 1u;     /* one pad byte at the beginning? */
        default:
            break;
    }

    return 0;
}

