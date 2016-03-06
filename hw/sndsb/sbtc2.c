
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

unsigned long sndsb_real_sample_rate(struct sndsb_ctx *cx) {
	unsigned long total_rate;
	unsigned char timeconst;
	unsigned long real_rate;

	total_rate = (unsigned long)cx->buffer_rate * (cx->buffer_stereo ? 2UL : 1UL);
	if (total_rate < 4000UL) total_rate = 4000UL;
	timeconst = (unsigned char)((65536UL - (256000000UL / total_rate)) >> 8UL);
	if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_4xx) return cx->buffer_rate;
	if (cx->dsp_play_method == SNDSB_DSPOUTMETHOD_DIRECT) return cx->buffer_rate;

	/* 256 - (1000000 / rate) = const
	 * -(1000000 / rate) = const - 256
	 * 1000000 / rate = -(const - 256)
	 * 1000000 / rate = -const + 256
	 * 1000000 = (-const + 256) * rate
	 * 1000000 / (-const + 256) = rate
	 * 1000000 / (256 - const) = rate */
	real_rate = 1000000UL / (unsigned long)(256 - timeconst);
	if (cx->buffer_stereo) real_rate /= 2UL;
	return real_rate;
}

