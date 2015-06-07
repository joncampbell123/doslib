
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

; struct cpu_serial_number cpu_serial;
extern _cpu_serial

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
%if TARGET_MSDOS == 16
 %ifidni MMODE,l
	push		ds
 %endif
 %ifidni MMODE,c
	push		ds
 %endif
%endif
	push		eax
	push		ebx
	push		ecx
	push		edx
	
%if TARGET_MSDOS == 16
 %ifidni MMODE,l
	mov		ax,seg _cpu_serial
	mov		ds,ax
 %endif
 %ifidni MMODE,c
	mov		ax,seg _cpu_serial
	mov		ds,ax
 %endif
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
%if TARGET_MSDOS == 16
 %ifidni MMODE,l
	pop		ds
 %endif
 %ifidni MMODE,c
	pop		ds
 %endif
%endif
	retnative

