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

#if TARGET_MSDOS == 32
void *dpmi_alloc_dos(unsigned long len,uint16_t *selector) {
	unsigned short rm=0,pm=0,fail=0;

	/* convert len to paragraphs */
	len = (len + 15) >> 4UL;
	if (len >= 0xFF00UL) return NULL;

	__asm {
		mov	bx,WORD PTR len
		mov	ax,0x100
		int	0x31

		mov	rm,ax
		mov	pm,dx
		sbb	ax,ax
		mov	fail,ax
	}

	if (fail) return NULL;

	*selector = pm;
	return (void*)((unsigned long)rm << 4UL);
}

void dpmi_free_dos(uint16_t selector) {
	__asm {
		mov	ax,0x101
		mov	dx,selector
		int	0x31
	}
}
#endif

