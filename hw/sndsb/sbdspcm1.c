
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

int sndsb_write_dsp_timeconst(struct sndsb_ctx *cx,uint8_t tc) {
	if (!sndsb_write_dsp(cx,0x40))
		return 0;
	if (!sndsb_write_dsp(cx,tc))
		return 0;
	return 1;
}

int sndsb_write_dsp_blocksize(struct sndsb_ctx *cx,uint16_t tc) {
	if (!sndsb_write_dsp(cx,0x48))
		return 0;
	if (!sndsb_write_dsp(cx,tc-1))
		return 0;
	if (!sndsb_write_dsp(cx,(tc-1)>>8))
		return 0;
	return 1;
}

int sndsb_write_dsp_outrate(struct sndsb_ctx *cx,unsigned long rate) {
	if (!sndsb_write_dsp(cx,0x41))
		return 0;
	if (!sndsb_write_dsp(cx,rate>>8)) /* Ugh, Creative, be consistent! */
		return 0;
	if (!sndsb_write_dsp(cx,rate))
		return 0;
	return 1;
}

