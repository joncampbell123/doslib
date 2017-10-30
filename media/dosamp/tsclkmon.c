
#include <stdio.h>
#include <stdint.h>
#ifdef LINUX
#include <time.h>
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

#include "dosamp.h"
#include "timesrc.h"

#if defined(HAS_CLOCK_MONOTONIC)

/* previous count, and period (in case host changes timer) */
static uint64_t clk_p = 0;

/* common function to read time and return as int */
static uint64_t clk_read(void) {
    struct timespec ts = {0,0};

    clock_gettime(CLOCK_MONOTONIC,&ts);

    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

int dosamp_FAR dosamp_time_source_clock_monotonic_close(dosamp_time_source_t inst) {
    inst->open_flags = 0;
    inst->counter = 0;
    return 0;
}

int dosamp_FAR dosamp_time_source_clock_monotonic_open(dosamp_time_source_t inst) {
    inst->open_flags = (unsigned int)(-1);
    clk_p = clk_read();
    return 0;
}

unsigned long long dosamp_FAR dosamp_time_source_clock_monotonic_poll(dosamp_time_source_t inst) {
    uint64_t c;

    if (inst->open_flags == 0) return 0UL;

    c = clk_read();
    inst->counter += (uint64_t)(c - clk_p);
    clk_p = c;

    return inst->counter;
}

struct dosamp_time_source               dosamp_time_source_clock_monotonic = {
    .obj_id =                           dosamp_time_source_id_clock_monotonic,
    .open_flags =                       0,
    .clock_rate =                       1000000000UL, /* resolution is in nanoseconds */
    .counter =                          0,
    .close =                            dosamp_time_source_clock_monotonic_close,
    .open =                             dosamp_time_source_clock_monotonic_open,
    .poll =                             dosamp_time_source_clock_monotonic_poll
};

#endif /* defined(HAS_clock_monotonic) */

