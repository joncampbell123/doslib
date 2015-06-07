
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

