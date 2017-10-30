
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

extern struct dosamp_time_source                dosamp_time_source_rdtsc;

static int calibrate_rdtsc(const dosamp_time_source_t current_time_source) {
    unsigned long long ts_prev,ts_cur,tcc=0,rcc=0;
    rdtsc_t rd_prev,rd_cur;

    /* if we can't open the current time source then exit */
    if (current_time_source->open(current_time_source) < 0)
        return -1;

    /* announce (or the user will complain about 1-second delays at startup) */
    printf("Your CPU offers RDTSC instruction, calibrating RDTSC clock...\n");

    /* begin */
    ts_cur = current_time_source->poll(current_time_source);
    rd_cur = cpu_rdtsc();

    /* wait for one tick, because we may have just started in the middle of a clock tick */
    do {
        ts_prev = ts_cur; ts_cur = current_time_source->poll(current_time_source);
        rd_prev = rd_cur; rd_cur = cpu_rdtsc();
    } while (ts_cur == ts_prev);

    /* time for 1 second */
    do {
        tcc += ts_cur - ts_prev;
        rcc += rd_cur - rd_prev;
        ts_prev = ts_cur; ts_cur = current_time_source->poll(current_time_source);
        rd_prev = rd_cur; rd_cur = cpu_rdtsc();
    } while (tcc < current_time_source->clock_rate);

    /* close current time source */
    current_time_source->close(current_time_source);

    /* reject if out of range, or less precision than current time source */
    if (rcc < 10000000ULL) return -1; /* Pentium processors started at 60MHz, we expect at least 10MHz precision */
    if (rcc < current_time_source->clock_rate) return -1; /* RDTSC must provide more precision than current source */

    printf("RDTSC clock rate: %lluHz\n",rcc);

    dosamp_time_source_rdtsc.clock_rate = rcc;
    return 0;
}

int dosamp_time_source_rdtsc_available(const dosamp_time_source_t clk) {
    if (cpu_flags & CPU_FLAG_CPUID) {
        if (!(cpu_flags & CPU_FLAG_DONT_USE_RDTSC)) {
            if (cpu_cpuid_features.a.raw[2] & 0x10/*RDTSC*/) {
                /* RDTSC is available. We just have to figure out how fast it runs. */
                if (calibrate_rdtsc(clk) >= 0)
                    return 1;
            }
        }
    }

    return 0;
}

#endif

