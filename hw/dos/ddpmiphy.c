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
void *dpmi_phys_addr_map(uint32_t phys,uint32_t size) {
	uint32_t retv = 0;

	__asm {
		mov	ax,0x0800
		mov	cx,word ptr phys
		mov	bx,word ptr phys+2
		mov	di,word ptr size
		mov	si,word ptr size+2
		int	0x31
		jc	endf
		mov	word ptr retv,cx
		mov	word ptr retv+2,bx
endf:
	}

	return (void*)retv;
}

void dpmi_phys_addr_free(void *base) {
	__asm {
		mov	ax,0x0801
		mov	cx,word ptr base
		mov	bx,word ptr base+2
		int	0x31
	}
}
#endif

