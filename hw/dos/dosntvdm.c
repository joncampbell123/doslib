
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
uint16_t ntvdm_dosntast_detect() {
	const char *str = "DOSNTAST.VDD";
	uint16_t str_len = 12;
	uint16_t handle = (~0U);
	unsigned int i,max=0x200;
#if TARGET_MSDOS == 32
	unsigned char *p = (unsigned char*)0x400;
#else
	unsigned char FAR *p = (unsigned char FAR*)MK_FP(0x40,0x00);
#endif

	for (i=0;i <= (max-str_len);i++) {
#if TARGET_MSDOS == 32
		if (memcmp(str,p+i,str_len) == 0)
			handle = *((uint16_t*)(p+i+str_len));
#else
		if (_fmemcmp(str,p+i,str_len) == 0)
			handle = *((uint16_t FAR*)(p+i+str_len));
#endif

		if (ntvdm_RM_ERR(handle))
			handle = DOSNTAST_HANDLE_UNASSIGNED;
		else
			break;
	}

	return handle;
}
#endif

