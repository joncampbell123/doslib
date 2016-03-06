
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

int sndsb_read_dsp_timeout(struct sndsb_ctx *cx,unsigned long timeout_ms) {
	unsigned int patience = (unsigned int)(timeout_ms >> 4UL);
	unsigned long pause = t8254_us2ticks(16);
	int c = -1;

	do {
		if (inp(cx->baseio+SNDSB_BIO_DSP_READ_STATUS+(cx->dsp_alias_port?1:0)) & 0x80) { /* data available? */
			c = inp(cx->baseio+SNDSB_BIO_DSP_READ_DATA);
			break;
		}

		t8254_wait(pause);
		if (--patience == 0) {
			DEBUG(fprintf(stdout,"sndsb_read_dsp() read timeout\n"));
			return -1;
		}
	} while (1);

	DEBUG(fprintf(stdout,"sndsb_read_dsp() == 0x%02X\n",c));
	return c;
}

int sndsb_write_dsp_timeout(struct sndsb_ctx *cx,uint8_t d,unsigned long timeout_ms) {
	unsigned int patience = (unsigned int)(timeout_ms >> 4UL);
	unsigned long pause = t8254_us2ticks(16);

	DEBUG(fprintf(stdout,"sndsb_write_dsp(0x%02X)\n",d));
	do {
		if (inp(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0)) & 0x80)
			t8254_wait(pause);
		else {
			outp(cx->baseio+SNDSB_BIO_DSP_WRITE_DATA+(cx->dsp_alias_port?1:0),d);
			return 1;
		}
	} while (--patience != 0);
	DEBUG(fprintf(stdout,"sndsb_write_dsp() timeout\n"));
	return 0;
}

