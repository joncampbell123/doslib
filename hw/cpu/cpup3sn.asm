; cpup3sn.asm
;
; Pentium III serial number functions
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

; cpu_disable_serial(): Disable the Processor Serial Number on Intel
; Pentium III processors that have it.
;
; Note that you cannot re-enable the processor serial number from
; software and it will not come back until a full reboot. This is by
; design.
;
; Warning: This routine does not check for you whether the CPU supports
;          it. If your CPU does not support this function the resulting
;          fault is your responsibility.
;
; Warning: This code will likely cause a fault if run within virtual
;          8086 mode.
global cpu_disable_serial_
cpu_disable_serial_:
	push		eax
	push		ecx
	push		edx
	mov		ecx,0x119
	rdmsr
	or		eax,0x200000
	wrmsr
	pop		edx
	pop		ecx
	pop		eax
	retnative

; cpu_ask_serial(): Read the Processor Serial Number on Intel Pentium III
; processors.
;
; Warning: This does not check whether your CPU supports the feature. Checking
; for PSN support is your responsibility, else the CPU will issue a fault and
; crash your program. Note that if the serial number was disabled by software
; this code will cause a fault as if the feature were never present.
global cpu_ask_serial_
cpu_ask_serial_:
%ifdef MMODE_DATA_VAR_DEF_FAR
	push		ds
%endif
	push		eax
	push		ebx
	push		ecx
	push		edx

%ifdef MMODE_DATA_VAR_DEF_FAR
	mov		ax,seg _cpu_serial
	mov		ds,ax
%endif

	xor		ebx,ebx
	mov		ecx,ebx
	mov		edx,ebx
	mov		eax,3
	cpuid
	mov		dword [_cpu_serial+0],edx
	mov		dword [_cpu_serial+4],ecx
	mov		dword [_cpu_serial+8],ebx
	mov		dword [_cpu_serial+12],eax

	pop		edx
	pop		ecx
	pop		ebx
	pop		eax
%ifdef MMODE_DATA_VAR_DEF_FAR
	pop		ds
%endif
	retnative

; we must explicitly defined _DATA and _TEXT to become part of the program's code and data,
; else this code will not work correctly
group DGROUP _DATA

