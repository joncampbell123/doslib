#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
# ifndef TARGET_MSDOS
#  error TARGET_MSDOS isnt defined either
# endif
#endif

/* Windows programming notes:
 *
 *   - If you're writing your software to work only on Windows 3.1 or later, you can omit the
 *     use of MakeProcInstance(). Windows 3.0 however require your code to use it.
 *
 *   - The Window procedure must be exported at link time. Windows 3.0 demands it.
 *
 *   - The Window procedure must be PASCAL FAR __loadds if you want this code to work properly
 *     under Windows 3.0. If you only support Windows 3.1 and later, you can remove __loadds,
 *     though it's probably wiser to keep it.
 *
 *   - Testing so far shows that this program for whatever reason, either fails to load it's
 *     resources or simply hangs when Windows 3.0 is started in "real mode".
 */

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <windows/apihelp.h>

static const char* HelloWorldText = "Hello world!";

void DllEntryType HelloMsg() {
	MessageBox(NULL,"DLL is alive","Test",MB_OK);
}

void DllEntryType HelloDraw(HWND hwnd,HDC hdc,HICON icon) {
	TextOut(hdc,0,0,HelloWorldText,strlen(HelloWorldText));
	DrawIcon(hdc,5,20,icon);
}

/* this is REQUIRED. it is basically the entry point of this DLL */
#if TARGET_MSDOS == 16
unsigned int CALLBACK LibMain(HINSTANCE hInstanceDLL,WORD wDataSeg,WORD cbHeapSize,LPSTR lpszCmdLine) {
	return 1;
}
/* Win32s requires this... sort of... intermediate name. LibMain just like Win16, but with params like DllMain.
 * We *HAVE* to provide this, if we don't, Watcom provides it's own LibMain in WIN32SPL.DLL which of course
 * doesn't seem to exist. */
#elif TARGET_MSDOS == 32 && TARGET_WINDOWS == 31
BOOL WINAPI LibMain(HINSTANCE hInstanceDLL,DWORD fdwReason,LPVOID lpvReserved) {
	return TRUE;
}
#else
BOOL WINAPI DllMain(HINSTANCE hInstanceDLL,DWORD fdwReason,LPVOID lpvReserved) {
	return TRUE;
}
#endif

