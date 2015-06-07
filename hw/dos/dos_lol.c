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

/* DOS "list of lists" pointer */
unsigned char FAR *dos_LOL=NULL;

/* MS-DOS "list of lists" secret call */
#if TARGET_MSDOS == 32
# ifdef WIN386
unsigned char *dos_list_of_lists() {
	return NULL;/*not implemented*/
}
# else
static void dos_realmode_call(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x0021
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}

unsigned char *dos_list_of_lists() {
	struct dpmi_realmode_call rc={0};

	rc.eax = 0x5200;
	dos_realmode_call(&rc);
	if (rc.flags & 1) return NULL; /* CF */
	return (dos_LOL = ((unsigned char*)((rc.es << 4) + (rc.ebx & 0xFFFFUL))));
}
# endif
#else
unsigned char far *dos_list_of_lists() {
	unsigned int s=0,o=0;

	__asm {
		mov	ah,0x52
		int	21h
		jc	notwork
		mov	s,es
		mov	o,bx
notwork:
	}

	return (dos_LOL = ((unsigned char far*)MK_FP(s,o)));
}
#endif

