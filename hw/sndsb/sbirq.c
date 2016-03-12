
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

/* uncomment this to enable debugging messages */
//#define DBG

#if defined(DBG)
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

int sndsb_interrupt_reason(struct sndsb_ctx *cx) {
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
void sndsb_irq_continue(struct sndsb_ctx *cx,unsigned char c) {
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

