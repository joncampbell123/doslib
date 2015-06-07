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

%if TARGET_MSDOS == 16
global reset_8086_entry_
reset_8086_entry_:
	cli
	jmp		0xFFFF:0x0000
%endif

%if TARGET_MSDOS == 16
global bios_reset_cb_e_
bios_reset_cb_e_:
	cli
; DEBUG: Prove our routine is executing properly by drawing on VGA RAM
	mov		ax,0xB800
	mov		ds,ax
	mov		word [0],0x1E30
; now load state from 0x50:0x04 where the setup put it
	mov		ax,0x50
	mov		ds,ax
	mov		ax,[0x08]
	mov		sp,ax
	mov		ax,[0x0A]
	mov		ss,ax
	mov		bx,[0x0C]
	push		word [ds:0x06]
	push		word [ds:0x04]
	mov		ds,bx
	mov		es,bx
	retf
%endif

%if TARGET_MSDOS == 16
; incoming Watcom convention:
; DX:AX = CPUID register
; BX = (near) pointer to struct OR
; CX:BX = (far) pointer to struct
global cpu_cpuid_
cpu_cpuid_:
%ifidni MMODE,l
	push	ds
	push	es
%endif
%ifidni MMODE,c
	push	ds
	push	es
%endif
	pushf
	push	ax
	push	bx
	push	cx
	push	dx
	push	si
	cli

	; EAX = DX:AX
	shl	edx,16
	and	eax,0xFFFF
	or	eax,edx

%ifidni MMODE,l
	mov	dx,ds
	mov	es,dx
	mov	ds,cx
%endif
%ifidni MMODE,c
	mov	dx,ds
	mov	es,dx
	mov	ds,cx
%endif
	mov	si,bx	; CPUID will overwrite BX

; SO:
;    EAX = CPUID function to execute
;    (DS):SI = pointer to structure to fill in with CPUID results
	cpuid
	mov	dword [si],eax
	mov	dword [si+4],ebx
	mov	dword [si+8],ecx
	mov	dword [si+12],edx

	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	popf
%ifidni MMODE,l
	pop	es
	pop	ds
%endif
%ifidni MMODE,c
	pop	es
	pop	ds
%endif
	retnative
%endif

%if TARGET_MSDOS == 16
; WARNING: Do not execute this instruction when you are a Windows application.
;          The Windows VM doesn't take it well.
; uint64_t __cdecl cpu_rdmsr(uint32_t val):
; read MSR and return.
; places 64-bit value in AX:BX:CX:DX according to Watcom register calling convention
global cpu_rdmsr_
cpu_rdmsr_:
	pushf
	cli

	; assemble EAX = DX:AX 32-bit value given to us
	shl	edx,16
	mov	dx,ax
	mov	ecx,edx

	; doit
	rdmsr

	; reshuffle to return as 64-bit value in AX:BX:CX:DX
	xchg eax,edx
	mov ecx,edx
	mov ebx,eax
	shr eax,16
	shr ecx,16

	popf
	retnative
%endif

%if TARGET_MSDOS == 16
 %ifndef TARGET_WINDOWS
; unsigned int _cdecl cpu_dpmi_win9x_sse_test();
; NOTE: The caller should have first checked dpmi_present, and that dpmi_pm_entry, and dpmi_rm_entry
; are valid. We assume they are.
global _cpu_dpmi_win9x_sse_test
_cpu_dpmi_win9x_sse_test:
	push	ds
	push	es
	push	ss

; copy return vector
	mov	ax,seg _dpmi_rm_entry
	mov	ds,ax
	mov	ax,[_dpmi_rm_entry+0]
	mov	word [cs:_cpu_dpmi_return_vec+0],ax
	mov	ax,[_dpmi_rm_entry+2]
	mov	word [cs:_cpu_dpmi_return_vec+2],ax
	mov	ax,[_dpmi_rm_entry+4]
	mov	word [cs:_cpu_dpmi_return_vec+4],ax

; save some key registers
	mov	word [cs:_cpu_dpmi_saved_cs],cs
	mov	word [cs:_cpu_dpmi_saved_ss],ss
	mov	ax,sp
	mov	word [cs:_cpu_dpmi_saved_sp],ax

	mov	ax,seg _dpmi_entered
	mov	ds,ax
	mov	al,[_dpmi_entered]
	mov	byte [cs:_cpu_dpmi_mode],al
	mov	byte [cs:_cpu_dpmi_result],0

; enter protected mode
	mov	ax,seg _dpmi_pm_ds
	mov	ds,ax
	mov	ax,[_dpmi_pm_ds]
	mov	cx,ax

	mov	dx,seg _dpmi_pm_ss
	mov	ds,dx
	mov	dx,[_dpmi_pm_ss]

	mov	si,seg _dpmi_pm_cs
	mov	ds,si
	mov	si,[_dpmi_pm_cs]

	mov	bx,seg _dpmi_pm_entry
	mov	es,bx
	mov	bx,sp
	mov	di,.enterpm
	jmp far word [es:_dpmi_pm_entry]
.enterpm:
; remember: when the DOS code first entered, DS == CS so DS is an alias to CS

; the path we take depends on 16-bit or 32-bit dpmi!
	cmp	byte [cs:_cpu_dpmi_mode],32
	jz	.path32
; 16-bit path
	mov	ax,0x0202		; get current exception vector for #UD
	mov	bl,0x06
	int	31h
	push	cx
	push	dx			; save on stack

	mov	ax,0x0203		; set exception vector for #UD to our test routine
	mov	bl,0x06
	mov	cx,cs
	mov	edx,_cpu_dpmi_win9x_sse_test_except16
	int	31h

	; the exception handler will set DS:_cpu_dpmi_result = 1
	xorps	xmm0,xmm0

	pop	dx			; restore exception vector for #UD
	pop	cx
	mov	ax,0x0203
	mov	bl,0x06
	int	31h

	; now return to real mode
	mov	ax,word [_cpu_dpmi_saved_cs]
	mov	cx,ax
	mov	si,ax
	mov	dx,word [_cpu_dpmi_saved_ss]
	mov	bx,sp
	mov	di,.exitpm
	jmp far word [_cpu_dpmi_return_vec]
; 32-bit path
.path32:
	mov	ax,0x0202		; get current exception vector for #UD
	mov	bl,0x06
	int	31h
	push	ecx
	push	edx			; save on stack

	mov	ax,0x0203		; set exception vector for #UD to our test routine
	mov	bl,0x06
	mov	cx,cs
	mov	edx,_cpu_dpmi_win9x_sse_test_except32
	int	31h

	; the exception handler will set DS:_cpu_dpmi_result = 1
	xorps	xmm0,xmm0

	pop	edx			; restore exception vector for #UD
	pop	ecx
	mov	ax,0x0203
	mov	bl,0x06
	int	31h

	; now return to real mode
	mov	ax,word [_cpu_dpmi_saved_cs]
	mov	cx,ax
	mov	si,ax
	mov	dx,word [_cpu_dpmi_saved_ss]
	mov	bx,sp
	mov	di,.exitpm
	jmp far dword [_cpu_dpmi_return_vec]

.exitpm:
; exit point
	pop	ss
	pop	es
	pop	ds
	movzx	result,byte [cs:_cpu_dpmi_result]
	retnative

; exception handler. skip the offending instruction by directly modifying "IP" on the stack
_cpu_dpmi_win9x_sse_test_except32:
	push	bp
	mov	bp,sp
	mov	byte [_cpu_dpmi_result],1
	add	dword [bp+2+12],3	; 'xorps xmm0,xmm0' is 3 bytes long
	pop	bp
	o32 retf

; exception handler. skip the offending instruction by directly modifying "IP" on the stack
_cpu_dpmi_win9x_sse_test_except16:
	push	bp
	mov	bp,sp
	mov	byte [_cpu_dpmi_result],1
	add	word [bp+2+6],3	; 'xorps xmm0,xmm0' is 3 bytes long
	pop	bp
	retf

_cpu_dpmi_saved_cs	dw	0
_cpu_dpmi_saved_ss	dw	0
_cpu_dpmi_saved_sp	dw	0
_cpu_dpmi_mode		db	0
_cpu_dpmi_result	db	0
_cpu_dpmi_return_vec	dd	0
			dw	0

 %endif
%endif

