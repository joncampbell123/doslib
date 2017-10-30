
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

unsigned int dosamp_FAR dosamp_file_source_addref(dosamp_file_source_t const inst) {
    return ++(inst->refcount);
}

unsigned int dosamp_FAR dosamp_file_source_release(dosamp_file_source_t const inst) {
    if (inst->refcount != 0)
        inst->refcount--;
    // TODO: else, debug message if refcount == 0

    return inst->refcount;
}

unsigned int dosamp_FAR dosamp_file_source_autofree(dosamp_file_source_ptr_t inst) {
    unsigned int r = 0;

    /* assume inst != NULL */
    if (*inst != NULL) {
        r = (*inst)->refcount;
        if (r == 0) {
            (*inst)->close(*inst);
            (*inst)->free(*inst);
            *inst = NULL;
        }
    }

    return r;
}

