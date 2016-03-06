
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

void sndsb_timer_tick_goldi_cpy(struct sndsb_ctx *cx) {
	cx->timer_tick_signal = 1;
#if TARGET_MSDOS == 32
	memcpy(cx->buffer_lin+cx->direct_dsp_io,cx->goldplay_dma->lin,cx->gold_memcpy);
#else
	_fmemcpy(cx->buffer_lin+cx->direct_dsp_io,cx->goldplay_dma,cx->gold_memcpy);
#endif
	if (cx->backwards) {
		if (cx->direct_dsp_io < cx->gold_memcpy) cx->direct_dsp_io = cx->buffer_size - cx->gold_memcpy;
		else cx->direct_dsp_io -= cx->gold_memcpy;
	}
	else {
		if ((cx->direct_dsp_io += cx->gold_memcpy) >= cx->buffer_size) cx->direct_dsp_io = 0;
	}
}

void sndsb_timer_tick_goldo_cpy(struct sndsb_ctx *cx) {
	cx->timer_tick_signal = 1;
#if TARGET_MSDOS == 32
	memcpy(cx->goldplay_dma->lin,cx->buffer_lin+cx->direct_dsp_io,cx->gold_memcpy);
#else
	_fmemcpy(cx->goldplay_dma,cx->buffer_lin+cx->direct_dsp_io,cx->gold_memcpy);
#endif
	if (cx->backwards) {
		if (cx->direct_dsp_io < cx->gold_memcpy) cx->direct_dsp_io = cx->buffer_size - cx->gold_memcpy;
		else cx->direct_dsp_io -= cx->gold_memcpy;
	}
	else {
		if ((cx->direct_dsp_io += cx->gold_memcpy) >= cx->buffer_size) cx->direct_dsp_io = 0;
	}
}

