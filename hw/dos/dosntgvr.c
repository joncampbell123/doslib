
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
unsigned int ntvdm_dosntast_getversionex(OSVERSIONINFO *ovi) {
	unsigned int retv=0;

	if (ntvdm_dosntast_handle == DOSNTAST_HANDLE_UNASSIGNED)
		return 0; /* failed */
	if (!ntvdm_rm_code_alloc())
		return 0;

#if TARGET_MSDOS == 32
	{
		uint16_t myds=0;
		struct dpmi_realmode_call rc={0};
		__asm mov myds,ds
		rc.ebx = DOSNTAST_GETVERSIONEX_C;
		rc.esi = (uint32_t)ovi;
		rc.ecx = 1;
		rc.ds = myds;
		ntvdm_DispatchCall_dpmi(ntvdm_dosntast_handle,&rc);
		retv = rc.ebx;
	}
#else
	{
		const uint16_t s = FP_SEG(ovi),o = FP_OFF(ovi),h = ntvdm_dosntast_handle;

		__asm {
			.386p
			push	ds
			push	esi
			push	ecx
			mov	ebx,DOSNTAST_GETVERSIONEX
			xor	esi,esi
			mov	ax,h
			mov	si,s
			mov	ds,si
			mov	si,o
			xor	cx,cx
			ntvdm_Dispatch_ins_asm_db
			mov	retv,bx
			pop	esi
			pop	ebx
			pop	ds
		}
	}
#endif

	return retv;
}
#endif

