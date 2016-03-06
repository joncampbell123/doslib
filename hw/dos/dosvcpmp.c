
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

