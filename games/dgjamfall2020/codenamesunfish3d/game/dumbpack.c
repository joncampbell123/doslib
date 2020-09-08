
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

struct dumbpack *dumbpack_open(const char *path) {
    struct dumbpack *d = calloc(1,sizeof(struct dumbpack));

    if (d != NULL) {
        d->fd = open(path,O_RDONLY | O_BINARY);
        if (d->fd < 0) goto fail;

        {
            uint16_t count;
            if (read(d->fd,&count,2) != 2) goto fail;
            d->offset_count = (unsigned int)count;
        }

        if (d->offset_count >= (0xFF00u / sizeof(uint32_t))) goto fail;
        if ((d->offset=malloc(sizeof(uint32_t) * (d->offset_count + 1))) == NULL) goto fail;
        if (read(d->fd,d->offset,sizeof(uint32_t) * (d->offset_count + 1)) != (sizeof(uint32_t) * (d->offset_count + 1))) goto fail;
    }

    return d;
fail:
    dumbpack_close(&d);
    return NULL;
}

void dumbpack_close(struct dumbpack **d) {
    if (*d != NULL) {
        if ((*d)->fd >= 0) close((*d)->fd);
        (*d)->fd = -1;

        if ((*d)->offset != NULL) free((*d)->offset);
        (*d)->offset = NULL;

        free(*d);
        *d = NULL;
    }
}

uint32_t dumbpack_ent_offset(struct dumbpack *p,unsigned int x) {
    if (p != NULL) {
        if (p->offset != NULL && x < p->offset_count)
            return p->offset[x];
    }

    return 0ul;
}

uint32_t dumbpack_ent_size(struct dumbpack *p,unsigned int x) {
    if (p != NULL) {
        if (p->offset != NULL && x < p->offset_count)
            return p->offset[x+1] - p->offset[x];
    }

    return 0ul;
}

