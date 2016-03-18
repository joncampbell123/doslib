; cpuiopd.asm
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

; ---------- CODE segment -----------------
%include "_segcode.inc"

; NASM won't do it for us... make sure "retnative" is defined
%ifndef retnative
 %error retnative not defined
%endif

%if TARGET_MSDOS == 16
; uint32_t __cdecl inpd(uint16_t port);
global _inpd
_inpd:	push		bp
	mov		bp,sp
	mov		dx,[bp+cdecl_param_offset]
	in		eax,dx
	mov		dx,ax
	shr		eax,16
	xchg		dx,ax
	pop		bp
	retnative

; void __cdecl outpd(uint16_t port,uint32_t data);
global _outpd
_outpd:	push		bp
	mov		bp,sp
	mov		dx,[bp+cdecl_param_offset]
	mov		eax,[bp+cdecl_param_offset+2]
	out		dx,eax
	pop		bp
	retnative
%endif

