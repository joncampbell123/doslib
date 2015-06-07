
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
%endif

%if TARGET_MSDOS == 16
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

