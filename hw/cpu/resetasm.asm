; resetasm.asm
;
; <fixme>
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
%ifndef TARGET_PC98 ; this doesn't work on PC-98
global reset_8086_entry_
reset_8086_entry_:
	cli
	jmp		0xFFFF:0x0000
%endif
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

; we must explicitly defined _DATA and _TEXT to become part of the program's code and data,
; else this code will not work correctly
group DGROUP _DATA

