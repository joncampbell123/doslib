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
int dpmi_linear_lock(uint32_t lin,uint32_t size) {
	int retv = 0;

	__asm {
		mov	ax,0x0600
		mov	cx,word ptr lin
		mov	bx,word ptr lin+2
		mov	di,word ptr size
		mov	si,word ptr size+2
		int	0x31
		jc	endf
		mov	retv,1
endf:
	}

	return retv;
}

int dpmi_linear_unlock(uint32_t lin,uint32_t size) {
	int retv = 0;

	__asm {
		mov	ax,0x0601
		mov	cx,word ptr lin
		mov	bx,word ptr lin+2
		mov	di,word ptr size
		mov	si,word ptr size+2
		int	0x31
		jc	endf
		mov	retv,1
endf:
	}

	return retv;
}

void *dpmi_linear_alloc(uint32_t try_lin,uint32_t size,uint32_t flags,uint32_t *handle) {
	void *retv = 0;
	uint32_t han = 0;

	__asm {
		mov	ax,0x0504
		mov	ebx,try_lin
		mov	ecx,size
		mov	edx,flags
		int	0x31
		jc	endf
		mov	retv,ebx
		mov	han,esi
endf:
	}

	if (retv != NULL && handle != NULL)
		*handle = han;

	return retv;
}

int dpmi_linear_free(uint32_t handle) {
	int retv = 0;

	__asm {
		mov	ax,0x0502
		mov	di,word ptr handle
		mov	si,word ptr handle+2
		int	0x31
		jc	endf
		mov	retv,1
endf:
	}

	return retv;
}
#endif

