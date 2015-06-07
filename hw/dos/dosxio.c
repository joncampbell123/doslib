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

#if TARGET_MSDOS == 32
int _dos_xread(int fd,void *buffer,int bsz) { /* NTS: The DOS extender takes care of translation here for us */
	int rd = -1;
	__asm {
		mov	ah,0x3F
		mov	ebx,fd
		mov	ecx,bsz
		mov	edx,buffer
		int	0x21
		mov	ebx,eax
		sbb	ebx,ebx
		or	eax,ebx
		mov	rd,eax
	}
	return rd;
}
#else
int _dos_xread(int fd,void far *buffer,int bsz) {
	int rd = -1;
	__asm {
		mov	ah,0x3F
		mov	bx,fd
		mov	cx,bsz
		mov	dx,word ptr [buffer+0]
		mov	si,word ptr [buffer+2]
		push	ds
		mov	ds,si
		int	0x21
		pop	ds
		mov	bx,ax
		sbb	bx,bx
		or	ax,bx
		mov	rd,ax
	}
	return rd;
}
#endif

#if TARGET_MSDOS == 32
int _dos_xwrite(int fd,void *buffer,int bsz) { /* NTS: The DOS extender takes care of translation here for us */
	int rd = -1;
	__asm {
		mov	ah,0x40
		mov	ebx,fd
		mov	ecx,bsz
		mov	edx,buffer
		int	0x21
		mov	ebx,eax
		sbb	ebx,ebx
		or	eax,ebx
		mov	rd,eax
	}
	return rd;
}
#else
int _dos_xwrite(int fd,void far *buffer,int bsz) {
	int rd = -1;
	__asm {
		mov	ah,0x40
		mov	bx,fd
		mov	cx,bsz
		mov	dx,word ptr [buffer+0]
		mov	si,word ptr [buffer+2]
		push	ds
		mov	ds,si
		int	0x21
		pop	ds
		mov	bx,ax
		sbb	bx,bx
		or	ax,bx
		mov	rd,ax
	}
	return rd;
}
#endif

