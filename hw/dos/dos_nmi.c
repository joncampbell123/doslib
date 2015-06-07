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

#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* TODO: This should be moved into the hw/DOS library */
unsigned char			nmi_32_hooked = 0;
int				nmi_32_refcount = 0;
void				(interrupt *nmi_32_old_vec)() = NULL;
#endif

#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* NMI reflection (32-bit -> 16-bit)
   This code is VITAL if we want to work with SBOS and MEGA-EM
   from protected mode. */
static struct dpmi_realmode_call nmi_32_nr={0};
static void interrupt far nmi_32() {
	/* trigger a real-mode INT 02h */
	__asm {
		mov	ax,0x0300
		mov	bx,0x02
		xor	cx,cx
		mov	edi,offset nmi_32_nr	; we trust Watcom has left ES == DS
		int	0x31			; call DPMI
	}
}

void do_nmi_32_unhook() {
	if (nmi_32_refcount > 0)
		nmi_32_refcount--;

	if (nmi_32_refcount == 0) {
		if (nmi_32_hooked) {
			nmi_32_hooked = 0;
			_dos_setvect(0x02,nmi_32_old_vec);
			nmi_32_old_vec = NULL;
		}
	}
}

void do_nmi_32_hook() {
	if (nmi_32_refcount == 0) {
		if (!nmi_32_hooked) {
			nmi_32_hooked = 1;
			nmi_32_old_vec = _dos_getvect(0x02);
			_dos_setvect(0x02,nmi_32);
		}
	}
	nmi_32_refcount++;
}
#endif

