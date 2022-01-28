/* cpu.c
 *
 * Runtime CPU detection library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS
 *   - Windows 3.0/3.1/95/98/ME
 *   - Windows NT 3.1/3.51/4.0/2000/XP/Vista/7
 *   - OS/2 16-bit
 *   - OS/2 32-bit
 *
 * A common library to autodetect the CPU at runtime. If the program calling us
 * is interested, we can also provide Pentium CPUID and extended CPUID information.
 * Also includes code to autodetect at runtime 1) if SSE is present and 2) if SSE
 * is enabled by the OS and 3) if we can enable SSE. */

/* FIXME: The 16-bit real mode DOS builds of this program are unable to detect CPUID under OS/2 2.x and OS/2 Warp 3. Why? */

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
/* Win16: We're probably on a 386, but we could be on a 286 if Windows 3.1 is in standard mode.
 *        If the user manages to run us under Windows 3.0, we could also run in 8086 real mode.
 *        We still do the tests so the Windows API cannot deceive us, but we still need GetWinFlags
 *        to tell between 8086 real mode + virtual8086 mode and protected mode. */
# include <windows.h>
# include <toolhelp.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>

/* DEBUG: Flush out calls that aren't there */
#ifdef TARGET_OS2
# define int86 ___EVIL___
# define ntvdm_RegisterModule ___EVIL___
# define ntvdm_UnregisterModule ___EVIL___
# define _dos_getvect ___EVIL___
# define _dos_setvect ___EVIL___
#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
# include <hw/dos/winfcon.h>
#endif

char				cpu_cpuid_vendor[13]={0};
struct cpu_cpuid_features	cpu_cpuid_features = {0};
signed char			cpu_basic_level = -1;
uint32_t			cpu_cpuid_max = 0;
unsigned short			cpu_flags = 0;
uint16_t			cpu_tmp1 = 0;

void cpu_probe() {
#if TARGET_MSDOS == 32
	/* we're obviously in 32-bit protected mode, or else this code would not be running at all */
	/* Applies to: 32-bit DOS, Win32, Win95/98/ME/NT/2000/XP/etc. */
	cpu_flags = CPU_FLAG_PROTECTED_MODE | CPU_FLAG_PROTECTED_MODE_32;
#else
	cpu_flags = 0;
#endif

	cpu_basic_level = cpu_basic_probe();

#if defined(TARGET_OS2)
	/* OS/2 wouldn't let a program like myself touch control registers. Are you crazy?!? */
	cpu_flags |= CPU_FLAG_DONT_WRITE_RDTSC;
#elif defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
	/* Under Windows 3.1 Win32s and Win 9x/ME/NT it's pretty much a given any attempt to work with
	 * control registers will fail. Win 9x/ME will silently ignore, and NT will fault it */
	cpu_flags |= CPU_FLAG_DONT_WRITE_RDTSC;
#elif !defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
	/* 32-bit DOS: Generally yes we can, but only if we're Ring 0 */
	{
		const uint16_t s = cpu_read_my_cs() & 3;
		if (s != 0) cpu_flags |= CPU_FLAG_DONT_WRITE_RDTSC;
	}
#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
	/* Windows 3.0/3.1 specific: are we in 32-bit protected mode? 16-bit protected mode? real mode?
	 * real mode with v86 mode (does Windows even work that way?). Note that GetWinFlags only appeared
	 * in Windows 3.0. If we're under Windows 2.x we have to use alternative detection methods, or
	 * else assume Real Mode since Windows 2.x usually does run that way. But: There are 286 and 386
	 * enhanced versions of Windows 2.x, so how do we detect those?
	 *
	 * NTS: This code doesn't even run under Windows 2.x. If we patch the binary to report itself as
	 *      a 2.x compatible and try to run it under Windows 2.11, the Windows kernel says it's
	 *      "out of memory" and then everything freezes (?!?). */
	{
# if TARGET_WINDOWS >= 30 /* If targeting Windows 3.0 or higher at compile time, then assume GetWinFlags() exists */
		DWORD flags = GetWinFlags();
		if (1) {
# elif TARGET_WINDOWS >= 20 /* If targeting Windows 2.0 or higher, then check the system first in case we're run under 3.0 or higher */
		/* FIXME: If locating the function by name fails, what ordinal do we search by? */
		DWORD (PASCAL FAR *__GetWinFlags)() = (LPVOID)GetProcAddress(GetModuleHandle("KERNEL"),"GETWINFLAGS");
		if (__GetWinFlags != NULL) {
			DWORD flags = __GetWinFlags();
			MessageBox(NULL,"Found it","",MB_OK);
# else /* don't try. Windows 1.0 does not have GetWinFlags() and does not have any dynamic library functions either. There is
          no GetModuleHandle, GetProcAddress, etc. */
		if (0) {
# endif
			if (WF_PMODE) {
				cpu_flags |= CPU_FLAG_DONT_WRITE_RDTSC;
				if (flags & WF_ENHANCED)
					cpu_flags |= CPU_FLAG_PROTECTED_MODE | CPU_FLAG_PROTECTED_MODE_32;
				else if (flags & WF_STANDARD)
					cpu_flags |= CPU_FLAG_PROTECTED_MODE;
			}
			/* I highly doubt Windows 3.0 "real mode" every involves virtual 8086 mode, but
			 * just in case, check the machine status register. Since Windows 3.0 could run
			 * on an 8086, we must be cautious to do this test only on a 286 or higher */
			else if (cpu_basic_level >= 2) {
				unsigned int tmp=0;

				__asm {
					.286
					smsw tmp
				}

				if (tmp & 1) {
					/* if the PE bit is set, we're under Protected Mode
					 * that must mean that all of windows is in Real Mode, but overall
					 * the whole show is in virtual 8086 mode.
					 * We're assuming here that Windows would not lie to us about what mode is active.
					 *
					 * THEORY: Could this happen if Windows 3.0 were started in Real Mode
					 *         while EMM386.EXE is resident and active? */
					cpu_flags |= CPU_FLAG_V86_ACTIVE;
					cpu_flags |= CPU_FLAG_DONT_WRITE_RDTSC;
				}
			}
		}
	}
#endif

    /* FIXME: Prior to adding this check I had no problem with reading CR4 on later
     *        versions (post 1993) revisions of the 80486. It was only on older
     *        (pre 1993) version of the 80486 that reading CR4 caused a fault and
     *        crash.
     *
     *        When exactly did the change-over happen?
     *
     *        I'm guessing it happened probably at the same time that later 80486
     *        processors gained the CPUID instruction.
     *
     *        But I'm not sure.
     *
     *        Also we can't assume clone 80486's from other companies act the same
     *        way. But if CPUID is around we could report it available if and only
     *        if the vendor string is "GenuineIntel" I suppose.
     *
     *        Until I can pin down when it's legal to do it, we assume CR4 exists
     *        only on Pentium or higher.
     *
     *        Not only is this needed to detect if SSE is enabled, but it is also
     *        needed by the DOSLTP library to detect certain characteristics about
     *        paging.
     *
     *        We will also need to read CR4 to determine if, as possibly ring-3
     *        user-space code, whether we're allowed to use the RDTSC instruction. */
    /* Don't try to read CR4 on pre-Pentium CPUs, it will cause a crash. */
	if (cpu_basic_level >= 5)
		cpu_flags |= CPU_FLAG_CR4_EXISTS;

    /* Don't use RDTSC in Virtual 8086 mode because EMM386.EXE may not have enabled it
     * and may not be prepared to handle the Invalid Opcode exception that occurs when
     * you try it.
     *
     * 32-bit DOS applications are not affected by this flaw.
     *
     * TODO: What combinations of MS-DOS and EMM386.EXE have this flaw? */
#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2) && TARGET_MSDOS == 16
    if (cpu_v86_active) {
        cpu_flags |= CPU_FLAG_DONT_USE_RDTSC;
    }
#endif
}

