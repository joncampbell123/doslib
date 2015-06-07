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

#if defined(TARGET_MSDOS) && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* Windows 9x/NT Close-awareness */
void dos_close_awareness_cancel() {
	__asm {
		.386p
		mov	ax,0x168F
		mov	dx,0x0300
		int	0x2F
	}
}

void dos_close_awareness_ack() {
	__asm {
		.386p
		mov	ax,0x168F
		mov	dx,0x0200
		int	0x2F
	}
}

int dos_close_awareness_enable(unsigned char en) {
	uint16_t r=0;

	en = (en != 0) ? 1 : 0;

	__asm {
		.386p
		mov	ax,0x168F
		xor	dx,dx
		mov	dl,en
		int	0x2F
		mov	r,ax
	}

	return (int)r;
}

int dos_close_awareness_query() {
	uint16_t r=0;

	__asm {
		.386p
		mov	ax,0x168F
		mov	dx,0x0100
		int	0x2F
		mov	r,ax
	}

	if (r == 0x168F)
		return -1;

	return (int)r;
}

int dos_close_awareness_available() {
	/* "close-awareness" is provided by Windows */
	return (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT);
}

void dos_vm_yield() {
	__asm {
		mov	ax,0x1680	/* RELEASE VM TIME SLICE */
			xor	bx,bx		/* THIS VM */
			int	0x2F
	}
}
#endif

