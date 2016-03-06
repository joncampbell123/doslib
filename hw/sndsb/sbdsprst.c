
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

int sndsb_reset_dsp(struct sndsb_ctx *cx) {
	if (cx->baseio == 0) {
		DEBUG(fprintf(stdout,"BUG: sndsb baseio == 0\n"));
		return 0;
	}

	/* DSP reset takes the ESS out of extended mode */
	if (cx->ess_chipset != 0)
		cx->ess_extended_mode = 0;

	/* DSP reset procedure */
	/* "write 1 to the DSP and wait 3 microseconds" */
	DEBUG(fprintf(stdout,"sndsb_reset_dsp() reset in progress\n"));
	if (cx->ess_extensions)
		outp(cx->baseio+SNDSB_BIO_DSP_RESET,3); /* ESS reset and flush FIFO */
	else
		outp(cx->baseio+SNDSB_BIO_DSP_RESET,1); /* normal reset */

	t8254_wait(t8254_us2ticks(1000));	/* be safe and wait 1ms */
	outp(cx->baseio+SNDSB_BIO_DSP_RESET,0);

	/* wait for the DSP to return 0xAA */
	/* "typically the DSP takes about 100us to initialize itself" */
	if (sndsb_read_dsp(cx) != 0xAA) {
		if (sndsb_read_dsp(cx) != 0xAA) {
			DEBUG(fprintf(stdout,"sndsb_read_dsp() did not return satisfactory answer\n"));
			return 0;
		}
	}

	return 1;
}

