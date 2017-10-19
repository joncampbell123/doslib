
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
#include "trkrbase.h"
#include "tmpbuf.h"

/* resample state */
unsigned char                               resample_on = 0;
struct resampler_state_t                    resample_state;

void resampler_state_reset(struct resampler_state_t *r) {
    r->frac = 0;
    r->init = 0;
    r->p[0] = 0;
    r->p[1] = 0;
    r->c[0] = 0;
    r->c[1] = 0;
}

int resampler_init(struct resampler_state_t *r,struct wav_cbr_t * const d,const struct wav_cbr_t * const s) {
    if (d->sample_rate == 0 || s->sample_rate == 0)
        return -1;

    if (d->number_of_channels == 0 || s->number_of_channels == 0)
        return -1;
    if (d->number_of_channels > resample_max_channels || s->number_of_channels > resample_max_channels)
        return -1;

    {
        /* NTS: Always compute with uint64_t even on 16-bit builds.
         *      This gives us the precision we need to compute the resample step value.
         *      Also remember that on 16-bit builds the resample intermediate type is
         *      32 bits wide, and the shift is 16. As signed integers, that means that
         *      this code will produce the wrong value when sample rates >= 32768Hz are
         *      involved unless we do the math in 64-bit wide integers. */
        uint64_t tmp;

        tmp  = (uint64_t)s->sample_rate << (uint64_t)resample_100_shift;
        tmp /= (uint64_t)d->sample_rate;

        r->step = (resample_intermediate_t)tmp;
        r->frac = 0;
    }

    if (r->step == resample_100 && d->number_of_channels == s->number_of_channels && d->bits_per_sample == s->bits_per_sample)
        resample_on = 0;
    else
        resample_on = 1;

    return 0;
}

