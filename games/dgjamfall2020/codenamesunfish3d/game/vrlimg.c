
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

#include "vrlimg.h"

void free_vrl(struct vrl_image *img) {
    if (img->buffer != NULL) {
        free(img->buffer);
        img->buffer = NULL;
    }
    if (img->vrl_lineoffs != NULL) {
        free(img->vrl_lineoffs);
        img->vrl_lineoffs = NULL;
    }
    img->vrl_header = NULL;
    img->bufsz = 0;
}

