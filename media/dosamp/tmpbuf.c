
#include <stdio.h>
#include <stdint.h>
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
#ifndef LINUX
#include <dos.h>
#endif

#ifndef LINUX
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
#endif

#include "wavefmt.h"
#include "dosamp.h"
#include "timesrc.h"
#include "dosptrnm.h"
#include "filesrc.h"
#include "resample.h"
#include "cvrdbuf.h"
#include "cvip.h"
#include "trkrbase.h"
#include "tmpbuf.h"

unsigned char dosamp_FAR *      tmpbuffer = NULL;
size_t                          tmpbuffer_sz = 4096;

void tmpbuffer_free(void) {
    if (tmpbuffer != NULL) {
#if TARGET_MSDOS == 32 || defined(LINUX)
        free(tmpbuffer);
#else
        _ffree(tmpbuffer);
#endif
        tmpbuffer = NULL;
    }
}

unsigned char dosamp_FAR * tmpbuffer_get(uint32_t *sz) {
    if (tmpbuffer == NULL) {
#if TARGET_MSDOS == 32 || defined(LINUX)
        tmpbuffer = malloc(tmpbuffer_sz);
#else
        tmpbuffer = _fmalloc(tmpbuffer_sz);
#endif
    }

    if (sz != NULL) *sz = tmpbuffer_sz;
    return tmpbuffer;
}

