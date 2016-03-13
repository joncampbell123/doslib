
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

int sndsb_exit_autoinit_mode(struct sndsb_ctx *cx) {
	if (!cx->dsp_playing) return 1;
	if (!cx->chose_autoinit_dsp) return 1;
	if (cx->buffer_hispeed && cx->hispeed_blocking) return 0; // DSP does not respond to commands in high-speed mode
	if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_1xx) return 1;

	if (cx->buffer_16bit && !cx->ess_extensions && !cx->is_gallant_sc6600)
		sndsb_write_dsp(cx,0xD9); /* Exit auto-initialize DMA 16-bit */
	else
		sndsb_write_dsp(cx,0xDA); /* Exit auto-initialize DMA 8-bit */

	return 1;
}

int sndsb_continue_autoinit_mode(struct sndsb_ctx *cx) {
	if (!cx->dsp_playing) return 1;
	if (!cx->chose_autoinit_dsp) return 1;
	if (cx->is_gallant_sc6600) return 0; // Reveal SC400 cards do not support Continue auto-init DMA
	if (cx->buffer_hispeed && cx->hispeed_blocking) return 0; // DSP does not respond to commands in high-speed mode
	if (cx->dsp_play_method < SNDSB_DSPOUTMETHOD_4xx) return 1; // SB16 at least

	/* Note these DSP commands are so little used even DOSBox doesn't implement them! */
	if (cx->buffer_16bit && !cx->ess_extensions)
		sndsb_write_dsp(cx,0x47); /* Continue auto-initialize DMA 16-bit */
	else
		sndsb_write_dsp(cx,0x45); /* Continue auto-initialize DMA 8-bit */

	return 1;
}

