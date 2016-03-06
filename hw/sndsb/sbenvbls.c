
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

struct sndsb_ctx *sndsb_try_blaster_var() {
	int A=-1,I=-1,D=-1,H=-1,P=-1;
	struct sndsb_ctx *e;
	char *s;

	if (sndsb_card_blaster != NULL)
		return sndsb_card_blaster;

	s = getenv("BLASTER");
	if (s == NULL) return NULL;

	while (*s != 0) {
		if (*s == ' ') {
			s++;
			continue;
		}

		if (*s == 'A') {
			s++;
			A = strtol(s,&s,16);
		}
		else if (*s == 'P') {
			s++;
			P = strtol(s,&s,16);
		}
		else if (*s == 'I') {
			s++;
			I = strtol(s,&s,10);
		}
		else if (*s == 'D') {
			s++;
			D = strtol(s,&s,10);
		}
		else if (*s == 'H') {
			s++;
			H = strtol(s,&s,10);
		}
		else {
			while (*s && *s != ' ') s++;
			while (*s == ' ') s++;
		}
	}

	if (A < 0 || I < 0 || D < 0 || I > 15 || D > 7)
		return NULL;

	if (sndsb_by_base(A) != NULL)
		return 0;

	e = sndsb_alloc_card();
	if (e == NULL) return NULL;
#if TARGET_MSDOS == 32
	e->goldplay_dma = NULL;
#endif
	e->is_gallant_sc6600 = 0;
	e->baseio = (uint16_t)A;
	e->mpuio = (uint16_t)(P > 0 ? P : 0);
	e->dma8 = (int8_t)D;
	e->dma16 = (int8_t)H;
	e->irq = (int8_t)I;
	e->dsp_vmaj = 0;
	e->dsp_vmin = 0;
	e->mixer_ok = 0;
	e->mixer_probed = 0;
	e->dsp_ok = 0;
	return (sndsb_card_blaster=e);
}

