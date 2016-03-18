; cpurdtsc.asm
;
; RDTSC support for 16-bit code
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
; cpu_rdtsc():
; read RDTSC and return.
; places 64-bit value in AX:BX:CX:DX according to Watcom register calling convention
global cpu_rdtsc_
cpu_rdtsc_:
	pushf
	cli
	rdtsc
	xchg eax,edx
	mov ecx,edx
	mov ebx,eax
	shr eax,16
	shr ecx,16
	popf
	retnative

; WARNING: Do not execute this instruction when you are a Windows application.
;          The Windows VM doesn't take it well.
; void __cdecl cpu_rdtsc_write(uint64_t val):
; write RDTSC and return.
global cpu_rdtsc_write_
cpu_rdtsc_write_:
	pushf
	push	ax
	push	bx
	push	cx
	push	dx
	cli

	; assemble EAX = AX:BX
	shl	eax,16
	mov	ax,bx

	; assemble EDX = CX:DX
	shl	ecx,16
	and	edx,0xFFFF
	or	edx,ecx

	; doit
	xchg	eax,edx
	mov	ecx,0x10
	wrmsr

	pop	dx
	pop	cx
	pop	bx
	pop	ax
	popf
	retnative
%endif

