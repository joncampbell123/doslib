
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

