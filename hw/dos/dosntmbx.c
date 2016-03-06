
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
int ntvdm_dosntast_MessageBox(const char *text) {
	uint16_t port;

	if (ntvdm_dosntast_io_base == 0)
		return 0;

	/* FUNCTION */
	outpw(ntvdm_dosntast_io_base+0,DOSNTAST_FUNCTION_GENERAL);
	/* SUBFUNCTION */
	outpw(ntvdm_dosntast_io_base+1,DOSNTAST_FUN_GEN_SUB_MESSAGEBOX);
	/* COMMAND */
	port = ntvdm_dosntast_io_base+2;
#if TARGET_MSDOS == 32
	__asm {
		.386p
		push	esi
		push	ecx
		push	edx
		cld
		movzx	edx,port
		mov	esi,text
		mov	ecx,1		/* NTS: duration dosn't matter, our DLL passes DS:SI directly to MessageBoxA */
		rep	outsb
		pop	edx
		pop	ecx
		pop	esi
	}
#elif defined(__LARGE__) || defined(__COMPACT__)
	__asm {
		.386p
		push	ds
		push	si
		push	cx
		push	dx
		cld
		mov	dx,port
		lds	si,text
		mov	cx,1
		rep	outsb
		pop	dx
		pop	cx
		pop	si
		pop	ds
	}
#else
	__asm {
		.386p
		push	si
		push	cx
		push	dx
		cld
		mov	dx,port
		mov	si,text
		mov	cx,1
		rep	outsb
		pop	dx
		pop	cx
		pop	si
	}
#endif

	return 1;
}
#endif

