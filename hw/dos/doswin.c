
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

const char *windows_version_method = NULL;

/* return value:
 *   0 = not running under Windows
 *   1 = running under Windows */
const char *windows_emulation_comment_str = NULL;
uint8_t windows_emulation = 0;
uint16_t windows_version = 0;		/* NOTE: 0 for Windows NT */
uint8_t windows_mode = 0;
uint8_t windows_init = 0;

const char *windows_mode_strs[WINDOWS_MAX] = {
	"None",
	"Real",
	"Standard",
	"Enhanced",
	"NT",
	"OS/2"
};

/* TESTING (whether or not it correctly detects the presence of Windows):
 *    Note that in some columns the API returns insufficient information and the
 *    API has to use it's best guess on the correct value, which might be
 *    inaccurate or wrong (marked: GUESSES).
 *
 *    For Windows NT/2000/XP/Vista/7 tests, the code does not have any way of
 *    knowing (yet) which version of the NT kernel is involved, so the best
 *    it can offer is "I am running under NT" (marked as ONLY NT)
 *
 *    OS, shell, configuration & mode         Detects   Correct mode    Correct version
 *    Microsoft Windows 3.0 (DOSBox 0.74)
 *       386 Enhanced Mode                    YES       YES             YES
 *       286 Standard Mode                    YES       GUESSES         YES
 *       8086 Real Mode                       YES       GUESSES         YES
 *    Microsoft Windows 3.1 (DOSBox 0.74)
 *       386 Enhanced Mode                    YES       YES             YES
 *       286 Standard Mode                    YES       YES             YES
 *    Microsoft Windows 3.11 (DOSBox 0.74)
 *       386 Enhanced Mode                    YES       YES             YES*
 *       286 Standard Mode                    YES       YES             YES*
 *         * = Despite being v3.11 it still reports itself as v3.1
 *    Microsoft Windows 95 (4.00.950) (DOS 7.00) (Qemu)
 *       Normal                               YES       YES*            YES (4.0)
 *       Safe mode                            YES       YES*            YES (4.0)
 *       Prevent DOS apps detecting Windows   NO        -               -
 *         * = Reports self as "enhanced mode" which is really the only mode supported
 *    Microsoft Windows 95 OSR2.5 (4.00.950 C) (DOS 7.10) (Qemu)
 *       Normal                               YES       YES             YES (4.0)
 *       Safe mode                            YES       YES             YES (4.0)
 *    Microsoft Windows 95 SP1 (4.00.950a) (DOS 7.00) (Qemu)
 *       Normal                               YES       YES             YES (4.0)
 *       Safe mode                            YES       YES             YES (4.0)
 *    Microsoft Windows 98 (4.10.1998) (DOS 7.10) (Qemu)
 *       Normal                               YES       YES             YES (4.10)
 *       Safe mode                            YES       YES             YES (4.10)
 *    Microsoft Windows 98 SE (4.10.2222 A) (DOS 7.10) (Qemu)
 *       Normal                               YES       YES             YES (4.10)
 *       Safe mode                            YES       YES             YES (4.10)
 *    Microsoft Windows ME (4.90.3000) (DOS 8.00) (Qemu)
 *       Normal                               YES       YES             YES (4.90)
 *       Safe mode                            YES       YES             YES (4.90)
 *    Microsoft Windows 2000 Professional (5.00.2195) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
 *    Microsoft Windows XP Professional (5.1.2600) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
 *    Microsoft Windows XP Professional SP1 (5.1.2600) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
 *    Microsoft Windows XP Professional SP2 (5.1.2600) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
 *    Microsoft Windows XP Professional SP3 (5.1.2600) (VirtualBox)
 *       Normal                               YES       N/A             ONLY NT
*/

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

int detect_windows() {
#if defined(TARGET_WINDOWS)
# if TARGET_MSDOS == 32
#  ifdef WIN386
	/* Windows 3.0/3.1 with Win386 */
	if (!windows_init) {
		DWORD raw;

		windows_emulation = 0;
		windows_init = 1;
		windows_mode = WINDOWS_ENHANCED; /* most likely scenario is Windows 3.1 386 enhanced mode */

		raw = GetVersion();
		windows_version_method = "GetVersion";
		windows_version = (LOBYTE(LOWORD(raw)) << 8) | HIBYTE(LOWORD(raw));
		/* NTS: Microsoft documents GetVersion() as leaving bit 31 unset for NT, bit 31 set for Win32s and Win 9x/ME.
		 *      But that's not what the Win16 version of the function does! */

		if (dos_version == 0) probe_dos();

		/* Windows 3.1/9x/ME */
		raw = GetWinFlags();
		if (raw & WF_PMODE) {
			if (raw & WF_ENHANCED)
				windows_mode = WINDOWS_ENHANCED;
			else/* if (raw & WF_STANDARD)*/
				windows_mode = WINDOWS_STANDARD;
		}
		else {
			windows_mode = WINDOWS_REAL;
		}

		/* NTS: All 32-bit Windows systems since Windows NT 3.51 and Windows 95 return
		 *      major=3 minor=95 when Win16 applications query the version number. The only
		 *      exception to that rule is Windows NT 3.1, which returns major=3 minor=10,
		 *      the same version number returned by Windows 3.1. */
		if (windows_mode == WINDOWS_ENHANCED &&
			(dos_version >= 0x510 && dos_version <= 0x57f)/* MS-DOS v5.50 */ &&
			(windows_version == 0x035F /* Windows NT 4/2000/XP/Vista/7/8 */ ||
			 windows_version == 0x030A /* Windows NT 3.1/3.5x */)) {
			/* if the real DOS version is 5.50 then we're under NT */
			windows_mode = WINDOWS_NT;
		}

		switch (dos_version>>8) {
			case 10:	/* OS/2 1.x */
			case 20:	/* OS/2 2.x (low=0), and OS/2 Warp 3 (low=30), and OS/2 Warp 4 (low=40) */
				windows_version_method = "Deduce from DOS version";
				windows_version = dos_version;
				windows_mode = WINDOWS_OS2;
				break;
		};
	}
#  elif TARGET_WINDOWS >= 40 || defined(WINNT)
	/* Windows 95/98/ME/XP/2000/NT/etc. and Windows NT builds: We don't need to do anything.
	 * The fact we're running means Windows is present */
	/* TODO: Clarify which Windows: Are we running under NT? or 9X/ME? What version? */
	if (!windows_init) {
		OSVERSIONINFO ovi;

		windows_emulation = 0;
		windows_init = 1;
		memset(&ovi,0,sizeof(ovi));
		ovi.dwOSVersionInfoSize = sizeof(ovi);
		GetVersionEx(&ovi);

		windows_version_method = "GetVersionEx";
		windows_version = (ovi.dwMajorVersion << 8) | ovi.dwMinorVersion;
		if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT)
			windows_mode = WINDOWS_NT;
		else
			windows_mode = WINDOWS_ENHANCED; /* Windows 3.1 Win32s, or Windows 95/98/ME */

		win32_probe_for_wine();
	}
#  elif TARGET_WINDOWS == 31
	/* Windows 3.1 with Win32s target build. Note that such programs run in the Win32 layer on Win95/98/ME/NT/2000/etc. */
	/* TODO: Clarify which Windows, using the limited set of functions available under Win32s, or perhaps using GetProcAddress
	 *       to use the later GetVersionEx() functions offered by Win95/98/NT/etc. */
	if (!windows_init) {
		DWORD raw;

		windows_emulation = 0;
		windows_init = 1;
		windows_mode = WINDOWS_ENHANCED; /* Assume Windows 3.1 386 Enhanced Mode. This 32-bit app couldn't run otherwise */

		raw = GetVersion();
		windows_version_method = "GetVersion";
		windows_version = (LOBYTE(LOWORD(raw)) << 8) | HIBYTE(LOWORD(raw));
		if (!(raw & 0x80000000UL)) { /* FIXME: Does this actually work? */
			/* Windows NT/2000/XP/etc */
			windows_mode = WINDOWS_NT;
		}

		/* TODO: GetProcAddress() GetVersionEx() and get the REAL version number from Windows */

		win32_probe_for_wine();
	}
#  else
#   error Unknown 32-bit Windows variant
#  endif
# elif TARGET_MSDOS == 16
#  if TARGET_WINDOWS >= 30
	/* Windows 3.0/3.1, what we then want to know is what mode we're running under: Real? Standard? Enhanced?
	 * The API function GetWinFlags() only exists in 3.0 and higher, it doesn't exist under 2.x */
	/* TODO */
	if (!windows_init) {
		DWORD raw;

		windows_emulation = 0;
		windows_init = 1;
		windows_mode = WINDOWS_ENHANCED; /* most likely scenario is Windows 3.1 386 enhanced mode */

		raw = GetVersion();
		windows_version_method = "GetVersion";
		windows_version = (LOBYTE(LOWORD(raw)) << 8) | HIBYTE(LOWORD(raw));
		/* NTS: Microsoft documents GetVersion() as leaving bit 31 unset for NT, bit 31 set for Win32s and Win 9x/ME.
		 *      But that's not what the Win16 version of the function does! */

		if (dos_version == 0) probe_dos();

		/* Windows 3.1/9x/ME */
		raw = GetWinFlags();
		if (raw & WF_PMODE) {
			if (raw & WF_ENHANCED)
				windows_mode = WINDOWS_ENHANCED;
			else/* if (raw & WF_STANDARD)*/
				windows_mode = WINDOWS_STANDARD;
		}
		else {
			windows_mode = WINDOWS_REAL;
		}

		/* NTS: All 32-bit Windows systems since Windows NT 3.51 and Windows 95 return
		 *      major=3 minor=95 when Win16 applications query the version number. The only
		 *      exception to that rule is Windows NT 3.1, which returns major=3 minor=10,
		 *      the same version number returned by Windows 3.1. */
		if (windows_mode == WINDOWS_ENHANCED &&
			(dos_version >= 0x510 && dos_version <= 0x57f)/* MS-DOS v5.50 */ &&
			(windows_version == 0x035F /* Windows NT 4/2000/XP/Vista/7/8 */ ||
			 windows_version == 0x030A /* Windows NT 3.1/3.5x */)) {
			/* if the real DOS version is 5.50 then we're under NT */
			windows_mode = WINDOWS_NT;
		}

		switch (dos_version>>8) {
			case 10:	/* OS/2 1.x */
			case 20:	/* OS/2 2.x (low=0), and OS/2 Warp 3 (low=30), and OS/2 Warp 4 (low=40) */
				windows_version_method = "Deduce from DOS version";
				windows_version = dos_version;
				windows_mode = WINDOWS_OS2;
				break;
		};

		/* If we're running under Windows 9x/ME or Windows NT/2000 we can thunk our way into
		 * the Win32 world and call various functions to get a more accurate picture of the
		 * Windows system we are running on */
		/* NTS: Under Windows NT 3.51 or later this technique is the only way to get the
		 *      true system version number. The Win16 GetVersion() will always return
		 *      some backwards compatible version number except for NT 3.1:
		 *
		 *                   Win16             Win32
		 *               +==========================
		 *      NT 3.1   |   3.1               3.1
		 *      NT 3.51  |   3.1               3.51
		 *      NT 4.0   |   3.95              4.0
		 *      2000     |   3.95              5.0
		 *      XP       |   3.95              5.1
		 *      Vista    |   3.95              6.0
		 *      7        |   3.95              6.1
		 *      8        |   3.95              6.2
		 *
		 */
		if (genthunk32_init() && genthunk32w_kernel32_GetVersionEx != 0) {
			OSVERSIONINFO osv;

			memset(&osv,0,sizeof(osv));
			osv.dwOSVersionInfoSize = sizeof(osv);
			if (__CallProcEx32W(CPEX_DEST_STDCALL | 1/* convert param 1*/,
				1/*1 param*/,genthunk32w_kernel32_GetVersionEx,
				(DWORD)((void far*)(&osv))) != 0UL) {
				windows_version_method = "GetVersionEx [16->32 CallProcEx32W]";
				windows_version = (osv.dwMajorVersion << 8) | osv.dwMinorVersion;
				if (osv.dwPlatformId == 2/*VER_PLATFORM_WIN32_NT*/)
					windows_mode = WINDOWS_NT;
				else
					windows_mode = WINDOWS_ENHANCED;
			}
		}

		win16_probe_for_wine();
	}
#  elif TARGET_WINDOWS >= 20
	/* Windows 2.x or higher. Use GetProcAddress() to locate GetWinFlags() if possible, else assume real mode
	 * and find some other way to detect if we're running under the 286 or 386 enhanced versions of Windows 2.11 */
	/* TODO */
	if (!windows_init) {
		windows_init = 1;
		windows_version = 0x200;
		windows_mode = WINDOWS_REAL;
		windows_version_method = "Assuming";
	}
#  else
	/* Windows 1.x. No GetProcAddress, no GetWinFlags. Assume Real mode. */
	/* TODO: How exactly DO you get the Windows version in 1.1? */
	if (!windows_init) {
		windows_init = 1;
		windows_version = 0x101; /* Assume 1.1 */
		windows_mode = WINDOWS_REAL;
		windows_version_method = "Assuming";
	}
#  endif
# else
#  error Unknown Windows bit target
# endif
#elif defined(TARGET_OS2)
	/* OS/2 16-bit or 32-bit. Obviously as something compiled for OS/2, we're running under OS/2 */
	if (!windows_init) {
		windows_version_method = "I'm an OS/2 program, therefore the environment is OS/2";
		windows_version = dos_version;
		windows_mode = WINDOWS_OS2;
		windows_init = 1;
	}
#else
	/* MS-DOS 16-bit or 32-bit. MS-DOS applications must try various obscure interrupts to detect whether Windows is running */
	/* TODO: How can we detect whether we're running under OS/2? */
	if (!windows_init) {
		union REGS regs;

		windows_version = 0;
		windows_mode = 0;
		windows_init = 1;

		switch (dos_version>>8) {
			case 10:	/* OS/2 1.x */
			case 20:	/* OS/2 2.x (low=0), and OS/2 Warp 3 (low=30), and OS/2 Warp 4 (low=40) */
				windows_version_method = "Deduce from DOS version";
				windows_version = dos_version;
				windows_mode = WINDOWS_OS2;
				break;
		};

		if (windows_version == 0) {
			regs.w.ax = 0x160A;
#if TARGET_MSDOS == 32
			int386(0x2F,&regs,&regs);
#else
			int86(0x2F,&regs,&regs);
#endif
			if (regs.w.ax == 0x0000 && regs.w.bx >= 0x300 && regs.w.bx <= 0x700) { /* Windows 3.1 */
				windows_version = regs.w.bx;
				switch (regs.w.cx) {
					case 0x0002: windows_mode = WINDOWS_STANDARD; break;
					case 0x0003: windows_mode = WINDOWS_ENHANCED; break;
					default:     windows_version = 0; break;
				}

				if (windows_mode != 0)
					windows_version_method = "INT 2Fh AX=160Ah";
			}
		}

		if (windows_version == 0) {
			regs.w.ax = 0x4680;
#if TARGET_MSDOS == 32
			int386(0x2F,&regs,&regs);
#else
			int86(0x2F,&regs,&regs);
#endif
			if (regs.w.ax == 0x0000) { /* Windows 3.0 or DOSSHELL in real or standard mode */
			/* FIXME: Okay... if DOSSHELL triggers this test how do I tell between Windows and DOSSHELL? */
			/* Also, this call does NOT work when Windows 3.0 is in enhanced mode, and for Real and Standard modes
			 * does not tell us which mode is active.
			 *
			 * As far as I can tell there really is no way to differentiate whether it is running in Real or
			 * Standard mode, because on a 286 there is no "virtual 8086" mode. The only way Windows can run
			 * DOS level code is to thunk back into real mode. So for all purposes whatsoever, we might as well
			 * say that we're running in Windows Real mode because during that time slice we have complete control
			 * of the CPU. */
				windows_version = 0x300;
				windows_mode = WINDOWS_REAL;
				windows_version_method = "INT 2Fh AX=4680h";
			}
		}

		if (windows_version == 0) {
			regs.w.ax = 0x1600;
#if TARGET_MSDOS == 32
			int386(0x2F,&regs,&regs);
#else
			int86(0x2F,&regs,&regs);
#endif
			if (regs.h.al == 1 || regs.h.al == 0xFF) { /* Windows 2.x/386 */
				windows_version = 0x200;
				windows_mode = WINDOWS_ENHANCED;
			}
			else if (regs.h.al == 3 || regs.h.al == 4) {
				windows_version = (regs.h.al << 8) + regs.h.ah;
				windows_mode = WINDOWS_ENHANCED; /* Windows 3.0 */
			}

			if (windows_mode != 0)
				windows_version_method = "INT 2Fh AX=1600h";
		}

		if (windows_version == 0 && windows_mode == WINDOWS_NONE) {
		/* well... if the above fails, but the "true" DOS version is 5.50, then we're running under Windows NT */
		/* NOTE: Every copy of NT/2000/XP/Vista I have reports 5.50, but assuming that will continue is stupid.
		 *       Microsoft is free to change that someday. */
			if (dos_version == 0) probe_dos();
			if (dos_version >= 0x510 && dos_version <= 0x57f) { /* FIXME: If I recall Windows NT really does stick to v5.50, so this range check should be changed into == 5.50 */
				windows_mode = WINDOWS_NT;
				windows_version = 0;
				windows_version_method = "Assuming from DOS version number";
			}
		}

		/* now... if this is Windows NT, the next thing we can do is use NTVDM.EXE's
		 * BOP opcodes to load a "helper" DLL that allows us to call into Win32 */
# if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS)
		if (windows_mode == WINDOWS_NT && ntvdm_dosntast_init()) {
			/* OK. Ask for the version number */
			OSVERSIONINFO ovi;

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
# endif
	}
#endif

	return (windows_mode != WINDOWS_NONE);
}

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386)
/* API for exploiting the QT_Thunk Win32 -> Win16 thunking offered by Windows 9x/ME */
unsigned char		win9x_qt_thunk_probed = 0;
unsigned char		win9x_qt_thunk_available = 0;

void			(__stdcall *QT_Thunk)() = NULL;
DWORD			(__stdcall *LoadLibrary16)(LPSTR lpszLibFileName) = NULL;
VOID			(__stdcall *FreeLibrary16)(DWORD dwInstance) = NULL;
HGLOBAL16		(__stdcall *GlobalAlloc16)(UINT flags,DWORD size) = NULL;
HGLOBAL16		(__stdcall *GlobalFree16)(HGLOBAL16 handle) = NULL;
DWORD			(__stdcall *GlobalLock16)(HGLOBAL16 handle) = NULL;
BOOL			(__stdcall *GlobalUnlock16)(HGLOBAL16 handle) = NULL;
VOID			(__stdcall *GlobalUnfix16)(HGLOBAL16 handle) = NULL;
VOID			(__stdcall *GlobalFix16)(HGLOBAL16 handle) = NULL;
DWORD			(__stdcall *GetProcAddress16)(DWORD dwInstance, LPSTR lpszProcName) = NULL;
DWORD			win9x_kernel_win16 = 0;
DWORD			win9x_user_win16 = 0;

int Win9xQT_ThunkInit() {
	if (!win9x_qt_thunk_probed) {
		Win32OrdinalLookupInfo nfo;
		HMODULE kern32;

		win9x_qt_thunk_probed = 1;
		win9x_qt_thunk_available = 0;

		if (dos_version == 0) probe_dos();
		if (windows_mode == 0) detect_windows();
		if (windows_mode != WINDOWS_ENHANCED) return 0; /* This does not work under Windows NT */
		if (windows_version < 0x400) return 0; /* This does not work on Windows 3.1 Win32s (FIXME: Are you sure?) */

		/* This hack relies on undocumented Win16 support routines hidden in KERNEL32.DLL.
		 * They're so super seekret, Microsoft won't even let us get to them through GetProcAddress() */
		kern32 = GetModuleHandle("KERNEL32.DLL");
		if (windows_emulation == WINEMU_WINE) {
			/* FIXME: Direct ordinal lookup doesn't work. Returned
			 *        addresses point to invalid regions of KERNEL32.DLL.
			 *        I doubt WINE is even putting a PE-compatible image
			 *        of it out there.
			 *
			 *        WINE does allow us to GetProcAddress ordinals
			 *        (unlike Windows 9x which blocks it) but I'm not
			 *        really sure the returned functions are anything
			 *        like the Windows 9x equivalent. If we assume they
			 *        are, this code seems unable to get the address of
			 *        KRNL386.EXE's "GETVERSION" function.
			 *
			 *        So basically WINE's Windows 9x emulation is more
			 *        like Windows XP's Application Compatability modes
			 *        than any serious attempt at pretending to be
			 *        Windows 9x. And the entry points may well be
			 *        stubs or other random functions in the same way
			 *        that ordinal 35 is unrelated under Windows XP. */
			return 0;
		}
		else if (Win32GetOrdinalLookupInfo(kern32,&nfo)) {
			GlobalFix16 = (void*)Win32GetOrdinalAddress(&nfo,27);
			GlobalLock16 = (void*)Win32GetOrdinalAddress(&nfo,25);
			GlobalFree16 = (void*)Win32GetOrdinalAddress(&nfo,31);
			LoadLibrary16 = (void*)Win32GetOrdinalAddress(&nfo,35);
			FreeLibrary16 = (void*)Win32GetOrdinalAddress(&nfo,36);
			GlobalAlloc16 = (void*)Win32GetOrdinalAddress(&nfo,24);
			GlobalUnfix16 = (void*)Win32GetOrdinalAddress(&nfo,28);
			GlobalUnlock16 = (void*)Win32GetOrdinalAddress(&nfo,26);
			GetProcAddress16 = (void*)Win32GetOrdinalAddress(&nfo,37);
			QT_Thunk = (void*)GetProcAddress(kern32,"QT_Thunk");
		}
		else {
			GlobalFix16 = NULL;
			GlobalLock16 = NULL;
			GlobalFree16 = NULL;
			GlobalUnfix16 = NULL;
			LoadLibrary16 = NULL;
			FreeLibrary16 = NULL;
			GlobalAlloc16 = NULL;
			GlobalUnlock16 = NULL;
			GetProcAddress16 = NULL;
			QT_Thunk = NULL;
		}

		if (LoadLibrary16 && FreeLibrary16 && GetProcAddress16 && QT_Thunk) {
			/* Prove the API works by loading a reference to KERNEL */
			win9x_kernel_win16 = LoadLibrary16("KERNEL");
			if (win9x_kernel_win16 != 0) {
				win9x_qt_thunk_available = 1;
			}

			win9x_user_win16 = LoadLibrary16("USER");
		}
	}

	return win9x_qt_thunk_available;
}

void Win9xQT_ThunkFree() {
	if (win9x_kernel_win16) {
		FreeLibrary16(win9x_kernel_win16);
		win9x_kernel_win16 = 0;
	}
}
#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
unsigned char	ToolHelpProbed = 0;
HMODULE		ToolHelpDLL = 0;
BOOL		(PASCAL FAR *__TimerCount)(TIMERINFO FAR *t) = NULL;
BOOL		(PASCAL FAR *__InterruptUnRegister)(HTASK htask) = NULL;
BOOL		(PASCAL FAR *__InterruptRegister)(HTASK htask,FARPROC callback) = NULL;

int ToolHelpInit() {
	if (!ToolHelpProbed) {
		UINT oldMode;

		ToolHelpProbed = 1;

		/* BUGFIX: In case TOOLHELP.DLL is missing (such as when running under Windows 3.0)
		 *         this prevents sudden interruption by a "Cannot find TOOLHELP.DLL" error
		 *         dialog box */
		oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
		ToolHelpDLL = LoadLibrary("TOOLHELP.DLL");
		SetErrorMode(oldMode);
		if (ToolHelpDLL != 0) {
			__TimerCount = (void far*)GetProcAddress(ToolHelpDLL,"TIMERCOUNT");
			__InterruptRegister = (void far*)GetProcAddress(ToolHelpDLL,"INTERRUPTREGISTER");
			__InterruptUnRegister = (void far*)GetProcAddress(ToolHelpDLL,"INTERRUPTUNREGISTER");
		}
	}

	return (ToolHelpDLL != 0) && (__TimerCount != NULL) && (__InterruptRegister != NULL) &&
		(__InterruptUnRegister != NULL);
}

void ToolHelpFree() {
	if (ToolHelpDLL) {
		FreeLibrary(ToolHelpDLL);
		ToolHelpDLL = 0;
	}
	__InterruptUnRegister = NULL;
	__InterruptRegister = NULL;
	__TimerCount = NULL;
}
#endif

