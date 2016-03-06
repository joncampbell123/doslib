
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

