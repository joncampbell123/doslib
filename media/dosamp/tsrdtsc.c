
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
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
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

#if defined(HAS_RDTSC)

static uint64_t ts_rdtsc_prev;

int dosamp_FAR dosamp_time_source_rdtsc_close(dosamp_time_source_t inst) {
    inst->open_flags = 0;
    ts_rdtsc_prev = 0;
    return 0;
}

int dosamp_FAR dosamp_time_source_rdtsc_open(dosamp_time_source_t inst) {
    inst->open_flags = (unsigned int)(-1);
    ts_rdtsc_prev = cpu_rdtsc();
    return 0;
}

unsigned long long dosamp_FAR dosamp_time_source_rdtsc_poll(dosamp_time_source_t inst) {
    uint64_t t;

    if (inst->open_flags == 0) return 0UL;

    t = cpu_rdtsc();

    inst->counter += t - ts_rdtsc_prev;

    ts_rdtsc_prev = t;

    return inst->counter;
}

struct dosamp_time_source               dosamp_time_source_rdtsc = {
    .obj_id =                           dosamp_time_source_id_rdtsc,
    .open_flags =                       0,
    .clock_rate =                       0,
    .counter =                          0,
    .poll_requirement =                 0, /* counter is 64-bit */
    .close =                            dosamp_time_source_rdtsc_close,
    .open =                             dosamp_time_source_rdtsc_open,
    .poll =                             dosamp_time_source_rdtsc_poll
};

#endif /* defined(HAS_RDTSC) */

