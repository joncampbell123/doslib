
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

/* DEBUG: Flush out calls that aren't there */
#ifdef TARGET_OS2
# define int86 ___EVIL___
# define int386 ___EVIL___
# define ntvdm_RegisterModule ___EVIL___
# define ntvdm_UnregisterModule ___EVIL___
# define _dos_getvect ___EVIL___
# define _dos_setvect ___EVIL___
#endif

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
void (*detect_windows_ntdvm_dosntast_init_CB)() = NULL;

void detect_windows_ntdvm_dosntast_init_func() {
	OSVERSIONINFO ovi;

	if (!ntvdm_dosntast_init())
		return;

	/* OK. Ask for the version number */
	memset(&ovi,0,sizeof(ovi));
	ovi.dwOSVersionInfoSize = sizeof(ovi);
	if (ntvdm_dosntast_getversionex(&ovi)) {
		windows_version_method = "GetVersionEx [NTVDM.EXE + DOSNTAST.VDD]";
		windows_version = (ovi.dwMajorVersion << 8) | ovi.dwMinorVersion;
		if (ovi.dwPlatformId == 2/*VER_PLATFORM_WIN32_NT*/)
			windows_mode = WINDOWS_NT;
		else
			windows_mode = WINDOWS_ENHANCED;
	}
}
#endif

