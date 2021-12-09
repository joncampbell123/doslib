; cpuasm.asm
;
; Runtime CPU detection library, assembly language portion
; (C) 2009-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>
;

; NTS: We use NASM (Netwide Assembler) to achieve our goals here because WASM (Watcom Assembler) sucks.
;      I'll consider using their assembler when they get a proper conditional macro system in place.

; handy memory model defines
%include "_memmodl.inc"

; handy defines for watcall handling
%include "_watcall.inc"

; handy defines for common reg names between 16/32-bit
%include "_comregn.inc"

; extern defs for *.c code
%include "cpu.inc"

; ---------- CODE segment -----------------
%include "_segcode.inc"

; NASM won't do it for us... make sure "retnative" is defined
%ifndef retnative
 %error retnative not defined
%endif

%if TARGET_MSDOS == 16
 %ifdef TARGET_WINDOWS
  extern cpu_basic_probe_via_winflags_
 %endif
%endif

; 0 = 8086
; 1 = 186
; 2 = 286
; 3 = 386
; 4 = 486

; flags:
; bit 0 = CPUID
; bit 1 = FPU

; caller is expected to zero cpu_flags prior to calling.
; NTS: Even in Large memory model we do not have to worry about whether DS is set to allow us
;      access to DGROUP, because Watcom emits assembly setting DS = DGROUP for us when calling this function.
global cpu_basic_probe_
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
%ifdef MMODE_CODE_CALL_DEF_FAR
; BUGFIX: Calling near a function that returns far is like taking a long walk off a short pier
	push		cs 
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

; inner core of the function. outer core takes care of saving regs
cpu_basic_probe_f_:

	; check: is a FPU present?
	; FIXME: given the Open Watcom issue with 8086 processors, does this have the same problem??
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

int 1

%if TARGET_MSDOS == 16
 %ifdef TARGET_WINDOWS
	; 286 detection is not reliable from protected mode when running as a 16-bit Windows application,
	; the method depends on real-mode behavior. Try to use GetWinFlags()
  %ifdef MMODE_CODE_CALL_DEF_FAR
	call far	cpu_basic_probe_via_winflags_
  %else
	call		cpu_basic_probe_via_winflags_
  %endif
	; if return value is negative, continue to normal detect.
	; if return value is less than 4 (something less than a 486) return that immediately.
	; if a 486 or higher, jump straight to 486 CPUID detect.
	or		ax,ax
	js		no_winf
	cmp		ax,4
	jae		test486
	mov		result_reg,ax
	retnative
no_winf:
 %endif
%endif

; 32-bit mode: If this code is executing then DUH the CPU is at least a 386.
;              Skip the 8086 test.
%if TARGET_MSDOS == 16
	pushfn
	pop		result_reg

	; an 8086 will always set bits 12-15
	and		ax,0x0FFF
	push		result_reg
	popfn
	pushfn
	pop		result_reg
	and		ax,0xF000
	cmp		ax,0xF000
	jnz		test286
	mov		result_reg,0
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
	push		result_reg
	popfn
	pushfn
	pop		result_reg
	popfn				; restore original FLAGS
	and		ax,0xF000
	jnz		test386		; if ((val&0xF000) == 0) then AND shouldv'e left ZF=1
	mov		result_reg,2
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
	mov		result_reg,3
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
	mov		result_reg,4
	retnative

; we must explicitly defined _DATA and _TEXT to become part of the program's code and data,
; else this code will not work correctly
group DGROUP _DATA

