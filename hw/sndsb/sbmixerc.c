
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

int sndsb_read_mixer(struct sndsb_ctx *cx,uint8_t i) {
	outp(cx->baseio+SNDSB_BIO_MIXER_INDEX,i);
	return inp(cx->baseio+SNDSB_BIO_MIXER_DATA);
}

void sndsb_write_mixer(struct sndsb_ctx *cx,uint8_t i,uint8_t d) {
	outp(cx->baseio+SNDSB_BIO_MIXER_INDEX,i);
	outp(cx->baseio+SNDSB_BIO_MIXER_DATA,d);
}

