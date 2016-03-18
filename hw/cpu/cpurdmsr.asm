; cpurdmsr.asm
;
; RDMSR support for 16-bit code
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

; we must explicitly defined _DATA and _TEXT to become part of the program's code and data,
; else this code will not work correctly
group DGROUP _DATA

