
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

struct sndsb_ctx *sndsb_by_irq(int8_t x) {
	int i;

	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		if (sndsb_card[i].irq == x)
			return sndsb_card+i;
	}

	return 0;
}

struct sndsb_ctx *sndsb_by_dma(uint16_t x) {
	int i;

	for (i=0;i < SNDSB_MAX_CARDS;i++) {
		if (sndsb_card[i].baseio > 0 && (sndsb_card[i].dma8 == x || sndsb_card[i].dma16 == x))
			return sndsb_card+i;
	}

	return 0;
}

