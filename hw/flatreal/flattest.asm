; flatmode.asm
;
; Flat real mode support routines
; (C) 2011-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>

; NTS: We use NASM (Netwide Assembler) to achieve our goals here because WASM (Watcom Assembler) sucks.
;      I'll consider using their assembler when they get a proper conditional macro system in place.

; handy memory model defines
%include "_memmodl.inc"

; handy defines for watcall handling
%include "_watcall.inc"

; handy defines for common reg names between 16/32-bit
%include "_comregn.inc"

; ---------- CODE segment -----------------
%include "_segcode.inc"

; NASM won't do it for us... make sure "retnative" is defined
%ifndef retnative
 %error retnative not defined
%endif

%if TARGET_MSDOS == 16
; int flatrealmode_testaddr(uint32_t addr)
global flatrealmode_testaddr_
flatrealmode_testaddr_:
		pushf
		push		ds
		push		esi
		push		edi
		push		cx

		; _watcall says the 32-bit int is in DX:AX.
		; we have to do this NOW before anything modifies DX:AX
		movzx		edi,dx
		shl		edi,16
		mov		di,ax

		; clear interrupts, to ensure IRQ 5 is not mistaken for a GP fault
		cli

		; set DS=0 to carry out the test, and zero AX
		xor		ax,ax
		mov		ds,ax

		; hook interrupt 0x0D (general protection fault and IRQ 5)
		mov		si,0x0D * 4
		mov		cx,[si]			; offset
		mov		word [si],_flatrealmode_testaddr_fail
		push		cx
		mov		cx,[si+2]
		mov		word [si+2],cs
		push		cx

		; now try it. either we'll make it through unscathed or it will cause a GP fault and branch to our handler
		mov		edi,[edi]

		; either nothing happened, or control jmp'd here from the exception handler (who also set AX=1)
		; restore interrupt routine and clean up
_flatrealmode_testaddr_conclude:
		mov		si,0x0D * 4
		pop		cx
		mov		word [si+2],cx
		pop		cx
		mov		word [si],cx

		pop		cx
		pop		edi
		pop		esi
		pop		ds
		popf
		retnative
_flatrealmode_testaddr_fail:
		add		sp,6			; throw away IRETF address (IP+CS+FLAGS)
		inc		ax			; make AX nonzero
		jmp short	_flatrealmode_testaddr_conclude
%endif

