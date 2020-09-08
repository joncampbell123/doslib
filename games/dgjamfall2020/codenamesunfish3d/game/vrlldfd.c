
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
#include "vrlimg.h"
#include "unicode.h"
#include "commtmp.h"

int load_vrl_fd(struct vrl_image *img,int fd,unsigned long sz) {
    struct vrl1_vgax_header *vrl_header;
    vrl1_vgax_offset_t *vrl_lineoffs;
    unsigned char *buffer = NULL;
    unsigned int bufsz = 0;

    {
        if (sz < sizeof(*vrl_header)) goto fail;
        if (sz >= 65535UL) goto fail;

        bufsz = (unsigned int)sz;
        buffer = malloc(bufsz);
        if (buffer == NULL) goto fail;

        if ((unsigned int)read(fd,buffer,bufsz) < bufsz) goto fail;

        vrl_header = (struct vrl1_vgax_header*)buffer;
        if (memcmp(vrl_header->vrl_sig,"VRL1",4) || memcmp(vrl_header->fmt_sig,"VGAX",4)) goto fail;
        if (vrl_header->width == 0 || vrl_header->height == 0) goto fail;
    }

    /* preprocess the sprite to generate line offsets */
    vrl_lineoffs = vrl1_vgax_genlineoffsets(vrl_header,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
    if (vrl_lineoffs == NULL) goto fail;

    img->vrl_header = vrl_header;
    img->vrl_lineoffs = vrl_lineoffs;
    img->buffer = buffer;
    img->bufsz = bufsz;

    return 0;
fail:
    if (buffer != NULL) free(buffer);
    return 1;
}

