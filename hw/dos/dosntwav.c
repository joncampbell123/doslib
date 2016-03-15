
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
unsigned int ntvdm_dosntast_waveOutGetNumDevs() {
	if (ntvdm_dosntast_io_base == 0)
		return 0;

	/* FUNCTION */
	outpw(ntvdm_dosntast_io_base+0,DOSNTAST_FUNCTION_WINMM);
	/* SUBFUNCTION */
	outpw(ntvdm_dosntast_io_base+1,DOSNTAST_FUN_WINMM_SUB_waveOutGetNumDevs);
	/* COMMAND */
	return inpw(ntvdm_dosntast_io_base+2);
}

uint32_t ntvdm_dosntast_waveOutGetDevCaps(uint32_t uDeviceID,WAVEOUTCAPS *pwoc,uint16_t cbwoc) {
	uint32_t retv=0,port;

	if (ntvdm_dosntast_io_base == 0)
		return 0;

	/* FUNCTION */
	outpw(ntvdm_dosntast_io_base+0,DOSNTAST_FUNCTION_WINMM);
	/* SUBFUNCTION */
	outpw(ntvdm_dosntast_io_base+1,DOSNTAST_FUN_WINMM_SUB_waveOutGetDevCaps);
	/* COMMAND */
	port = ntvdm_dosntast_io_base+2;

#if TARGET_MSDOS == 32
	__asm {
		.386p
		pushad
		/* we trust Watcom left ES == DS */
		mov		edx,port
		mov		eax,uDeviceID
		movzx		ebx,cbwoc
		mov		ecx,1
		mov		edi,pwoc
		rep		insb
		popad
	}
#elif defined(__LARGE__) || defined(__COMPACT__) || defined(__HUGE__)
	__asm {
		.386p
		push		es
		pushad
		mov		edx,port
		mov		eax,uDeviceID
		movzx		ebx,cbwoc
		mov		ecx,1
		les		di,pwoc
		rep		insb
		popad
		pop		es
	}
#else
	__asm {
		.386p
		pushad
		push		es
		mov		ax,ds
		mov		es,ax
		mov		edx,port
		mov		eax,uDeviceID
		movzx		ebx,cbwoc
		mov		ecx,1
		mov		di,pwoc
		rep		insb
		pop		es
		popad
	}
#endif

	return retv;
}
#endif

