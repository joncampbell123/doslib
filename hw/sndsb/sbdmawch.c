
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>

signed char sndsb_dsp_playback_will_use_dma_channel(struct sndsb_ctx *cx,unsigned long rate,unsigned char stereo,unsigned char bit16) {
    if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
        return -1;
    if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_4xx) {
        if (bit16) return cx->dma16;
    }

    return cx->dma8;
}

