#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

/* Windows programming notes:
 *
 *   - If you're writing your software to work only on Windows 3.1 or later, you can omit the
 *     use of MakeProcInstance(). Windows 3.0 however require your code to use it.
 *
 *   - The Window procedure must be exported at link time. Windows 3.0 demands it.
 *
 *   - If you want your code to run in Windows 3.0, everything must be MOVEABLE. Your code and
 *     data segments must be MOVEABLE. If you intend to load resources, the resources must be
 *     MOVEABLE. The constraints of real mode and fitting everything into 640KB or less require
 *     it.
 *
 *   - If you want to keep your sanity, never mark your data segment (DGROUP) as discardable.
 *     You can make your code discardable because the mechanisms of calling your code by Windows
 *     including the MakeProcInstance()-generated wrapper for your windows proc will pull your
 *     code segment back in on demand. There is no documented way to pull your data segment back
 *     in if discarded. Notice all the programs in Windows 2.0/3.0 do the same thing.
 */
/* FIXME: This code crashes when multiple instances are involved. Especially the win386 build. */

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>

#ifndef TEXT
#define TEXT(x) (x)
#endif

/* Ref: [https://github.com/open-watcom/open-watcom-v2/issues/852]
 * "Programs using LocalAlloc crash on Windows 9x" */
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	HLOCAL pCopy = LocalAlloc(LMEM_FIXED, 12);
	if (!pCopy) {
		return 0;
	}

	MessageBox(NULL, TEXT("Allocated successfully"), TEXT("Test"), MB_ICONHAND);

	LocalFree(pCopy);

	return 0;
}

