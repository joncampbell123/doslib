
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

#include "dosamp.h"
#include "timesrc.h"

/* previous count, and period (in case host changes timer) */
static uint16_t ts_8254_pcnt = 0;
static uint32_t ts_8254_pper = 0;

int dosamp_FAR dosamp_time_source_8254_close(dosamp_time_source_t inst) {
    inst->open_flags = 0;
    inst->counter = 0;
    return 0;
}

int dosamp_FAR dosamp_time_source_8254_open(dosamp_time_source_t inst) {
    inst->open_flags = (unsigned int)(-1);
    inst->clock_rate = T8254_REF_CLOCK_HZ;
    ts_8254_pcnt = read_8254(T8254_TIMER_INTERRUPT_TICK);
    ts_8254_pper = t8254_counter[T8254_TIMER_INTERRUPT_TICK];
    return 0;
}

unsigned long long dosamp_FAR dosamp_time_source_8254_poll(dosamp_time_source_t inst) {
    if (inst->open_flags == 0) return 0UL;

    if (ts_8254_pper == t8254_counter[T8254_TIMER_INTERRUPT_TICK]) {
        uint16_t ncnt = read_8254(T8254_TIMER_INTERRUPT_TICK);
        uint16_t delta;

        /* NTS: remember the timer chip counts DOWN to zero.
         *      so normally, ncnt < ts_8254_pcnt unless 16-bit rollover happened. */
        if (ncnt <= ts_8254_pcnt)
            delta = ts_8254_pcnt - ncnt;
        else
            delta = ts_8254_pcnt + (uint16_t)ts_8254_pper - ncnt;

        /* count */
        inst->counter += (unsigned long long)delta;

        /* store the prev counter */
        ts_8254_pcnt = ncnt;
    }
    else {
        ts_8254_pcnt = read_8254(T8254_TIMER_INTERRUPT_TICK);
        ts_8254_pper = t8254_counter[T8254_TIMER_INTERRUPT_TICK];
    }

    return inst->counter;
}

struct dosamp_time_source               dosamp_time_source_8254 = {
    .obj_id =                           dosamp_time_source_id_8254,
    .open_flags =                       0,
    .clock_rate =                       0,
    .counter =                          0,
    .poll_requirement =                 0xE000UL, /* counter is 16-bit */
    .close =                            dosamp_time_source_8254_close,
    .open =                             dosamp_time_source_8254_open,
    .poll =                             dosamp_time_source_8254_poll
};

