/* dos.c
 *
 * Code to detect the surrounding DOS/Windows environment and support routines to work with it
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#ifdef TARGET_WINDOWS
# include <windows.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosntvdm.h>

unsigned char FAR *dos_mcb_get_psp(struct dos_mcb_enum *e) {
	if (e->psp < 0x80 || e->psp == 0xFFFFU)
		return NULL;

#if TARGET_MSDOS == 32
	return (unsigned char*)((uint32_t)e->psp << 4UL);
#else
	return (unsigned char FAR*)MK_FP(e->psp,0);
#endif
}

int mcb_name_is_junk(char *s/*8 char*/) {
	int junk=0,i;
	unsigned char c;

	for (i=0;i < 8;i++) {
		c = (unsigned char)s[i];
		if (c == 0)
			break;
		else if (c < 32 || c >= 127)
			junk = 1;
	}

	return junk;
}

uint16_t dos_mcb_first_segment() {
	if (dos_LOL == NULL)
		return 0;

	return *((uint16_t FAR*)(dos_LOL-2)); /* NTS: This is not a mistake. You take the pointer given by DOS and refer to the WORD starting 2 bytes PRIOR. I don't know why they did that... */
}

void mcb_filter_name(struct dos_mcb_enum *e) {
	if (e->psp > 0 && e->psp < 0x80) { /* likely special DOS segment */
		if (!memcmp(e->name,"SC",2) || !memcmp(e->name,"SD",2))
			memset(e->name+2,0,6);
		else
			memset(e->name,0,8);
	}
	else if (mcb_name_is_junk(e->name)) {
		memset(e->name,0,8);
	}
}

int dos_mcb_next(struct dos_mcb_enum *e) {
	unsigned char FAR *mcb;
	unsigned int i;
	uint16_t nxt;

	if (e->type == 0x5A || e->segment == 0x0000U || e->segment == 0xFFFFU)
		return 0;
	if (e->counter >= 16384)
		return 0;

#if TARGET_MSDOS == 32
	mcb = (unsigned char*)((uint32_t)(e->segment) << 4UL);
	e->ptr = mcb + 16;
#else
	mcb = (unsigned char far*)(MK_FP(e->segment,0));
	e->ptr = (unsigned char far*)(MK_FP(e->segment+1U,0));
#endif

	e->cur_segment = e->segment;
	e->type = *((uint8_t FAR*)(mcb+0));
	e->psp = *((uint16_t FAR*)(mcb+1));
	e->size = *((uint16_t FAR*)(mcb+3));
	for (i=0;i < 8;i++) e->name[i] = mcb[i+8]; e->name[8] = 0;
	if (e->type != 0x5A && e->type != 0x4D) return 0;
	nxt = e->segment + e->size + 1;
	if (nxt <= e->segment) return 0;
	e->segment = nxt;
	return 1;
}

int dos_mcb_first(struct dos_mcb_enum *e) {
	if (dos_LOL == NULL)
		return 0;

	e->counter = 0;
	e->segment = dos_mcb_first_segment();
	if (e->segment == 0x0000U || e->segment == 0xFFFFU)
		return 0;

	return dos_mcb_next(e);
}

