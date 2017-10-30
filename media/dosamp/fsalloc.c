
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

dosamp_file_source_t dosamp_FAR dosamp_file_source_alloc(const_dosamp_file_source_t const inst_template) {
    dosamp_file_source_t inst;

    /* the reason we have a common alloc() is to enable pooling of structs in the future */
#if TARGET_MSDOS == 16
    inst = _fmalloc(sizeof(*inst));
#else
    inst = malloc(sizeof(*inst));
#endif

    if (inst != NULL)
        *inst = *inst_template;

    return inst;
}

void dosamp_FAR dosamp_file_source_free(dosamp_file_source_t const inst) {
    /* the reason we have a common free() is to enable pooling of structs in the future */
    /* ASSUME: inst != NULL */
#if TARGET_MSDOS == 16
    _ffree(inst);
#else
    free(inst);
#endif
}

