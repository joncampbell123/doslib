
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

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386)
int probe_dos_version_win9x_qt_thunk_func() {
	DWORD fptr,raw=0;

	if (!Win9xQT_ThunkInit())
		return 0;

	fptr = GetProcAddress16(win9x_kernel_win16,"GETVERSION");
	if (fptr != 0) {
		dos_version_method = "Read from Win16 GetVersion() [32->16 QT_Thunk]";

		__asm {
			mov	edx,fptr
			mov	eax,dword ptr [QT_Thunk]

			; QT_Thunk needs 0x40 byte of data storage at [EBP]
			; give it some, right here on the stack
			push	ebp
			mov	ebp,esp
			sub	esp,0x40

			call	eax	; <- QT_Thunk

			; release stack storage
			mov	esp,ebp
			pop	ebp

			; take Win16 response in DX:AX translate to EAX
			shl	edx,16
			and	eax,0xFFFF
			or	eax,edx
			mov	raw,eax
		}

		if (raw != 0) {
			dos_version = (((raw >> 24UL) & 0xFFUL) << 8UL) | (((raw >> 16UL) & 0xFFUL) << 0UL);
			return 1;
		}
	}

	return 0;
}
#endif

