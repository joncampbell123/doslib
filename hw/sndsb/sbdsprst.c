
#include <conio.h>

#include <hw/8254/8254.h>
#include <hw/sndsb/sndsb.h>

/* uncomment this to enable debugging messages */
//#define DBG

#if defined(DBG)
# define DEBUG(x) (x)
#else
# define DEBUG(x)
#endif

int sndsb_reset_dsp(struct sndsb_ctx * const cx) {
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

