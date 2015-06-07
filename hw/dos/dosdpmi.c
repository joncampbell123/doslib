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

#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
int8_t dpmi_no_0301h = -1; /* whether or not the DOS extender provides function 0301h */
#endif

#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS))
uint16_t dpmi_flags=0;
uint16_t dpmi_version=0;
unsigned char dpmi_init=0;
uint32_t dpmi_entry_point=0;
unsigned char dpmi_present=0;
unsigned char dpmi_processor_type=0;
uint16_t dpmi_private_data_length_paragraphs=0;
uint16_t dpmi_private_data_segment=0xFFFF;	/* when we DO enter DPMI, we store the private data segment here. 0 = no private data. 0xFFFF = not yet entered */
unsigned char dpmi_entered = 0;	/* 0=not yet entered, 16=entered as 16bit, 32=entered as 32bit */
uint64_t dpmi_rm_entry = 0;
uint32_t dpmi_pm_entry = 0;

/* once having entered DPMI, keep track of the selectors registers given to us in p-mode */
uint16_t dpmi_pm_cs,dpmi_pm_ds,dpmi_pm_es,dpmi_pm_ss;

void __cdecl dpmi_enter_core(); /* Watcom's inline assembler is too limiting to carry out the DPMI entry and switch back */
#endif

/* 16-bit real mode DOS or 16-bit protected mode Windows */
void probe_dpmi() {
#if defined(TARGET_OS2)
	/* OS/2 apps do not run under DPMI */
#elif TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
	/* Win32 apps do not bother with DPMI */
#else
	if (!dpmi_init) {
		/* BUGFIX: WINE (Wine Is Not an Emulator) can run Win16 applications
		 *         but does not emulate the various low level interrupts one
		 *         can call. To avoid crashing under WINE we must not use
		 *         direct interrupts. */
		if (windows_emulation == WINEMU_WINE) {
			dpmi_present = 0;
			dpmi_init = 1;
			return;
		}

		{
			unsigned char present=0,proc=0;
			uint16_t version=0,privv=0,flags=0;
			uint32_t entry=0;

			__asm {
				push	es
				mov	ax,0x1687
				int	2Fh
				or	ax,ax
				jnz	err1
				mov	present,1
				mov	flags,bx
				mov	proc,cl
				mov	version,dx
				mov	privv,si
				mov	word ptr [entry+0],di
				mov	word ptr [entry+2],es
				pop	es
err1:
			}

			dpmi_flags = flags;
			dpmi_present = present;
			dpmi_version = version;
			dpmi_entry_point = entry;
			dpmi_processor_type = proc;
			dpmi_private_data_segment = 0xFFFF;
			dpmi_private_data_length_paragraphs = privv;
		}

#if TARGET_MSDOS == 32 || defined(TARGET_WINDOWS)
		/* when we ask for the "entry point" we mean we want the real-mode entrypoint.
		 * apparently some DPMI servers like Windows XP+NTVDM.EXE translate ES:DI coming
		 * back to a protected mode entry point, which is not what we're looking for.
		 *
		 * Interesting fact: When compiled as a Win16 app, the DPMI call actually works,
		 * but returns an entry point appropriate for Win16 apps. So.... apparently we
		 * can enter DPMI protected mode from within Win16? Hmm.... that might be something
		 * fun to experiment with :) */
		if (dpmi_present) {
			struct dpmi_realmode_call rc={0};
			rc.eax = 0x1687;
			mux_realmode_2F_call(&rc);
			if ((rc.eax&0xFFFF) == 0) {
				dpmi_flags = rc.ebx&0xFFFF;
				dpmi_present = 1;
				dpmi_version = rc.edx&0xFFFF;
				dpmi_entry_point = (((uint32_t)rc.es << 16UL) & 0xFFFF0000UL) + (uint32_t)(rc.edi&0xFFFFUL);
				dpmi_processor_type = rc.ecx&0xFF;
				dpmi_private_data_segment = 0xFFFF;
				dpmi_private_data_length_paragraphs = rc.esi&0xFFFF;
			}
			else {
				dpmi_present = 0;
			}
		}
#endif

#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
		dpmi_no_0301h = 0;
#endif
		dpmi_init = 1;

#if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
		if (dpmi_present) {
			/* Thanks to bullshit like DOS4/GW we have to test the extender to know
			   whether or not core routines we need are actually there or not. If they
			   are not, then alternative workarounds are required. The primary reason
			   for this test is to avoid HIMEM.SYS API code returning nonsensical values
			   caused by DOS4/GW not supporting such vital functions as DPMI 0301H:
			   Call far real-mode procedure. Knowing this should also fix the VESA BIOS
			   test bug where the protected-mode version is unable to use the BIOS's
			   direct-call window bank-switching function.

			   At least, this code so far can rely on DPMI function 0300H: call real-mode
			   interrupt.*/

			/* test #1: allocate a 16-bit region, put a RETF instruction there,
			   and ask the DPMI server to call it (0301H test).

				Success:
				Registers unchanged
				CF=0

				Failure (DOS4/GW):
				CF=1
				AX=0301H  (wait wha?) */
			{
				uint16_t sel = 0;
				struct dpmi_realmode_call rc={0};
				unsigned char *proc = dpmi_alloc_dos(16,&sel);
				if (proc != NULL) {
					*proc = 0xCB; /* <- RETF */

					rc.cs = ((size_t)proc) >> 4UL;
					rc.ip = ((size_t)proc) & 0xFUL;
					if (dpmi_test_rm_entry_call(&rc) != 0)
						dpmi_no_0301h = 1;

					dpmi_free_dos(sel);
				}
			}
		}
#endif
	}
#endif
}

#if TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS)
int dpmi_private_alloc() {
	unsigned short sss=0;

	if (!dpmi_present || dpmi_private_data_segment != 0xFFFFU)
		return 1; /* OK, nothing to do */

	if (dpmi_private_data_length_paragraphs == 0) {
		dpmi_private_data_segment = 0;
		return 0;
	}

	__asm {
		push	ds
		mov	ah,0x48
		mov	bx,seg dpmi_private_data_length_paragraphs
		mov	ds,bx
		mov	bx,dpmi_private_data_length_paragraphs
		int	21h
		pop	ds
		jc	fail1
		mov	sss,ax
fail1:
	}

	if (sss == 0)
		return 0;

	dpmi_private_data_segment = sss;
	return 1;
}

/* NTS: This enters DPMI. There is no exit from DPMI. And if you re-enter DPMI by switching back to protected mode,
 *      you only serve to confuse the server somewhat.
 *
 *         Re-entry results:
 *           - Windows XP: Allows it, even going 16 to 32 bit mode, but the console window gets confused and drops our
 *             output when changing bit size.
 *           - Windows 9x: Allows it, doesn't allow changing bit mode, so once in 16-bit mode you cannot enter 32-bit mode.
 *             The mode persists until the DOS Box exits.
 *
 *      This also means that once you init in one mode, you cannot re-enter another mode. If you init in 16-bit DPMI,
 *      you cannot init into 32-bit DPMI.
 *
 *      If all you want is the best mode, call with mode == 0xFF instead to automatically select. */
int dpmi_enter(unsigned char mode) {
/* TODO: Cache results, only need to scan once */
	if (mode == 0xFF) {
		if ((cpu_basic_level == -1 || cpu_basic_level >= 3) && (dpmi_flags&1) == 1)
			mode = 32; /* if 32-bit capable DPMI server and 386 or higher, choose 32-bit */
		else
			mode = 16; /* for all else, choose 16-bit */
	}

	if (dpmi_entered != 0) {
		if (dpmi_entered != mode) return 0;
		return 1;
	}

	if (mode != 16 && mode != 32)
		return 0;
	if (mode == 32 && !(dpmi_flags & 1))
		return 0;
	if (dpmi_entry_point == 0)
		return 0;
	if (!dpmi_private_alloc())
		return 0;
	if (dpmi_private_data_length_paragraphs != 0 && dpmi_private_data_segment == 0)
		return 0;
	if (dpmi_private_data_segment == 0xFFFFU)
		return 0;

	dpmi_entered = mode;
	dpmi_enter_core();
	return (dpmi_entered != 0); /* NTS: dpmi_enter_core() will set dpmi_entered back to zero on failure */
}
#endif

#if TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* TODO: Switch into DPMI protected mode, allocate and setup selectors, do memcpy to
 *       DOS realmode segment, then return to real mode. When the code is stable,
 *       move it into hw/dos/dos.c. The purpose of this code is to allow 16-bit realmode
 *       DOS programs to reach into DPMI linear address space, such as the Win 9x
 *       kernel area when run under the Win 9x DPMI server. */
int dpmi_lin2fmemcpy(unsigned char far *dst,uint32_t lsrc,uint32_t sz) {
	if (dpmi_entered == 32)
		return dpmi_lin2fmemcpy_32(dst,lsrc,sz);
	else if (dpmi_entered == 16) {
		_fmemset(dst,0,sz);
		return dpmi_lin2fmemcpy_16(dst,lsrc,sz);
	}

	return 0;
}

int dpmi_lin2fmemcpy_init() {
	probe_dpmi();
	if (!dpmi_present)
		return 0;
	if (!dpmi_enter(DPMI_ENTER_AUTO))
		return 0;

	return 1;
}
#endif

