#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

/* FIXME: Windows 3.1, 16-bit:
       This code manages to do a fullscreen display, but then for whatever reason, all keyboard
       input is disabled. Why? */

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

/* Explanation of this library:

   When compiled as Win16 code: This library provides a buffer that the calling program can write
   16-bit code to and execute.

   When compiled as Win32 code: This library provides a buffer that the calling program can write
   16-bit code to, and execute via Win32->Win16 thunking in Windows 9x/ME.

   WARNING: This library is intended for small one-off subroutines (such as a quick INT 10h call
   that Win32 cannot do directly). If your subroutine is complex it is recommended you write a
   Win16 DLL and use the Win9x thunk library code in hw/dos instead. */

#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <time.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <hw/dos/dos.h>
#include <windows/apihelp.h>

extern unsigned char Win16EBinit;
extern unsigned char Win16EBavailable;
extern DWORD Win16EBbuffersize;
extern WORD Win16EBbufferhandle;
extern BYTE FAR *Win16EBbufferdata16;
extern WORD Win16EBbuffercode16;
#if TARGET_MSDOS == 32
extern WORD Win16EBbufferdata16sel;
#else
# define Win16EBbufferdata16sel FP_SEG(Win16EBbufferdata16)
#endif

DWORD ExecuteWin16EBbuffer(DWORD ptr1616);
void FreeCodeDataSelectorsWin16EB();
DWORD GetCodeAliasWin16EBbuffer();
BYTE FAR *LockWin16EBbuffer();
void UnlockWin16EBbuffer();
void FreeWin16EBBuffer();
void FreeWin16EB();
int InitWin16EB();

