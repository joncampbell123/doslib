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

#if TARGET_MSDOS == 32 && defined(WIN386) /* Watcom Win386 does NOT translate LPARAM for us */
void far *win386_alt_winnt_MapAliasToFlat(DWORD farptr) {
	/* FIXME: This only works by converting a 16:16 pointer directly to 16:32.
	 *        It works perfectly fine in QEMU and DOSBox, but I seem to remember something
	 *        about the x86 architecture and possible ways you can screw up using 16-bit
	 *        data segments with 32-bit code. Are those rumors true? Am I going to someday
	 *        load up Windows 3.1/95 on an ancient PC and find out this code crashes
	 *        horribly or has random problems?
	 *
	 *        We need this alternate path for Windows NT/2000/XP/Vista/7 because NTVDM.EXE
	 *        grants Watcom386 a limited ~2GB limit for the flat data segment (usually
	 *        0x7B000000 or something like that) instead of the full 4GB limit the 3.x/9x/ME
	 *        kernels usually grant. This matters because without the full 4GB limit we can't
	 *        count on overflow/rollover to reach below our data segment base. Watcom386's
	 *        MapAliasToFlat() unfortunately assumes just that. If we were to blindly rely
	 *        on it, then we would work just fine under 3.x/9x/ME but crash under
	 *        NT/2000/XP/Vista/7 the minute we need to access below our data segment (such as,
	 *        when handling the WM_GETMINMAXINFO message the LPARAM far pointer usually
	 *        points somewhere into NTVDM.EXE's DOS memory area when we're usually located
	 *        at the 2MB mark or so) */
	return MK_FP(farptr>>16,farptr&0xFFFF);
}

void far *win386_help_MapAliasToFlat(DWORD farptr) {
	if (windows_mode == WINDOWS_NT)
		return win386_alt_winnt_MapAliasToFlat(farptr);

	return (void far*)MapAliasToFlat(farptr); /* MapAliasToFlat() returns near pointer, convert to far! */
}
#endif

