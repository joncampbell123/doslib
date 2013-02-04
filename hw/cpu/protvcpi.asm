; protvcpi.asm
;
; Test program: Protected mode via VCPI
; (C) 2010-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>
;
; proot of concept:
; switching the CPU into 386 16-bit protected mode (and back) using VCPI
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
VCPI0_SEL	equ		48
VCPI1_SEL	equ		56
VCPI2_SEL	equ		64
TSS_SEL		equ		72
TSS2_SEL	equ		80
MAX_SEL		equ		88

; ===== ENTRY POINT
		call		cpu_is_386
		je		is_386
		mov		dx,str_cpu_not_386
		jmp		exit2dos_with_message
is_386:

; ===== CHECK FOR VIRTUAL 8086 MODE. IN THIS CASE WE WANT IT, IT MEANS VCPI IS PRESENT
		smsw		ax			; 386 or higher: If we're in real mode
		test		al,1			; and bit 0 is already set, we're in virtual 8086
		jnz		isnt_realmode		; and our switch to prot mode will cause problems.
		mov		dx,str_cpu_need_v86_mode
		jmp		exit2dos_with_message
isnt_realmode:

; choose memory location for PAGE0, PAGEDIR. they must be 4K aligned
		xor		ecx,ecx
		mov		cx,cs
		shl		ecx,4

		lea		edi,[ecx+ENDOI]
		add		edi,0xFFF
		and		edi,0xF000
		sub		edi,ecx
		mov		dword [PAGE0],edi
		add		edi,0x1000
		mov		dword [PAGEDIR],edi

; ===== CHECK FOR VALID INT 67h
		xor		ax,ax
		mov		es,ax
		mov		ax,[es:(0x67*4)]
		or		ax,[es:(0x67*4)+2]
		or		ax,ax
		jnz		int67_valid
		mov		dx,str_cpu_need_int67
		jmp		exit2dos_with_message

; ===== CHECK FOR VCPI
int67_valid:	mov		ax,0xDE00
		int		67h
		cmp		ah,0
		jz		vcpi_valid
		mov		dx,str_cpu_need_vcpi
		jmp		exit2dos_with_message

; fill in the page table, get VCPI selectors, and entry point
vcpi_valid:	mov		ax,0xDE01
		push		ds
		pop		es
		mov		edi,[PAGE0]
		mov		esi,GDT + VCPI0_SEL
		int		67h
		cmp		ah,0
		jz		vcpi_valid2
		mov		dx,str_cpu_need_vcpi_info
		jmp		exit2dos_with_message
vcpi_valid2:	mov		dword [vcpi_entry],ebx

; ===== zero both TSS
		cld
		push		ds
		pop		es
		xor		ax,ax

		mov		di,TSS1
		mov		cx,104/2
		rep		stosw

		mov		di,TSS2
		mov		cx,104/2
		rep		stosw

; ===== BUILD THE GLOBAL DESCRIPTOR TABLE AND GDTR REGISTER
		mov		ax,cs
		mov		word [MY_SEGMENT],ax
		mov		word [MY_SEGMENT+2],0
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
		mov		ah,[MY_PHYS_BASE+3]	; LIMIT[19:16] flags=0 BASE[31:24]
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
		mov		ah,[MY_PHYS_BASE+3]	; LIMIT[19:16] flags=0 BASE[31:24]
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
		mov		ah,[MY_PHYS_BASE+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
;     Code selector (32-bit)
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
;     Data selector (32-bit)
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
;     VCPI0
		add		di,8
;     VCPI1
		add		di,8
;     VCPI2
		add		di,8
;     TSS selector (TSS_SEL)
		mov		ebx,[MY_PHYS_BASE]
		add		ebx,TSS1
		mov		ax,104-1
		stosw					; LIMIT
		mov		ax,bx
		shr		ebx,16
		stosw					; BASE[15:0]
		mov		al,bl
		mov		ah,0x89
		stosw					; BASE[23:16] access byte=data writeable non-busy TSS type 9
		mov		al,0x0F
		mov		ah,bh			; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
;     TSS selector (TSS_VM86_SEL)
		mov		ebx,[MY_PHYS_BASE]
		add		ebx,TSS2
		mov		ax,104-1
		stosw					; LIMIT
		mov		ax,bx
		shr		ebx,16
		stosw					; BASE[15:0]
		mov		al,bl
		mov		ah,0x89
		stosw					; BASE[23:16] access byte=data writeable non-busy TSS type 9
		mov		al,0x0F
		mov		ah,bh			; LIMIT[19:16] flags=0 BASE[31:24]
		stosw

; prepare page directory
		cli
		cld
		push		ds
		pop		es
		mov		edi,[PAGEDIR]
		mov		edx,[MY_PHYS_BASE]

		mov		ebx,[PAGE0]
		lea		eax,[edx+ebx+7]
		stosd

		xor		eax,eax
		mov		ecx,1023
		rep		stosd

; prepare to switch
		cli
		cld
		push		ds
		pop		es
		mov		di,VCPI_SETUP
		mov		edx,[MY_PHYS_BASE]

		mov		ebx,[PAGEDIR]
		lea		eax,[edx+ebx]
		mov		dword [di+0],eax	; ESI+0 = CR3 register = (SEG<<4)+PAGE0

		lea		eax,[edx+GDTR]
		mov		dword [di+4],eax	; ESI+4 = GDTR

		lea		eax,[edx+IDTR]
		mov		dword [di+8],eax	; ESI+8 = IDTR

		mov		word [di+0xC],0		; ESI+C = LDTR
		mov		word [di+0xE],TSS2_SEL	; ESI+E = TR
		mov		dword [di+0x10],prot16_entry ; ESI+12 = prot16_entry
		mov		dword [di+0x14],CODE_SEL	; ESI+10 = CS

; =============== JUMP INTO PROTECTED MODE USING VCPI
		mov		esi,VCPI_SETUP
		add		esi,[MY_PHYS_BASE]	; ESI = *LINEAR* address (not DS:ESI!)
		mov		ax,0xDE0C
		int		67h
prot16_entry:

		mov		ax,DATA_SEL
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

; now, jump into 32-bit protected mode
		jmp		CODE32_SEL:prot32_entry
bits 32
prot32_entry:	mov		ax,DATA32_SEL
		mov		ds,ax
		mov		es,ax
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
		mov		ss,ax

; =============== JUMP OUT OF PROTECTED MODE USING VCPI
		mov		ebx,esp
		xor		eax,eax
		mov		ax,[MY_SEGMENT]
		push		eax			; SS:ESP+0x28 GS
		push		eax			; SS:ESP+0x24 FS
		push		eax			; SS:ESP+0x20 DS
		push		eax			; SS:ESP+0x1C ES
		push		eax			; SS:ESP+0x18 SS
		push		ebx			; SS:ESP+0x14 ESP
		pushfd					; SS:ESP+0x10 EFLAGS
		push		eax			; SS:ESP+0x0C CS
		push		dword realmode_entry	; SS:ESP+0x08 EIP
		push		dword VCPI0_SEL		; SS:ESP+0x04
		push		dword [vcpi_entry]	; SS:ESP+0x00
		mov		eax,0xDE0C		; 0xDE0C = switch to v86 mode
		jmp far		dword [esp]		; simulate a CALL FAR (SS: override implied)

; ====== PROVE WE MADE IT TO REAL MODE
realmode_entry:	mov		si,vdraw2_msg
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
str_cpu_need_v86_mode: db	"Virtual 8086 mode required (VCPI)$"
str_cpu_need_int67:db		"INT 67h service required$"
str_cpu_need_vcpi:db		"VCPI server required$"
str_cpu_need_vcpi_info:db	"VCPI server failed to return info$"
vdraw2_msg:	db		"This message was drawn on screen back from real mode!",0
vdraw_msg:	db		"This message was drawn on screen from 386 16-bit protected mode!",0
vdraw32_msg:	db		"This message was drawn on screen from 386 32-bit protected mode!",0

; vars
		section		.bss align=8
vcpi_entry:	resd		1
GDTR:		resq		1
IDTR:		resq		1
MY_PHYS_BASE:	resd		1
MY_SEGMENT:	resd		1
GDT:		resb		MAX_SEL
IDT:		resb		8*256
VCPI_SETUP:	resb		0x80
TSS1:		resb		108
TSS2:		resb		108
PAGEDIR:	resd		1
PAGE0:		resd		1
ENDOI:		resd		1

