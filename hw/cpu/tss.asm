; tss.asm
;
; Test program: 80386 task switching (Task State Segments)
; (C) 2010-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>
;
; proot of concept:
; switching the CPU into 386 16-bit protected mode (and back)
; while playing with the Task State Segment mechanism.
; note that the 286 TSS is very similar (with different values
; for the TYPE fields in the GDT, and different register layout)
; so I will not (yet) bother making a 286 version.
bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

; assume ES == DS and SS == DS and DS == CS

; SELECTORS
NULL_SEL	equ		0
CODE_SEL	equ		8
DATA_SEL	equ		16
VIDEO_SEL	equ		24
CODE32_SEL	equ		32
DATA32_SEL	equ		40
TSS_SEL		equ		48
TSS_2_SEL	equ		56
LDT_SEL		equ		64
MAX_SEL		equ		72

; ===== ENTRY POINT
		call		cpu_is_386
		je		is_386
		mov		dx,str_cpu_not_386
		jmp		exit2dos_with_message
is_386:

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
;     NULL selector (NULL_SEL)
		xor		ax,ax
		stosw
		stosw
		stosw
		stosw
;     Code selector (CODE_SEL)
		dec		ax			; 0x0000 - 1 = 0xFFFF
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		stosw					; BASE[15:0]
		mov		al,[MY_PHYS_BASE+2]
		mov		ah,0x9A
		stosw					; BASE[23:16] access byte=executable readable
		mov		al,0x0F
		mov		ah,[MY_PHYS_BASE+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
;     Data selector (DATA_SEL)
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		stosw					; BASE[15:0]
		mov		al,[MY_PHYS_BASE+2]
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x0F
		mov		ah,[MY_PHYS_BASE+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
;     Data selector (VIDEO_SEL)
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		mov		ax,0x8000
		stosw					; BASE[15:0]
		mov		al,0x0B			; BASE=0xB8000
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x0F
		mov		ah,[MY_PHYS_BASE+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
;     Code selector (32-bit) (CODE32_SEL)
		dec		ax			; 0x0000 - 1 = 0xFFFF
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		stosw					; BASE[15:0]
		mov		al,[MY_PHYS_BASE+2]
		mov		ah,0x9A
		stosw					; BASE[23:16] access byte=executable readable
		mov		al,0xCF
		mov		ah,[MY_PHYS_BASE+3]	; LIMIT[19:16] flags=granular 32-bit BASE[31:24]
		stosw
;     Data selector (32-bit) (DATA32_SEL)
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		stosw					; BASE[15:0]
		mov		al,[MY_PHYS_BASE+2]
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0xCF
		mov		ah,[MY_PHYS_BASE+3]	; LIMIT[19:16] flags=granular 32-bit BASE[31:24]
		stosw
;     TSS selector (32-bit) (TSS_SEL)
		mov		ax,TSS_AREA_SIZE - 1
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		mov		bx,[MY_PHYS_BASE+2]
		add		ax,TSS_AREA
		adc		bx,0
		stosw					; BASE[15:0]
		mov		al,bl
		mov		ah,0x89			; present, non-segment, type=9 (TSS busy)
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x00
		mov		ah,bh			; LIMIT[19:16] flags=granular BASE[31:24]
		stosw
;     TSS selector (32-bit) (TSS_2_SEL)
		mov		ax,TSS_AREA_2_SIZE - 1
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		mov		bx,[MY_PHYS_BASE+2]
		add		ax,TSS_AREA_2
		adc		bx,0
		stosw					; BASE[15:0]
		mov		al,bl
		mov		ah,0x89			; present, non-segment, type=9 (TSS non busy)
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x00
		mov		ah,bh			; LIMIT[19:16] flags=granular BASE[31:24]
		stosw
;     LDT selector (32-bit) (LDT_SEL)
		mov		ax,LDT_AREA_SIZE - 1
		stosw					; LIMIT
		mov		ax,[MY_PHYS_BASE]
		mov		bx,[MY_PHYS_BASE+2]
		add		ax,LDT_AREA
		adc		bx,0
		stosw					; BASE[15:0]
		mov		al,bl
		mov		ah,0x82			; present, non-segment, type=2 (LDT)
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x00
		mov		ah,bh			; LIMIT[19:16] flags=granular BASE[31:24]
		stosw

; load CPU registers
		cli					; disable interrupts
		lgdt		[GDTR]			; load into processor GDTR
		lidt		[IDTR]

; switch into protected mode
		mov		eax,0x00000001
		mov		cr0,eax
		jmp		CODE_SEL:prot_entry
prot_entry:	mov		ax,DATA_SEL		; now reload the segment registers
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		ss,ax
		mov		sp,0xFFF0
		
; load LDT
		mov		ax,LDT_SEL
		lldt		ax
		
; zero the first TSS
		cld
		mov		edi,TSS_AREA
		mov		ecx,TSS_AREA_SIZE / 4
		xor		eax,eax
		rep		stosd

; zero the second TSS
		cld
		mov		edi,TSS_AREA_2
		mov		ecx,TSS_AREA_2_SIZE / 4
		xor		eax,eax
		rep		stosd

; set up the task register. for now, leave it at the first one.
		mov		ax,TSS_SEL
		ltr		ax
		
; prepare the second one
		cld
		xor		eax,eax		; prepare EAX=0
		mov		ebx,eax
		mov		ecx,0x12345678	; check value
		mov		edi,TSS_AREA_2
		stosd				; TSS+0x00 = back link
		mov		ax,0xF000
		stosd				; TSS+0x04 = ESP0
		mov		ax,DATA_SEL
		stosd				; TSS+0x08 = SS0
		mov		ax,0xF000
		stosd				; TSS+0x0C = ESP1
		mov		ax,DATA_SEL
		stosd				; TSS+0x10 = SS1
		mov		ax,0xF000
		stosd				; TSS+0x14 = ESP2
		mov		ax,DATA_SEL
		stosd				; TSS+0x18 = SS2
		xor		ax,ax
		stosd				; TSS+0x1C = CR3
		mov		eax,tss_jump_1
		stosd				; TSS+0x20 = EIP
		pushfd
		pop		eax
		stosd				; TSS+0x24 = EFLAGS
		xor		eax,eax
		stosd				; TSS+0x28 = EAX
		stosd				; TSS+0x2C = ECX
		stosd				; TSS+0x30 = EDX
		stosd				; TSS+0x34 = EBX
		mov		ax,0xF000
		stosd				; TSS+0x38 = ESP
		xor		ax,ax
		stosd				; TSS+0x3C = EBP
		stosd				; TSS+0x40 = ESI
		stosd				; TSS+0x44 = EDI
		mov		ax,DATA_SEL
		stosd				; TSS+0x48 = ES
		mov		ax,CODE_SEL
		stosd				; TSS+0x4C = CS
		mov		ax,DATA_SEL
		stosd				; TSS+0x50 = SS
		stosd				; TSS+0x54 = DS
		stosd				; TSS+0x58 = FS
		stosd				; TSS+0x5C = GS
		mov		ax,LDT_SEL
		stosd				; TSS+0x60 = LDT selector (meh, I don't use it anyway)
		xor		ax,ax
		stosd				; TSS+0x64 = I/O map base=0, T=0

; now, SWITCH!
		jmp		TSS_2_SEL:0

; TSS switch should end up HERE.
; Task register now points to TSS_2_SEL as active task.
; TEST: If the CPU truly loaded state from TSS_2_SEL, all general regs should be zero
tss_jump_1:	or		eax,ebx
		or		eax,ecx
		or		eax,edx
		or		eax,esi
		or		eax,edi
		jz		tss_jump_1_zero
		
		mov		ax,VIDEO_SEL
		mov		es,ax
		mov		word [es:0],0x4E30	; '0'
		mov		al,3
		out		61h,al			; turn on bell
		hlt
tss_jump_1_zero:
; TEST: All segment registers except CS should be DATA_SEL
		mov		ax,ds
		sub		ax,DATA_SEL
		
		mov		bx,es
		sub		bx,DATA_SEL
		or		ax,bx
		
		mov		bx,fs
		sub		bx,DATA_SEL
		or		ax,bx
		
		mov		bx,gs
		sub		bx,DATA_SEL
		or		ax,bx
		
		mov		bx,ss
		sub		bx,DATA_SEL
		or		ax,bx
		
		jz		tss_jump_1_sreg_ok
		
		mov		ax,VIDEO_SEL
		mov		es,ax
		mov		word [es:0],0x4E31	; '1'
		mov		al,3
		out		61h,al			; turn on bell
		hlt
tss_jump_1_sreg_ok:

; if the CPU truly saved state into TSS_SEL, the memory location
; corresponding to ECX should be 0x12345678 (because we loaded ECX
; with that value prior to switching state, remember?)
		cmp		dword [TSS_AREA+0x2C],0x12345678
		jz		tss_jump_1_ecx_ok
		
		mov		ax,VIDEO_SEL
		mov		es,ax
		mov		word [es:0],0x4E32	; '2'
		mov		al,3
		out		61h,al			; turn on bell
		hlt
tss_jump_1_ecx_ok:

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

; now, jump into 32-bit protected mode
		jmp		CODE32_SEL:prot32_entry
bits 32
prot32_entry:	mov		ax,DATA32_SEL
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		ss,ax
		mov		esp,0xFFF0

; draw directly onto VGA alphanumeric RAM at 0xB8000
		cld
		mov		esi,vdraw32_msg
		mov		edi,0xB8000+(80*2)
		sub		edi,[MY_PHYS_BASE]
vdraw321:	lodsb					; AL = DS:SI++
		or		al,al
		jz		vdraw321e
		mov		ah,0x1E
		stosw					; ES:DI = AX, DI += 2
		jmp		vdraw321
vdraw321e:

; jump 32-bit to 16-bit
		jmp		CODE_SEL:prot32_to_prot
bits 16
prot32_to_prot:	mov		ax,DATA_SEL
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		ss,ax
		
; active task is TSS_2_SEL. Prove we can switch tasks again by modifying
; EIP in TSS_SEL, then switching tasks.
		mov		dword [TSS_AREA+0x20],tss_jump_2
		jmp		TSS_SEL:0
tss_jump_2:

; having switched back to TSS_SEL, the value we left in ECX should still
; be there.
		cmp		ecx,0x12345678
		jz		tss_jump_2_ecx_ok
		
		mov		ax,VIDEO_SEL
		mov		es,ax
		mov		word [es:0],0x4E33	; '3'
		mov		al,3
		out		61h,al			; turn on bell
		hlt
tss_jump_2_ecx_ok:

; switch back to real mode.
; unlike the 286, switching back means clearing bit 0 of CR0
		xor		eax,eax			; clear bit 0
		mov		cr0,eax

real_entry_patch:jmp		0x0000:real_entry	; the segment field is patched by code above
real_entry:	mov		ax,cs
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
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
		mov		di,80*4
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
cpu_is_386:	pushf
		pop		ax
		and		ax,0x0FFF
		push		ax
		popf
		pushf
		pop		ax
		and		ax,0xF000
		cmp		ax,0xF000
		jz		cpu_is_386_not
; 286 test: EFLAGS will always have bits 12-15 clear
		or		ax,0xF000
		push		ax
		popf
		pushf
		pop		ax
		and		ax,0xF000
		jz		cpu_is_386_not
; it's a 386
		xor		ax,ax			; ZF=1
		ret
cpu_is_386_not:	mov		ax,1
		or		ax,ax			; ZF=0
		ret

; strings
str_cpu_not_386: db		"386 or higher required$"
str_cpu_v86_mode: db		"Virtual 8086 mode detected$"
vdraw2_msg:	db		"This message was drawn on screen back from real mode!",0
vdraw_msg:	db		"This message was drawn on screen from 386 16-bit protected mode!",0
vdraw32_msg:	db		"This message was drawn on screen from 386 32-bit protected mode!",0

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
  IDT_SIZE      equ             2048
TSS_AREA	equ		IDT+IDT_SIZE
  TSS_AREA_SIZE equ             2048
TSS_AREA_2	equ		TSS_AREA+TSS_AREA_SIZE
  TSS_AREA_2_SIZE equ		2048
LDT_AREA	equ		TSS_AREA_2+TSS_AREA_2_SIZE
  LDT_AREA_SIZE equ		64

