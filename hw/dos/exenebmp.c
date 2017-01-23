
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <hw/dos/exehdr.h>
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>

unsigned int exe_ne_header_BITMAPINFOHEADER_get_palette_count(const struct exe_ne_header_BITMAPINFOHEADER *bmphdr) {
    if (bmphdr->biBitCount <= 8) {
        unsigned int c = 1UL << bmphdr->biBitCount;

        if (bmphdr->biSize >= sizeof(struct exe_ne_header_BITMAPINFOHEADER) && bmphdr->biClrUsed != 0) {
            if (c > bmphdr->biClrUsed) c = bmphdr->biClrUsed;
        }

        return c;
    }

    return 0;
}

