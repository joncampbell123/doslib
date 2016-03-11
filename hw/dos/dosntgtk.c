
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
#include <hw/dos/dosntvdm.h>

/* DEBUG: Flush out calls that aren't there */
#ifdef TARGET_OS2
# define int86 ___EVIL___
# define int386 ___EVIL___
# define ntvdm_RegisterModule ___EVIL___
# define ntvdm_UnregisterModule ___EVIL___
# define _dos_getvect ___EVIL___
# define _dos_setvect ___EVIL___
#endif

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
uint32_t ntvdm_dosntast_GetTickCount() {
	uint32_t retv = 0xFFFFFFFFUL;

	if (ntvdm_dosntast_handle == DOSNTAST_HANDLE_UNASSIGNED)
		return 0; /* failed */
	if (!ntvdm_rm_code_alloc())
		return 0;

#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.ebx = DOSNTAST_GET_TICK_COUNT_C;
		ntvdm_DispatchCall_dpmi(ntvdm_dosntast_handle,&rc);
		retv = rc.ebx;
	}
#else
	{
		const uint16_t h = ntvdm_dosntast_handle;

		__asm {
			.386p
			push	ebx
			mov	ebx,DOSNTAST_GET_TICK_COUNT
			mov	ax,h
			ntvdm_Dispatch_ins_asm_db
			mov	retv,ebx
			pop	ebx
		}
	}
#endif

	return retv;
}
#endif

