; cpussea.asm
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
	movzx	result_reg,byte [cs:_cpu_dpmi_result]
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

; we must explicitly defined _DATA and _TEXT to become part of the program's code and data,
; else this code will not work correctly
group DGROUP _DATA

