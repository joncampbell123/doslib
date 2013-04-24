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
 *   - The Window procedure must be PASCAL FAR __loadds if you want this code to work properly
 *     under Windows 3.0. If you only support Windows 3.1 and later, you can remove __loadds,
 *     though it's probably wiser to keep it.
 *
 *   - Testing so far shows that this program for whatever reason, either fails to load it's
 *     resources or simply hangs when Windows 3.0 is started in "real mode".
 *
 *   - When running this program under Windows 3.1 in DOSBox, do NOT use core=normal and cputype=486_slow.
 *     Weird random errors show up and you will pull your hair out and waste entire weekends trying to debug
 *     them. In this program's case, some random "Unhandled Exception 0xC0000005" error will show up when
 *     you attempt to run this program. Set core=dynamic and cputype=pentium_slow, which seems to provide
 *     more correct emulation and allow Win32s to work properly.
 *
 *     This sort of problem doesn't exactly surprise me since DOSBox was designed to run...
 *     you know... *DOS games* anyway, so this doesn't exactly surprise me.
 *
 *   - There is a QEMU ray of False Hope that can occur with this program: If you run Windows 3.1
 *     under QEMU (without the KVM emulation) and run this program, this program will work 100%
 *     correctly and catch page faults. It is only when you run with KVM emulation enabled that the
 *     more accurate emulation reveals Windows 3.1's crashiness with respect to page faults.
 *
 *   - Windows NT/2000/XP/Vista/7/etc: The Win16 version cannot use TOOLHELP.DLL or DPMI hooks to catch
 *     page faults. No matter what, NTVDM.EXE will never pass them down to us properly. However, as a
 *     guest of the NTVDM.EXE world, we can use the Windows-on-Windows libraries to call directly into
 *     the Win32 world, where we can then use some assembly language magic to setup a SEH frame, do the
 *     memcpy(), and then return safely whether or not a page fault happened. So far, this technique works
 *     very well.
 *
 *     Also of interest: The Windows NT technique described above also works under Linux + WINE.
 */
/* TODO list:
 *   - Test this program + Windows 3.1 on real hardware. Is Windows 3.1 *REALLY* that crashy when handling
 *     a page fault??? If not, then is it possible for this program to work correctly?
 *
 *   - What about Windows 95? Win95 in QEMU is promising enough, but Win95 doesn't work in QEMU when you
 *     enable KVM mode, so I have no idea if as noted above it happens to work because of the QEMU ray of
 *     False Hope.
 *
 *           1. Bring up Win95 in VirtualBox, turn on all acceleration, and see if this program works properly.
 *           2. Dig out an ancient Pentium or 486 and an IDE hard drive, install Win95, and see if this program works properly.
 *
 *   - If Win95 works, then what about Win98? WinME? Does the exception counter/limit in WinME apply to Win16 apps?
 *
 *   - Windows Millenium: The technique used by the Win32 program can be hampered by the fact KERNEL32.DLL
 *     enforces an upper limit on the number of exceptions (or exceptions per second? or some kind of rate?).
 *     When this upper limit is reached, the next page fault is handled directly by the system and our
 *     structured exception handling is bypassed entirely. How do we work around this? Is there a kernel
 *     function I can call to reset that counter? Any way to disable it? Will I have to stoop to the level
 *     of zeroing a specific memory location or patching KERNEL32.DLL code to disable this limit? (I hope not!!).
 *
 */
/* Known memory maps in various versions of Windows:
 *   - Windows 3.1/3.11:
 *      * If Win32s program:
 *         - 32-bit data segment base=0xFFFF0000 limit=0xFFFFFFFF
 *         - The "first" 64KB of the data segment (which becomes linear address 0xFFFF0000-0xFFFFFFFF)
 *           is mapped out (read or write will fault)
 *         - Starting at 64KB the first 1MB of DOS memory is visible
 *           from ptr=0x0001000-0x0010FFFF, or linear address 0x00000000-0x000FFFFF.
 *         - Windows also repeats the first 64KB of lower DOS memory at
 *           ptr=0x00110000-0x0011FFFF, or linear address 0x00100000-0x0010FFFF.
 *         - For some reason, the first 1MB of DOS memory is also mapped to
 *           ptr=0x002D0000-0x00357FFF, or linear address 0x002C0000-0x00347FFF.
 *         - The BIOS (or a copy of the BIOS?) is at ptr=0x00390000-0x003DFFFF.
 *         - The entire contents of system RAM are mapped at ptr=0x00410000.
 *           In the test situation used to take these notes, DOSBox was configured with
 *           memsize=12 and a 12MB region was observed from ptr=0x00410000-0x0100FFFF.
 *         - VXD kernel structures are visible at ptr=0x80011000-0x8005FFFF (linear=0x80001000-0x8004FFFF).
 *
 *      * If Win16 program:
 *         - I don't know. Hard to tell when attempting to catch page faults causes hard crashes and
 *           random dumps to DOS. But from what I have seen, the mapping is the same.
 *
 *   - Windows 95:
 *      * If Win32 program:
 *         - 32-bit data segment base=0 limit=0xFFFFFFFF
 *         - The entire first 1MB of system memory is visible from 0x00000000-0x0012FFFF.
 *           This normally includes the first 64KB of system memory which is readable AND
 *           writeable (!!) unless you booted Windows 95 into Safe Mode, in which case,
 *           the first 64KB is mapped out entirely.
 *         - VXD kernel structures are visible at 0xC0001000.
 *
 *      * If Win16 program:
 *         - Unlike the Win32 world. the first 1MB is still wide open and accessible. Other
 *           than that, Win16 can generally see the same structures and address layout as Win32.
 *         - BUG: Rapidly hooking/unhooking interrupts and exceptions using TOOLHELP.DLL eventually
 *           causes TOOLHELP.DLL to exaust it's resources, page fault, and then lose the ability
 *           to catch them.
 *
 *   - Windows 95 OSR 2.5 C:
 *      * If Win32 program:
 *         - 32-bit data segment base=0 limit=0xFFFFFFFF
 *         - The entire first 1MB of system memory is visible from 0x00000000-0x0012FFFF.
 *           This normally includes the first 64KB of system memory which is readable unless you
 *           booted Windows 95 into Safe Mode, in which case, the first 64KB is mapped out entirely.
 *         - VXD kernel structures are visible at 0xC0001000.
 *
 *      * If Win16 program:
 *         - Unlike the Win32 world. the first 1MB is still wide open and accessible. Other
 *           than that, Win16 can generally see the same structures and address layout as Win32.
 *         - BUG: Rapidly hooking/unhooking interrupts and exceptions using TOOLHELP.DLL eventually
 *           causes TOOLHELP.DLL to exaust it's resources, page fault, and then lose the ability
 *           to catch them.
 *
 *   - Windows 98:
 *      * If Win32 program:
 *         - 32-bit data segment base=0 limit=0xFFFFFFFF
 *         - The first 1MB of system memory is visible, except for the first 64KB, mapped to
 *           0x00010000-0x00121FFF.
 *         - VXD kernel structures are visible at 0xC0001000.
 *         - Various scattered regions are visible, including some repeating BIOS data at
 *           0xFF000000-0xFF0BFFFF and scattered bits between 0xC0000000-0xCFFFFFFF.
 *
 *      * If Win16 program:
 *         - Unlike the Win32 world. the first 1MB is still wide open and accessible. Other
 *           than that, Win16 can generally see the same structures and address layout as Win32.
 *
 *   - Windows 98 SE:
 *      * If Win32 program:
 *         - 32-bit data segment base=0 limit=0xFFFFFFFF
 *         - The first 1MB of system memory is visible, except for the first 64KB, mapped to
 *           0x00010000-0x00121FFF.
 *         - VXD kernel structures are visible at 0xC0001000.
 *         - Various scattered regions are visible, including some repeating BIOS data at
 *           0xFF000000-0xFF0BFFFF and scattered bits between 0xC0000000-0xCFFFFFFF.
 *
 *      * If Win16 program:
 *         - Unlike the Win32 world. the first 1MB is still wide open and accessible. Other
 *           than that, Win16 can generally see the same structures and address layout as Win32.
 *
 *   - Windows ME:
 *      * If Win32 program:
 *         - 32-bit data segment base=0 limit=0xFFFFFFFF
 *         - The first 64KB is mapped out entirely... but not in a straightforward manner.
 *           For some odd reason, the boundary at which memory I/O is allowed is NOT on a page
 *           boundary but is instead on some arbitrary memory address i.e. reads below
 *           linear memory address 0xFE61 trigger a page fault, but at or above that address,
 *           data is readable. I have no idea why Windows ME would do that.
 *         - Despite the weird mapping in the first 64KB, the first 1MB is visible as usual
 *           between 0x00010000-0x00123FFF-ish.
 *         - The Win32 version no longer seems to trigger the page fault limit in KERNEL32.DLL
 *           despite earlier versions of this code doing so. What did I do differently this time?
 *         - Various scattered regions are visible, including some repeating BIOS data at
 *           0xFF000000-0xFF0BFFFF and scattered bits between 0xC0000000-0xCFFFFFFF.
 *         - VXD kernel structures are visible at 0xC0001000.
 *         - Some list at 0x001A0000-0x0020AFFF (HO? HS?)
 *         - 0x00400000: This is where this program is usually loaded
 *         - 0x80001000-0x80006FFF: Unknown area with hardly any data. Windows 3.1 compatible VXD zone??
 *
 *      * If Win16 program:
 *         - Unlike the Win32 world. the first 1MB is still wide open and accessible. Other
 *           than that, Win16 can generally see the same structures and address layout as Win32.
 *         - 0x00400000: 'PCHE' environment block for WINOLDAP? (4KB large)
 */

#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <hw/dos/dos.h>
#include <windows/apihelp.h>

/* display "modes", i.e. what we are displaying */
enum {
	DIM_DATASEG=0,			/* "linear" memory seen from the point of view of our data segment */
	DIM_LINEAR,			/* "linear" memory view (as seen from a flat 4GB paged viewpoint) */
	DIM_PHYSICAL,			/* "physical" memory view (making us a Windows equivalent of misc/hexmem1) */
	DIM_SEGMENT,			/* we're viewing memory associated with a segment */
	DIM_GLOBALHANDLE,		/* we locked a global memory object and we're showing it's contents */
};

HWND		hwndMain;
HFONT		monospaceFont = NULL;
int		monospaceFontWidth=0,monospaceFontHeight=0;
const char*	WndProcClass = "HEXMEMWINDOWS";
HINSTANCE	myInstance;
HICON		AppIcon;
RECT		hwndMainClientArea;
int		hwndMainClientRows,hwndMainClientColumns;
int		hwndMainClientAddrColumns;
int		hwndMainClientDataColumns;
HMENU		hwndMainMenu = NULL;
HMENU		hwndMainFileMenu = NULL;
HMENU		hwndMainViewMenu = NULL;
int		displayMode = DIM_LINEAR;
DWORD		displayOffset = 0;
DWORD		displayMax = 0;
char		captureTmp[4096];
char		drawTmp[512];

#if TARGET_MSDOS == 16
static volatile unsigned char faulted = 0;
static UINT viewselector,my_ds=0,my_ss=0,my_cs=0;
static void far *oldDPMIPFHook = NULL;
static int hook_use_tlhelp = 0;
static DWORD wow32_GetCurrentProcess = 0;
static DWORD wow32_ReadProcessMemory = 0;
static DWORD wow32_GetLastError = 0;

static int DPMILock(uint32_t base,uint32_t limit) {
	int retv = 0;

	__asm {
		mov		ax,0x0600
		mov		bx,word ptr [base+2]
		mov		cx,word ptr [base]
		mov		si,word ptr [limit+2]
		mov		di,word ptr [limit]
		int		31h
		jc		nope
		mov		retv,1
nope:
	}

	return retv;
}

static void DPMIUnlock(uint32_t base,uint32_t limit) {
	__asm {
		mov		ax,0x0601
		mov		bx,word ptr [base+2]
		mov		cx,word ptr [base]
		mov		si,word ptr [limit+2]
		mov		di,word ptr [limit]
		int		31h
	}
}

static void __declspec(naked) _w16_capture() {
	__asm {
		nop
		nop
		mov		al,[es:si]
		nop
		nop
		ret
	}
}

/* SS:SP + 12h = SS (fault)
 * SS:SS + 10h = SP (fault)
 * SS:SP + 0Eh = FLAGS (fault)
 * SS:SP + 0Ch = CS (fault)
 * SS:SP + 0Ah = IP (fault)
 * SS:SP + 08h = handle (internal)
 * SS:SP + 06h = interrupt number
 * SS:SP + 04h = AX (to load into DS)
 * SS:SP + 02h = CS (toolhelp.dll)
 * SS:SP + 00h = IP (toolhelp.dll)
 *
 * to pass exception on: RETF
 * to restart instruction: clear first 10 bytes of the stack, and IRET (??).
 *
 * NTS: I suspect some of the crashes observed were simply caused by page faults that happen
 *      to occur in places OTHER than the main byte capture routine, which is why we check
 *      the instruction pointer BEFORE modifying it! */
static void __declspec(naked) fault_pf_toolhelp() {
	__asm {
		.386p
		push		ds
		push		ax
		push		bp
		mov		bp,sp

		/* is this a page fault? */
		cmp		word ptr [bp+6+6],14	/* SS:SP + 06h = interrupt number */
		jnz		pass_on

		/* did this happen in our OWN code segment? */
		mov		ax,seg _w16_capture
		cmp		word ptr [bp+6+10+2],ax
		jnz		pass_on

		/* did this happen in _w16_capture? */
		mov		ax,offset _w16_capture
		cmp		word ptr [bp+6+10],ax	/* if < _w16_capture then no */
		jl		pass_on
		add		ax,10
		cmp		word ptr [bp+6+10],ax	/* if > (_w16_capture+10) then no */
		jae		pass_on

		xchg		bx,bx			/* FIXME: Bochs magic breakpoint */

		/* set the faulted flag, change the return address, then IRET directly back */
		mov		ax,seg faulted
		mov		ds,ax
		inc		faulted
		add		word ptr [bp+6+10],3
		pop		bp
		pop		ax
		pop		ds
		add		sp,0Ah			/* throw away handle, int number, and CS:IP return */
		iret

pass_on:	pop		bp
		pop		ax
		pop		ds
		retf
	}
}

static void __declspec(naked) fault_pf_dpmi() { /* DPMI exception handler */
	__asm { /* FIXME: Doesn't work yet */
		.386p
		push		ds
		push		ax
		push		bp
		mov		bp,sp
		mov		ax,seg faulted
		mov		ds,ax
		inc		faulted
		add		word ptr [bp+6+6],3
		pop		bp
		pop		ax
		pop		ds
		retf
	}
}
#endif

/* Notes and findings so far:

   Windows 3.1, 16-bit: The first 1MB of RAM + 64KB is mapped 1:1 to what you would get in DOS.
                        At 0x80001000 begins the kernel-mode VXD/386 driver list.
			The remainder is various global memory pools.

   Windows 3.1, 32-bit (Win32s): Same as 16-bit view, offset by 64KB. The first 64KB is mapped out.

   Windows 95, 16-bit: Same as Windows 3.1, though with Windows 95 components resident.
   Windows 95, 32-bit: Same as 16-bit world, though the first 64KB is read-only.
   Windows 98, 16-bit: Same as Windows 95
   Windows 98, 32-bit: 32-bit side of the world has the first 64KB mapped out entirely.
   Windows ME, 16-bit: Same as Windows 95
   Windows ME, 32-bit: Same as Windows 98, first 64KB mapped out, except for strange behavior
                       around the 0xF000-0xFFFF area: that region suddenly becomes readable from
                       0xFE61-0xFFFF which means that someone in the system traps reads and actively
                       compares the fault location to pass or fail it. Weird. Full testing is impossible
		       because KERNEL32.DLL will automatically terminate our program if we do 160
		       consecutive faults in a row, ignoring our exception handler. */

static int CaptureMemoryLinear(DWORD memofs,unsigned int howmuch) {
#if TARGET_MSDOS == 32
	volatile unsigned char *s = (volatile unsigned char*)memofs;
	volatile unsigned char *d = captureTmp;
	volatile unsigned char *t,*sf,cc;
	int fail = 0;

	if (displayMode == DIM_LINEAR) {
		DWORD x = GetVersion();

		/* Windows 3.1 with Win32s: Despite "flat" memory Microsoft set the segment
		 * base to 0xFFFF0000 for whatever reason, therefore, we have to add 0x10000
		 * to the pointer value to convert the pointer to a linear address */
		if ((x&0x80000000UL) && (x&0xFF) == 3)
			s += 0x10000;
	}

	/* copy one page at a time */
	sf = s + howmuch;
	if (s > sf) {
		/* alternate version for top of 4GB access */
		sf = (volatile unsigned char*)(~0UL);
		do {
			__try {
				do {
					cc = *s;
					*d++ = cc;
				} while ((s++) != sf);
			}
			__except(1) {
				do {
					*d++ = 0xFF;
					fail++;
				} while ((s++) != sf);
			}
		} while (s != sf && s != (volatile unsigned char*)0);
	}
	else {
		while (s < sf) {
			t = (volatile unsigned char*)((((DWORD)s) | 0xFFF) + 1);

			/* handle 4GB wraparound. but if sf had wrapped around, we would be in the upper portion of this function */
			if (t == (volatile unsigned char*)0) t = sf;
			else if (t > sf) t = sf;

			__try {
				while (s < t) {
					cc = *s;
					*d++ = cc;
					s++;
				}
			}
			__except(1) {
				while (s < t) {
					*d++ = 0xFF;
					fail++;
					s++;
				}
			}
		}
	}

	return fail;
#else
	if (displayMode == DIM_DATASEG) {
		UINT my_ds=0;

		__asm mov my_ds,ds
		memofs += ((uint32_t)my_ds << 4UL);
	}

	if (windows_mode == WINDOWS_REAL) {
		unsigned char *d = captureTmp,cc=0;
		unsigned char far *sseg;
		unsigned int s=0,sf;

		sseg = MK_FP((unsigned int)(memofs >> 4UL),0);
		s = (unsigned int)(memofs & 0xFUL);
		sf = s + howmuch;
		while (s < sf) {
			cc = sseg[s];
			s++; *d++ = cc;
		}

		return 0;
	}
	else if (windows_mode == WINDOWS_NT) {
		DWORD linaddr=0,linhandle=0;
		DWORD handle;
		DWORD result;
		DWORD err;
		char tmp[64];

		/* Use DPMI to allocate a linear block.
		 * We will fill this block with executable code and then make the WOW Win32
		 * code execute it from the Win32 world. */
		__asm {
			mov	ax,0x0501
			xor	bx,bx
			mov	cx,4096
			int	31h
			jc	nope
			mov	word ptr linaddr,cx
			mov	word ptr linaddr+2,bx
			mov	word ptr linhandle,di
			mov	word ptr linhandle+2,si
nope:
		}

		if (linaddr == 0) return howmuch; /* failed */

		SetSelectorBase(viewselector,linaddr);
		SetSelectorLimit(viewselector,4096);

		/* ReadProcessMemory() refuses to do any work for us, so we'll just inject
		 * 32-bit code into linear memory and execute it ourself.
		 *
		 * See what happens Microsoft when your APIs arbitrarily restrict functionality?
		 * This is the hackery that programmers have to resort to, that you have to support
		 * in your OS. If you had simply provided Win16 apps in NT a way to catch their
		 * faults properly, none of this hack-fuckery would be necessary! */
		{
			static const unsigned int except_ofs = 128;
			unsigned char far *x;
			

/*========================================================================*
 *   MAIN SECTION                                                         *
 *========================================================================*/
			x = MK_FP(viewselector,0);

			*x++ = 0xFC;	/* CLD */

			*x++ = 0x68;	/* PUSH <addr> */
			*((uint32_t*)x) = linaddr + except_ofs; x += 4;

			*x++ = 0x64; *x++ = 0xFF; *x++ = 0x35; /* PUSH FS:[0] */
			*((uint32_t*)x) = 0; x += 4;

			*x++ = 0x64; *x++ = 0x89; *x++ = 0x25; /* MOV FS:[0],ESP */
			*((uint32_t*)x) = 0; x += 4;

			*x++ = 0x51;	/* PUSH CX */
			*x++ = 0x56;	/* PUSH SI */
			*x++ = 0x57;	/* PUSH DI */

			*x++ = 0xBE;	/* MOV ESI,... */
			*((uint32_t*)x) = memofs; x += 4;

			*x++ = 0xBF;	/* MOV EDI,... */
			*((uint32_t*)x) = __GetVDMPointer32W((void far*)captureTmp,1); x += 4;

			*x++ = 0xB9;	/* MOV ECX,... */
			*((uint32_t*)x) = (howmuch + 3) >> 2; x += 4;

			*x++ = 0xF3; *x++ = 0xA5; /* REP MOVSD */

			*x++ = 0x89;	/* MOV EAX,ECX */
			*x++ = 0xC8;

			*x++ = 0x5F;	/* POP DI */
			*x++ = 0x5E;	/* POP SI */
			*x++ = 0x59;	/* POP CX */

			*x++ = 0x64; *x++ = 0x8F; *x++ = 0x05; /* POP FS:[0] */
			*((uint32_t*)x) = 0; x += 4;

			*x++ = 0x83; *x++ = 0xC4; *x++ = 0x04; /* ADD ESP,4 */

			*x++ = 0xC3;	/* ret */

/*========================================================================*
 *   EXCEPTION HANDLER                                                    *
 *========================================================================*/
			x = MK_FP(viewselector,except_ofs);

			*x++ = 0x8B; *x++ = 0x44; *x++ = 0x24; *x++ = 0x0C; /* MOV EAX, [ESP+C]  aka the struct _CONTEXT */

			*x++ = 0x83; *x++ = 0x80;			/* ADD [EAX+0xB8],0x02   aka the EIP field */
			*((uint32_t*)x) = 0xB8; x += 4;
			*x++ = 0x02;

			*x++ = 0x31;	/* XOR EAX,EAX */
			*x++ = 0xC0;

			*x++ = 0xC3;	/* ret */
		}

		result = __CallProcEx32W(CPEX_DEST_STDCALL,0/*5 param*/,linaddr);

		__asm {
			mov	ax,0x0502
			mov	di,word ptr linhandle
			mov	si,word ptr linhandle+2
			int	31h
		}

		if (result != 0) {
			unsigned int i;
			for (i=0;i < howmuch;i++) captureTmp[i] = 0xFF;
		}

		return result;
	}
	else {
		volatile unsigned char *d = captureTmp,cc=0;
		unsigned int s=0,sf,t;
		int fail=0;

		SetSelectorBase(viewselector,memofs & 0xFFFFF000UL);
		SetSelectorLimit(viewselector,howmuch + 0x1000UL);

		s = (unsigned int)(memofs & 0xFFFUL);
		sf = s + howmuch;
		while (s < sf) {
			t = (s | 0xFFFU) + 1U;
			if (t > sf) t = sf;
			faulted = 0;

			while (s < t) {
				/* we have to do it this way to exert better control over how we resume from fault */
				__asm {
					push	si
					push	es

					mov	si,s
					mov	ax,viewselector
					mov	es,ax

					call	_w16_capture

					pop	es
					pop	si
					mov	cc,al
				}
				if (faulted) break;
				s++; *d++ = cc;
			}

			while (s < t) {
				s++; *d++ = 0xFF; fail++;
			}
		}

		return fail;
	}
#endif
}

static int CaptureMemory(DWORD memofs,unsigned int howmuch) {
	int fail = 0;

	if (howmuch > sizeof(captureTmp) || howmuch == 0) return howmuch;

	/* the actual routines don't handle 4GB wrap very well */
	if ((memofs+((DWORD)howmuch)) < memofs)
		howmuch = (unsigned int)(0UL - memofs);

	switch (displayMode) {
		case DIM_DATASEG:
		case DIM_LINEAR:
			fail = CaptureMemoryLinear(memofs,howmuch);
			break;
		default:
			memset(captureTmp,0xFF,howmuch);
			fail = howmuch;
			break;
	}

	return fail;
}

static void DoScanForward() { /* scan forward from current location to find next valid region */
	/* NTS: Because of the number of exceptions/second we generate, Windows ME will very
	 *      quickly revert to ignoring our exception handler and shutting down this program.
	 *      Do not run under Windows ME. Hopefully I can figure out how to hack Windows ME
	 *      not do to that! */

	/* if the current location is valid */
	if (CaptureMemory(displayOffset,1) == 0) {
		/* step forward one page */
		displayOffset = (displayOffset | 0xFFFUL) + 1UL;

		/* scan from the current location, until we hit an unmapped page */
		while (CaptureMemory(displayOffset,1) == 0) {
			displayOffset = (displayOffset | 0xFFFUL) + 1UL;
			if (displayOffset == 0UL) return; /* Whoops! Did we rollover past 4GB? */

			/* allow user to abort scan by pressing ESC */
			if (GetAsyncKeyState(VK_ESCAPE)) return;
		}
	}
	else {
		/* step forward one page */
		displayOffset = (displayOffset | 0xFFFUL) + 1UL;

		/* continue scanning until we hit a mapped page */
		while (CaptureMemory(displayOffset,1) != 0) {
			displayOffset = (displayOffset | 0xFFFUL) + 1UL;
			if (displayOffset == 0UL) return; /* Whoops! Did we rollover past 4GB? */

			/* allow user to abort scan by pressing ESC */
			if (GetAsyncKeyState(VK_ESCAPE)) return;
		}
	}
}

static void DoScanBackward() { /* scan forward from current location to find next valid region */
	/* NTS: Because of the number of exceptions/second we generate, Windows ME will very
	 *      quickly revert to ignoring our exception handler and shutting down this program.
	 *      Do not run under Windows ME. Hopefully I can figure out how to hack Windows ME
	 *      not do to that! */

	/* if the current location is valid */
	if (CaptureMemory(displayOffset,1) == 0) {
		/* step back one page */
		displayOffset = (displayOffset - 1UL) & (~0xFFFUL);

		/* scan from the current location, until we hit an unmapped page */
		while (CaptureMemory(displayOffset,1) == 0) {
			displayOffset = (displayOffset - 1UL) & (~0xFFFUL);
			if (displayOffset == 0UL) return; /* Whoops! Did we rollover past 4GB? */

			/* allow user to abort scan by pressing ESC */
			if (GetAsyncKeyState(VK_ESCAPE)) return;
		}

		/* continue scanning until we hit an unmapped page */
		while (CaptureMemory(displayOffset,1) != 0) {
			displayOffset = (displayOffset - 1UL) & (~0xFFFUL);
			if (displayOffset == 0UL) return; /* Whoops! Did we rollover past 4GB? */

			/* allow user to abort scan by pressing ESC */
			if (GetAsyncKeyState(VK_ESCAPE)) return;
		}

		displayOffset = (displayOffset | 0xFFFUL) + 1UL;
	}
	else {
		/* step forward one page */
		displayOffset = (displayOffset - 1UL) & (~0xFFFUL);

		/* continue scanning until we hit a mapped page */
		while (CaptureMemory(displayOffset,1) != 0) {
			displayOffset = (displayOffset - 1UL) & (~0xFFFUL);
			if (displayOffset == 0UL) return; /* Whoops! Did we rollover past 4GB? */

			/* allow user to abort scan by pressing ESC */
			if (GetAsyncKeyState(VK_ESCAPE)) return;
		}

		/* continue scanning until we hit an unmapped page */
		while (CaptureMemory(displayOffset,1) == 0) {
			displayOffset = (displayOffset - 1UL) & (~0xFFFUL);
			if (displayOffset == 0UL) return; /* Whoops! Did we rollover past 4GB? */

			/* allow user to abort scan by pressing ESC */
			if (GetAsyncKeyState(VK_ESCAPE)) return;
		}

		displayOffset = (displayOffset | 0xFFFUL) + 1UL;
	}
}

static const char *hexes = "0123456789ABCDEF";

static void updateDisplayModeMenuCheck() {
	CheckMenuItem(hwndMainViewMenu,IDC_VIEW_DATASEG,
		MF_BYCOMMAND|(displayMode==DIM_DATASEG?MF_CHECKED:0));
	CheckMenuItem(hwndMainViewMenu,IDC_VIEW_LINEAR,
		MF_BYCOMMAND|(displayMode==DIM_LINEAR?MF_CHECKED:0));
}

static void RedrawMemory(HWND hwnd,HDC hdc) {
	unsigned char *src,cc;
	int rowspercopy;
	DWORD memofs;
	int x,y,fail;
	HFONT of;

	if (hwndMainClientDataColumns == 0)
		return;

	SetBkMode(hdc,OPAQUE);
	SetBkColor(hdc,RGB(255,255,255));
	of = (HFONT)SelectObject(hdc,monospaceFont);
	rowspercopy = sizeof(captureTmp) / hwndMainClientDataColumns;

	memofs = displayOffset;
	for (y=0;y < hwndMainClientRows;y++) {
		sprintf(drawTmp,"%08lX ",(unsigned long)memofs);
		memofs += hwndMainClientDataColumns;
		TextOut(hdc,0,y*monospaceFontHeight,drawTmp,hwndMainClientAddrColumns);
	}

	memofs = displayOffset;
	for (y=0;y < hwndMainClientRows;y++,memofs += hwndMainClientDataColumns) {
		fail = CaptureMemory(memofs,hwndMainClientDataColumns);
		src = captureTmp;

		if (fail > 0)
			SetTextColor(hdc,RGB(128,0,0));
		else
			SetTextColor(hdc,RGB(0,0,0));

		for (x=0;x < hwndMainClientDataColumns;x++) {
			cc = *src++;
			drawTmp[(x*3)+0] = hexes[cc>>4];
			drawTmp[(x*3)+1] = hexes[cc&0xF];
			drawTmp[(x*3)+2] = ' ';

			if (cc == 0) cc = '.';
			drawTmp[(hwndMainClientDataColumns*3)+x] = cc;
		}
		drawTmp[hwndMainClientDataColumns*4] = 0;
		TextOut(hdc,(hwndMainClientAddrColumns+1)*monospaceFontWidth,y*monospaceFontHeight,drawTmp,hwndMainClientDataColumns*4);
	}

	SelectObject(hdc,of);
}

static void ClearDisplayMode() {
}

static void SetDisplayModeLinear(DWORD ofs) {
	ClearDisplayMode();

	displayMode = DIM_LINEAR;
	displayOffset = ofs;
	displayMax = 0xFFFFFFFFUL;
}

static void UpdateClientAreaValues() {
	GetClientRect(hwndMain,&hwndMainClientArea);
	hwndMainClientRows = (hwndMainClientArea.bottom + monospaceFontHeight - 1) / monospaceFontHeight;
	hwndMainClientColumns = hwndMainClientArea.right / monospaceFontWidth;
	hwndMainClientAddrColumns = 8;
	hwndMainClientDataColumns = (hwndMainClientColumns - (hwndMainClientAddrColumns + 1)) / 4;
	if (hwndMainClientDataColumns == 15 || hwndMainClientDataColumns == 17) hwndMainClientDataColumns = 16;
	if (hwndMainClientDataColumns < 0) hwndMainClientDataColumns = 0;
	if (hwndMainClientDataColumns > 128) hwndMainClientDataColumns = 128;
}

BOOL CALLBACK FAR __loadds GoToDlgProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_INITDIALOG) {
		char tmp[128];
		sprintf(tmp,"0x%08lX",(unsigned long)displayOffset);
		SetDlgItemText(hwnd,IDC_GOTO_VALUE,tmp);
		return TRUE;
	}
	else if (message == WM_COMMAND) {
		if (wparam == IDOK) {
			char tmp[128];
			GetDlgItemText(hwnd,IDC_GOTO_VALUE,tmp,sizeof(tmp));
			displayOffset = strtoul(tmp,NULL,0);
			EndDialog(hwnd,1);
		}
		else if (wparam == IDCANCEL) {
			EndDialog(hwnd,0);
		}
	}

	return FALSE;
}

static void ShowHelp() {
	MessageBox(hwndMain,
		"F1 = This help\n"
		"< = Scan backward\n"
		"> = Scan forward\n"
		"up/down/left/right arrow keys = Navigate\n"
		"ESC = If scanning, abort scan",
		"Help key",MB_OK);
}

#if TARGET_MSDOS == 16
FARPROC WndProc_MPI;
#endif
WindowProcType WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0; /* OK */
	}
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
	else if (message == WM_ERASEBKGND) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,FALSE)) {
			HBRUSH oldBrush,newBrush;
			HPEN oldPen,newPen;

			newPen = (HPEN)GetStockObject(NULL_PEN);
			newBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

			oldPen = SelectObject((HDC)wparam,newPen);
			oldBrush = SelectObject((HDC)wparam,newBrush);

			Rectangle((HDC)wparam,um.left,um.top,um.right+1,um.bottom+1);

			SelectObject((HDC)wparam,oldBrush);
			SelectObject((HDC)wparam,oldPen);
		}

		return 1; /* Important: Returning 1 signals to Windows that we processed the message. Windows 3.0 gets really screwed up if we don't! */
	}
	else if (message == WM_KEYDOWN) {
		if (wparam == VK_LEFT) {
			displayOffset--;
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_RIGHT) {
			displayOffset++;
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_UP) {
			displayOffset -= hwndMainClientDataColumns;
			if (GetAsyncKeyState(VK_SHIFT)) displayOffset &= ~(0xFUL);
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_DOWN) {
			displayOffset += hwndMainClientDataColumns;
			if (GetAsyncKeyState(VK_SHIFT)) displayOffset &= ~(0xFUL);
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_PRIOR) {/*page up*/
			displayOffset -= hwndMainClientDataColumns * (hwndMainClientRows-2);
			if (GetAsyncKeyState(VK_SHIFT)) displayOffset &= ~(0xFUL);
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_NEXT) {/*page down*/
			displayOffset += hwndMainClientDataColumns * (hwndMainClientRows-2);
			if (GetAsyncKeyState(VK_SHIFT)) displayOffset &= ~(0xFUL);
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_F1) {
			ShowHelp();
		}
		else if (wparam == 0xBE/*VK_PERIOD?*/) {
			SetCursor(LoadCursor(NULL,IDC_WAIT)); /* Let the user know we're scanning */

			DoScanForward();
			InvalidateRect(hwnd,NULL,FALSE);

			SetCursor(LoadCursor(NULL,IDC_ARROW)); /* Let the user know we're done */
		}
		else if (wparam == 0xBC/*VK_COMMA?*/) {
			SetCursor(LoadCursor(NULL,IDC_WAIT)); /* Let the user know we're scanning */

			DoScanBackward();
			InvalidateRect(hwnd,NULL,FALSE);

			SetCursor(LoadCursor(NULL,IDC_ARROW)); /* Let the user know we're done */
		}
	}
	else if (message == WM_COMMAND) {
		if (HIWORD(lparam) == 0) { /* from a menu */
			switch (LOWORD(wparam)) {
				case IDC_FILE_QUIT:
					SendMessage(hwnd,WM_CLOSE,0,0);
					break;
				case IDC_FILE_GO_TO:
					if (DialogBox(myInstance,MAKEINTRESOURCE(IDD_GOTO),hwnd,MakeProcInstance(GoToDlgProc,myInstance)))
						InvalidateRect(hwnd,NULL,FALSE);
					break;
				case IDC_VIEW_DATASEG:
					displayMode = DIM_DATASEG;
					InvalidateRect(hwnd,NULL,FALSE);
					updateDisplayModeMenuCheck();
					break;
				case IDC_VIEW_LINEAR:
					displayMode = DIM_LINEAR;
					InvalidateRect(hwnd,NULL,FALSE);
					updateDisplayModeMenuCheck();
					break;
			}
		}
	}
	else if (message == WM_PAINT) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			PAINTSTRUCT ps;

			UpdateClientAreaValues();
			BeginPaint(hwnd,&ps);
			RedrawMemory(hwnd,ps.hdc);
			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

static int CreateAppMenu() {
	hwndMainMenu = CreateMenu();
	if (hwndMainMenu == NULL) return 0;

	hwndMainFileMenu = CreateMenu();
	if (hwndMainFileMenu == NULL) return 0;

	if (!AppendMenu(hwndMainFileMenu,MF_STRING|MF_ENABLED,IDC_FILE_GO_TO,"&Go to..."))
		return 0;

	if (!AppendMenu(hwndMainFileMenu,MF_SEPARATOR,0,""))
		return 0;

	if (!AppendMenu(hwndMainFileMenu,MF_STRING|MF_ENABLED,IDC_FILE_QUIT,"&Quit"))
		return 0;

	if (!AppendMenu(hwndMainMenu,MF_POPUP|MF_STRING|MF_ENABLED,(UINT)hwndMainFileMenu,"&File"))
		return 0;

	hwndMainViewMenu = CreateMenu();
	if (hwndMainViewMenu == NULL) return 0;

	if (!AppendMenu(hwndMainViewMenu,MF_STRING|MF_ENABLED,IDC_VIEW_DATASEG,"&Data segment"))
		return 0;

	if (!AppendMenu(hwndMainViewMenu,MF_STRING|MF_ENABLED,IDC_VIEW_LINEAR,"&Linear memory map"))
		return 0;

	if (!AppendMenu(hwndMainMenu,MF_POPUP|MF_STRING|MF_ENABLED,(UINT)hwndMainViewMenu,"&View"))
		return 0;

	return 1;
}

/* NOTES:
 *   For Win16, programmers generally use WINAPI WinMain(...) but WINAPI is defined in such a way
 *   that it always makes the function prolog return FAR. Unfortunately, when Watcom C's runtime
 *   calls this function in a memory model that's compact or small, the call is made as if NEAR,
 *   not FAR. To avoid a GPF or crash on return, we must simply declare it PASCAL instead. */
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	WNDCLASS wnd;
	MSG msg;

	/* some important decision making rests on what version of Windows we're running under */
	probe_dos();
	detect_windows();

#if TARGET_MSDOS == 16
	if (windows_mode == WINDOWS_OS2) {
		if (MessageBox(NULL,
			"The 16-bit version of this program does not have the ability\n"
			"to catch Page Faults under OS/2. If you browse a region of\n"
			"memory that causes a Page Fault, you will see the appropriate\n"
			"notifcation from OS/2.\n\n"
			"Proceed?",
			"WARNING",MB_YESNO) == IDNO)
			return 1;
	}
	else if (windows_mode == WINDOWS_ENHANCED && windows_version < (0x300 + 95)/*Windows 3.x*/) {
		/* Windows 3.1 has tested my fucking patience to the max and it still crashes going through
		 * our page fault handler. At this point, having burned an entire weekend and one workday
		 * fighting this bullshit I don't really fucking care anymore. Tell the user to use another
		 * version if possible.
		 *
		 * NOTES: The problem it seems is not our page fault handler. The problem is how Windows 3.1
		 *        underneath us handles the page fault. No matter what we do, or even regardless of
		 *        whether or not we hook the page fault, Windows 3.1 will hang or hard crash to DOS
		 *        on a page fault that it can't back up from the swap file. The good news is that
		 *        whatever the hell is wrong was apparently fixed in Windows 95 and the Win16 hooking
		 *        technique we use at least works properly under Windows 95, 98, and ME. */
		if (MessageBox(NULL,
			"The 16-bit version is not reliable at catching page faults\n"
			"under Windows 3.0/3.1 386 Enhanced Mode. If you browse a region\n"
			"of memory that causes a Page Fault, Windows may hard crash or\n"
			"suddenly drop to DOS. It is recommended that you either start\n"
			"Windows 3.x in 286 Standard Mode or use the 32-bit Win32s version\n"
			"if the Win32s subsystem is installed.\n\n"
			"Proceed?",
			"WARNING",MB_YESNO) == IDNO)
			return 1;
	}

	__asm {
		mov my_cs,cs
		mov my_ds,ds
		mov my_ss,ss
	}

	if (windows_mode != WINDOWS_REAL) {
		viewselector = AllocSelector(my_ds);
		if (viewselector == 0) {
			MessageBox(NULL,"Unable to alloc selector","Error",MB_OK);
			return 1;
		}
	}

	/* We really only need to trap Page Fault from 386 enhanced mode Windows 3.x or Windows 9x/ME/NT.
	   Windows 3.x "standard mode" does not involve paging */
	/* NTS: As of 2013/04/21: We no longer hook and unhook once per invocation of the memory copy function.
	 *      TOOLHELP.DLL is one of those DLLs apparently where the MS dev assumed you would only hook/unhook
	 *      once because the Windows 95 version will eventually pagefault in TOOLHELP.DLL if you repeatedly
	 *      hook and unhook rapidly. Once that happens, TOOLHELP.DLL apparently loses it's ability to catch
	 *      page faults and our attempts show up then as the standard KERNEL32.DLL error dialog. So to make
	 *      this program work, we now hook ONCE at startup and unhook ONCE at shutdown. Testing shows the
	 *      program is now fully capable of recovering from page faults and safely scanning the linear memory
	 *      areas. */
	/* TODO: Some long-term Win95 stress testing is required.
	 *       Also test Win98, and WinME */
	if (windows_mode == WINDOWS_NT) {
		/* use "Windows on Windows" to call into the Win32 world and use ReadProcessMemory() */
		if (!genthunk32_init() || genthunk32w_kernel32 == 0) {
			MessageBox(NULL,"Unable to initialize Win16->32 thunking code","Error",MB_OK);
			return 1;
		}

		if ((wow32_GetCurrentProcess=__GetProcAddress32W(genthunk32w_kernel32,"GetCurrentProcess")) == 0) {
			MessageBox(NULL,"Failed to obtain GetCurrentProcess","",MB_OK);
			return 1;
		}
		if ((wow32_ReadProcessMemory=__GetProcAddress32W(genthunk32w_kernel32,"ReadProcessMemory")) == 0) {
			MessageBox(NULL,"Failed to obtain ReadProcessMemory","",MB_OK);
			return 1;
		}
		if ((wow32_GetLastError=__GetProcAddress32W(genthunk32w_kernel32,"GetLastError")) == 0) {
			MessageBox(NULL,"Failed to obtain GetLastError","",MB_OK);
			return 1;
		}
	}
	else if (windows_mode == WINDOWS_ENHANCED) {
		/* Windows 3.1 or higher: Use Toolhelp.
		   Windows 3.0: Use DPMI hacks.
		   Windows NT: Use Toolhelp, or DPMI hack. Doesn't matter. NTVDM.EXE will not pass it down.
		               However, this code is not executed under NT because a better technique is available. */
		hook_use_tlhelp = (windows_version >= ((3 << 8) + 10)); /* Windows 3.1 or higher */

		/* we may cause page faults. be prepared */
		if (hook_use_tlhelp) {
			if (!ToolHelpInit() || __InterruptRegister == NULL || __InterruptUnRegister == NULL)
				hook_use_tlhelp = 0;

			if (hook_use_tlhelp && !__InterruptRegister(NULL,MakeProcInstance((FARPROC)fault_pf_toolhelp,myInstance))) {
				MessageBox(NULL,"Unable to InterruptRegister()","",MB_OK);
				hook_use_tlhelp = 0;
			}
		}
		if (!hook_use_tlhelp) {
			oldDPMIPFHook = win16_getexhandler(14);
			if (!win16_setexhandler(14,fault_pf_dpmi)) {
				MessageBox(NULL,"Unable to hook Page Fault via DPMI","",MB_OK);
				return 1;
			}
		}
	}
#endif

	myInstance = hInstance;

	/* FIXME: Windows 3.0 Real Mode: Why can't we load our own app icon? */
	AppIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
	if (!AppIcon) MessageBox(NULL,"Unable to load app icon","Oops!",MB_OK);

#if TARGET_MSDOS == 16
	WndProc_MPI = MakeProcInstance((FARPROC)WndProc,hInstance);
#endif

	/* NTS: In the Windows 3.1 environment all handles are global. Registering a class window twice won't work.
	 *      It's only under 95 and later (win32 environment) where Windows always sets hPrevInstance to 0
	 *      and window classes are per-application.
	 *
	 *      Windows 3.1 allows you to directly specify the FAR pointer. Windows 3.0 however demands you
	 *      MakeProcInstance it to create a 'thunk' so that Windows can call you (ick). */
	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
#if TARGET_MSDOS == 16
		wnd.lpfnWndProc = (WNDPROC)WndProc_MPI;
#else
		wnd.lpfnWndProc = WndProc;
#endif
		wnd.cbClsExtra = 0;
		wnd.cbWndExtra = 0;
		wnd.hInstance = hInstance;
		wnd.hIcon = AppIcon;
		wnd.hCursor = NULL;
		wnd.hbrBackground = NULL;
		wnd.lpszMenuName = NULL;
		wnd.lpszClassName = WndProcClass;

		if (!RegisterClass(&wnd)) {
			MessageBox(NULL,"Unable to register Window class","Oops!",MB_OK);
			return 1;
		}
	}

	monospaceFont = CreateFont(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH | FF_DONTCARE,"Terminal");
	if (!monospaceFont) {
		MessageBox(NULL,"Unable to create Font","Oops!",MB_OK);
		return 1;
	}

	{
		HWND hwnd = GetDesktopWindow();
		HDC hdc = GetDC(hwnd);
		monospaceFontHeight = 12;
		if (!GetCharWidth(hdc,'A','A',&monospaceFontWidth)) monospaceFontWidth = 9;
		ReleaseDC(hwnd,hdc);
	}

	if (!CreateAppMenu())
		return 1;

	/* default to "linear" view */
	SetDisplayModeLinear(0xF0000);

	hwndMain = CreateWindow(WndProcClass,"HEXMEM memory dumping tool",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,
		(monospaceFontWidth*80)+(GetSystemMetrics(SM_CXFRAME)*2),
		(monospaceFontHeight*25)+(GetSystemMetrics(SM_CYFRAME)*2)+GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYBORDER)+GetSystemMetrics(SM_CYMENU),
		NULL,NULL,
		hInstance,NULL);
	if (!hwndMain) {
		MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

	SetMenu(hwndMain,hwndMainMenu);
	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain);

	updateDisplayModeMenuCheck();
	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#if TARGET_MSDOS == 16
	if (windows_mode == WINDOWS_NT) {
		genthunk32_free();
	}
	else if (windows_mode == WINDOWS_ENHANCED) {
		/* remove page fault handler */
		if (hook_use_tlhelp) __InterruptUnRegister(NULL);
		else win16_setexhandler(14,oldDPMIPFHook);
	}

	if (windows_mode != WINDOWS_REAL)
		FreeSelector(viewselector);
#endif

	DestroyMenu(hwndMainMenu);
	hwndMainMenu = NULL;
	DeleteObject(monospaceFont);
	monospaceFont = NULL;
	return msg.wParam;
}

