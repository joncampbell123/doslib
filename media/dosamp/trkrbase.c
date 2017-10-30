
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

struct audio_playback_rebase_t      wav_rebase_events[MAX_REBASE];
unsigned char                       wav_rebase_read=0,wav_rebase_write=0;

void wav_rebase_clear(void) {
    wav_rebase_read = 0;
    wav_rebase_write = 0;
}

void rebase_flush_old(unsigned long before) {
    int howmany;

    howmany = (int)wav_rebase_write - (int)wav_rebase_read;
    if (howmany < 0) howmany += MAX_REBASE;

    /* flush all but one */
    while (wav_rebase_read != wav_rebase_write && howmany > 1) {
        if (wav_rebase_events[wav_rebase_read].event_at >= before)
            break;

        howmany--;
        wav_rebase_read++;
        if (wav_rebase_read >= MAX_REBASE) wav_rebase_read = 0;
    }
}

struct audio_playback_rebase_t *rebase_find(unsigned long event) {
    struct audio_playback_rebase_t *r = NULL;
    unsigned char i = wav_rebase_read;

    while (i != wav_rebase_write) {
        if (event >= wav_rebase_events[i].event_at)
            r = &wav_rebase_events[i];
        else
            break;

        i++;
        if (i >= MAX_REBASE) i = 0;
    }

    return r;
}

struct audio_playback_rebase_t *rebase_add(void) {
    unsigned char i = wav_rebase_write;

    {
        unsigned char j = i + 1;
        if (j >= MAX_REBASE) j -= MAX_REBASE;
        if (j == wav_rebase_read) {
            /* well then we just throw the oldest event away */
            wav_rebase_read++;
            if (wav_rebase_read >= MAX_REBASE) wav_rebase_read = 0;
        }
        wav_rebase_write = j;
    }

    {
        struct audio_playback_rebase_t *p = &wav_rebase_events[i];

        memset(p,0,sizeof(*p));
        return p;
    }
}

