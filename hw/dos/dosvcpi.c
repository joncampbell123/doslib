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

unsigned char vcpi_probed = 0;
unsigned char vcpi_present = 0;
unsigned char vcpi_major_version,vcpi_minor_version;

/* NTS: According to the VCPI specification this call is only supposed to report
 *      the physical memory address for anything below the 1MB boundary. And so
 *      far EMM386.EXE strictly adheres to that rule by not reporting success for
 *      addresses >= 1MB. The 32-bit limitation is a result of the VCPI system
 *      call, since the physical address is returned in EDX. */
uint32_t dos_linear_to_phys_vcpi(uint32_t pn) {
	uint32_t r=0xFFFFFFFFUL;

	__asm {
		.586p
		mov	ax,0xDE06
		mov	ecx,pn
		int	67h
		or	ah,ah
		jnz	err1		; if AH == 0 then EDX = page phys addr
		mov	r,edx
err1:
	}

	return r;
}

#if !defined(TARGET_WINDOWS)
static int int67_null() {
	uint32_t ptr;

#if TARGET_MSDOS == 32
	ptr = ((uint32_t*)0)[0x67];
#else
	ptr = *((uint32_t far*)MK_FP(0,0x67*4));;
#endif

	return (ptr == 0);
}
#endif

int probe_vcpi() {
#if defined(TARGET_WINDOWS)
	if (!vcpi_probed) {
		/* NTS: Whoever said Windows 3.0 used VCPI at it's core, is a liar */
		vcpi_probed = 1;
		vcpi_present = 0;
	}
#else
/* =================== MSDOS ================== */
	unsigned char err=0xFF;

	if (!vcpi_probed) {
		vcpi_probed = 1;

		/* if virtual 8086 mode isn't active, then VCPI isn't there */
		/* FIXME: What about cases where VCPI might be there, but is inactive (such as: EMM386.EXE resident but disabled) */
		if (!cpu_v86_active)
			return 0;

		/* NOTE: VCPI can be present whether we're 16-bit real mode or
		 * 32-bit protected mode. Unlike DPMI we cannot assume it's
		 * present just because we're 32-bit. */

		/* Do not call INT 67h if the vector is uninitialized */
		if (int67_null())
			return 0;

		/* Do not attempt to probe for VCPI if Windows 3.1/95/98/ME
		 * is running. Windows 9x blocks VCPI and if called, interrupts
		 * our execution to inform the user that the program should be
		 * run in DOS mode. */
		detect_windows();
		if (windows_mode != WINDOWS_NONE)
			return 0;

		/* NTS: we load DS for each var because Watcom might put it in
		 *      another data segment, especially in Large memory model.
		 *      failure to account for this was the cause of mysterious
		 *      hangs and crashes. */
		__asm {
			; NTS: Save DS and ES because Microsoft EMM386.EXE
			;      appears to change their contents.
			push	ds
			push	es
			mov	ax,0xDE00
			int	67h
			mov	err,ah

			mov	ax,seg vcpi_major_version
			mov	ds,ax
			mov	vcpi_major_version,bh

			mov	ax,seg vcpi_minor_version
			mov	ds,ax
			mov	vcpi_minor_version,bl

			pop	es
			pop	ds
		}

		if (err != 0)
			return 0;

		vcpi_present = 1;
	}
#endif

	return vcpi_present;
}

