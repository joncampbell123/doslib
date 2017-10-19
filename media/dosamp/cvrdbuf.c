
#include <stdio.h>
#ifdef LINUX
#include <endian.h>
#else
#include <hw/cpu/endian.h>
#endif
#ifndef LINUX
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/cpu/cpurdtsc.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"
#include "resample.h"
#include "cvrdbuf.h"
#include "cvip.h"

void convert_rdbuf_free(void) {
    if (convert_rdbuf.buffer != NULL) {
#if TARGET_MSDOS == 32
        free(convert_rdbuf.buffer);
#else
        _ffree(convert_rdbuf.buffer);
#endif
        convert_rdbuf.buffer = NULL;
        convert_rdbuf.len = 0;
        convert_rdbuf.pos = 0;
    }
}

unsigned char dosamp_FAR * convert_rdbuf_get(uint32_t *sz) {
    if (convert_rdbuf.buffer == NULL) {
        if (convert_rdbuf.size == 0)
            convert_rdbuf.size = 4096;

#if TARGET_MSDOS == 32
        convert_rdbuf.buffer = malloc(convert_rdbuf.size);
#else
        convert_rdbuf.buffer = _fmalloc(convert_rdbuf.size);
#endif
        convert_rdbuf.len = 0;
        convert_rdbuf.pos = 0;
    }

    if (sz != NULL) *sz = convert_rdbuf.size;
    return convert_rdbuf.buffer;
}

void convert_rdbuf_clear(void) {
    convert_rdbuf.len = 0;
    convert_rdbuf.pos = 0;
}

