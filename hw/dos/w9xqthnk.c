/* TODO: This should be moved to it's own library under the /windows subdirectory */

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

