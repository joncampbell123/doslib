
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

unsigned int sndsb_bread_dsp(struct sndsb_ctx * const cx,unsigned char *buf,unsigned int count) {
	register unsigned int readcount = 0;
    register int c;

    while ((count--) != 0U) {
        if ((c=sndsb_read_dsp(cx)) >= 0) {
            *buf++ = (unsigned char)c;
            readcount++;
        }
        else {
            break;
        }
    }

	return readcount;
}

unsigned int sndsb_bwrite_dsp(struct sndsb_ctx * const cx,const unsigned char *buf,unsigned int count) {
    register unsigned int writecount = 0;

    while ((count--) > 0) {
        if (sndsb_write_dsp(cx,*buf++)) {
            writecount++;
        }
        else {
            break;
        }
    }

	return writecount;
}

