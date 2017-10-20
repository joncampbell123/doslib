
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
#include "snirq.h"

struct irq_state_t                      soundcard_irq = { NULL, 0, 0, 0, 0, 0, 0 };

int init_prepare_irq(void) {
    if (!soundcard_irq.hooked)
        return -1;

    /* make sure the IRQ is acked */
    if (soundcard_irq.irq_number >= 8) {
        p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (soundcard_irq.irq_number & 7)); /* IRQ */
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2); /* IRQ cascade */
    }
    else if (soundcard_irq.irq_number >= 0) {
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | soundcard_irq.irq_number); /* IRQ */
    }

    /* unmask the IRQ, prepare */
    if (soundcard_irq.irq_number >= 0)
        p8259_unmask(soundcard_irq.irq_number);

    return 0;
}

int hook_irq(uint8_t irq,void (interrupt *irq_handler)()) {
    if (!soundcard_irq.hooked) {
        /* take notw whether the IRQ was masked at the time we hooked.
         * on older DOS systems a masked IRQ can be a sign the handler
         * doesn't exist and/or that we don't need to chain interrupt handlers.
         *
         * we do not unmask the IRQ until we begin playback.
         *
         * sometimes IRQ handlers exist but they do not do anything to
         * dismiss or acknowledge the IRQ (DOSBox default handler, many older
         * pre-486 BIOSes, etc.). */
        soundcard_irq.irq_number = irq;
        soundcard_irq.int_number = irq2int(soundcard_irq.irq_number);

        soundcard_irq.was_masked = p8259_is_masked(soundcard_irq.irq_number);

        soundcard_irq.was_iret = vector_is_iret(soundcard_irq.int_number);

        soundcard_irq.old_handler = _dos_getvect(soundcard_irq.int_number);
        _dos_setvect(soundcard_irq.int_number,irq_handler);

        soundcard_irq.chain_irq = (!soundcard_irq.was_masked && !soundcard_irq.was_iret) && (soundcard_irq.old_handler != NULL);

        soundcard_irq.hooked = 1;
    }

    return 0;
}

int unhook_irq(void) {
    if (soundcard_irq.hooked) {
        /* if the IRQ was masked at the time we started playback, then mask it again */
        if (soundcard_irq.was_masked) p8259_mask(soundcard_irq.irq_number);

        _dos_setvect(soundcard_irq.int_number,soundcard_irq.old_handler);

        soundcard_irq.old_handler = NULL;
        soundcard_irq.int_number = 0;
        soundcard_irq.irq_number = 0;
        soundcard_irq.was_masked = 0;
        soundcard_irq.was_iret = 0;
        soundcard_irq.hooked = 0;
    }

    return 0;
}

