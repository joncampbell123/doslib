
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

/* Windows 9x VxD enumeration */
#include <windows/w9xvmm/vxd_enum.h>

unsigned int sndsb_will_dsp_nag(struct sndsb_ctx *cx) {
	if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT)
		return 0;

	if (cx->chose_autoinit_dma && !cx->chose_autoinit_dsp) {
		/* NTS: Do not nag the DSP when it's in "highspeed" DMA mode. Normal DSPs cannot accept
		 *      commands in that state and any attempt will cause this function to hang for the
		 *      DSP timeout period causing the main loop to jump and stutter. But if the user
		 *      really *wants* us to do it (signified by setting dsp_nag_highspeed) then we'll do it */
		if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_4xx && cx->buffer_hispeed && cx->hispeed_matters && cx->hispeed_blocking && !cx->dsp_nag_hispeed)
			return 0;
	}

	return 1;
}

/* general main loop idle function. does nothing, unless we're playing with no IRQ,
 * in which case we're expected to poll IRQ status */
void sndsb_main_idle(struct sndsb_ctx *cx) {
	unsigned int oflags;

	oflags = get_cpu_flags();
	_cli();
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && cx->timer_tick_signal) {
		/* DSP "nag" mode: when the host program's IRQ handler called our timer tick callback, we
		 * noted it so that at idle() we can nag the DSP at moderate intervals. Note that nag mode
		 * only makes sense when autoinit DMA is in use, otherwise we risk skipping popping and
		 * crackling. We also don't nag if the DSP is doing auto-init playback, because it makes
		 * no sense to do so.
		 *
		 * The idea is to mimic for testing purposes the DSP "nagging" technique used by the
		 * Triton Crystal Dreams demo that allow it to do full DMA playback Goldplay style
		 * without needing to autodetect what IRQ the card is on. The programmer did not
		 * write the code to use auto-init DSP commands. Instead, the demo uses the single
		 * cycle DSP playback command (1.xx commands) with the DMA settings set to one sample
		 * wide (Goldplay style), then, from the same IRQ 0 handler that does the music,
		 * polls the DSP write status register to check DSP busy state. If the DSP is not busy,
		 * it counts down a timer internally on each IRQ 0, then when it hits zero, begins
		 * sending another DSP playback block (DSP command 0x14,xx,xx). It does this whether
		 * or not the last DSP 0x14 command has finished playing or not, thus, "nagging" the
		 * DSP. The upshot of this bizarre technique is that it doesn't need to pay any
		 * attention to the Sound Blaster IRQ. The downside, of course, is that later 
		 * "emulations" of the Sound Blaster don't recognize the technique and playback will
		 * not work properly like that.
		 *
		 * The other reason to use such a technique is to avoid artifacts caused by the amount
		 * of time it takes the signal an IRQ vs the CPU to program another single-cycle block
		 * (longer than one sample period), since nagging the DSP ensures it never stops despite
		 * the single-cycle mode it's in. The side effect of course is that since the DSP is
		 * never given a chance to complete a whole block, it never fires the IRQ! */
		if (cx->dsp_nag_mode && sndsb_will_dsp_nag(cx))
			sndsb_send_buffer_again(cx);

		cx->timer_tick_signal = 0;
	}
	if (oflags & 0x200/* if interrupts were enabled */) _sti();

	/* if DMA based playback and no IRQ assigned, then we need to poll the ack register to keep
	 * playback from halting on SB16 hardware. Clones and SBpro and earlier don't seem to care. */
	if (cx->dsp_play_method >= SNDSB_DSPOUTMETHOD_1xx && cx->irq < 0 && cx->poll_ack_when_no_irq)
		sndsb_interrupt_ack(cx,3);
}

