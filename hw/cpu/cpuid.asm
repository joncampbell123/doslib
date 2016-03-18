; cpuid.asm
;
; Runtime CPUID for 16-bit code
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

; ---------- CODE segment -----------------
%include "_segcode.inc"

; NASM won't do it for us... make sure "retnative" is defined
%ifndef retnative
 %error retnative not defined
%endif

%if TARGET_MSDOS == 16
; incoming Watcom convention:
; DX:AX = CPUID register
; BX = (near) pointer to struct OR
; CX:BX = (far) pointer to struct
global cpu_cpuid_
cpu_cpuid_:
 %ifdef MMODE_DATA_VAR_DEF_FAR
	push	ds
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

 %ifdef MMODE_DATA_VAR_DEF_FAR
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
 %ifdef MMODE_DATA_VAR_DEF_FAR
	pop	ds
 %endif
	retnative		; FIXME: NASM assembles this regardless of whether or not _watcall is included??
%endif

