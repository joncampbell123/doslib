
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

int sndsb_query_dsp_version(struct sndsb_ctx *cx) {
	int a,b;

	if (!sndsb_write_dsp(cx,SNDSB_DSPCMD_GET_VERSION))
		return 0;

	if ((a=sndsb_read_dsp(cx)) < 0)
		return 0;
	if ((b=sndsb_read_dsp(cx)) < 0)
		return 0;
	if (a == 0xFF || b == 0xFF)
		return 0;

	cx->dsp_vmaj = (uint8_t)a;
	cx->dsp_vmin = (uint8_t)b;
	DEBUG(fprintf(stdout,"sndsb_query_dsp_version() == v%u.%u\n",a,b));
	return 1;
}

