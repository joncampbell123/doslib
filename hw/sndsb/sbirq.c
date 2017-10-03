
#include <hw/sndsb/sndsb.h>

int sndsb_interrupt_reason(struct sndsb_ctx * const cx) {
	if (cx->dsp_vmaj >= 4) {
		/* Sound Blaster 16: We can read a mixer byte to determine why the interrupt happened */
		/* bit 0: 1=8-bit DSP or MIDI */
		/* bit 1: 1=16-bit DSP */
		/* bit 2: 1=MPU-401 */
		return sndsb_read_mixer(cx,0x82) & 7;
	}
	else if (cx->ess_extensions) {
		return 1; /* ESS DMA is always 8-bit. You always clear the IRQ through port 0x22E, even 16-bit PCM */
	}

	/* DSP 3.xx and earlier: just assume the interrupt happened because of the DSP */
	return 1;
}

/* meant to be called from an IRQ */
void sndsb_irq_continue(struct sndsb_ctx * const cx,const unsigned char c) {
	if (cx->dsp_nag_mode) {
		/* if the main loop is nagging the DSP then we shouldn't do anything */
		if (sndsb_will_dsp_nag(cx)) return;
	}

	/* only call send_buffer_again if 8-bit DMA completed
	   and bit 0 set, or if 16-bit DMA completed and bit 1 set */
	if ((c & 1) && (!cx->buffer_16bit || cx->ess_extensions))
		sndsb_send_buffer_again(cx);
	else if ((c & 2) && cx->buffer_16bit)
		sndsb_send_buffer_again(cx);
}

