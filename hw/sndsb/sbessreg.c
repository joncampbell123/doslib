
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

unsigned int sndsb_ess_set_extended_mode(struct sndsb_ctx *cx,int enable) {
	if (cx->ess_chipset == 0) return 0; /* if not an ESS chipset then, no */
	if (!cx->ess_extensions) return 0; /* if caller/user says not to use extensions, then, no */
	if (cx->ess_extended_mode == !!enable) return 1;

	if (!sndsb_write_dsp(cx,enable?0xC6:0xC7))
		return 0;

	cx->ess_extended_mode = !!enable;
	return 1;
}

int sndsb_ess_read_controller(struct sndsb_ctx *cx,int reg) {
	if (reg < 0xA0 || reg >= 0xC0) return -1;
	if (sndsb_ess_set_extended_mode(cx,1) == 0) return -1;
	/* "Reading the data buffer of the ESS 1869: Command C0h is used to read
	 * the ES1869 controller registers used for Extended mode. Send C0h
	 * followed by the register number, Axh or Bxh. */
	if (!sndsb_write_dsp_timeout(cx,0xC0,20000UL)) return -1;
	if (!sndsb_write_dsp_timeout(cx,reg,20000UL)) return -1;
	return sndsb_read_dsp(cx);
}

int sndsb_ess_write_controller(struct sndsb_ctx *cx,int reg,unsigned char value) {
	if (reg < 0xA0 || reg >= 0xC0) return -1;
	if (sndsb_ess_set_extended_mode(cx,1) == 0) return -1;
	if (!sndsb_write_dsp_timeout(cx,reg,20000UL)) return -1;
	if (!sndsb_write_dsp_timeout(cx,value,20000UL)) return -1;
	return 0;
}

