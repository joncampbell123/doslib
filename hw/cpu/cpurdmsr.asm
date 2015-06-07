
global cpu_basic_probe_
extern _cpu_flags
extern _cpu_tmp1
extern _cpu_cpuid_max
; char cpu_v86_active
; extern _cpu_v86_active
; char cpu_cpuid_vendor[13];
extern _cpu_cpuid_vendor
; struct cpu_cpuid_feature cpu_cpuid_features;
extern _cpu_cpuid_features
; NTS: Do NOT define variables here, Watcom or NASM is putting them in the wrong places (like at 0x000!)

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

