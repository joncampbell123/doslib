; prot286.asm
;
; Test program: 80286 protected mode
; (C) 2010-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>
;
; proot of concept:
; switching the CPU into 286 protected mode (and back)
bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

; assume ES == DS and SS == DS and DS == CS

; SELECTORS
NULL_SEL	equ		0
CODE_SEL	equ		8
DATA_SEL	equ		16
VIDEO_SEL	equ		24
MAX_SEL		equ		32

; ===== ENTRY POINT
		call		cpu_is_286
		je		is_286
		mov		dx,str_cpu_not_286
		jmp		exit2dos_with_message
is_286:

; ===== CHECK FOR VIRTUAL 8086 MODE
		smsw		ax			; 386 or higher: If we're in real mode
		test		al,1			; and bit 0 is already set, we're in virtual 8086
		jz		is_realmode		; and our switch to prot mode will cause problems.
		mov		dx,str_cpu_v86_mode
		jmp		exit2dos_with_message
is_realmode:

; ===== WE NEED TO PATCH SOME OF OUR OWN CODE
		mov		ax,cs
		mov		word [real_entry_patch+3],ax	; overwrite segment field of JMP SEG:OFF

; ===== BUILD THE GLOBAL DESCRIPTOR TABLE AND GDTR REGISTER
		mov		ax,cs
		mov		bx,ax
		shr		bx,12
		shl		ax,4			; BX:AX = 32-bit physical addr of our segment
		mov		word [MY_PHYS_BASE],ax
		mov		word [MY_PHYS_BASE+2],bx

		add		ax,GDT
		adc		bx,0			; BX:AX += offset of GDT

		mov		word [GDTR],MAX_SEL - 1
		mov		word [GDTR+2],ax
		mov		word [GDTR+4],bx	; GDTR: limit MAX_SEL-1 base=physical mem addr of GDT

		mov		ax,word [MY_PHYS_BASE]
		mov		bx,word [MY_PHYS_BASE+2]
		add		ax,IDT
		adc		bx,0

		mov		word [IDTR],2047
		mov		word [IDTR+2],ax
		mov		word [IDTR+4],bx

		cld

;     zero IDT
		mov		di,IDT
		mov		cx,1023
		xor		ax,ax
		rep		stosw

		mov		di,GDT
;     NULL selector
		xor		ax,ax
		stosw
		stosw
		stosw
		stosw
;     Code selector
		dec		ax			; 0x0000 - 1 = 0xFFFF
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		stosw					; BASE[15:0]
		mov		al,[MY_PHYS_BASE+2]
		mov		ah,0x9A
		stosw					; BASE[23:16] access byte=executable readable
		mov		al,0x0F
		xor		ah,ah			; LIMIT[19:16] flags=0
		stosw
;     Data selector
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		stosw					; BASE[15:0]
		mov		al,[MY_PHYS_BASE+2]
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x0F
		xor		ah,ah			; LIMIT[19:16] flags=0
		stosw
;     Data selector (video)
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		mov		ax,0x8000
		stosw					; BASE[15:0]
		mov		al,0x0B			; BASE=0xB8000
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x0F
		xor		ah,ah			; LIMIT[19:16] flags=0
		stosw

; make sure the BIOS knows what to do and where to jump if we have to do the 286 reset method of
; getting back to real mode
		cli
		mov		al,0x0F
		out		70h,al
		mov		al,0x0A
		out		71h,al
		push		es
		mov		ax,40h
		mov		es,ax
		mov		di,0x67			; ES:DI = 0x40:0x67 BIOS RESET VECTOR
		mov		ax,real_entry
		stosw
		mov		ax,cs
		stosw
		pop		es

		cli					; disable interrupts
		lgdt		[GDTR]			; load into processor GDTR
		lidt		[IDTR]

; switch into protected mode
		mov		ax,1
		lmsw		ax
		jmp		CODE_SEL:prot_entry
prot_entry:	mov		ax,DATA_SEL		; now reload the segment registers
		mov		ds,ax
		mov		es,ax
		mov		ss,ax

; draw directly onto VGA alphanumeric RAM at 0xB8000
		cld
		push		es
		mov		ax,VIDEO_SEL
		mov		es,ax
		mov		si,vdraw_msg
		xor		di,di
vdraw1:		lodsb					; AL = DS:SI++
		or		al,al
		jz		vdraw1e
		mov		ah,0x1E
		stosw					; ES:DI = AX, DI += 2
		jmp		vdraw1
vdraw1e:	pop		es

; switch back to real mode.
; Unfortunately a true 286 system cannot switch back to real mode. The only way to do it is to
; reset the CPU, having set special bytes into CMOS and a reset vector for the BIOS to jump to after
; CPU reset.

; one good way to do it is to purposely make the interrupt vector table unusable
		xor		ax,ax
		mov		di,IDTR
		mov		word [di],ax
		mov		word [di+2],ax
		mov		word [di+4],ax
		lidt		[IDTR]
; then... use 386 instructions to clear protected mode. if we're running on an
; actual 286, these instructions will trigger an invalid opcode exception,
; which because of what we did to the IDTR, is not available. This causes a
; double, and then triple fault which then causes the CPU to reset.
;
; It's a lot easier to code than trying every corner case between port 92h
; and writing the keyboard controller. On 386 or higher systems, this code
; executes perfectly and does not cause a fault and reset at all.
		xor		eax,eax			; clear bit 0
		mov		cr0,eax

real_entry_patch:jmp		0x0000:real_entry	; the segment field is patched by code above
real_entry:	mov		ax,cs
		mov		ds,ax
		mov		ss,ax
		mov		sp,0xFFF0

; ===== REBUILD GDTR FOR PROPER REAL MODE OPERATION
		mov		word [GDTR],0xFFFF
		mov		word [GDTR+2],0
		mov		word [GDTR+4],0		; GDTR: limit 0xFFFF base 0x00000000
		lgdt		[GDTR]			; load into processor GDTR

		mov		word [IDTR],0xFFFF
		mov		word [IDTR+2],0
		mov		word [IDTR+4],0		; IDTR: limit 0xFFFF base 0x00000000
		lidt		[IDTR]

; ====== PROVE WE MADE IT TO REAL MODE
		mov		si,vdraw2_msg
		mov		ax,0xB800
		mov		es,ax
		mov		di,80*2
vdraw2:		lodsb					; AL = DS:SI++
		or		al,al
		jz		vdraw2e
		mov		ah,0x1E
		stosw					; ES:DI = AX, DI += 2
		jmp		vdraw2
vdraw2e:	mov		ax,cs
		mov		es,ax

		sti

; ===== DONE
		jmp		exit2dos

; ===== EXIT TO DOS WITH ERROR MESSAGE DS:DX
exit2dos_with_message:
		mov		ah,9
		int		21h
; ===== EXIT TO DOS
exit2dos:	mov		ax,4C00h
		int		21h

; 8086 test: EFLAGS will always have bits 12-15 set
cpu_is_286:	pushf
		pop		ax
		and		ax,0x0FFF
		push		ax
		popf
		pushf
		pop		ax
		and		ax,0xF000
		cmp		ax,0xF000
		jz		cpu_is_286_not
		xor		ax,ax			; ZF=1
		ret
cpu_is_286_not:	mov		ax,1
		or		ax,ax			; ZF=0
		ret

; strings
str_cpu_not_286: db		"286 or higher required$"
str_cpu_v86_mode: db		"Virtual 8086 mode detected$"
vdraw_msg:	db		"This message was drawn on screen from 286 protected mode!",0
vdraw2_msg:	db		"This message was drawn on screen back from real mode!",0

; THESE VARIABLES DO NOT EXIST IN THE ACTUAL .COM FILE.
; They exist in the yet-uninitialized area of RAM just beyond the
; end of the loaded COM file image.
		align		8
RALLOC:		db		0xAA
GDTR		equ		RALLOC+0
IDTR		equ		GDTR+8
MY_PHYS_BASE	equ		IDTR+8
GDT		equ		MY_PHYS_BASE+8
IDT		equ		GDT+MAX_SEL

