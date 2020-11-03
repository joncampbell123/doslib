
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8237/8237.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/sndsb/sndsb.h>
#include <hw/adlib/adlib.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "seqcomm.h"
#include "keyboard.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"
#include "cutscene.h"
#include "craptn52.h"
#include "ldwavsn.h"

#include <hw/8042/8042.h>

struct dma_8237_allocation*         sound_blaster_dma = NULL; /* DMA buffer */
struct sndsb_ctx*                   sound_blaster_ctx = NULL;

unsigned char                       sound_blaster_stop_on_irq = 0;
unsigned char                       sound_blaster_irq_hook = 0;
unsigned char                       sound_blaster_old_irq_masked = 0; /* very important! If the IRQ was masked prior to running this program there's probably a good reason */
void                                (interrupt *sound_blaster_old_irq)() = NULL;

void free_sound_blaster_dma(void) {
    if (sound_blaster_dma != NULL) {
        sndsb_assign_dma_buffer(sound_blaster_ctx,NULL);
        dma_8237_free_buffer(sound_blaster_dma);
        sound_blaster_dma = NULL;
    }
}

int realloc_sound_blaster_dma(const unsigned buffer_size) {
    uint32_t choice;
    int8_t ch;

    if (sound_blaster_dma != NULL) {
        if (sound_blaster_dma->length >= buffer_size)
            return 0;
    }

    free_sound_blaster_dma();

    ch = sndsb_dsp_playback_will_use_dma_channel(sound_blaster_ctx,22050,0/*mono*/,0/*8-bit*/);

    if (ch >= 4)
        choice = sndsb_recommended_16bit_dma_buffer_size(sound_blaster_ctx,buffer_size);
    else
        choice = sndsb_recommended_dma_buffer_size(sound_blaster_ctx,buffer_size);

    if (ch >= 4)
        sound_blaster_dma = dma_8237_alloc_buffer_dw(choice,16);
    else
        sound_blaster_dma = dma_8237_alloc_buffer_dw(choice,8);

    if (sound_blaster_dma == NULL)
        return 1;

    if (!sndsb_assign_dma_buffer(sound_blaster_ctx,sound_blaster_dma))
        return 1;

    return 0;
}

void interrupt sound_blaster_irq() {
	unsigned char c;

	sound_blaster_ctx->irq_counter++;

	/* ack soundblaster DSP if DSP was the cause of the interrupt */
	/* NTS: Experience says if you ack the wrong event on DSP 4.xx it
	   will just re-fire the IRQ until you ack it correctly...
	   or until your program crashes from stack overflow, whichever
	   comes first */
	c = sndsb_interrupt_reason(sound_blaster_ctx);
	sndsb_interrupt_ack(sound_blaster_ctx,c);

    if (!sound_blaster_stop_on_irq) {
        /* FIXME: The sndsb library should NOT do anything in
           send_buffer_again() if it knows playback has not started! */
        /* for non-auto-init modes, start another buffer */
        sndsb_irq_continue(sound_blaster_ctx,c);
    }

	/* NTS: we assume that if the IRQ was masked when we took it, that we must not
	 *      chain to the previous IRQ handler. This is very important considering
	 *      that on most DOS systems an IRQ is masked for a very good reason---the
	 *      interrupt handler doesn't exist! In fact, the IRQ vector could easily
	 *      be unitialized or 0000:0000 for it! CALLing to that address is obviously
	 *      not advised! */
	if (sound_blaster_old_irq_masked || sound_blaster_old_irq == NULL) {
		/* ack the interrupt ourself, do not chain */
		if (sound_blaster_ctx->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to the previous IRQ, who will acknowledge the interrupt */
		sound_blaster_old_irq();
	}
}

void sound_blaster_unhook_irq(void) {
    if (sound_blaster_irq_hook && sound_blaster_ctx != NULL) {
        if (sound_blaster_ctx->irq >= 0) {
            /* If the IRQ was masked on hooking, then mask the IRQ again */
            if (sound_blaster_old_irq_masked)
                p8259_mask(sound_blaster_ctx->irq);

            /* Restore the old IRQ handler */
            _dos_setvect(irq2int(sound_blaster_ctx->irq),sound_blaster_old_irq);
        }

        sound_blaster_old_irq = NULL;
        sound_blaster_irq_hook = 0;
    }
}

void sound_blaster_hook_irq(void) {
    if (!sound_blaster_irq_hook && sound_blaster_ctx != NULL) {
        if (sound_blaster_ctx->irq >= 0) {
            /* If the IRQ was masked on entry, there's probably a good reason for it, such as
             * a NULL vector, a BIOS (or DOSBox) with just an IRET instruction that doesn't
             * acknowledge the interrupt, or perhaps some junk. Whatever the reason, take it
             * as a sign not to chain to the previous interrupt handler. */
            sound_blaster_old_irq_masked = p8259_is_masked(sound_blaster_ctx->irq);
            if (vector_is_iret(irq2int(sound_blaster_ctx->irq)))
                sound_blaster_old_irq_masked = 1;

            /* hook the IRQ, install our own, then unmask the IRQ */
            sound_blaster_irq_hook = 1;
            sound_blaster_old_irq = _dos_getvect(irq2int(sound_blaster_ctx->irq));
            _dos_setvect(irq2int(sound_blaster_ctx->irq),sound_blaster_irq);
            p8259_unmask(sound_blaster_ctx->irq);
        }
    }
}

void sound_blaster_stop_playback(void) {
    if (sound_blaster_ctx != NULL)
        sndsb_stop_dsp_playback(sound_blaster_ctx);
}

