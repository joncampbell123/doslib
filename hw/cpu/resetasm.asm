
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
global reset_8086_entry_
reset_8086_entry_:
	cli
	jmp		0xFFFF:0x0000
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

