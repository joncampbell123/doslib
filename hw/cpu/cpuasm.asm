; cpuasm.asm
;
; Runtime CPU detection library, assembly language portion
; (C) 2009-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>
;

; FIXME: This needs to provide CPUID for 16-bit real mode code (32-bit versions can use #pragma aux or inline asm)

; NTS: We use NASM (Netwide Assembler) to achieve our goals here because WASM (Watcom Assembler) sucks.
;      I'll consider using their assembler when they get a proper conditional macro system in place.

%if TARGET_MSDOS == 16
 %ifndef TARGET_WINDOWS
extern _dpmi_pm_cs
extern _dpmi_pm_ds
extern _dpmi_pm_ss
extern _dpmi_pm_es
extern _dpmi_entered
extern _dpmi_pm_entry
extern _dpmi_rm_entry
 %endif
%endif

global cpu_basic_probe_
extern _cpu_flags
extern _cpu_tmp1
extern _cpu_cpuid_max
; char cpu_v86_active
; extern _cpu_v86_active
; char cpu_cpuid_vendor[13];
extern _cpu_cpuid_vendor
; struct cpu_cpuid_feature cpu_cpuid_features;
extern _cpu_cpuid_features
; NTS: Do NOT define variables here, Watcom or NASM is putting them in the wrong places (like at 0x000!)

section .text class=CODE

; NTS: If we code 'push ax' and 'popf' for the 16-bit tests in 32-bit protected mode we will screw up the stack pointer and crash
;      so we avoid duplicate code by defining 'native' pushf/popf functions and 'result' to ax or eax depending on CPU mode
%if TARGET_MSDOS == 32
 %define point_s esi
 %define result eax
 %define pushfn pushfd
 %define popfn popfd
use32
%else
 %define point_s si
 %define result ax
 %define pushfn pushf
 %define popfn popf
use16
%endif

%if TARGET_MSDOS == 16
 %ifndef MMODE
  %error You must specify MMODE variable (memory model) for 16-bit real mode code
 %endif
%endif

%if TARGET_MSDOS == 16
 %ifidni MMODE,l
  %define retnative retf
  %define cdecl_param_offset 6	; RETF addr + PUSH BP
 %else
  %ifidni MMODE,m
   %define retnative retf
   %define cdecl_param_offset 6	; RETF addr + PUSH BP
  %else
   %define retnative ret
   %define cdecl_param_offset 4	; RET addr + PUSH BP
  %endif
 %endif
%else
 %define retnative ret
 %define cdecl_param_offset 8	; RET addr + PUSH EBP
%endif

; 0 = 8086
; 1 = 186
; 2 = 286
; 3 = 386
; 4 = 486

; flags:
; bit 0 = CPUID
; bit 1 = FPU

; caller is expected to zero cpu_flags prior to calling

cpu_basic_probe_:
%if TARGET_MSDOS == 16
	push		bx
	push		cx
	push		dx
	push		si
	push		di
	push		bp
%else
	push		ebx
	push		ecx
	push		edx
	push		esi
	push		edi
	push		ebp
%endif
%if TARGET_MSDOS == 16
 %ifidni MMODE,l
; BUGFIX: Calling near a function that returns far is like taking a long walk off a short pier
	push		cs
 %else
  %ifidni MMODE,m
; BUGFIX: Calling near a function that returns far is like taking a long walk off a short pier
	push		cs
  %endif
 %endif
%endif
	call		cpu_basic_probe_f_
%if TARGET_MSDOS == 16
	pop		bp
	pop		di
	pop		si
	pop		dx
	pop		cx
	pop		bx
%else
	pop		ebp
	pop		edi
	pop		esi
	pop		edx
	pop		ecx
	pop		ebx
%endif
	retnative

cpu_basic_probe_f_:

	; check: is a FPU present?
	fninit
	mov		word [_cpu_tmp1],0x5A5A
	fnstsw		word [_cpu_tmp1]
	cmp		word [_cpu_tmp1],0
	jnz		no_fpu			; so... if the FPU is not present the memory addr will never be updated?
	
	fnstcw		word [_cpu_tmp1]
	mov		ax,word [_cpu_tmp1]
	and		ax,0x103F		; look at TOP and lower status flags (after FNINIT they are set?)
	cmp		ax,0x003F
	jnz		no_fpu

	or		byte [_cpu_flags],2

no_fpu:

; 32-bit mode: If this code is executing then DUH the CPU is at least a 386.
;              Skip the 8086 test.
%if TARGET_MSDOS == 16
	pushfn
	pop		result

	; an 8086 will always set bits 12-15
	and		ax,0x0FFF
	push		result
	popfn
	pushfn
	pop		result
	and		ax,0xF000
	cmp		ax,0xF000
	jnz		test286
	mov		result,0
	retnative
%endif

test286:

; 32-bit mode: If this code is executing then DUH the CPU is at least a 386.
;              Skip the 286 test.
; 16-bit mode: Do the test, make sure to restore the original CFLAGS. Failure to do so is not fatal in
;              16-bit real mode, but under Windows 3.1 can cause a nasty crash when this process terminates.
%if TARGET_MSDOS == 16
	; a 286 will always clear bits 12-15 (in real mode)
	pushfn				; save FLAGS
	or		ax,0xF000
	push		result
	popfn
	pushfn
	pop		result
	popfn				; restore original FLAGS
	and		ax,0xF000
	jnz		test386		; if ((val&0xF000) == 0) then AND shouldv'e left ZF=1
	mov		result,2
	retnative
%endif

test386:

; 32-bit mode: The following test is only useful for 16-bit real mode code.
; in fact, in earlier 32-bit builds this part may have been responsible for hard crashes observed
; with DOS4/GW and EMM386.EXE resident.
; Don't do this test for Windows 3.1, it will always see the PM bit set. Instead the code in cpu.c
; will call GetWinFlags
%if TARGET_MSDOS == 16
 %ifndef TARGET_WINDOWS
	; quick test: now that we know we're on a 386 or higher, are we running in Virtual 8086 mode?
	smsw		ax
	and		al,1
	mov		cl,3
	shl		al,cl			; NTS: do not use shl AL,3 we could be running on an 8086 which doesn't support it
	or		byte [_cpu_flags],al	; cpu_flags |= (1 << 3) if v86 mode active
 %endif
%endif

	pushfd
	pop		eax

	; a 386 will not allow setting the AC bit (bit 18)
	or		eax,0x40000
	push		eax
	popfd
	pushfd
	pop		eax
	test		eax,0x40000
	jnz		test486
	mov		result,3
	retnative

test486:

	; this is a 486 or higher, the last thing we want to check is whether the ID bit works (bit 21)
	and		eax,~0x200000		; clear ID
	push		eax
	popfd
	pushfd
	pop		eax
	test		eax,0x200000
	jnz		no_cpuid		; we masked it off, it should be zero

	or		eax,0x200000		; set ID
	push		eax
	popfd
	pushfd
	pop		eax
	test		eax,0x200000
	jz		no_cpuid		; we set it, it should be nonzero

	or		byte [_cpu_flags],1

	; 486 or Pentium with CPUID, so get the vendor string
	xor		eax,eax
	cpuid
	
	mov		point_s,_cpu_cpuid_vendor
	mov		dword [point_s+0],ebx
	mov		dword [point_s+4],edx
	mov		dword [point_s+8],ecx
	mov		byte [point_s+12],0
	mov		[_cpu_cpuid_max],eax

	mov		eax,1			; ++EAX == 1
	cpuid

	mov		point_s,_cpu_cpuid_features
	mov		dword [point_s+0],eax
	mov		dword [point_s+4],ebx
	mov		dword [point_s+8],edx
	mov		dword [point_s+12],ecx

	; we care at this point ("basic check" remember?) only if it's a Pentium, Pentium II or higher (686), or otherwise
	; NTS: EAX already loaded with EAX from CPUID(1)
	shr		eax,8
	and		eax,0xF
	cmp		al,4
	jz		cpuid_as_is
	cmp		al,5
	jz		cpuid_as_is
	cmp		al,6
	jz		cpuid_as_is
	mov		al,6

cpuid_as_is:
	retnative

no_cpuid:
	mov		result,4
	retnative

