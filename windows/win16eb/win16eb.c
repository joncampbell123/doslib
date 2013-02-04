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
#include <windows/win16eb/win16eb.h>

unsigned char Win16EBinit = 0;
unsigned char Win16EBavailable = 0;
DWORD Win16EBbuffersize = 512;
WORD Win16EBbufferhandle = 0;
BYTE FAR *Win16EBbufferdata16 = NULL; /* nonzero when locked, return value of GlobalLock */
WORD Win16EBbuffercode16 = 0;
#if TARGET_MSDOS == 32
WORD Win16EBbufferdata16sel = 0;
DWORD Win16EBtoolhelp_dll = 0; /* Win16 TOOLHELP.DLL */
DWORD Win16EBtoolhelp_GlobalHandleToSel = 0;
DWORD Win16EBkernel_AllocDStoCSAlias = 0;
DWORD Win16EBkernel_FreeSelector = 0;
#endif

int InitWin16EB() {
	if (!Win16EBinit) {
		Win16EBinit = 1;
		Win16EBavailable = 0;

		probe_dos();
		detect_windows();

#if TARGET_MSDOS == 32
		/* Under Windows 9x we can do this from the Win32 world, but we have to use the thunk library */
		if (windows_mode == WINDOWS_NT)
			return 0;
		if (!Win9xQT_ThunkInit())
			return 0;
		if (QT_Thunk == NULL || GlobalAlloc16 == NULL || GlobalFree16 == NULL ||
			GlobalLock16 == NULL || GlobalUnlock16 == NULL || GlobalUnfix16 == NULL ||
			GlobalFix16 == NULL || GetProcAddress16 == NULL || LoadLibrary16 == NULL ||
			FreeLibrary16 == NULL)
			return 0;

		/* we need additional functions from KRNL386.EXE */
		Win16EBkernel_AllocDStoCSAlias = GetProcAddress16(win9x_kernel_win16,"ALLOCDSTOCSALIAS");
		if (Win16EBkernel_AllocDStoCSAlias == 0) return 0;

		Win16EBkernel_FreeSelector = GetProcAddress16(win9x_kernel_win16,"FREESELECTOR");
		if (Win16EBkernel_FreeSelector == 0) return 0;

		/* we need TOOLHELP.DLL too */
		Win16EBtoolhelp_dll = LoadLibrary16("TOOLHELP.DLL");
		if (Win16EBtoolhelp_dll == 0) return 0;
		Win16EBtoolhelp_GlobalHandleToSel = GetProcAddress16(Win16EBtoolhelp_dll,"GLOBALHANDLETOSEL");
		if (Win16EBtoolhelp_GlobalHandleToSel == 0) return 0;

		Win16EBbuffersize = 4096;
		Win16EBbufferhandle = GlobalAlloc16(0/*GMEM_FIXED*/|0x40/*GMEM_ZEROINIT*/,Win16EBbuffersize);
		if (Win16EBbufferhandle == 0) return 0;
		Win16EBbufferdata16 = NULL;
		Win16EBbuffercode16 = 0;
#else
		Win16EBbuffersize = 512;
		Win16EBbufferhandle = GlobalAlloc(GMEM_FIXED|GMEM_ZEROINIT,Win16EBbuffersize);
		if (Win16EBbufferhandle == 0) return 0;
		Win16EBbufferdata16 = NULL;
		Win16EBbuffercode16 = 0;
#endif

		Win16EBavailable = 1;
	}

	return Win16EBavailable;
}

#if TARGET_MSDOS == 32
static WORD Win16EBtoolhelp_GlobalHandleToSel_DoCall() {
	WORD retv=0;

	__asm {
		mov	edx,Win16EBtoolhelp_GlobalHandleToSel
		mov	eax,dword ptr [QT_Thunk]

		; QT_Thunk needs 0x40 byte of data storage at [EBP]
		; give it some, right here on the stack
		push	ebp
		mov	ebp,esp
		sub	esp,0x40

		mov	bx,WORD PTR [Win16EBbufferhandle]
		push	bx
		call	eax	; <- QT_Thunk

		; release stack storage
		mov	esp,ebp
		pop	ebp

		mov	retv,ax
	}

	return retv;
}

static WORD Win16EBkernel_AllocDStoCSAlias_DoCall(WORD seli) {
	WORD retv=0;

	__asm {
		mov	edx,Win16EBkernel_AllocDStoCSAlias
		mov	eax,dword ptr [QT_Thunk]
		mov	bx,WORD PTR [seli]

		; QT_Thunk needs 0x40 byte of data storage at [EBP]
		; give it some, right here on the stack
		push	ebp
		mov	ebp,esp
		sub	esp,0x40

		push	bx
		call	eax	; <- QT_Thunk

		; release stack storage
		mov	esp,ebp
		pop	ebp

		mov	retv,ax
	}

	return retv;
}

static WORD Win16EBkernel_FreeSelector_DoCall(WORD seli) {
	WORD retv=0;

	__asm {
		mov	edx,Win16EBkernel_FreeSelector
		mov	eax,dword ptr [QT_Thunk]
		mov	bx,WORD PTR [seli]

		; QT_Thunk needs 0x40 byte of data storage at [EBP]
		; give it some, right here on the stack
		push	ebp
		mov	ebp,esp
		sub	esp,0x40

		push	bx
		call	eax	; <- QT_Thunk

		; release stack storage
		mov	esp,ebp
		pop	ebp

		mov	retv,ax
	}

	return retv;
}
#endif

void FreeCodeDataSelectorsWin16EB() {
#if TARGET_MSDOS == 32
	if (Win16EBbuffercode16 != 0) Win16EBkernel_FreeSelector_DoCall(Win16EBbuffercode16);
#else
	if (Win16EBbuffercode16 != 0) FreeSelector(Win16EBbuffercode16);
#endif
	Win16EBbuffercode16 = 0;
}

BYTE FAR *LockWin16EBbuffer() {
	if (Win16EBbufferhandle == 0) return NULL;

	if (Win16EBbufferdata16 == NULL) {
#if TARGET_MSDOS == 32
		/* It turns out the KERNEL32.DLL version returns a Win32 flat pointer :) */
		Win16EBbufferdata16 = (BYTE*)GlobalLock16(Win16EBbufferhandle);
		if (Win16EBbufferdata16 == NULL) return NULL;

		/* OK, it's nice KERNEL32.DLL wraps GlobalAlloc16 and returns a flat Win32 pointer,
		   but we really do need that selector value too! */
		Win16EBbufferdata16sel = Win16EBtoolhelp_GlobalHandleToSel_DoCall();
#else
		Win16EBbufferdata16 = GlobalLock(Win16EBbufferhandle);
		if (Win16EBbufferdata16 == NULL) return NULL;
#endif
	}

#if TARGET_MSDOS == 32
	return Win16EBbufferdata16;
#else
	return Win16EBbufferdata16;
#endif
}

void UnlockWin16EBbuffer() {
	if (Win16EBbufferhandle == 0) return;
	FreeCodeDataSelectorsWin16EB(); /* release CS alias */
#if TARGET_MSDOS == 32
	if (Win16EBbufferdata16) GlobalUnlock16(Win16EBbufferhandle);
#else
	if (Win16EBbufferdata16) GlobalUnlock(Win16EBbufferhandle);
#endif
	Win16EBbufferdata16 = NULL; /* unlocked buffer, pointer isn't valid anymore */
}

void FreeWin16EBBuffer() {
	UnlockWin16EBbuffer();
#if TARGET_MSDOS == 32
	if (Win16EBbufferhandle) GlobalFree16(Win16EBbufferhandle);
#else
	if (Win16EBbufferhandle) GlobalFree(Win16EBbufferhandle);
#endif
	Win16EBbufferhandle = 0;
}

void FreeWin16EB() {
	FreeWin16EBBuffer();

#if TARGET_MSDOS == 32
	Win16EBtoolhelp_GlobalHandleToSel = 0;	
	if (Win16EBtoolhelp_dll != 0) FreeLibrary16(Win16EBtoolhelp_dll);
	Win16EBtoolhelp_dll = 0;
#endif
}

DWORD GetCodeAliasWin16EBbuffer() {
	if (Win16EBbufferhandle == 0) return 0;
	if (Win16EBbufferdata16 == NULL) return 0;

#if TARGET_MSDOS == 32
	if (Win16EBbuffercode16 == 0) {
		if (Win16EBbufferdata16sel == 0) return 0;
		Win16EBbuffercode16 = Win16EBkernel_AllocDStoCSAlias_DoCall(Win16EBbufferdata16sel);
		if (Win16EBbuffercode16 == 0) return 0;
	}

	return ((DWORD)Win16EBbuffercode16 << 16); /* FIXME: Is the segment offset involved? */
#else
	if (Win16EBbuffercode16 == 0) {
		Win16EBbuffercode16 = AllocDStoCSAlias(FP_SEG(Win16EBbufferdata16));
		if (Win16EBbuffercode16 == 0) return 0;
	}

	return (DWORD)MK_FP(Win16EBbuffercode16,FP_OFF(Win16EBbufferdata16));
#endif
}

DWORD ExecuteWin16EBbuffer(DWORD ptr1616) {
	DWORD retv=0;

#if TARGET_MSDOS == 32
	__asm {
		pushad
		push		ds
		push		es
	
		mov		edx,ptr1616
		mov		eax,dword ptr [QT_Thunk]

		; QT_Thunk needs 0x40 byte of data storage at [EBP]
		; give it some, right here on the stack
		push		ebp
		mov		ebp,esp
		sub		esp,0x40

		call		eax	; <- QT_Thunk

		; release stack storage
		mov		esp,ebp
		pop		ebp

		pop		es
		pop		ds
		mov		retv,eax
		popad
	}
#else
	/* TODO: If CPU is 286, follow alternate code path that reads back only AX */
	/* NTS: It's best to assume we save all registers and that it is unwise to let them get clobbered */
	__asm {
		.386p
		pushad
		push		ds
		push		es
		call		DWORD PTR [ptr1616]
		pop		es
		pop		ds
		mov		retv,eax
		popad
	}
#endif

	return retv;
}

