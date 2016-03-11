
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
int ntvdm_dosntast_load_vdd() {
	uint32_t t1=0,t2=0;

	/* TODO: Right now this works for the current path, or if it's in the Windows system dir.
	 *       Adopt a strategy where the user can also set an environment variable to say where
	 *       the DLL is. */
	ntvdm_dosntast_handle = ntvdm_RegisterModule("DOSNTAST.VDD","Init","Dispatch");
	if (ntvdm_RM_ERR(ntvdm_dosntast_handle)) return 0;

	/* test out the dispatch call: give the DLL back his handle */
#if TARGET_MSDOS == 32
	{
		struct dpmi_realmode_call rc={0};
		rc.ebx = DOSNTAST_INIT_REPORT_HANDLE_C;
		rc.ecx = ntvdm_dosntast_handle;
		ntvdm_DispatchCall_dpmi(ntvdm_dosntast_handle,&rc);
		t1 = rc.ebx;
		t2 = rc.ecx;
	}
#else
	t1 = ntvdm_dosntast_handle;
	__asm {
		.386p
		push	ebx
		push	ecx
		mov	ebx,DOSNTAST_INIT_REPORT_HANDLE
		mov	eax,t1
		mov	ecx,eax
		ntvdm_Dispatch_ins_asm_db
		mov	t1,ebx
		mov	t2,ecx
		pop	ecx
		pop	ebx
	}
#endif

	if (t1 != 0x55AA55AA || !(t2 >= 0x400 && t2 <= 0x600)) {
		ntvdm_UnregisterModule(ntvdm_dosntast_handle);
		return 0;
	}
#if TARGET_MSDOS == 32
	if (memcmp((void*)t2,"DOSNTAST.VDD\xFF\xFF",14)) {
		ntvdm_UnregisterModule(ntvdm_dosntast_handle);
		return 0;
	}
	*((uint16_t*)(t2+12)) = ntvdm_dosntast_handle;
#else
	if (_fmemcmp(MK_FP(t2>>4,t2&0xF),"DOSNTAST.VDD\xFF\xFF",14)) {
		ntvdm_UnregisterModule(ntvdm_dosntast_handle);
		return 0;
	}
	*((uint16_t FAR*)MK_FP((t2+12)>>4,(t2+12)&0xF)) = ntvdm_dosntast_handle;
#endif

	return (ntvdm_dosntast_handle != DOSNTAST_HANDLE_UNASSIGNED)?1:0;
}
#endif

