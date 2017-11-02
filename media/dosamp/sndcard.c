
#define HW_DOS_DONT_DEFINE_MMSYSTEM

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
#include "snirq.h"
#include "sndcard.h"

void soundcard_str_return_common(char dosamp_FAR *data,unsigned int dosamp_FAR *len,const char *str) {
    /* assume: *len != 0 */
    unsigned int l = (unsigned int)strlen(str);

    if (l >= *len) l = *len - 1;

    if (l != 0) {
#if TARGET_MSDOS == 16
        _fmemcpy(data,str,l);
#else
        memcpy(data,str,l);
#endif
    }

    *len = l + 1U;
    data[l] = 0;
}

int soundcardlist_init(void) {
    soundcardlist_count = 0;
    soundcardlist_alloc = 0;
    return 0;
}

void soundcardlist_close(void) {
    soundcardlist_count = 0;
    soundcardlist_alloc = 0;
}

soundcard_t soundcardlist_new(const soundcard_t template) {
    while (soundcardlist_alloc < soundcardlist_count) {
        if (soundcardlist[soundcardlist_alloc].driver == soundcard_none) {
            soundcardlist[soundcardlist_alloc] = *template;
            return &soundcardlist[soundcardlist_alloc++];
        }
        else {
            soundcardlist_alloc++;
        }
    }

    if (soundcardlist_count < SOUNDCARDLIST_MAX) {
        soundcardlist[soundcardlist_count] = *template;
        return &soundcardlist[soundcardlist_count++];
    }

    return NULL;
}

soundcard_t soundcardlist_free(const soundcard_t sc) {
    if (sc != NULL) {
        soundcardlist_alloc = 0;
        sc->driver = soundcard_none;
    }

    return NULL;
}

