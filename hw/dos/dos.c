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

/* DEBUG: Flush out calls that aren't there */
#ifdef TARGET_OS2
# define int86 ___EVIL___
# define int386 ___EVIL___
# define ntvdm_RegisterModule ___EVIL___
# define ntvdm_UnregisterModule ___EVIL___
# define _dos_getvect ___EVIL___
# define _dos_setvect ___EVIL___
#endif

struct lib_dos_options lib_dos_option={0};

/* DOS version info */
uint8_t dos_flavor = 0;
uint16_t dos_version = 0;
uint32_t freedos_kernel_version = 0;
const char *dos_version_method = NULL;

#if TARGET_MSDOS == 32
char *freedos_kernel_version_str = NULL;
#else
char far *freedos_kernel_version_str = NULL;
#endif

void probe_dos() {
#if TARGET_MSDOS == 32 && 0
	assert(sizeof(struct dpmi_realmode_call) == 0x32);
	assert(offsetof(struct dpmi_realmode_call,ss) == 0x30);
	assert(offsetof(struct dpmi_realmode_call,cs) == 0x2C);
#endif

	if (dos_version == 0) {
#ifdef TARGET_WINDOWS
# if TARGET_MSDOS == 32
#  ifdef WIN386
/* =================== Windows 3.0/3.1 Win386 ================= */
		DWORD raw = GetVersion(); /* NTS: The Win16 version does not tell us if we're running under Windows NT */

		dos_version_method = "GetVersion";
		dos_version = (((raw >> 24UL) & 0xFFUL) << 8UL) | (((raw >> 16UL) & 0xFFUL) << 0UL);

		/* Windows NT/2000/XP NTVDM.EXE lies to us, reporting Windows 95 numbers and MS-DOS 5.0 */
		if (dos_version == 0x500) {
			uint16_t x = 0;

			/* Sorry Microsoft, but you make it hard for us to detect and we have to break your OS to find the info we need */
			__asm {
				mov	ax,0x3306
				mov	bx,0
				int	21h
				jc	err1
				mov	x,bx
err1:
			}

			if (x != 0 && x != 0x005) { /* Once pushed to reveal the true DOS version, NTVDM.EXE responds with v5.50 */
				dos_version = (x >> 8) | (x << 8);
				dos_version_method = "INT 21h AX=3306h/NTVDM.EXE";
			}
		}
#  else
/* =================== Windows 32-bit ================== */
		DWORD raw;
		/* GetVersion() 32-bit doesn't return the DOS version at all. The upper WORD has build number instead. */
		/* Instead, use GetVersionEx() to detect system. If system is Windows 3.1 or 9x/ME we might be able
		 * to get away with abusing the DPMI server deep within Windows to get what we want. Else, if it's
		 * Windows NT, we simply assume v5.50 */

		/* assume v5.0 */
		dos_version = 0x500;
		dos_version_method = "Guessing";

		/* use the Win32 version of GetVersion() to determine what OS we're under */
		raw = GetVersion();
		if (raw & 0x80000000UL) { /* Windows 9x/ME */
			/* Start by guessing the version number based on which version of Windows we're under */
			unsigned char major = raw & 0xFF,minor = (raw >> 8) & 0xFF,ok=0;

			dos_version_method = "Guessing by Windows version";
			if (major < 4) { /* Windows 3.1 Win32s */
				dos_version = 0x616;	/* Assume MS-DOS 6.22, though it could be 6.20 or even 6.00 */
			}
			else if (major == 4) { /* Windows 95/98/ME */
				if (minor >= 90)
					dos_version = 0x800; /* Windows ME (8.00) */
				else if (minor >= 10)
					dos_version = 0x70A; /* Windows 98 (7.10) */
				else
					dos_version = 0x700; /* Windows 95 */
			}

			/* Try: Windows 9x/ME QT_Thunk hack to call down into the Win16 layer's version of GetVersion() */
			if (!ok && major == 4 && Win9xQT_ThunkInit()) {
				DWORD fptr,raw=0;

				fptr = GetProcAddress16(win9x_kernel_win16,"GETVERSION");
				if (fptr != 0) {
					dos_version_method = "Read from Win16 GetVersion() [32->16 QT_Thunk]";

					{
						__asm {
							mov	edx,fptr
							mov	eax,dword ptr [QT_Thunk]

							; QT_Thunk needs 0x40 byte of data storage at [EBP]
							; give it some, right here on the stack
							push	ebp
							mov	ebp,esp
							sub	esp,0x40

							call	eax	; <- QT_Thunk

							; release stack storage
							mov	esp,ebp
							pop	ebp

							; take Win16 response in DX:AX translate to EAX
							shl	edx,16
							and	eax,0xFFFF
							or	eax,edx
							mov	raw,eax
						}
					}

					if (raw != 0) {
						dos_version = (((raw >> 24UL) & 0xFFUL) << 8UL) | (((raw >> 16UL) & 0xFFUL) << 0UL);
						ok = 1;
					}
				}
			}
			/* Tried: Windows 3.1 with Win32s. Microsoft Win32 documentation gleefully calls Dos3Call "obsolete",
			 *        yet inspection of the Win32s DLLs shows that W32SKRNL.DLL has a _Dos3Call@0 symbol in it
			 *        that acts just like the Win16 version, calling down into DOS, and most of the Win32s DLLs
			 *        rely on it quite heavily to implement Win32 functions (the GetSystemTime function for example
			 *        using it to call INT 21h AH=2Ah).
			 *
			 *        Some old MSDN documentation I have has a list of INT 21h calls and corresponding Win32
			 *        functions to use. Again of course, they skip right over "Get MS-DOS version", no help there.
			 *
			 *        Anyway, calling this function with AX=0x3306 or AH=0x30 yields no results. Apparently, Microsoft
			 *        implemented passing through file I/O, date/time, and code page conversions, yet never considered
			 *        people might use it for something like... asking DOS it's version number. Attempting to make
			 *        these calls yields zero in AX and BX, or for AX=3306, a false return number that would imply
			 *        MS-DOS v1.0 (EAX=1). So, _Dos3Call@0 is not an option.
			 *
			 *        But then that means we have absolutely no way to determine the DOS kernel version (short of
			 *        poking our nose into segments and memory locations we have no business being in!). We can't
			 *        use _Dos3Call@0, we can't use GetVersion() because the Win32 GetVersion() doesn't return
			 *        the DOS version, and we can't use Win95 style thunks because Win32s doesn't have a publicly
			 *        available and documented way to thunk down into Win16. We have absolutely jack shit to go by.
			 *
			 *        Hey, Microsoft... When you released Win32s in 1993, did you ever stop to consider someone
			 *        might want to do something as simple as query the DOS version? Why couldn't you guys have
			 *        done something straightforward like a "GetDOSVersion()" API function that works under
			 *        Windows 9x/ME and returns an error under NT? I know it's silly of me to ask this in 2012
			 *        when Windows 8 is around the corner and Win32s are long dead, but often it seems like you
			 *        guys really don't stop to think about things like that and you make really stupid mistakes
			 *        with your APIs. */
		}
		else {
			dos_version = 0x532; /* Windows NT v5.50 */
		}
#  endif
# elif TARGET_MSDOS == 16
/* =================== Windows 16-bit ================== */
		DWORD raw = GetVersion(); /* NTS: The Win16 version does not tell us if we're running under Windows NT */

		dos_version_method = "GetVersion";
		dos_version = (((raw >> 24UL) & 0xFFUL) << 8UL) | (((raw >> 16UL) & 0xFFUL) << 0UL);

		/* Windows NT/2000/XP NTVDM.EXE lies to us, reporting Windows 95 numbers and MS-DOS 5.0 */
		if (dos_version == 0x500) {
			uint16_t x = 0;

			/* Sorry Microsoft, but you make it hard for us to detect and we have to break your OS to find the info we need */
			__asm {
				mov	ax,0x3306
				mov	bx,0
				int	21h
				jc	err1
				mov	x,bx
err1:
			}

			if (x != 0 && x != 0x005) { /* Once pushed to reveal the true DOS version, NTVDM.EXE responds with v5.50 */
				dos_version = (x >> 8) | (x << 8);
				dos_version_method = "INT 21h AX=3306h/NTVDM.EXE";
			}
		}

		/* TODO: DOS "flavor" detection */
		/* TODO: If FreeDOS, get the kernel version and allocate a selector to point at FreeDOS's revision string */
# else
#  error dunno
# endif
#elif defined(TARGET_OS2)
/* =================== OS/2 ==================== */
		dos_version = (10 << 8) | 0;
		dos_version_method = "Blunt guess";

# if TARGET_MSDOS == 32
		{
			ULONG major=0,minor=0,rev=0;
			DosQuerySysInfo(QSV_VERSION_MAJOR,QSV_VERSION_MAJOR,&major,sizeof(major));
			DosQuerySysInfo(QSV_VERSION_MINOR,QSV_VERSION_MINOR,&minor,sizeof(minor));
			DosQuerySysInfo(QSV_VERSION_REVISION,QSV_VERSION_REVISION,&rev,sizeof(rev));
			if (major != 0) {
				dos_version_method = "DosQuerySysInfo (OS/2)";
				dos_version = (major << 8) | minor;
				/* TODO: store the revision value too somewhere! */
			}
		}
# elif TARGET_MSDOS == 16
		{
			USHORT x=0;
			DosGetVersion(&x);
			if (x != 0) {
				dos_version_method = "DosGetVersion (OS/2)";
				dos_version = x;
			}
		}
# else
#  error dunno
# endif
#else
/* =================== MS-DOS ================== */
		union REGS regs;

		regs.w.ax = 0x3000;
# if TARGET_MSDOS == 32
		int386(0x21,&regs,&regs);
# else
		int86(0x21,&regs,&regs);
# endif
		dos_version = (regs.h.al << 8) | regs.h.ah;
		dos_version_method = "INT 21h AH=30h";

		if (dos_version >= 0x500 && regs.h.bh == 0xFD) {
			dos_flavor = DOS_FLAVOR_FREEDOS;
			freedos_kernel_version = (((uint32_t)regs.h.ch) << 16UL) |
				(((uint32_t)regs.h.cl) << 8UL) |
				((uint32_t)regs.h.bl);

			/* now retrieve the FreeDOS kernel string */
			/* FIXME: Does this syscall have a way to return an error or indicate that it didn't return a string? */
			regs.w.ax = 0x33FF;
# if TARGET_MSDOS == 32
			int386(0x21,&regs,&regs);
# else
			int86(0x21,&regs,&regs);
# endif

# if TARGET_MSDOS == 32
			freedos_kernel_version_str = (unsigned char*)(((uint32_t)regs.w.dx << 4UL) + (uint32_t)regs.w.ax);
# else
			freedos_kernel_version_str = MK_FP(regs.w.dx,regs.w.ax);
# endif
		}
		else if (dos_version >= 0x200 && regs.h.bh == 0xFF)
			dos_flavor = DOS_FLAVOR_MSDOS;

		/* but, SETVER can arrange for DOS to lie to us. so get the real version via
		 * undocumented subfunctions (DOS 5.0+ or later, apparently) */
		regs.w.ax = 0x3306;		/* AH=0x33 AL=0x06 */
		regs.w.bx = 0;			/* in case early DOS versions fail to set CF set BX to zero */
# if TARGET_MSDOS == 32
		int386(0x21,&regs,&regs);
# else
		int86(0x21,&regs,&regs);
# endif
		if ((regs.w.cflag & 1) == 0 && regs.h.bl >= 5 && regs.h.bl <= 8) {
			dos_version = (regs.h.bl << 8) | regs.h.bh;
			dos_version_method = "INT 21h AX=3306h";
		}
#endif
	}
}

