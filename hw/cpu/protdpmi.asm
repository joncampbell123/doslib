; protdpmi.asm
;
; Test program: Protected mode via DPMI (DOS Protected Mode Interface)
; (C) 2010-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>
;
; proot of concept:
; switching the CPU into 386 16-bit protected mode (and back) using DPMI
bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

; assume ES == DS and SS == DS and DS == CS

; ===== ENTRY POINT
		mov		ax,cs
		mov		ds,ax
		mov		es,ax
		mov		ss,ax

; we need to use DOS memory allocation, so we first need to resize our
; memory ownership down to the actual size of the COM file.
; Windows NT/XP/etc. seem to assume the COM takes 64KB, while Windows 95
; follows the DOS way and has the COM assume all memory. So for this
; code to work under Win9x much less DOS we need to do this step.
		mov		ah,0x4A		; INT 21h AH=4Ah resize memory block
		mov		bx,cs
		mov		es,bx		; ES=our PSP segment
		mov		bx,ENDOI
		shr		bx,4		; in paragraphs, the size of this program + data
		int		21h

		mov		[MY_SEGMENT],ax
		mov		bx,ax
		shr		bx,12
		shl		ax,4
		mov		[MY_PHYS_BASE+2],bx
		mov		[MY_PHYS_BASE+0],ax

		mov		ax,0x1687
		int		2Fh
		or		ax,ax
		jz		dpmi_present
		mov		dx,str_cpu_need_dpmi
		jmp		exit2dos_with_message
dpmi_present:
		mov		[dpmi_entry+0],di
		mov		[dpmi_entry+2],es
		mov		[dpmi_data_size],si

; allocate private data for DPMI server, if needed
		mov		word [dpmi_data_seg],0
		cmp		word [dpmi_data_size],0
		jz		dpmi_no_private
		mov		ah,0x48
		mov		bx,[dpmi_data_size]	; in paragraphs
		int		21h
		jnc		.allocd
		mov		dx,str_cpu_dpmi_private_alloc_fail
		jmp		exit2dos_with_message
.allocd:	mov		[dpmi_data_seg],ax
dpmi_no_private:
; at this point: either we allocated memory successfully, or the DPMI server does not need private data segment
; make the jump
		mov		es,[dpmi_data_seg]
		mov		ax,1		; 16-bit app
		call far	word [dpmi_entry]
		jnc		dpmi_ok
		mov		dx,str_dpmi_entry_fail
		jmp		exit2dos_with_message
dpmi_ok:
; save our protected mode side
		mov		ax,cs
		mov		[MY_CODE_SEL],ax
		mov		ax,ds
		mov		[MY_DATA_SEL],ax

; we need a selector to draw on the screen with
		mov		ax,0x0002
		mov		bx,0xB800
		int		31h
		mov		[vga_sel],ax

; draw on the screen
		mov		si,vdraw_msg
		xor		di,di
		call		vga_puts

; now switch back to real mode
		mov		ax,0x0306
		int		31h
		mov		[raw_entry],cx
		mov		[raw_entry+2],bx
		mov		[raw_exit],edi
		mov		[raw_exit+4],si

; switch
		mov		ax,[MY_SEGMENT]	; AX will become DS
		mov		cx,ax		; CX will become ES
		mov		dx,ax		; DX will become SS
		movzx		ebx,sp		; EBX will become (E)SP
		mov		si,ax		; SI will become CS
		mov		edi,.realmode
		jmp far		dword [raw_exit]
.realmode:

; we made it!
		mov		si,vdraw2_msg
		mov		di,80*2
		call		vga_puts_real

; jump back to protected mode
		mov		ax,[MY_DATA_SEL]	; AX will become DS
		mov		cx,ax			; CX will become ES
		mov		dx,ax			; DX will become SS
		movzx		ebx,sp			; EBX will become (E)SP
		mov		si,[MY_CODE_SEL]	; SI will become CS
		mov		edi,.protagain
		jmp far		word [raw_entry]
.protagain:

; we made it!
		mov		si,vdraw3_msg
		mov		di,80*2*2
		call		vga_puts

; ===== DONE
		jmp		exit2dos

; ===== EXIT TO DOS WITH ERROR MESSAGE DS:DX
exit2dos_with_message:
		mov		ah,9
		int		21h
; ===== EXIT TO DOS
exit2dos:	mov		ax,4C00h
		int		21h

vga_puts_real:
; DS:SI = what to put
;    DI = where to put it
		push		es
		push		ax
		push		si
		push		di
		cld
		mov		ax,0xB800
		mov		es,ax
.l1:		lodsb
		or		al,al
		jz		.le
		mov		ah,0x1E
		stosw
		jmp		.l1
.le:		pop		di
		pop		si
		pop		ax
		pop		es
		ret

vga_puts:
; DS:SI = what to put
;    DI = where to put it
		push		es
		push		ax
		push		si
		push		di
		cld
		mov		ax,[vga_sel]
		mov		es,ax
.l1:		lodsb
		or		al,al
		jz		.le
		mov		ah,0x1E
		stosw
		jmp		.l1
.le:		pop		di
		pop		si
		pop		ax
		pop		es
		ret

; strings
str_cpu_need_dpmi:db		"DPMI server required$"
str_cpu_dpmi_private_alloc_fail:db "Unable to allocate private data for DPMI$"
str_dpmi_entry_fail:db		"Unable to enter DPMI protected mode$"
vdraw_msg:	db		"This message was drawn on screen from DPMI 16-bit protected mode!",0
vdraw3_msg:	db		"This message was drawn on screen back into DPMI 16-bit protected mode!",0
vdraw2_msg:	db		"This message was drawn on screen back from real mode!",0

; vars
		section		.bss align=8
dpmi_entry:	resd		1
raw_entry:	resd		1
raw_exit:	resd		1
		resw		1
dpmi_data_size:	resw		1
dpmi_data_seg:	resw		1
MY_PHYS_BASE:	resd		1
MY_CODE_SEL:	resw		1
MY_DATA_SEL:	resw		1
MY_SEGMENT:	resd		1
vga_sel:	resw		1
ENDOI:		resb		1

