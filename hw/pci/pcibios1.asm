; pcibios1.asm
;
; <fixme>
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

; extern defs for *.c code
%include "pci.inc"

; ---------- CODE segment -----------------
%include "_segcode.inc"

; NASM won't do it for us... make sure "retnative" is defined
%ifndef retnative
 %error retnative not defined
%endif

; int try_pci_bios2();
try_pci_bios2_:
		pushan
		mov		ax,0xB101
		xor		edi,edi			; BUGFIX: Some BIOSes don't set EDI so the "entry point" address we get is whatever Watcom left on the stack.
							;         This fix resolves the weird erratic values seen when running under Microsoft Virtual PC 2007.
		xor		edx,edx
		xor		ebx,ebx
		xor		ecx,edx
		int		0x1A
		jnc		@1			; CF=0?
		jmp		fail
@1:		cmp		ah,0			; AH=0?
		jz		@2
		jmp		fail
@2:		cmp		edx,0x20494350		; EDX = 'PCI '?
		jz		@3
		jmp		fail
@3:		mov		dword [_pci_bios_protmode_entry_point],edi
		mov		byte [_pci_bios_hw_characteristics],al
		mov		byte [_pci_bios_interface_level],bl
		mov		byte [_pci_bios_interface_level+1],bh
		xor		ch,ch
		mov		word [_pci_bios_last_bus],cx

		popan
		mov		result_reg,1
		retnative
fail:		popan
		xor		result_reg,result_reg
		retnative

; int try_pci_bios1();
try_pci_bios1_:
		pushan
		mov		ax,0xB001
		int		0x1A
		jc		fail2			; CF=0?
		cmp		dx,0x4350		; DX:CX should spell out 'PCI '
		jnz		fail2
		cmp		cx,0x2049
		jnz		fail2
		popan
		mov		result_reg,1
		retnative
fail2:		popan
		xor		result_reg,result_reg
		retnative

%if TARGET_MSDOS == 16
; uint32_t __cdecl pci_bios_read_dword_16(uint16_t bx,uint16_t di)
_pci_bios_read_dword_16:
		push		bp
		mov		bp,sp

		push		cx
		push		bx
		push		di
		mov		bx,[bp+cdecl_param_offset]
		mov		di,[bp+cdecl_param_offset+2]
		mov		ax,0xB10A
		int		0x1A
		jc		@2r		; CF=1 means failure
		cmp		ah,0x00
		jz		@1r		; AH!=0 means failure
@2r:		xor		ecx,ecx
		dec		ecx
@1r:		pop		di
		pop		bx
		mov		ax,cx
		shr		ecx,16
		mov		dx,cx
		pop		cx

		pop		bp
		retnative

; void __cdecl pci_bios_write_dword_16(uint16_t bx,uint16_t di,uint32_t data);
_pci_bios_write_dword_16:
		push		bp
		mov		bp,sp

		push		cx
		push		bx
		push		di
		mov		bx,[bp+cdecl_param_offset]
		mov		di,[bp+cdecl_param_offset+2]
		mov		ecx,[bp+cdecl_param_offset+4]
		mov		ax,0xB10D
		int		0x1A
		pop		di
		pop		bx
		pop		cx

		pop		bp
		retnative
%endif

; we must explicitly defined _DATA and _TEXT to become part of the program's code and data,
; else this code will not work correctly
group DGROUP _DATA

