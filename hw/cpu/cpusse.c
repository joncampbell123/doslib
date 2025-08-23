
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
#include <hw/dos/doswin.h>
#include <hw/cpu/cpusse.h>

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

unsigned char			cpu_sse_usable = 0;
unsigned char			cpu_sse_usable_probed = 0;
unsigned char			cpu_sse_usable_can_turn_on = 0;
unsigned char			cpu_sse_usable_can_turn_off = 0;
const char*			cpu_sse_unusable_reason = NULL;
const char*			cpu_sse_usable_detection_method = NULL;

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
static unsigned char faulted = 0;

static void __declspec(naked) fault_int6_vec() {
	/* the test routine executes a XORPS xmm0,xmm0 (3 bytes long).
	 * if we just IRET the CPU will go right back and execute it again */
# if TARGET_MSDOS == 32
	__asm {
		push	ds
		push	eax
		mov	ax,seg faulted
		mov	ds,ax
		pop	eax
		mov	faulted,1
		pop	ds
		add	dword ptr [esp],3
		iretd
	}
# else
	__asm {
		push	bp
		mov	bp,sp
		add	word ptr [bp+2],3
		push	ds
		mov	bp,seg faulted
		mov	ds,bp
		mov	faulted,1
		pop	ds
		pop	bp
		iretf
	}
# endif
}

# if TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
unsigned int _cdecl cpu_dpmi_win9x_sse_test();
# endif

# if TARGET_MSDOS == 32 && !defined(TARGET_OS2)
static void __declspec(naked) fault_int6() { /* DPMI exception handler */
	__asm {
		.386p
		push		ds
		push		eax
		mov		ax,seg faulted
		mov		ds,ax
		mov		faulted,1
		add		dword ptr [esp+8+12],3 /* +3 bytes for 'xorps xmm0,xmm0' */
		pop		eax
		pop		ds
		retf
	}
}
# endif
#elif defined(TARGET_WINDOWS) && TARGET_MSDOS == 16 && !defined(TARGET_OS2)
static unsigned char faulted = 0;

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
 * to restart instruction: clear first 10 bytes of the stack, and IRET (??) */
static void __declspec(naked) fault_int6_toolhelp() {
	__asm {
		.386p
		push		ds
		push		ax
		push		bp
		mov		bp,sp

		/* is this for INT 6h? */
		cmp		word ptr [bp+6+6],6	/* SS:SP + 06h = interrupt number */
		jnz		pass_on

		/* set the faulted flag, change the return address, then IRET directly back */
		mov		ax,seg faulted
		mov		ds,ax
		mov		faulted,1
		add		word ptr [bp+6+10],3
		pop		bp
		pop		ax
		pop		ds
		add		sp,0Ah			/* throw away handle, int number, and CS:IP return */
		iret

/* tell ToolHelp we didn't handle the interrupt */
pass_on:	retf
	}
}

static void __declspec(naked) fault_int6() { /* DPMI exception handler */
	__asm {
		.386p
		push		ds
		push		ax
		push		bp
		mov		bp,sp
		mov		ax,seg faulted
		mov		ds,ax
		mov		faulted,1
		add		word ptr [bp+6+6],3
		pop		bp
		pop		ax
		pop		ds
		retf
	}
}
#endif

/* check if SSE is usable. Just because CPUID says it's there
 * doesn't mean the OS enabled it.
 *
 * if do_enable is nonzero and the function detects that it's
 * possible, the function will enable SSE and return success */
int cpu_check_sse_is_usable() {
	if (!cpu_sse_usable_probed) {
		cpu_sse_usable_probed = 1;
		cpu_sse_usable = 0;
		cpu_sse_unusable_reason = "";
		cpu_sse_usable_detection_method = "None";
		cpu_sse_usable_can_turn_off = 0;
		cpu_sse_usable_can_turn_on = 0;

		if (cpu_basic_level < 0) cpu_probe();

		if (!(cpu_flags & CPU_FLAG_CPUID)) {
			cpu_sse_unusable_reason = "No CPUID available";
			return 0;
		}
		if (!(cpu_cpuid_features.a.raw[2] & (1UL << 25UL))) {
			cpu_sse_unusable_reason = "CPUID indicates SSE is not present";
			return 0;
		}

#ifdef TARGET_WINDOWS
/* ==================== RUNNING WITHIN WINDOWS =================== */
/* Within Windows, we have no way to enable SSE if the OS kernel doesn't support it.
 * So we first must learn whether or not the OS supports it.
 *    Guide: Windows NT/2000/XP/Vista/etc... you can use IsProcessorFeaturePresent()
 *           Windows 95/98/ME... there is no way other than attempting the instruction to see if it causes a fault.
 *           Windows 3.1/3.0... NO. And you cannot enable it either! Buuuuut.... if the Windows 3.1 binary is run
 *           under Windows XP/ME/98 and the kernel has SSE enabled, then it is possible to use SSE instructions.
 *
 *    NOTE: Any Windows 9x kernel prior to 98SE does not support SSE, but if something happens to enable it in CR4
 *          prior to Windows taking control, then it will stay enabled while Windows is running. Those versions
 *          treat the SSE enable bit as unknown and they don't change it. BUT also realize that the SSE registers
 *          are not saved and restored by the kernel scheduler! If you are the only process that will be using SSE
 *          that is fine, but if two or more tasks try to use SSE there will be serious conflicts and possibly
 *          a crash.
 *
 * Reading CR4 to determine availability has the same effects as it does for the MS-DOS code, either
 * nonsense values (Windows 9x/ME) or it causes a fault (Windows NT). */
		detect_windows();
# if TARGET_MSDOS == 32
#  ifdef WIN386
		/* Windows 3.0/3.1 Win386: the underlying system is 16-bit and we're 32-bit through an extender */
		cpu_sse_unusable_reason = "SSE support for Windows 3.x Win386 is not implemented";
		return 0;
#  else
		{
			BOOL (WINAPI *__IsProcFeaturePresent)(DWORD f) = NULL;

			if (windows_mode == WINDOWS_NT)
				__IsProcFeaturePresent = (BOOL (WINAPI *)(DWORD))GetProcAddress(GetModuleHandle("KERNEL32.DLL"),"IsProcessorFeaturePresent");

			if (__IsProcFeaturePresent != NULL) {
				cpu_sse_usable_detection_method = "IsProcessorFeaturePresent [WinNT]";
				printf("Using IsProcessorFeaturePresent\n");
				if (!__IsProcFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE))
					cpu_sse_unusable_reason = "Windows NT HAL says SSE not enabled";
				else
					cpu_sse_usable = 1;
			}
			else {
				BYTE ok=1;

				cpu_sse_usable_detection_method = "Executing SSE to see if it causes a fault [Win31s/9x/ME/NT]";

				/* we just have to try */
				__try {
					__asm {
						.686p
							.xmm
							xorps	xmm0, xmm0
					}
				}
				__except (EXCEPTION_EXECUTE_HANDLER) {
					ok = 0;
				}

				if (!ok) {
					cpu_sse_unusable_reason = "Windows 3.1/9x/ME kernel does not have SSE enabled";
					return 0;
				}
				else {
					cpu_sse_usable = 1;
				}
			}
		}
#  endif
# else /* TARGET_MSDOS == 16 */
		if ((windows_mode == WINDOWS_STANDARD || windows_mode == WINDOWS_ENHANCED) && windows_version < 0x35F) {
			/* Windows 3.0/3.1: We can abuse the DPMI server to hook INT 6h exceptions.
			 * Very clean, and it does not require TOOLHELP.DLL. But it doesn't work under Windows 9x/ME.
			 * The DPMI call silently fails and KERNEL32.DLL catches the fault without passing it on to us. */
			void far *op;

			op = win16_getexhandler(6);
			cpu_sse_usable_detection_method = "Hooking INT 6, executing SSE to see if it causes a fault [Win16]";
			if (win16_setexhandler(6,fault_int6)) {
				faulted = 0;
				__asm {
					.686p
					.xmm
					xorps	xmm0, xmm0
				}
				win16_setexhandler(6,op);
			}
			else {
				win16_setexhandler(6,op);
				cpu_sse_unusable_reason = "Unable to hook INT 6 by DPMI";
				return 0;
			}

			if (faulted) {
				cpu_sse_unusable_reason = "Windows 3.x kernel does not have SSE enabled";
				return 0;
			}
			else {
				cpu_sse_usable = 1;
			}
		}
		/* Windows 9x/ME. Abusing DPMI as in Windows 3.1 doesn't work, the calls silently fail. But Microsoft
		 * apparently made sure the TOOLHELP API functions do their job, so we use that technique. It even works
		 * under NTVDM.EXE in Windows NT/2000/XP */
		else if ((windows_mode == WINDOWS_STANDARD || windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT) && ToolHelpInit()) {
			cpu_sse_usable_detection_method = "InterruptRegister/TOOLHELP.DLL, executing SSE to see if it causes a fault [Win16]";
			if (__InterruptRegister(NULL,(FARPROC)fault_int6_toolhelp)) {
				faulted = 0;
				__asm {
					.686p
					.xmm
					xorps	xmm0, xmm0
				}

				if (!__InterruptUnRegister(NULL))
					MessageBox(NULL,"WARNING: Unable to unregister interrupt","",MB_OK);

				if (faulted) {
					cpu_sse_unusable_reason = "Windows 9x/ME/NT kernel does not have SSE enabled";
					return 0;
				}
				else {
					cpu_sse_usable = 1;
				}
			}
			else {
				MessageBox(NULL,"WARNING: Unable to register interrupt","",MB_OK);
			}
		}
		else {
			cpu_sse_unusable_reason = "This library does not have support for detecting SSE from Win16 under Windows 3.0 Real Mode";
			return 0;
		}
# endif
#elif defined(TARGET_OS2)
/* ==================== RUNNING AS OS/2 APP ====================== */
		/* TODO */
		cpu_sse_unusable_reason = "Assuming not present";
		cpu_sse_usable = 0;
		cpu_sse_usable_detection_method = "None";
		cpu_sse_usable_can_turn_on = 0;
		cpu_sse_usable_can_turn_off = 0;
#else
/* ==================== RUNNING AS MS-DOS APP ==================== */
		detect_windows();

		if ((windows_mode == WINDOWS_STANDARD || windows_mode == WINDOWS_ENHANCED) && TARGET_MSDOS == 16) {
#if TARGET_MSDOS == 16
			/* Windows 9x/ME DOS box.
			 * we can't read the control registers, and hooking INT 6 doesn't work. If the SSE test causes
			 * an Invalid Opcode exception Windows 9x will NOT forward the exception down to us!
			 * Same with Windows 3.0/3.1 Standard/Enhanced mode!
			 *
			 * But, Windows 9x does offer DPMI. So to carry out the test, we use DPMI to enter protected mode,
			 * set up DPMI exception handlers, perform the test, and then use DPMI to exit back to real mode
			 * with the results.
			 *
			 * Note that this code is not necessary for Windows NT/2000/XP/Vista/etc. as NTVDM.EXE contains
			 * code to forward the Invalid Opcode exception to us if it sees that we hooked it */
			cpu_sse_usable_detection_method = "Hook INT 6h, execute SSE to see if it causes a fault [DPMI under Win 3.1/9x/ME DOS box]";
			probe_dpmi();
			if (dpmi_present) {
				/* to carry out this test, we must enter protected mode through DPMI. we must initialize
				 * DPMI if not already done, or if the caller has already gone through DPMI, we must
				 * enter using whatever bit width he chose.
				 *
				 * NOTE: This means then, that if the calling program needs DPMI the program must have
				 *       set it's own preferences up prior to calling this function, because otherwise
				 *       you're going to be stuck with our preferences. Now maybe if DPMI had thought
				 *       about something like... I dunno... a way to un-initialize DPMI for itself,
				 *       maybe we wouldn't have this problem, would we? */
				if (dpmi_entered == 0)
					dpmi_enter(DPMI_ENTER_AUTO);

				if (dpmi_entered != 0) {
					unsigned int reason = cpu_dpmi_win9x_sse_test();
					if (reason == 0) {
						/* it worked */
						cpu_sse_usable = 1;
					}
					else if (reason == 1) {
						/* test OK, sse not available */
						cpu_sse_unusable_reason = "SSE is currently disabled, nobody has enabled it yet";
					}
				}
				else {
					cpu_sse_unusable_reason = "Unable to enter DPMI protected mode";
				}
			}
			else {
				cpu_sse_unusable_reason = "As an MS-DOS application I have no way to test for SSE from within Win 3.1/9x/ME DOS box, DPMI is not available";
			}
#endif
		}
		else if (cpu_v86_active || windows_mode == WINDOWS_NT ||
			((windows_mode == WINDOWS_STANDARD || windows_mode == WINDOWS_ENHANCED) && TARGET_MSDOS == 32)) {
			/* pure DOS mode with virtual 8086 mode, or Windows NT DOS box.
			 * we can't read the control registers (or under EMM386.EXE we can, but within v86 mode it's
			 * not wise to assume that we can). Note that DOS32a is able to catch Invalid Opcode exceptions,
			 * even from within Windows 9x/ME/NT, so when compiled as a 32-bit DOS app we also need to use DPMI functions
			 * "set exception handler" to ensure that we catch the fault. */
			void far *oh;
#if TARGET_MSDOS == 32
			void far *oh_ex;
#endif

			cpu_sse_usable_detection_method = "Hook INT 6h, execute SSE to see if it causes a fault [MS-DOS]";

#if TARGET_MSDOS == 32
			oh_ex = dpmi_getexhandler(6);
			dpmi_setexhandler(6,(void far*)fault_int6);
#endif

			oh = _dos_getvect(6);
			_dos_setvect(6,(void far*)fault_int6_vec);

			__asm {
				.686p
				.xmm
				xorps	xmm0, xmm0
			}

			_dos_setvect(6,oh);
#if TARGET_MSDOS == 32
			dpmi_setexhandler(6,oh_ex);
#endif

			/* TODO: If we're in pure DOS mode, and virtual 8086 mode is active, and we know VCPI is present,
			 *       then it is possible for us to enable/disable SSE by switching into protected mode via VCPI */

			if (faulted) {
				cpu_sse_unusable_reason = "SSE is currently disabled, nobody has enabled it yet";
			}
			else {
				cpu_sse_usable = 1;
			}
		}
		else {
			/* pure DOS mode. we can read the control registers without crashing */
			uint32_t v=0;

			cpu_sse_usable_detection_method = "80386 MOV CR4 [MS-DOS]";

			__asm {
				.386p
				mov	eax,cr4
				mov	v,eax
			}

			if (v & 0x200) {
				cpu_sse_usable = 1;
			}
			else {
				cpu_sse_unusable_reason = "SSE is currently disabled";
			}

			cpu_sse_usable_can_turn_on = 1;
			cpu_sse_usable_can_turn_off = 1;
		}
#endif /* TARGET_WINDOWS */
	}

	return cpu_sse_usable;
}

int cpu_sse_disable() {
#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
	uint32_t confidence=0;
#endif

	if (!(cpu_flags & CPU_FLAG_CPUID))
		return 0;
	if (!(cpu_cpuid_features.a.raw[2] & (1UL << 25UL)))
		return 0;
	if (!cpu_sse_usable_can_turn_off)
		return 0;

#if defined(TARGET_WINDOWS) || defined(TARGET_OS2)
	return 0; /* it's very unlikely we could ever touch the control registers from within Windows */
	/* FIXME: Maybe as a Win32 app under Windows 9x we could try? */
#else
	__asm {
		.386p
		mov	eax,cr4
		and	eax,0xFFFFFDFF	/* bit 9 */
		mov	cr4,eax
		mov	ebx,eax		/* remember what we wrote */

		mov	eax,cr4
		and	eax,0x200
		and	ebx,0x200
		xor	eax,ebx
		mov	confidence,eax	/* EAX = nonzero if write didn't work */
	}

	if (confidence) {
		cpu_sse_unusable_reason = "Attempting to write CR4 failed";
		return 0;
	}

	cpu_sse_usable = 0;
	return 1;
#endif
}

int cpu_sse_enable() {
#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
	uint32_t confidence=0;
#endif

	if (!(cpu_flags & CPU_FLAG_CPUID))
		return 0;
	if (!(cpu_cpuid_features.a.raw[2] & (1UL << 25UL)))
		return 0;
	if (!cpu_sse_usable_can_turn_on)
		return 0;

#if defined(TARGET_WINDOWS) || defined(TARGET_OS2)
	return 0; /* it's very unlikely we could ever touch the control registers from within Windows */
	/* FIXME: Maybe as a Win32 app under Windows 9x we could try? */
#else
	__asm {
		.386p
		mov	eax,cr4
		or	eax,0x200	/* bit 9 */
		mov	cr4,eax
		mov	ebx,eax		/* remember what we wrote */

		mov	eax,cr4
		and	eax,0x200
		and	ebx,0x200
		xor	eax,ebx
		mov	confidence,eax	/* EAX = nonzero if write didn't work */
	}

	if (confidence) {
		cpu_sse_unusable_reason = "Attempting to write CR4 failed";
		return 0;
	}

	cpu_sse_usable = 1;
	return 1;
#endif
}

