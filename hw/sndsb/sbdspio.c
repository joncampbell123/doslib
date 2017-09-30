
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

int sndsb_read_dsp(struct sndsb_ctx * const cx) {
    /* TODO: convert this to use cx->dspio_adj_base so we don't compute baseio+READ_STATUS+alias every time */
    const unsigned int status_port = cx->baseio+SNDSB_BIO_DSP_READ_STATUS+(cx->dsp_alias_port?1:0);
    const unsigned int read_data_port = cx->baseio+SNDSB_BIO_DSP_READ_DATA+(cx->dsp_alias_port?1:0);
    register unsigned int pa_hi = (unsigned int)(SNDSB_IO_COUNTDOWN >> 16UL) + 1U;
    register unsigned char c; /* encourage C/C++ optimizer to convert c = inp() ... return c to just return inp() */

    do {
        unsigned int pa_lo = (unsigned int)(SNDSB_IO_COUNTDOWN & 0xFFFFUL);

        do {
            if (inp(status_port) & 0x80) {/* data available? */
                c = inp(read_data_port);
                DEBUG(fprintf(stdout,"sndsb_read_dsp() == 0x%02X\n",c));
                return c;
            }
        } while (--pa_lo != 0U);
    } while (--pa_hi != 0U);

    DEBUG(fprintf(stdout,"sndsb_read_dsp() read timeout\n"));
    return -1;
}

int sndsb_write_dsp(struct sndsb_ctx * const cx,const uint8_t d) {
    /* TODO: convert this to use cx->dspio_adj_base so we don't compute baseio+READ_STATUS+alias every time */
    const unsigned int status_port = cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0);
    const unsigned int write_data_port = cx->baseio+SNDSB_BIO_DSP_WRITE_DATA+(cx->dsp_alias_port?1:0);
    register unsigned int pa_hi = (unsigned int)(SNDSB_IO_COUNTDOWN >> 16UL) + 1U;

	DEBUG(fprintf(stdout,"sndsb_write_dsp(0x%02X)\n",d));

    do {
        register unsigned int pa_lo = (unsigned int)(SNDSB_IO_COUNTDOWN & 0xFFFFUL);

        do {
            if ((inp(status_port) & 0x80) == 0) {
                outp(write_data_port,d);
                return 1;
            }
        } while (--pa_lo != 0U);
    } while (--pa_hi != 0U);

	DEBUG(fprintf(stdout,"sndsb_write_dsp() timeout\n"));
	return 0;
}

