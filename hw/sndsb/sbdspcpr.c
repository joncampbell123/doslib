
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

int sndsb_read_dsp_copyright(struct sndsb_ctx *cx,char *buf,unsigned int buflen) {
	unsigned int i;
	int c;

	if (buflen == 0) return 0;
	sndsb_write_dsp(cx,0xE3);
	for (i=0;i < (buflen-1U);i++) {
		c = sndsb_read_dsp(cx);
		if (c < 0) break;
		cx->dsp_copyright[i] = (char)c;
		if (c == 0) break;
	}
	cx->dsp_copyright[i] = (char)0;
	DEBUG(fprintf(stdout,"sndsb_init_card() copyright == '%s'\n",cx->dsp_copyright));
	return 1;
}

