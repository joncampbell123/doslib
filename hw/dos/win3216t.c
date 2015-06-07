/* dos.c
 *
 * Code to detect the surrounding DOS/Windows environment and support routines to work with it
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

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

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

/* if provided by the system, these functions allow library and application code to call out to the Win32 world from Win16.
 * Which is absolutely necessary given that Win16 APIs tend to lie for compatibility reasons. */

DWORD		genthunk32w_ntdll = 0;
DWORD		genthunk32w_kernel32 = 0;
DWORD		genthunk32w_kernel32_GetVersion = 0;
DWORD		genthunk32w_kernel32_GetVersionEx = 0;
DWORD		genthunk32w_kernel32_GetLastError = 0;
BOOL		__GenThunksExist = 0;
BOOL		__GenThunksChecked = 0;
DWORD		(PASCAL FAR *__LoadLibraryEx32W)(LPCSTR lpName,DWORD reservedhfile,DWORD dwFlags) = NULL;
BOOL		(PASCAL FAR *__FreeLibrary32W)(DWORD hinst) = NULL;
DWORD		(PASCAL FAR *__GetProcAddress32W)(DWORD hinst,LPCSTR name) = NULL;
DWORD		(PASCAL FAR *__GetVDMPointer32W)(LPVOID ptr,UINT mask) = NULL;
DWORD		(PASCAL FAR *__CallProc32W)(DWORD procaddr32,DWORD convertMask,DWORD params,...) = NULL; /* <- FIXME: How to use? */
DWORD		(_cdecl _far *__CallProcEx32W)(DWORD params,DWORD convertMask,DWORD procaddr32,...) = NULL;

int genthunk32_init() {
	if (!__GenThunksChecked) {
		HMODULE kern32;

		genthunk32_free();
		__GenThunksExist = 0;
		__GenThunksChecked = 1;
		kern32 = GetModuleHandle("KERNEL");
		if (kern32 != NULL) {
			__LoadLibraryEx32W = (void far*)GetProcAddress(kern32,"LOADLIBRARYEX32W");
			__FreeLibrary32W = (void far*)GetProcAddress(kern32,"FREELIBRARY32W");
			__GetProcAddress32W = (void far*)GetProcAddress(kern32,"GETPROCADDRESS32W");
			__GetVDMPointer32W = (void far*)GetProcAddress(kern32,"GETVDMPOINTER32W");
			__CallProcEx32W = (void far*)GetProcAddress(kern32,"_CALLPROCEX32W"); /* <- Hey thanks Microsoft
												    maybe if your docs mentioned
												    the goddamn underscore I would
												    have an easier time linking to it */
			__CallProc32W = (void far*)GetProcAddress(kern32,"CALLPROC32W");

			if (__LoadLibraryEx32W && __FreeLibrary32W && __GetProcAddress32W && __GetVDMPointer32W &&
				__CallProc32W && __CallProcEx32W) {
				__GenThunksExist = 1;

				genthunk32w_kernel32 = __LoadLibraryEx32W("KERNEL32.DLL",0,0);
				if (genthunk32w_kernel32 != 0) {
					genthunk32w_kernel32_GetVersion =
						__GetProcAddress32W(genthunk32w_kernel32,"GetVersion");
					genthunk32w_kernel32_GetVersionEx =
						__GetProcAddress32W(genthunk32w_kernel32,"GetVersionExA");
					genthunk32w_kernel32_GetLastError =
						__GetProcAddress32W(genthunk32w_kernel32,"GetLastError");
				}

				genthunk32w_ntdll = __LoadLibraryEx32W("NTDLL.DLL",0,0);
			}
		}
	}

	return __GenThunksExist;
}

void genthunk32_free() {
	genthunk32w_kernel32_GetVersion = 0;
	genthunk32w_kernel32_GetVersionEx = 0;
	genthunk32w_kernel32_GetLastError = 0;
	if (genthunk32w_kernel32) {
		__FreeLibrary32W(genthunk32w_kernel32);
		genthunk32w_kernel32 = 0;
	}
}

#endif

