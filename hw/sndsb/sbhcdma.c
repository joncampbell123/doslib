
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <direct.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>
#include <hw/dos/tgusumid.h>

int sndsb_halt_dma(struct sndsb_ctx *cx) {
	if (!cx->dsp_playing) return 1;
	if (cx->goldplay_mode) return 1;
	if (!cx->chose_autoinit_dsp) return 1;
	if (cx->buffer_hispeed && cx->hispeed_blocking) return 0;
	if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_1xx) return 1;

	if (cx->buffer_16bit && !cx->ess_extensions && !cx->is_gallant_sc6600)
		sndsb_write_dsp(cx,0xD5); /* Halt DMA 16-bit */
	else
		sndsb_write_dsp(cx,0xD0); /* Halt DMA 8-bit */

	return 1;
}

int sndsb_continue_dma(struct sndsb_ctx *cx) {
	if (!cx->dsp_playing) return 1;
	if (cx->goldplay_mode) return 1;
	if (!cx->chose_autoinit_dsp) return 1;
	if (cx->buffer_hispeed && cx->hispeed_blocking) return 0;
	if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_1xx) return 1;

	if (cx->buffer_16bit && !cx->ess_extensions && !cx->is_gallant_sc6600)
		sndsb_write_dsp(cx,0xD6); /* Continue DMA 16-bit */
	else
		sndsb_write_dsp(cx,0xD4); /* Continue DMA 8-bit */

	return 1;
}

