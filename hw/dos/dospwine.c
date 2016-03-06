
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

const char *windows_emulation_comment_str = NULL;
uint8_t windows_emulation = 0;

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
/* it's nice to know if we're running under WINE (and therefore possibly Linux)
 * as opposed to real Windows, so that we can adjust our techniques accordingly.
 * I doubt for example that WINE would support Windows NT's NTVDM.EXE BOP codes,
 * or that our Win9x compatible VxD enumeration would know not to try enumerating
 * drivers. */
void win32_probe_for_wine() { /* Probing for WINE from the Win32 layer */
	HMODULE ntdll;

	ntdll = LoadLibrary("NTDLL.DLL");
	if (ntdll) {
		const char *(__stdcall *p)() = (const char *(__stdcall *)())GetProcAddress(ntdll,"wine_get_version");
		if (p != NULL) {
			windows_emulation = WINEMU_WINE;
			windows_emulation_comment_str = p(); /* and the function apparently returns a string */
		}
		FreeLibrary(ntdll);
	}
}
#elif defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
void win16_probe_for_wine() { /* Probing for WINE from the Win16 layer */
	DWORD callw,retv;

	if (!genthunk32_init()) return;
	if (genthunk32w_ntdll == 0) return;

	callw = __GetProcAddress32W(genthunk32w_ntdll,"wine_get_version");
	if (callw == 0) return;

	retv = __CallProcEx32W(CPEX_DEST_STDCALL/*nothing to convert*/,0/*0 param*/,callw);
	if (retv == 0) return;

	windows_emulation = WINEMU_WINE;
	{
		/* NTS: We assume that WINE, just like real Windows, will not move or relocate
		 *      NTDLL.DLL and will not move the string it just returned. */
		/* TODO: You need a function the host program can call to free the selector
		 *       you allocated here, in case it wants to reclaim resources */
		uint16_t sel;
		uint16_t myds=0;
		__asm mov myds,ds
		sel = AllocSelector(myds);
		if (sel != 0) {
			/* the selector is directed at the string, then retained in this
			 * code as a direct FAR pointer to WINE's version string */
			SetSelectorBase(sel,retv);
			SetSelectorLimit(sel,0xFFF);	/* WINE's version string is rarely longer than 14 chars */
			windows_emulation_comment_str = MK_FP(sel,0);
		}
	}
}
#endif

