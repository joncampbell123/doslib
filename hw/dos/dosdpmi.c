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
;--------------------------------------------------------------
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
;--------------------------------------------------------------
err1:
				pop	es
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

