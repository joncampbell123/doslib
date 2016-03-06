
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

