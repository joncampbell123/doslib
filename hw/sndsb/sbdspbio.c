
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

int sndsb_bread_dsp_timeout(struct sndsb_ctx *cx,unsigned long timeout_ms,unsigned char *buf,int count) {
	unsigned long pause = t8254_us2ticks(16);
	int c = 0;

    while ((count--) > 0) {
	    unsigned int patience = (unsigned int)(timeout_ms >> 4UL);

        do {
            if (inp(cx->baseio+SNDSB_BIO_DSP_READ_STATUS+(cx->dsp_alias_port?1:0)) & 0x80) { /* data available? */
                *buf++ = inp(cx->baseio+SNDSB_BIO_DSP_READ_DATA);
                c++;
                break;
            }

            if (patience-- == 0) {
                if (c == 0) return -1;
                break;
            }

            t8254_wait(pause);
        } while (1);
    }

	return c;
}

int sndsb_bwrite_dsp_timeout(struct sndsb_ctx *cx,unsigned long timeout_ms,const unsigned char *buf,int count) {
	unsigned long pause = t8254_us2ticks(16);
    int c = 0;

    while ((count--) > 0) {
        do {
            unsigned int patience = (unsigned int)(timeout_ms >> 4UL);

            if ((inp(cx->baseio+SNDSB_BIO_DSP_WRITE_STATUS+(cx->dsp_alias_port?1:0)) & 0x80) == 0) {
                outp(cx->baseio+SNDSB_BIO_DSP_WRITE_DATA+(cx->dsp_alias_port?1:0),*buf++);
                c++;
                break;
            }

            if (patience-- == 0) {
                if (c == 0) return -1;
                break;
            }

            t8254_wait(pause);
        } while (1);
    }

	return c;
}

