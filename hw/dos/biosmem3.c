/* biosmem3.c
 *
 * Support functions for calling BIOS INT 15h E820 to query extended memory layout
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/dos/biosmem.h>

#if TARGET_MSDOS == 16
int _biosmem_size_E820_3(unsigned long *index,struct bios_E820 *nfo) {
	unsigned long idx = *index;
	unsigned long retv = 0;
	unsigned int len = 0;

	memset(nfo,0,sizeof(*nfo));

	__asm {
		push	es
		mov	eax,0xE820
		mov	ebx,idx
		mov	ecx,24
		mov	edx,0x534D4150
#if defined(__LARGE__) || defined(__COMPACT__)
		mov	di,word ptr [nfo+2]	; segment portion of far pointer
		mov	es,di
#endif
		mov	di,word ptr [nfo]	; offset of pointer
		int	15h
		pop	es
		jc	noway
		mov	retv,eax
		mov	idx,ebx
		mov	len,cx			; Watcom doesn't know what "CL" is? It's only the lower 8 bits of CX/ECX Duhhhh...
noway:
	}

	if (retv == 0x534D4150UL) {
		*index = idx;
		return len & 0xFF;
	}

	return 0;
}
#endif

