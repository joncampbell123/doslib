; v86kern2.asm
;
; Test program: Proof-of-concept minimalist virtual 8086 "monitor"
; (C) 2010-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>
;
;
; MODE: 16-bit real mode MS-DOS .COM executable
;       Assumes DS == ES == SS

; NOTES:
;   - This works... for the most part.
;   - Somehow this works even with DOSBox's funky ROM-based interrupt emulation
;   - The emulation provided is sufficient for real-mode exceptions including INT 3h debug and
;     INT 1h trace. It also handles the correct exception to permit DOS programs to use the
;     FPU if present.
;   - This program also demonstrates a minimal I/O port trapping implementation. In this example,
;     port 0x64 (keyboard controller command port) and port 0x92 (PS/2 & AT Port A) are intercepted
;     to ensure that whatever the program does, the A20 gate is always enabled. This resolves the
;     random crashes observed in Windows 95 DOS mode where HIMEM.SYS and the DOS kernel were
;     apparently playing around with the A20 line.
;
; FIXME:
;   - Privileged instructions like mov cr0,<reg> trigger an exception and this program makes no
;     attempt to emulate those instructions.
;     
;   - Whatever the BIOS does in response to CTRL+ALT+DEL it doesn't work well when we are active.
;
;   - Incomplete VCPI implementation and EMM emulation with no pages to alloc
;
;   - Paging is not enabled
;
; OTHER NOTES:
;   - This code makes no attempt to emulate the LDT manipulation that most BIOS implementations
;     apparently like to do when handling INT 15H extended memory copy. Programs that use extended
;     memory via HIMEM.SYS or via INT 15H will crash. [FIXED: VM86 monitor intercepts INT 15H
;     calls. If the function call is for extended memory copy, the VM86 monitor will carry out
;     the copy itself and return].

; Standard selectors in protected mode
NULL_SEL	equ		(0 << 3)
CODE16_SEL	equ		(1 << 3)
DATA16_SEL	equ		(2 << 3)
CODE32_SEL	equ		(3 << 3)
DATA32_SEL	equ		(4 << 3)
FLAT16_SEL	equ		(5 << 3)
FLAT32_SEL	equ		(6 << 3)
LDT_SEL		equ		(7 << 3)
TSS_SEL		equ		(8 << 3)
TSS_VM86_SEL	equ		(9 << 3)
MAX_SEL		equ		(10 << 3)

; We reprogram the PIC to direct IRQ 0-15 to this base interrupt
RM_INT_API	equ		0xEF
IRQ_BASE_INT	equ		0xF0

; Extensible virtual 8086 mode kernel for DOS
; (C) 2011 Jonathan Campbell

		bits		16
		section		.code
		[map		v86kern2.map]
		org		0x100

; ============= ENTRY POINT
		mov		ax,cs
		mov		word [my_realmode_seg],ax
		mov		bp,stack_base
		mov		sp,stack_init			; SP is normally at 0xFFF0 so move it back down
		mov		word [himem_sys_buffer_handle],0
		mov		word [himem_sys_buffer_phys+2],0
		mov		word [himem_sys_buffer_phys],0
		mov		word [himem_sys_entry+2],0
		mov		word [himem_sys_entry],0
		mov		byte [user_req_unload],0
		mov		byte [user_req_iopl],3
		mov		byte [irq_pending],0
		mov		byte [i_am_tsr],0
		mov		byte [v86_if],0
		jmp		_entry

; ============= CPU DETECT
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

; ============= EXIT WITH MESSAGE ($-terminated string at DS:DX)
_exit_with_msg:	mov		ah,9
		int		21h			; fall through to _exit
; ============= EXIT
_exit:		mov		ax,cs
		mov		ds,ax
		cmp		word [himem_sys_buffer_handle],0 ; if there is a handle to free, then do it
		jz		.no_handle
		mov		ah,0Dh			; HIMEM.SYS function 0Dh unlock memory block
		mov		dx,word [himem_sys_buffer_handle]
		call far	word [himem_sys_entry]
		mov		ah,0Ah			; HIMEM.SYS function 0Ah free memory block
		mov		dx,word [himem_sys_buffer_handle]
		call far	word [himem_sys_entry]
.no_handle:	mov		ax,4C00h
		int		21h

; ============= PROGRAM STARTS HERE
_entry:		call		parse_argv
		jz		.argv_ok
		mov		dx,str_help
		call		_exit_with_msg
.argv_ok:	call		cpu_is_386		; CHECK: 386 or higher
		jz		.is386
		mov		dx,str_require_386
		jmp		_exit_with_msg
.is386:		cmp		byte [user_req_unload],0; CHECK: Did user request that we unload?
		jz		.not_unload
		jmp		unload_this_program
.not_unload:	smsw		ax			; CHECK: Virtual 8086 mode not already enabled
		test		al,1
		jz		.not_v86
		mov		dx,str_v86_detected
		jmp		_exit_with_msg
.not_v86:	cmp		dword [himem_sys_buffer_size],64*1024	; CHECK: buffer size is 64KB or larger
		jge		.buffer_size_large_enough
		mov		dx,str_buffer_too_small
		jmp		_exit_with_msg
.buffer_size_large_enough:
		cmp		dword [himem_sys_buffer_size],16*1024*1024
		jle		.buffer_size_small_enough
		mov		dx,str_buffer_too_large
		jmp		_exit_with_msg
.buffer_size_small_enough:
		mov		ax,4300h		; CHECK: HIMEM.SYS is present
		int		2Fh
		cmp		al,80h
		jz		.yes_himem_sys
		jmp		.skip_himem
.yes_himem_sys:	mov		ax,4310h		; Get HIMEM.SYS entry point (cannot fail)
		int		2Fh
		mov		word [himem_sys_entry],bx
		mov		word [himem_sys_entry+2],es
		mov		ah,5h			; HIMEM.SYS Local Enable A20
		call far	word [himem_sys_entry]
		cmp		ax,1
		jz		.yes_himem_a20
		mov		dx,str_himem_a20_error
		jmp		_exit_with_msg
.yes_himem_a20:	mov		ah,09h			; HIMEM.SYS allocate block
		cli					; <- in case BIOS interrupts do not save upper 16 bits
		mov		edx,[himem_sys_buffer_size]
		add		edx,1023
		shr		edx,10			; EDX = (X BYTES+1023)/1024 KB
		call far	word [himem_sys_entry]
		cmp		ax,1
		jz		.yes_himem_buf
		mov		dx,str_himem_alloc_err
		jmp		_exit_with_msg
.yes_himem_buf:	mov		word [himem_sys_buffer_handle],dx ; store memory handle
		mov		ah,0Ch			; HIMEM.SYS lock memory block
		call far	word [himem_sys_entry]	; NOTE: DX = memory handle (still)
		cmp		ax,1
		jz		.yes_himem_lock
		mov		dx,str_himem_lock_err
		jmp		_exit_with_msg
.yes_himem_lock:mov		word [himem_sys_buffer_phys],bx	; store DX:BX physical memory address
		mov		word [himem_sys_buffer_phys+2],dx
.skip_himem:

; tss physical addrs
		xor		eax,eax
		mov		ax,cs
		shl		eax,4
		add		eax,tss_main
		mov		[tss_phys_base],eax ; = 104 bytes

		xor		eax,eax
		mov		ax,cs
		shl		eax,4
		add		eax,tss_vm86
		mov		[tss_vm86_phys_base],eax ; = 8192+104 bytes

; PRINT "BUFFER AT: " + *((DWORD*)himem_sys_buffer_phys) + "\n"
		mov		dx,str_buffer_at
		call		dos_puts
		cli
		mov		eax,[himem_sys_buffer_phys]
		mov		di,scratch_str
		call		eax_to_hex_16_dos
		mov		dx,di
		call		dos_puts

		cli
		mov		eax,[himem_sys_buffer_phys]
		add		eax,[himem_sys_buffer_size]
		dec		eax
		mov		byte [scratch_str],'-'
		mov		di,scratch_str+1
		call		eax_to_hex_16_dos
		mov		dx,scratch_str
		call		dos_puts

		mov		dx,str_crlf
		call		dos_puts

		xor		eax,eax
		mov		ax,cs
		mov		es,ax
		shl		eax,4
		mov		dword [my_phys_base],eax

; we use the extended memory buffer for VCPI emulation (page allocation)
		mov		eax,dword [himem_sys_buffer_phys]
		mov		dword [vcpi_alloc_bitmap_phys],eax
		mov		dword [vcpi_alloc_bitmap_size],eax
		mov		dword [vcpi_alloc_pages_phys],eax

		cmp		eax,0
		jz		.nothing

		add		eax,4096		; assume bitmap size of 4K, enough for 4MB of pages
		mov		dword [vcpi_alloc_pages_phys],eax

		mov		ebx,dword [himem_sys_buffer_size]
		add		ebx,0xFFF
		shr		ebx,12+3		; EBX = size of bitmap (up to 4K)
		mov		dword [vcpi_alloc_bitmap_size],ebx
.nothing:

; clear the IDT and GDT
		cld
		xor		ax,ax

		mov		cx,MAX_SEL / 2
		mov		di,gdt
		rep		stosw

; prepare the IDTR and GDTR.
; real mode versions: limit=0xFFFF base=0
		xor		eax,eax
		dec		ax			; AX = 0xFFFF
		mov		word [idtr_real],ax
		mov		word [gdtr_real],ax
		inc		ax
		mov		dword [idtr_real+2],eax
		mov		dword [gdtr_real+2],eax
; protected mode GDTR limit=MAX_SEL-1 base=(code segment)+var
		mov		word [gdtr_pmode],MAX_SEL - 1
		mov		word [idtr_pmode],(256 << 3) - 1
		mov		eax,[my_phys_base]
		add		eax,gdt
		mov		dword [gdtr_pmode+2],eax
		mov		eax,[my_phys_base]
		add		eax,idt
		mov		dword [idtr_pmode+2],eax

; build the GDT
		cld
		lea		di,[gdt+CODE16_SEL]
; Code selector (CODE_16SEL)
		dec		ax			; 0x0000 - 1 = 0xFFFF
		stosw					; LIMIT
		mov		ax,[my_phys_base]
		stosw					; BASE[15:0]
		mov		al,[my_phys_base+2]
		mov		ah,0x9A
		stosw					; BASE[23:16] access byte=executable readable
		mov		al,0x0F
		mov		ah,[my_phys_base+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
; Data selector (DATA16_SEL)
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		mov		ax,[my_phys_base]
		stosw					; BASE[15:0]
		mov		al,[my_phys_base+2]
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x0F
		mov		ah,[my_phys_base+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
; Code selector (CODE_32SEL)
		dec		ax			; 0x0000 - 1 = 0xFFFF
		stosw					; LIMIT
		mov		ax,[my_phys_base]
		stosw					; BASE[15:0]
		mov		al,[my_phys_base+2]
		mov		ah,0x9A
		stosw					; BASE[23:16] access byte=executable readable
		mov		al,0xCF
		mov		ah,[my_phys_base+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
; Data selector (DATA32_SEL)
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		mov		ax,[my_phys_base]
		stosw					; BASE[15:0]
		mov		al,[my_phys_base+2]
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0xCF
		mov		ah,[my_phys_base+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
; Data selector (FLAT16_SEL)
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		xor		ax,ax
		stosw					; BASE[15:0]
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0x8F
		xor		ah,ah
		stosw
; Data selector (FLAT32_SEL)
		xor		ax,ax
		dec		ax			; 0xFFFF
		stosw					; LIMIT
		xor		ax,ax
		stosw					; BASE[15:0]
		mov		ah,0x92
		stosw					; BASE[23:16] access byte=data writeable
		mov		al,0xCF
		xor		ah,ah
		stosw
; LDT selector (LDT_SEL)
		mov		ax,7			; I have no use for the LDT
		stosw					; LIMIT
		mov		ax,[my_phys_base]
		stosw					; BASE[15:0]
		mov		al,[my_phys_base+2]
		mov		ah,0x82
		stosw					; BASE[23:16] access byte=data writeable LDT type 2
		mov		al,0x0F
		mov		ah,[my_phys_base+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
; TSS selector (TSS_SEL)
		mov		ax,104-1
		stosw					; LIMIT
		mov		ax,[tss_phys_base]
		stosw					; BASE[15:0]
		mov		al,[tss_phys_base+2]
		mov		ah,0x89
		stosw					; BASE[23:16] access byte=data writeable non-busy TSS type 9
		mov		al,0x0F
		mov		ah,[tss_phys_base+3]	; LIMIT[19:16] flags=0 BASE[31:24]
		stosw
; TSS selector (TSS_VM86_SEL)
		mov		ax,104+8192-1
		stosw					; LIMIT
		mov		ax,[tss_vm86_phys_base]
		stosw					; BASE[15:0]
		mov		al,[tss_vm86_phys_base+2]
		mov		ah,0x89
		stosw					; BASE[23:16] access byte=data writeable non-busy TSS type 9
		mov		al,0x0F
		mov		ah,[tss_vm86_phys_base+3] ; LIMIT[19:16] flags=0 BASE[31:24]
		stosw

; prepare the CPU registers
		lidt		[idtr_pmode]
		lgdt		[gdtr_pmode]

; enter protected mode
		mov		eax,0x00000011
		or		eax,[cr0_more]
		mov		cr0,eax
		jmp		CODE16_SEL:pmode16_entry
pmode16_entry:	mov		ax,DATA16_SEL
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		ss,ax
		mov		sp,stack_init

; load task register
		mov		ax,TSS_SEL
		ltr		ax

; load LDT
		mov		ax,LDT_SEL
		lldt		ax

; now enter 32-bit protected mode
		jmp		CODE32_SEL:pmode32_entry
		bits		32
pmode32_entry:	mov		ax,DATA32_SEL
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		ax,FLAT32_SEL
		mov		fs,ax
		mov		gs,ax
		mov		esp,stack_init
; at this point: we are in 32-bit protected mode!

; ============= setup the TSS representing our task (for when we return)
		cld
		mov		edi,[tss_phys_base]
		sub		edi,[my_phys_base]

		xor		eax,eax					; TSS+0x00 = no backlink
		stosd
		mov		eax,stack_init				; TSS+0x04 = ESP for CPL0
		stosd
		mov		eax,DATA32_SEL				; TSS+0x08 = SS for CPL0
		stosd
		mov		eax,stack_init				; TSS+0x0C = ESP for CPL1
		stosd
		mov		eax,DATA32_SEL				; TSS+0x10 = SS for CPL1
		stosd
		mov		eax,stack_init				; TSS+0x14 = ESP for CPL2
		stosd
		mov		eax,DATA32_SEL				; TSS+0x18 = SS for CPL2
		stosd
		xor		eax,eax					; TSS+0x1C = CR3
		stosd
		mov		eax,0					; TSS+0x20 = EIP [FIXME]
		stosd
		mov		eax,0x00000002				; TSS+0x24 = EFLAGS VM=0
		stosd
		xor		eax,eax					; TSS+0x28 = EAX
		stosd
		xor		eax,eax					; TSS+0x2C = ECX
		stosd
		xor		eax,eax					; TSS+0x30 = EDX
		stosd
		xor		eax,eax					; TSS+0x34 = EBX
		stosd
		mov		eax,stack_init				; TSS+0x38 = ESP
		stosd
		xor		eax,eax					; TSS+0x3C = EBP
		stosd
		xor		eax,eax					; TSS+0x40 = ESI
		stosd
		xor		eax,eax					; TSS+0x44 = EDI
		stosd
		mov		ax,DATA32_SEL				; TSS+0x48 = ES
		stosd
		mov		ax,CODE32_SEL				; TSS+0x4C = CS
		stosd
		mov		ax,DATA32_SEL				; TSS+0x50 = SS
		stosd
		mov		ax,DATA32_SEL				; TSS+0x54 = DS
		stosd
		mov		ax,DATA32_SEL				; TSS+0x58 = FS
		stosd
		mov		ax,DATA32_SEL				; TSS+0x5C = GS
		stosd
		xor		eax,eax					; TSS+0x60 = LDTR
		stosd
		mov		eax,(104 << 16)				; TSS+0x64 = I/O map base
		stosd

; ============= setup the TSS representing the virtual 8086 mode task
		cld
		mov		edi,[tss_vm86_phys_base]
		sub		edi,[my_phys_base]

		xor		eax,eax					; TSS+0x00 = no backlink
		stosd
		mov		eax,stack_init				; TSS+0x04 = ESP for CPL0
		stosd
		mov		eax,DATA32_SEL				; TSS+0x08 = SS for CPL0
		stosd
		mov		eax,stack_init				; TSS+0x0C = ESP for CPL1
		stosd
		mov		eax,DATA32_SEL				; TSS+0x10 = SS for CPL1
		stosd
		mov		eax,stack_init				; TSS+0x14 = ESP for CPL2
		stosd
		mov		eax,DATA32_SEL				; TSS+0x18 = SS for CPL2
		stosd
		xor		eax,eax					; TSS+0x1C = CR3
		stosd
		mov		eax,vm86_entry				; TSS+0x20 = EIP
		stosd
		mov		eax,0x00020202				; TSS+0x24 = EFLAGS VM=1 IOPL=N IF=1
		movzx		ebx,byte [user_req_iopl]
		and		bl,3
		shl		ebx,12
		or		eax,ebx					; EFLAGS |= user_req_iopl << 12
		stosd
		xor		eax,eax					; TSS+0x28 = EAX
		stosd
		xor		eax,eax					; TSS+0x2C = ECX
		stosd
		xor		eax,eax					; TSS+0x30 = EDX
		stosd
		xor		eax,eax					; TSS+0x34 = EBX
		stosd
		mov		eax,stack_init_vm86			; TSS+0x38 = ESP
		stosd
		xor		eax,eax					; TSS+0x3C = EBP
		stosd
		xor		eax,eax					; TSS+0x40 = ESI
		stosd
		xor		eax,eax					; TSS+0x44 = EDI
		stosd
		mov		ax,[my_realmode_seg]			; TSS+0x48 = ES
		stosd
		mov		ax,[my_realmode_seg]			; TSS+0x4C = CS
		stosd
		mov		ax,[my_realmode_seg]			; TSS+0x50 = SS
		stosd
		mov		ax,[my_realmode_seg]			; TSS+0x54 = DS
		stosd
		mov		ax,[my_realmode_seg]			; TSS+0x58 = FS
		stosd
		mov		ax,[my_realmode_seg]			; TSS+0x5C = GS
		stosd
		xor		eax,eax					; TSS+0x60 = LDTR
		stosd
		mov		eax,(104 << 16)				; TSS+0x64 = I/O map base (0x68 == 104)
		stosd
		xor		eax,eax
		mov		ecx,8192 >> 2				; TSS+0x68 = I/O permission map (pre-set to all open)
		rep		stosd

; actually, we want it to trap some ports off the bat
		mov		edi,[tss_vm86_phys_base]
		sub		edi,[my_phys_base]
		add		edi,0x68				; TSS+0x68 = I/O permission map
		or		byte [edi+(0x64/8)],1 << (0x64 & 7)	; trap port 0x64
		or		byte [edi+(0x92/8)],1 << (0x92 & 7)	; trap port 0x92

; zero VCPI page map
		mov		eax,dword [vcpi_alloc_bitmap_phys]
		cmp		eax,0
		jz		.nothing
		push		es
		mov		ax,FLAT32_SEL
		mov		es,ax
		xor		eax,eax
		cld
		mov		edi,dword [vcpi_alloc_bitmap_phys]
		mov		ecx,dword [vcpi_alloc_bitmap_size]
		shr		ecx,2
		rep		stosd
		pop		es
.nothing:

; set up the IDT
		cld

		mov		edi,idt
		mov		esi,interrupt_procs

; the first 32 interrupts (0x00-0x1F) are marked CPL=0. This causes
; real-mode software interrupts in that range to instead cause a GPF
; which we can then safely reflect back to real mode. If all were
; marked CPL=3 then a real-mode software interrupt would trigger an
; actual INT in the IDT and it would be extremely difficult for
; fault handlers to differentiate between actual faults vs. software
; interrupts.
		mov		ecx,0x14
.idtdef0:	lodsw
		stosw					; base[15:0]
		mov		ax,CODE32_SEL
		stosw
		mov		ax,0x8E00		; DPL=0 (so that software interrupts 0x00-0x1F can be handled by our GPF)
		stosw
		xor		ax,ax
		stosw
		loop		.idtdef0

		mov		ecx,0x100 - 0x14
.idtdef3:	lodsw
		stosw					; base[15:0]
		mov		ax,CODE32_SEL
		stosw
		mov		ax,0xEE00		; DPL=3
		stosw
		xor		ax,ax
		stosw
		loop		.idtdef3

; next we need to reprogram the PIC so that IRQ 0-7 do not conflict with the CPU exceptions.
		mov		al,0x10			; ICW1 A0=0
		out		20h,al
		mov		al,IRQ_BASE_INT		; ICW2 A0=1
		out		21h,al
		mov		al,0x04			; ICW3 A0=1 slave on IRQ 2
		out		21h,al

		mov		al,0x10			; ICW1 A0=0
		out		0xA0,al
		mov		al,IRQ_BASE_INT+8	; ICW2 A0=1
		out		0xA1,al
		mov		al,0x02			; ICW3 A0=1
		out		0xA1,al

; jump into virtual 8086 mode
		jmp		TSS_VM86_SEL:0

; interrupt jump table. emits a 4-byte instruction (no error code)
;    interrupt_entry_4 <vector>
%macro interrupt_entry_4 1
; non-error code version. We push a dummy error code along with the intnum.
interrupt_%1:	push		byte 0
		push		%1
		jmp		interrupt_routine
%endmacro

; interrupt jump table. emits a 4-byte instruction (no error code), meant for IRQs
;    interrupt_entry_4 <vector>
%macro interrupt_entry_4irq 1
; non-error code version. We push a dummy error code along with the intnum.
interrupt_%1:	push		byte 0
		push		%1
		jmp		interrupt_routine_irq
%endmacro

; interrupt jump table. emits a 4-byte instruction (no error code), meant for IRQs
;    interrupt_entry_4 <vector>
%macro interrupt_entry_4_tf 1
; non-error code version. We push a dummy error code along with the intnum.
interrupt_%1:	push		byte 0
		push		%1
		jmp		interrupt_routine_tf
%endmacro

; error code version. The CPU pushes an error code on the stack. We just push the intnum and move on,
%macro interrupt_entry_4_ec 1
interrupt_%1:	push		%1
		jmp		interrupt_routine
%endmacro

; specialized for GPF fault. no time to jump to generic interrupt handlers.
%macro interrupt_entry_4gpf 1
interrupt_%1:	push		%1
		jmp		interrupt_routine_gpf
%endmacro

; specialized for EMM/VCPI entry. no time to jump to generic interrupt handlers.
%macro interrupt_entry_4emm 1
interrupt_%1:	push		byte 0
		push		%1
		jmp		interrupt_routine_emm
%endmacro

		interrupt_entry_4    0x00
		interrupt_entry_4_tf 0x01
		interrupt_entry_4    0x02
		interrupt_entry_4    0x03
		interrupt_entry_4    0x04
		interrupt_entry_4    0x05
		interrupt_entry_4    0x06
		interrupt_entry_4    0x07
		interrupt_entry_4_ec 0x08
		interrupt_entry_4    0x09
		interrupt_entry_4_ec 0x0A
		interrupt_entry_4_ec 0x0B
		interrupt_entry_4_ec 0x0C
		interrupt_entry_4gpf 0x0D
		interrupt_entry_4    0x0E
		interrupt_entry_4    0x0F
		interrupt_entry_4    0x10
		interrupt_entry_4    0x11
		interrupt_entry_4    0x12
		interrupt_entry_4    0x13
		interrupt_entry_4    0x14		; <== FIRST INTERRUPT NOT TO TRAP VIA GPF
		interrupt_entry_4    0x15
		interrupt_entry_4    0x16
		interrupt_entry_4    0x17
		interrupt_entry_4    0x18
		interrupt_entry_4    0x19
		interrupt_entry_4    0x1A
		interrupt_entry_4    0x1B
		interrupt_entry_4    0x1C
		interrupt_entry_4    0x1D
		interrupt_entry_4    0x1E
		interrupt_entry_4    0x1F
		interrupt_entry_4    0x20
		interrupt_entry_4    0x21
		interrupt_entry_4    0x22
		interrupt_entry_4    0x23
		interrupt_entry_4    0x24
		interrupt_entry_4    0x25
		interrupt_entry_4    0x26
		interrupt_entry_4    0x27
		interrupt_entry_4    0x28
		interrupt_entry_4    0x29
		interrupt_entry_4    0x2A
		interrupt_entry_4    0x2B
		interrupt_entry_4    0x2C
		interrupt_entry_4    0x2D
		interrupt_entry_4    0x2E
		interrupt_entry_4    0x2F
		interrupt_entry_4    0x30
		interrupt_entry_4    0x31
		interrupt_entry_4    0x32
		interrupt_entry_4    0x33
		interrupt_entry_4    0x34
		interrupt_entry_4    0x35
		interrupt_entry_4    0x36
		interrupt_entry_4    0x37
		interrupt_entry_4    0x38
		interrupt_entry_4    0x39
		interrupt_entry_4    0x3A
		interrupt_entry_4    0x3B
		interrupt_entry_4    0x3C
		interrupt_entry_4    0x3D
		interrupt_entry_4    0x3E
		interrupt_entry_4    0x3F
		interrupt_entry_4    0x40
		interrupt_entry_4    0x41
		interrupt_entry_4    0x42
		interrupt_entry_4    0x43
		interrupt_entry_4    0x44
		interrupt_entry_4    0x45
		interrupt_entry_4    0x46
		interrupt_entry_4    0x47
		interrupt_entry_4    0x48
		interrupt_entry_4    0x49
		interrupt_entry_4    0x4A
		interrupt_entry_4    0x4B
		interrupt_entry_4    0x4C
		interrupt_entry_4    0x4D
		interrupt_entry_4    0x4E
		interrupt_entry_4    0x4F
		interrupt_entry_4    0x50
		interrupt_entry_4    0x51
		interrupt_entry_4    0x52
		interrupt_entry_4    0x53
		interrupt_entry_4    0x54
		interrupt_entry_4    0x55
		interrupt_entry_4    0x56
		interrupt_entry_4    0x57
		interrupt_entry_4    0x58
		interrupt_entry_4    0x59
		interrupt_entry_4    0x5A
		interrupt_entry_4    0x5B
		interrupt_entry_4    0x5C
		interrupt_entry_4    0x5D
		interrupt_entry_4    0x5E
		interrupt_entry_4    0x5F
		interrupt_entry_4    0x60
		interrupt_entry_4    0x61
		interrupt_entry_4    0x62
		interrupt_entry_4    0x63
		interrupt_entry_4    0x64
		interrupt_entry_4    0x65
		interrupt_entry_4    0x66
		interrupt_entry_4emm 0x67
		interrupt_entry_4    0x68
		interrupt_entry_4    0x69
		interrupt_entry_4    0x6A
		interrupt_entry_4    0x6B
		interrupt_entry_4    0x6C
		interrupt_entry_4    0x6D
		interrupt_entry_4    0x6E
		interrupt_entry_4    0x6F
		interrupt_entry_4    0x70
		interrupt_entry_4    0x71
		interrupt_entry_4    0x72
		interrupt_entry_4    0x73
		interrupt_entry_4    0x74
		interrupt_entry_4    0x75
		interrupt_entry_4    0x76
		interrupt_entry_4    0x77
		interrupt_entry_4    0x78
		interrupt_entry_4    0x79
		interrupt_entry_4    0x7A
		interrupt_entry_4    0x7B
		interrupt_entry_4    0x7C
		interrupt_entry_4    0x7D
		interrupt_entry_4    0x7E
		interrupt_entry_4    0x7F
		interrupt_entry_4    0x80
		interrupt_entry_4    0x81
		interrupt_entry_4    0x82
		interrupt_entry_4    0x83
		interrupt_entry_4    0x84
		interrupt_entry_4    0x85
		interrupt_entry_4    0x86
		interrupt_entry_4    0x87
		interrupt_entry_4    0x88
		interrupt_entry_4    0x89
		interrupt_entry_4    0x8A
		interrupt_entry_4    0x8B
		interrupt_entry_4    0x8C
		interrupt_entry_4    0x8D
		interrupt_entry_4    0x8E
		interrupt_entry_4    0x8F
		interrupt_entry_4    0x90
		interrupt_entry_4    0x91
		interrupt_entry_4    0x92
		interrupt_entry_4    0x93
		interrupt_entry_4    0x94
		interrupt_entry_4    0x95
		interrupt_entry_4    0x96
		interrupt_entry_4    0x97
		interrupt_entry_4    0x98
		interrupt_entry_4    0x99
		interrupt_entry_4    0x9A
		interrupt_entry_4    0x9B
		interrupt_entry_4    0x9C
		interrupt_entry_4    0x9D
		interrupt_entry_4    0x9E
		interrupt_entry_4    0x9F
		interrupt_entry_4    0xA0
		interrupt_entry_4    0xA1
		interrupt_entry_4    0xA2
		interrupt_entry_4    0xA3
		interrupt_entry_4    0xA4
		interrupt_entry_4    0xA5
		interrupt_entry_4    0xA6
		interrupt_entry_4    0xA7
		interrupt_entry_4    0xA8
		interrupt_entry_4    0xA9
		interrupt_entry_4    0xAA
		interrupt_entry_4    0xAB
		interrupt_entry_4    0xAC
		interrupt_entry_4    0xAD
		interrupt_entry_4    0xAE
		interrupt_entry_4    0xAF
		interrupt_entry_4    0xB0
		interrupt_entry_4    0xB1
		interrupt_entry_4    0xB2
		interrupt_entry_4    0xB3
		interrupt_entry_4    0xB4
		interrupt_entry_4    0xB5
		interrupt_entry_4    0xB6
		interrupt_entry_4    0xB7
		interrupt_entry_4    0xB8
		interrupt_entry_4    0xB9
		interrupt_entry_4    0xBA
		interrupt_entry_4    0xBB
		interrupt_entry_4    0xBC
		interrupt_entry_4    0xBD
		interrupt_entry_4    0xBE
		interrupt_entry_4    0xBF
		interrupt_entry_4    0xC0
		interrupt_entry_4    0xC1
		interrupt_entry_4    0xC2
		interrupt_entry_4    0xC3
		interrupt_entry_4    0xC4
		interrupt_entry_4    0xC5
		interrupt_entry_4    0xC6
		interrupt_entry_4    0xC7
		interrupt_entry_4    0xC8
		interrupt_entry_4    0xC9
		interrupt_entry_4    0xCA
		interrupt_entry_4    0xCB
		interrupt_entry_4    0xCC
		interrupt_entry_4    0xCD
		interrupt_entry_4    0xCE
		interrupt_entry_4    0xCF
		interrupt_entry_4    0xD0
		interrupt_entry_4    0xD1
		interrupt_entry_4    0xD2
		interrupt_entry_4    0xD3
		interrupt_entry_4    0xD4
		interrupt_entry_4    0xD5
		interrupt_entry_4    0xD6
		interrupt_entry_4    0xD7
		interrupt_entry_4    0xD8
		interrupt_entry_4    0xD9
		interrupt_entry_4    0xDA
		interrupt_entry_4    0xDB
		interrupt_entry_4    0xDC
		interrupt_entry_4    0xDD
		interrupt_entry_4    0xDE
		interrupt_entry_4    0xDF
		interrupt_entry_4    0xE0
		interrupt_entry_4    0xE1
		interrupt_entry_4    0xE2
		interrupt_entry_4    0xE3
		interrupt_entry_4    0xE4
		interrupt_entry_4    0xE5
		interrupt_entry_4    0xE6
		interrupt_entry_4    0xE7
		interrupt_entry_4    0xE8
		interrupt_entry_4    0xE9
		interrupt_entry_4    0xEA
		interrupt_entry_4    0xEB
		interrupt_entry_4    0xEC
		interrupt_entry_4    0xED
		interrupt_entry_4    0xEE
		interrupt_entry_4    0xEF
		interrupt_entry_4irq 0xF0
		interrupt_entry_4irq 0xF1
		interrupt_entry_4irq 0xF2
		interrupt_entry_4irq 0xF3
		interrupt_entry_4irq 0xF4
		interrupt_entry_4irq 0xF5
		interrupt_entry_4irq 0xF6
		interrupt_entry_4irq 0xF7
		interrupt_entry_4irq 0xF8
		interrupt_entry_4irq 0xF9
		interrupt_entry_4irq 0xFA
		interrupt_entry_4irq 0xFB
		interrupt_entry_4irq 0xFC
		interrupt_entry_4irq 0xFD
		interrupt_entry_4irq 0xFE
		interrupt_entry_4irq 0xFF

interrupt_procs:dw		interrupt_0x00
		dw		interrupt_0x01
		dw		interrupt_0x02
		dw		interrupt_0x03
		dw		interrupt_0x04
		dw		interrupt_0x05
		dw		interrupt_0x06
		dw		interrupt_0x07
		dw		interrupt_0x08
		dw		interrupt_0x09
		dw		interrupt_0x0A
		dw		interrupt_0x0B
		dw		interrupt_0x0C
		dw		interrupt_0x0D
		dw		interrupt_0x0E
		dw		interrupt_0x0F
		dw		interrupt_0x10
		dw		interrupt_0x11
		dw		interrupt_0x12
		dw		interrupt_0x13
		dw		interrupt_0x14
		dw		interrupt_0x15
		dw		interrupt_0x16
		dw		interrupt_0x17
		dw		interrupt_0x18
		dw		interrupt_0x19
		dw		interrupt_0x1A
		dw		interrupt_0x1B
		dw		interrupt_0x1C
		dw		interrupt_0x1D
		dw		interrupt_0x1E
		dw		interrupt_0x1F

		dw		interrupt_0x20
		dw		interrupt_0x21
		dw		interrupt_0x22
		dw		interrupt_0x23
		dw		interrupt_0x24
		dw		interrupt_0x25
		dw		interrupt_0x26
		dw		interrupt_0x27
		dw		interrupt_0x28
		dw		interrupt_0x29
		dw		interrupt_0x2A
		dw		interrupt_0x2B
		dw		interrupt_0x2C
		dw		interrupt_0x2D
		dw		interrupt_0x2E
		dw		interrupt_0x2F
		dw		interrupt_0x30
		dw		interrupt_0x31
		dw		interrupt_0x32
		dw		interrupt_0x33
		dw		interrupt_0x34
		dw		interrupt_0x35
		dw		interrupt_0x36
		dw		interrupt_0x37
		dw		interrupt_0x38
		dw		interrupt_0x39
		dw		interrupt_0x3A
		dw		interrupt_0x3B
		dw		interrupt_0x3C
		dw		interrupt_0x3D
		dw		interrupt_0x3E
		dw		interrupt_0x3F

		dw		interrupt_0x40
		dw		interrupt_0x41
		dw		interrupt_0x42
		dw		interrupt_0x43
		dw		interrupt_0x44
		dw		interrupt_0x45
		dw		interrupt_0x46
		dw		interrupt_0x47
		dw		interrupt_0x48
		dw		interrupt_0x49
		dw		interrupt_0x4A
		dw		interrupt_0x4B
		dw		interrupt_0x4C
		dw		interrupt_0x4D
		dw		interrupt_0x4E
		dw		interrupt_0x4F
		dw		interrupt_0x50
		dw		interrupt_0x51
		dw		interrupt_0x52
		dw		interrupt_0x53
		dw		interrupt_0x54
		dw		interrupt_0x55
		dw		interrupt_0x56
		dw		interrupt_0x57
		dw		interrupt_0x58
		dw		interrupt_0x59
		dw		interrupt_0x5A
		dw		interrupt_0x5B
		dw		interrupt_0x5C
		dw		interrupt_0x5D
		dw		interrupt_0x5E
		dw		interrupt_0x5F

		dw		interrupt_0x60
		dw		interrupt_0x61
		dw		interrupt_0x62
		dw		interrupt_0x63
		dw		interrupt_0x64
		dw		interrupt_0x65
		dw		interrupt_0x66
		dw		interrupt_0x67
		dw		interrupt_0x68
		dw		interrupt_0x69
		dw		interrupt_0x6A
		dw		interrupt_0x6B
		dw		interrupt_0x6C
		dw		interrupt_0x6D
		dw		interrupt_0x6E
		dw		interrupt_0x6F
		dw		interrupt_0x70
		dw		interrupt_0x71
		dw		interrupt_0x72
		dw		interrupt_0x73
		dw		interrupt_0x74
		dw		interrupt_0x75
		dw		interrupt_0x76
		dw		interrupt_0x77
		dw		interrupt_0x78
		dw		interrupt_0x79
		dw		interrupt_0x7A
		dw		interrupt_0x7B
		dw		interrupt_0x7C
		dw		interrupt_0x7D
		dw		interrupt_0x7E
		dw		interrupt_0x7F

		dw		interrupt_0x80
		dw		interrupt_0x81
		dw		interrupt_0x82
		dw		interrupt_0x83
		dw		interrupt_0x84
		dw		interrupt_0x85
		dw		interrupt_0x86
		dw		interrupt_0x87
		dw		interrupt_0x88
		dw		interrupt_0x89
		dw		interrupt_0x8A
		dw		interrupt_0x8B
		dw		interrupt_0x8C
		dw		interrupt_0x8D
		dw		interrupt_0x8E
		dw		interrupt_0x8F
		dw		interrupt_0x90
		dw		interrupt_0x91
		dw		interrupt_0x92
		dw		interrupt_0x93
		dw		interrupt_0x94
		dw		interrupt_0x95
		dw		interrupt_0x96
		dw		interrupt_0x97
		dw		interrupt_0x98
		dw		interrupt_0x99
		dw		interrupt_0x9A
		dw		interrupt_0x9B
		dw		interrupt_0x9C
		dw		interrupt_0x9D
		dw		interrupt_0x9E
		dw		interrupt_0x9F

		dw		interrupt_0xA0
		dw		interrupt_0xA1
		dw		interrupt_0xA2
		dw		interrupt_0xA3
		dw		interrupt_0xA4
		dw		interrupt_0xA5
		dw		interrupt_0xA6
		dw		interrupt_0xA7
		dw		interrupt_0xA8
		dw		interrupt_0xA9
		dw		interrupt_0xAA
		dw		interrupt_0xAB
		dw		interrupt_0xAC
		dw		interrupt_0xAD
		dw		interrupt_0xAE
		dw		interrupt_0xAF
		dw		interrupt_0xB0
		dw		interrupt_0xB1
		dw		interrupt_0xB2
		dw		interrupt_0xB3
		dw		interrupt_0xB4
		dw		interrupt_0xB5
		dw		interrupt_0xB6
		dw		interrupt_0xB7
		dw		interrupt_0xB8
		dw		interrupt_0xB9
		dw		interrupt_0xBA
		dw		interrupt_0xBB
		dw		interrupt_0xBC
		dw		interrupt_0xBD
		dw		interrupt_0xBE
		dw		interrupt_0xBF

		dw		interrupt_0xC0
		dw		interrupt_0xC1
		dw		interrupt_0xC2
		dw		interrupt_0xC3
		dw		interrupt_0xC4
		dw		interrupt_0xC5
		dw		interrupt_0xC6
		dw		interrupt_0xC7
		dw		interrupt_0xC8
		dw		interrupt_0xC9
		dw		interrupt_0xCA
		dw		interrupt_0xCB
		dw		interrupt_0xCC
		dw		interrupt_0xCD
		dw		interrupt_0xCE
		dw		interrupt_0xCF
		dw		interrupt_0xD0
		dw		interrupt_0xD1
		dw		interrupt_0xD2
		dw		interrupt_0xD3
		dw		interrupt_0xD4
		dw		interrupt_0xD5
		dw		interrupt_0xD6
		dw		interrupt_0xD7
		dw		interrupt_0xD8
		dw		interrupt_0xD9
		dw		interrupt_0xDA
		dw		interrupt_0xDB
		dw		interrupt_0xDC
		dw		interrupt_0xDD
		dw		interrupt_0xDE
		dw		interrupt_0xDF

		dw		interrupt_0xE0
		dw		interrupt_0xE1
		dw		interrupt_0xE2
		dw		interrupt_0xE3
		dw		interrupt_0xE4
		dw		interrupt_0xE5
		dw		interrupt_0xE6
		dw		interrupt_0xE7
		dw		interrupt_0xE8
		dw		interrupt_0xE9
		dw		interrupt_0xEA
		dw		interrupt_0xEB
		dw		interrupt_0xEC
		dw		interrupt_0xED
		dw		interrupt_0xEE
		dw		interrupt_0xEF
		dw		interrupt_0xF0
		dw		interrupt_0xF1
		dw		interrupt_0xF2
		dw		interrupt_0xF3
		dw		interrupt_0xF4
		dw		interrupt_0xF5
		dw		interrupt_0xF6
		dw		interrupt_0xF7
		dw		interrupt_0xF8
		dw		interrupt_0xF9
		dw		interrupt_0xFA
		dw		interrupt_0xFB
		dw		interrupt_0xFC
		dw		interrupt_0xFD
		dw		interrupt_0xFE
		dw		interrupt_0xFF

interrupt_0x00_strs:
		dw		str_fault_0x00
		dw		str_fault_0x01
		dw		str_fault_0x02
		dw		str_fault_0x03
		dw		str_fault_0x04
		dw		str_fault_0x05
		dw		str_fault_0x06
		dw		str_fault_0x07
		dw		str_fault_0x08
		dw		str_fault_0x09
		dw		str_fault_0x0A
		dw		str_fault_0x0B
		dw		str_fault_0x0C
		dw		str_fault_0x0D
		dw		str_fault_0x0E
		dw		str_fault_0x0F
		dw		str_fault_0x10
		dw		str_fault_0x11
		dw		str_fault_0x12
		dw		str_fault_0x13

; VCPI PM entry
%macro pm_int_entry 0
		pusha
		push		ds
		push		es
		mov		ax,DATA32_SEL
		mov		ds,ax
		mov		es,ax
		lea		eax,[esp+4+4+32+4+4+12]	; ESP + DS + ES + PUSHA + INTNUM + ERRCODE + IRET
		; TODO: MOVE EAX over ESP image on stack
		lea		ebp,[esp]
%endmacro
%macro pm_int_exit 0
		pop		es
		pop		ds
		popa
		retf
%endmacro

; this must reside here. Any further down the jmps in the table will be out of range.
; Check:
;           (EFLAGS & VM) = Happend in v86 mode?
%macro int_entry 0
		pusha
		push		ds
		push		es
		mov		ax,DATA32_SEL
		mov		ds,ax
		mov		es,ax
		lea		eax,[esp+4+4+32+4+4+12]	; ESP + DS + ES + PUSHA + INTNUM + ERRCODE + IRET
		; TODO: MOVE EAX over ESP image on stack
		lea		ebp,[esp]
%endmacro
%macro int_exit 0
		pop		es
		pop		ds
		popa
		add		esp,8		; drop error code + interrupt num
		iret
%endmacro
%define		int_ES		word [ebp+0]
%define		int_DS		word [ebp+4]
%define		int_EDI		dword [ebp+8]
%define		int_ESI		dword [ebp+12]
%define		int_EBP		dword [ebp+16]
%define		int_ESP		dword [ebp+20]
%define		int_EBX		dword [ebp+24]
%define		int_EDX		dword [ebp+28]
%define		int_ECX		dword [ebp+32]
%define		int_EAX		dword [ebp+36]
%define		int_AX		word [ebp+36]
%define		int_AL		byte [ebp+36]
%define		int_INTNUM	dword [ebp+40]
%define		int_INTNUM_b	byte [ebp+40]
%define		int_ERRCODE	dword [ebp+44]
%define		int_EIP		dword [ebp+48]
%define		int_EIP_w	word [ebp+48]
%define		int_CS		word [ebp+52]
%define		int_EFLAGS	dword [ebp+56]
%define		int_FLAGS	word [ebp+56]
; the following also exist if coming out of v86 mode
%define		int_v86_ESP	dword [ebp+60]
%define		int_v86_SS	word [ebp+64]
%define		int_v86_ES	word [ebp+68]
%define		int_v86_DS	word [ebp+72]
%define		int_v86_FS	word [ebp+76]
%define		int_v86_GS	word [ebp+80]

irq_rm_map:	db		0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F
		db		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77

; return: EAX = physical memory address of vm86 SS:SP pointer
vm86_ss_esp_to_phys:
		xor		eax,eax
		mov		ax,int_v86_SS
		shl		eax,4
		mov		ebx,int_v86_ESP
		and		ebx,0xFFFF
		add		eax,ebx
		ret

; return: EAX = physical memory address of vm86 CS:IP pointer
vm86_cs_eip_to_phys:
		xor		eax,eax
		mov		ax,int_CS
		shl		eax,4
		mov		ebx,int_EIP
		and		ebx,0xFFFF
		add		eax,ebx
		ret

; EAX = interrupt to direct v86 mode to.
; the caller must be on a stack frame formed from v86 mode to protected mode transition, or this won't work
vm86_push_interrupt:
		push		es
		mov		bx,FLAT32_SEL
		mov		es,bx
		mov		ecx,[es:eax*4]
		sub		int_v86_ESP,6			; IRET stack frame
		jc		.stack_overflow
		call		vm86_ss_esp_to_phys
		mov		ebx,int_EIP			; save IP,CS,FLAGS to stack
		mov		word [es:eax+0],bx
		mov		bx,int_CS
		mov		word [es:eax+2],bx
		mov		ebx,int_EFLAGS
		mov		word [es:eax+4],bx

		mov		int_EIP_w,cx			; load new CS:IP from interrupt vector
		shr		ecx,16
		mov		int_CS,cx

		pop		es
		ret
.stack_overflow:
		call		interrupt_routine_halt_prepare
		mov		edx,int_INTNUM
		mov		esi,str_stack_overflow
		jmp		fault_jmp_unhandled

interrupt_routine_tf: ; INT 0x01 Trap Interrupt
		int_entry
		test		int_EFLAGS,0x20000		; did this happen from v86 mode?
		jz		.not_vm86
		mov		eax,1				; reflect INT 0x01 to vm86
		call		vm86_push_interrupt
		and		int_EFLAGS,~0x100		; clear the trap flag
		int_exit
.not_vm86:	call		interrupt_routine_halt_prepare
		mov		edx,int_INTNUM
		mov		esi,str_unexpected_int_not_v86
		jmp		fault_jmp_unhandled

interrupt_routine_irq:
		int_entry
		test		int_EFLAGS,0x20000		; did this happen from v86 mode?
		jz		.not_vm86
		mov		eax,int_INTNUM
		sub		eax,IRQ_BASE_INT
		movzx		eax,byte [eax+irq_rm_map]
		call		vm86_push_interrupt
		int_exit
.not_vm86:	call		interrupt_routine_halt_prepare
		mov		edx,int_INTNUM
		mov		esi,str_unexpected_int_not_v86
		jmp		fault_jmp_unhandled

vm86_stack_overflow:
		call		interrupt_routine_halt_prepare
		mov		edx,int_INTNUM
		mov		esi,str_stack_overflow
		jmp		fault_jmp_unhandled

; General Protection Fault
interrupt_routine_gpf:
		int_entry
		mov		ax,FLAT32_SEL
		mov		es,ax
		test		int_EFLAGS,0x20000		; did this happen from v86 mode?
		jz		.not_vm86

		call		vm86_cs_eip_to_phys		; else, read the opcode at CS:IP to tell between exceptions and explicit INT xxh instructions
		mov		eax,[es:eax]			; EAX = first 4 bytes of the opcode

; INT 3?
		cmp		al,0xCC
		jz		.vm86_int3
; INT N?
		cmp		al,0xCD
		jz		.vm86_int_n
; IN AL,<imm>
		cmp		al,0xE4
		jz		.vm86_in_al_imm
; IN AX,<imm>
		cmp		al,0xE5
		jz		.vm86_in_ax_imm
; OUT <imm>,AL
		cmp		al,0xE6
		jz		.vm86_out_imm_al
; OUT <imm>,AX
		cmp		al,0xE7
		jz		.vm86_out_imm_ax
; IN AL,DX
		cmp		al,0xEC
		jz		.vm86_in_al_dx
; IN AX,DX
		cmp		al,0xED
		jz		.vm86_in_ax_dx
; OUT DX,AL
		cmp		al,0xEE
		jz		.vm86_out_dx_al
; OUT DX,AX
		cmp		al,0xEF
		jz		.vm86_out_dx_ax
; CLI?
		cmp		al,0xFA
		jz		.vm86_cli
; STI?
		cmp		al,0xFB
		jz		.vm86_sti
; PUSHF?
		cmp		al,0x9C
		jz		.vm86_pushf
; POPF?
		cmp		al,0x9D
		jz		.vm86_popf
; PUSHFD?
		cmp		ax,0x9C66
		jz		.vm86_pushfd
; POPFD?
		cmp		ax,0x9D66
		jz		.vm86_popfd
; IRET?
		cmp		al,0xCF
		jz		.vm86_iret
; well then I don't know
		call		interrupt_routine_halt_prepare
		mov		edx,0xD
		mov		esi,str_v86_unknown
		jmp		fault_jmp_unhandled

.vm86_in_al_imm:add		int_EIP,2			; EIP += 2
		movzx		edx,ah				; I/O port number in second byte
		mov		ecx,1				; ecx = size of I/O
		call		on_vm86_io_read
		mov		int_AL,al			; eax = byte value
		int_exit

.vm86_in_ax_imm:add		int_EIP,2			; EIP += 2
		movzx		edx,ah				; I/O port number in second byte
		mov		ecx,2				; ecx = size of I/O
		call		on_vm86_io_read
		mov		int_AX,ax			; eax = byte value
		int_exit

.vm86_in_al_dx:	inc		int_EIP				; EIP += 1
		mov		edx,int_EDX			; edx = port number
		and		edx,0xFFFF
		mov		ecx,1				; ecx = size of I/O
		call		on_vm86_io_read
		mov		int_AL,al			; eax = byte value
		int_exit

.vm86_in_ax_dx:	inc		int_EIP				; EIP += 1
		mov		edx,int_EDX			; edx = port number
		and		edx,0xFFFF
		mov		ecx,2				; ecx = size of I/O
		call		on_vm86_io_read
		mov		int_AX,ax			; eax = byte value
		int_exit

.vm86_out_imm_al:add		int_EIP,2			; EIP += 2
		movzx		edx,ah				; I/O port number in second byte
		movzx		eax,int_AL			; eax = byte value
		mov		ecx,1				; ecx = size of I/O
		call		on_vm86_io_write
		int_exit

.vm86_out_imm_ax:add		int_EIP,2			; EIP += 2
		movzx		edx,ah				; I/O port number in second byte
		mov		eax,int_EAX
		and		eax,0xFFFF
		mov		ecx,2				; ecx = size of I/O
		call		on_vm86_io_write
		int_exit

.vm86_out_dx_al:inc		int_EIP				; EIP += 1
		mov		edx,int_EDX			; edx = port number
		and		edx,0xFFFF
		movzx		eax,int_AL			; eax = byte value
		mov		ecx,1				; ecx = size of I/O
		call		on_vm86_io_write
		int_exit

.vm86_out_dx_ax:inc		int_EIP				; EIP += 1
		mov		edx,int_EDX			; edx = port number
		and		edx,0xFFFF
		mov		eax,int_EAX
		and		eax,0xFFFF
		mov		ecx,2				; ecx = size of I/O
		call		on_vm86_io_write
		int_exit

.vm86_pushfd:	push		es
		mov		bx,FLAT32_SEL
		mov		es,bx
		sub		int_v86_ESP,4			; push DWORD
		jc		vm86_stack_overflow
		call		vm86_ss_esp_to_phys		; EAX = SS:SP physical mem location
		mov		ebx,int_EFLAGS			; retrieve FLAGS
		mov		[es:eax],ebx			; store on stack
		pop		es
		add		int_EIP,2
		int_exit

.vm86_popfd:	push		es
		mov		bx,FLAT32_SEL
		mov		es,bx
		call		vm86_ss_esp_to_phys		; EAX = SS:SP physical mem location
		add		int_v86_ESP,4			; pop DWORD
		jc		vm86_stack_overflow
		mov		ebx,[es:eax]			; retrieve from stack
		or		ebx,0x20000			; don't let the guest disable virtual 8086 mode
		mov		int_EFLAGS,ebx			; place in FLAGS
		pop		es
		add		int_EIP,2
		int_exit

.vm86_pushf:	push		es
		mov		bx,FLAT32_SEL
		mov		es,bx
		sub		int_v86_ESP,2			; push WORD
		jc		vm86_stack_overflow
		call		vm86_ss_esp_to_phys		; EAX = SS:SP physical mem location
		mov		bx,int_FLAGS			; retrieve FLAGS
		mov		[es:eax],bx			; store on stack
		pop		es
		add		int_EIP,1
		int_exit

.vm86_popf:	push		es
		mov		bx,FLAT32_SEL
		mov		es,bx
		call		vm86_ss_esp_to_phys		; EAX = SS:SP physical mem location
		add		int_v86_ESP,2			; pop WORD
		jc		vm86_stack_overflow
		mov		bx,[es:eax]			; retrieve from stack
		mov		int_FLAGS,bx			; place in FLAGS
		pop		es
		add		int_EIP,1
		int_exit

.vm86_iret:	push		es
		mov		bx,FLAT32_SEL
		mov		es,bx
		call		vm86_ss_esp_to_phys		; EAX = SS:SP physical mem location
		add		int_v86_ESP,6			; pop interrupt stack frame
		jc		vm86_stack_overflow
		xor		ebx,ebx
		mov		bx,[es:eax+0]			; EBX = IP
		mov		int_EIP,ebx
		mov		bx,[es:eax+2]			; EBX = CS
		mov		int_CS,bx
		mov		bx,[es:eax+4]			; EBX = FLAGS
		mov		int_FLAGS,bx
		pop		es
		int_exit

.vm86_cli:	and		int_EFLAGS,~0x200		; IF=0
		inc		int_EIP
		int_exit

.vm86_sti:	or		int_EFLAGS,0x200		; IF=1
		inc		int_EIP
		int_exit

.vm86_int3:	inc		int_EIP				; EIP++
		mov		eax,3
		call		vm86_push_interrupt
		int_exit

.vm86_int_n:	movzx		eax,ah				; AL=CD AH=intnum
		add		int_EIP,2			; EIP += 2
		call		vm86_push_interrupt
		int_exit

.not_vm86:	call		interrupt_routine_halt_prepare
		call		edx_esi_default_int_str
		jmp		fault_jmp_unhandled

interrupt_routine:
		int_entry
		mov		ax,FLAT32_SEL
		mov		es,ax
		cmp		int_INTNUM,RM_INT_API		; interrupts sent to our RM_INT_API are handled directly
		jz		.rm_int_api
		test		int_EFLAGS,0x20000		; did this happen from v86 mode?
		jz		.not_vm86

; okay then, there are several reasons such interrupts fire.
		mov		al,int_INTNUM_b
; INT 15H interception
		cmp		al,0x15
		jz		.vm86_int15
; Anything int >= 0x14 on
		cmp		al,0x14
		jae		.vm86_intN
; INT 0x03 debug interrupt FIXME is this needed?
		cmp		al,0x03
		jz		.vm86_int3
; INT 0x01 debug trap FIXME is this needed?
		cmp		al,0x01
		jz		.vm86_int1
; INT 0x07 COPROCESSOR NOT PRESENT (vm86 task used a floating point instruction)
		cmp		al,0x07
		jz		.fpu_7

		call		interrupt_routine_halt_prepare
		call		edx_esi_default_int_str
		jmp		fault_jmp_unhandled

; COPROCESSOR NOT PRESENT. USUALLY SIGNALLED AFTER TASK SWITCH ON FIRST FPU INSTRUCTION.
.fpu_7:		clts
		int_exit

.vm86_int1:	mov		eax,1
		call		vm86_push_interrupt
		and		int_EFLAGS,~0x100		; clear trap flag
		int_exit

.vm86_int3:	mov		eax,3
		call		vm86_push_interrupt
		int_exit

.vm86_intN:	mov		eax,int_INTNUM
		call		vm86_push_interrupt
		int_exit

; INT 15h; we must intercept AH=87h for extended memory programs to work properly within our VM86 environment
.vm86_int15:	mov		ebx,int_EAX
		cmp		bh,0x87
		jz		.vm86_int15_87
; carry it forward to the BIOS
		mov		eax,int_INTNUM
		call		vm86_push_interrupt
		int_exit

; INT 15H AH=87h
.vm86_int15_87:	xor		eax,eax
		mov		ax,int_v86_ES
		shl		eax,4
		and		int_ESI,0xFFFF
		add		eax,int_ESI
		mov		ecx,int_ECX
		and		ecx,0xFFFF
; ECX = count of WORDs, EAX = physical memory addr of GDT
		mov		esi,[es:eax+0x10+2]
		and		esi,0xFFFFFF
		mov		edi,[es:eax+0x18+2]
		and		edi,0xFFFFFF
; ESI = physical memory addr (src), EDI = physical memory addr (dst)
		movzx		ebx,byte [es:eax+0x10+7]
		shl		ebx,24
		or		esi,ebx
		movzx		ebx,byte [es:eax+0x18+7]
		shl		ebx,24
		or		edi,ebx
; carry out the actual copy
		push		ds
		mov		ax,es
		mov		ds,ax
		cld
		rep		movsw
		pop		ds
; signal success
		mov		int_EAX,0
		int_exit

.not_vm86:	call		interrupt_routine_halt_prepare
		call		edx_esi_default_int_str
		jmp		fault_jmp_unhandled
.rm_int_api:	mov		eax,int_EAX
		cmp		eax,0xAABBAABB			; command to unload
		jz		.rm_int_api_unload
		cmp		eax,0xAABBAA55			; detection
		jz		.rm_int_api_detect
; unknown command
		mov		int_EAX,eax
		call		interrupt_routine_halt_prepare
		call		edx_esi_default_int_str
		jmp		fault_jmp_unhandled
.rm_int_api_unload:
		mov		eax,int_v86_ESP
		mov		[unload_int_stk+0],ax
		mov		ax,int_v86_SS
		mov		[unload_int_stk+2],ax

		mov		eax,int_EIP
		mov		[unload_int_ret+0],ax
		mov		ax,int_CS
		mov		[unload_int_ret+2],ax

		mov		ax,DATA32_SEL
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		esp,stack_init
		jmp		CODE32_SEL:_exit_from_prot32
.rm_int_api_detect:
		mov		int_EAX,0xBBAABB33
		int_exit
edx_esi_default_int_str:
		mov		edx,int_INTNUM
		cmp		edx,0x13
		jle		.l1
		mov		edx,0x13
.l1:		xor		esi,esi
		mov		si,word [(edx*2)+interrupt_0x00_strs]
		mov		edx,int_INTNUM
		ret
interrupt_routine_halt_prepare: ; <====== SET ESI = address of string to print. Trashes EAX.
		xor		eax,eax
		mov		dword [unhandled_fault_var_opcode],eax
		mov		eax,int_EAX
		mov		dword [unhandled_fault_var_eax],eax
		mov		eax,int_EBX
		mov		dword [unhandled_fault_var_ebx],eax
		mov		eax,int_ECX
		mov		dword [unhandled_fault_var_ecx],eax
		mov		eax,int_EDX
		mov		dword [unhandled_fault_var_edx],eax
		mov		eax,int_ESI
		mov		dword [unhandled_fault_var_esi],eax
		mov		eax,int_EDI
		mov		dword [unhandled_fault_var_edi],eax
		mov		eax,int_EBP
		mov		dword [unhandled_fault_var_ebp],eax
		mov		eax,int_ESP
		mov		dword [unhandled_fault_var_esp],eax
		mov		eax,int_EIP
		mov		dword [unhandled_fault_var_eip],eax
		mov		eax,int_ERRCODE
		mov		dword [unhandled_fault_var_errcode],eax
		xor		eax,eax
		mov		ax,int_ES
		mov		word [unhandled_fault_var_es],ax
		mov		ax,int_DS
		mov		word [unhandled_fault_var_ds],ax
		mov		ax,int_CS
		mov		word [unhandled_fault_var_cs],ax
		mov		ax,fs
		mov		word [unhandled_fault_var_fs],ax
		mov		ax,gs
		mov		word [unhandled_fault_var_gs],ax
		mov		ax,ss
		mov		word [unhandled_fault_var_ss],ax
		mov		eax,cr0
		mov		dword [unhandled_fault_var_cr0],eax
		mov		eax,cr3
		mov		dword [unhandled_fault_var_cr3],eax
		mov		eax,cr4
		mov		dword [unhandled_fault_var_cr4],eax
		mov		eax,int_EFLAGS
		mov		dword [unhandled_fault_var_eflags],eax
		test		eax,0x20000		; was the VM flag set in EFLAGS when it happened?
		jnz		.v86_mode
		test		int_CS,3		; did we come from ring 1/2/3?
		jz		.no_escalation
; ESP needs to reflect it's contents prior to jumping to ring 0
		mov		eax,int_v86_ESP
		mov		dword [unhandled_fault_var_esp],eax
.no_escalation:	ret
; the segment registers need to reflect what they were in vm86 mode
.v86_mode:	xor		eax,eax
		mov		ax,int_v86_ES
		mov		word [unhandled_fault_var_es],ax
		mov		ax,int_v86_DS
		mov		word [unhandled_fault_var_ds],ax
		mov		ax,int_v86_FS
		mov		word [unhandled_fault_var_fs],ax
		mov		ax,int_v86_GS
		mov		word [unhandled_fault_var_gs],ax
		mov		ax,int_v86_SS
		mov		word [unhandled_fault_var_ss],ax
		mov		eax,int_v86_ESP
		mov		dword [unhandled_fault_var_esp],eax
; we also might want to know what opcodes were involved
		push		es
		mov		ax,FLAT32_SEL
		call		vm86_cs_eip_to_phys
		mov		ebx,[es:eax]
		mov		dword [unhandled_fault_var_opcode],ebx
		pop		es
		ret

; ================ INT 67h EMM/VCPI entry point
; TODO: At some point this code needs to be written to a) reflect the INT 67H call back to v86 mode if it's not EMM/VCPI functions
;       and b) chain to v86 mode if the return address CS:IP is not the real-mode INT 67H handler we installed (so that other
;       programs can hook the interrupt on top of us)
interrupt_routine_emm:
		int_entry
		mov		eax,int_EAX
		cmp		ah,0DEh
		jz		.vcpi
		cmp		ah,040h
		jl		.pass
		cmp		ah,05Eh
		jae		.pass
		jmp		.emm
		int_exit
.pass:		jmp		emm_unhandled
; VCPI processing (AH=0xDE)
.vcpi:		cmp		al,0x0C
		ja		emm_unhandled
		movzx		eax,al
		jmp		word [vcpi_functions+(eax*2)]
; EMM processing (AH=0x40...0x5D)
.emm:		sub		ah,0x40
		movzx		eax,ah
		jmp		word [emm_functions+(eax*2)]

emm_unhandled:
vcpi_unhandled:
		call		interrupt_routine_halt_prepare
		mov		edx,67h
		mov		esi,str_unknown_emm_vcpi_call
		jmp		fault_jmp_unhandled

; VCPI call table
vcpi_functions:	dw		vcpi00_detect			; 0x00
		dw		vcpi01_get_pm_if		; 0x01
		dw		vcpi_unhandled			; 0x02
		dw		vcpi03_free_pages		; 0x03
		dw		vcpi04_alloc_page		; 0x04
		dw		vcpi05_free_page		; 0x05
		dw		vcpi06_get_phys_addr		; 0x06
		dw		vcpi_unhandled			; 0x07
		dw		vcpi_unhandled			; 0x08
		dw		vcpi_unhandled			; 0x09
		dw		vcpi0A_8259A_mapping		; 0x0A
		dw		vcpi_unhandled			; 0x0B
		dw		vcpi0C_enter_pm			; 0x0C

; emm call table
emm_functions:	dw		emm40_get_status		; 0x40
		dw		emm41_get_pfa			; 0x41
		dw		emm42_get_unalloc_page_count	; 0x42
		dw		emm43_alloc_pages		; 0x43
		dw		emm44_map_handle_page		; 0x44
		dw		emm45_dealloc_pages		; 0x45
		dw		emm46_get_version		; 0x46
		dw		emm47_save_page_map		; 0x47
		dw		emm48_restore_page_map		; 0x48
		dw		emm_unhandled			; 0x49
		dw		emm_unhandled			; 0x4A
		dw		emm4B_get_handle_count		; 0x4B
		dw		emm4C_get_handle_pages		; 0x4C
		dw		emm4D_get_all_handle_pages	; 0x4D
		dw		emm4E_combo			; 0x4E
		dw		emm4F_combo			; 0x4F
		dw		emm50_combo			; 0x50
		dw		emm51_realloc_pages		; 0x51
		dw		emm52_combo			; 0x52
		dw		emm53_combo			; 0x53
		dw		emm54_combo			; 0x54
		dw		emm55_combo			; 0x55
		dw		emm56_combo			; 0x56
		dw		emm57_combo			; 0x57
		dw		emm58_combo			; 0x58
		dw		emm59_combo			; 0x59
		dw		emm5A_combo			; 0x5A
		dw		emm5B_combo			; 0x5B
		dw		emm5C_prepare_warmboot		; 0x5C
		dw		emm5D_combo			; 0x5D

; 8042 trapping state
trap_8042_mode			db		0x00
; port 92 trapping
fake_port_92			db		0x02		; the reason we save this is some programs insist on
								; reading back to check. if we forced bits the program
								; will get stuck in a loop.

; VCPI I/O write
; in: EDX=port number EAX/AX/AL=byte value to write  ECX=I/O size
; out: EAX/AX/AL=byte value to return
on_vm86_io_write:		cmp		edx,0x64
				jz		.write_8042_controller
				cmp		edx,0x92
				jz		.write_port_A_92h
.unknown_port:			ret		; IGNORE
.write_8042_controller:		cmp		byte [trap_8042_mode],0xD1		; trap Write Output Port bitfield
				jz		.write_8042_controller_out
; now match the command directly
				cmp		al,0xF0					; trap attempts to pulse output bits
				jae		.write_8042_controller_outpulse
				cmp		al,0xD1					; trap Write Output Port
				jz		.write_8042_controller_out_and_latch
; command is OK, pass it through
				out		0x64,al
				ret
; the previous byte was the Write Output command, so the next byte is the actual bits to write. We must ensure A20 is on.
.write_8042_controller_out:	or		al,0x02		; ensure bit 1 (A20 gate) is always on
				out		0x64,al
				mov		byte [trap_8042_mode],0x00
				ret
; the program attempted to pulse output bits. silently ignore.
.write_8042_controller_outpulse:ret
; the program sent us a command that we pass through, but we must modify the data byte as it passes
.write_8042_controller_out_and_latch:
				mov		byte [trap_8042_mode],al
				out		0x64,al		; then pass the command through
				ret
; the program is writing port 0x92 to try and fiddle with A20
.write_port_A_92h:		mov		byte [fake_port_92],al ; store what the program wrote
				or		al,2		; make sure A20 gate is on
				and		al,0xFE		; don't let them reset CPU
				out		0x92,al		; then let it through
				ret

; VCPI I/O read
; in: EDX=port number ECX=I/O size
; out: EAX/AX/AL=byte value to return
on_vm86_io_read:		cmp		edx,0x64
				jz		.read_8042_controller
				cmp		edx,0x92
				jz		.read_port_A_92h
.unknown_port:			xor		eax,eax		; return 0xFF
				dec		eax
				ret
.read_8042_controller:		in		al,0x64		; let it through
				movzx		eax,al
				ret
.read_port_A_92h:		movzx		eax,byte [fake_port_92]	; return last written value
				ret

; VCPI protected mode entry (32-bit)
vcpi_pm_entry:			pm_int_entry
				call		interrupt_routine_halt_prepare
				mov		edx,67h
				mov		esi,str_unknown_emm_vcpi_pm_call
				jmp		fault_jmp_unhandled			
				pm_int_exit

; AX=0xDE00 VCPI detect
vcpi00_detect:			mov		int_EAX,0	; present
				mov		int_EBX,0x0100	; version v1.0
				int_exit

; AX=0xDE01 VCPI get protected mode interface
vcpi01_get_pm_if:		mov		ax,FLAT32_SEL
				mov		es,ax
; fill page table ES:DI and advance DI += 4096 on return to client
				xor		edi,edi
				mov		di,int_v86_ES
				shl		edi,4
				add		edi,int_EDI			; EDI = (ES<<4)+DI
				add		int_EDI,4096			; client (E)DI += 4
; make fake page table for first 4MB that is 1:1 identity mapping
				mov		ecx,4096>>2
				xor		ebx,ebx
				cld
.pmfill:			lea		eax,[ebx+0x007]			; EAX = PTE: base address + present + read/write, user
				add		ebx,0x1000
				stosd
				loop		.pmfill

; then fill in the GDT table given by the client
				cld
				xor		edi,edi
				mov		di,int_v86_DS
				shl		edi,4
				add		edi,int_ESI			; EDI = (DS<<4)+SI
				; CODE
				lea		esi,[gdt+(CODE32_SEL&0xFFF8)]	; ESI = our code32 GDT
				lodsd						; ****COPY****
				stosd
				lodsd
				stosd
				; DATA (not that it matters)
				lea		esi,[gdt+(DATA32_SEL&0xFFF8)]	; ESI = our data32 GDT
				lodsd						; ****COPY****
				stosd
				lodsd
				stosd
				; DATA (not that it matters)
				lea		esi,[gdt+(FLAT32_SEL&0xFFF8)]	; ESI = our data32 GDT
				lodsd						; ****COPY****
				stosd
				lodsd
				stosd

				mov		int_EAX,0			; OK, no error
				mov		int_EBX,vcpi_pm_entry		; our VCPI entry point
				int_exit

; AX=0xDE03 VCPI get free pages:
vcpi03_free_pages:		mov		int_EAX,0	; OK
				mov		int_EDX,(4*1024*1024)/4096	; just report 4MB and be done with it
				int_exit

; AX=0xDE04 VCPI alloc page:
vcpi04_alloc_page:		mov		ax,FLAT32_SEL
				mov		es,ax
				mov		esi,[vcpi_alloc_bitmap_phys]
				mov		ecx,[vcpi_alloc_bitmap_size]
				cld
.scan:				mov		al,byte [es:esi]
				cmp		al,0xFF
				jnz		.found
				inc		esi
				dec		ecx
				jnz		.scan
.not_found:			mov		int_EAX,0x8800			; not found
				mov		int_EDX,0
				int_exit
.found:				not		al
				xor		ah,ah
				xor		ebx,ebx
				bsf		bx,ax				; BX=# of bit that is zero
				jz		.bmp_error			; ZF=0 or else
				mov		cl,bl
				mov		al,1
				shl		al,cl
				or		byte [es:esi],al
				sub		esi,[vcpi_alloc_bitmap_phys]
				lea		eax,[esi*8+ebx]			; EAX = page number
				shl		eax,12				; EAX = page address
				add		eax,[vcpi_alloc_pages_phys]
				mov		int_EDX,eax			; EDX = physical page #
				mov		int_EAX,0
				int_exit
.bmp_error:			call		interrupt_routine_halt_prepare
				mov		edx,67h
				mov		esi,str_vcpi_alloc_page_bug
				jmp		fault_jmp_unhandled			

; AX=0xDE05 VCPI alloc page:
vcpi05_free_page:		mov		ax,FLAT32_SEL
				mov		es,ax

				mov		edx,int_EDX
				sub		edx,[vcpi_alloc_pages_phys]
				jc		.not_found
				shr		edx,12
				mov		esi,edx
				shr		esi,3
				cmp		esi,[vcpi_alloc_bitmap_size]
				jae		.not_found
				mov		ecx,edx
				and		ecx,7
				mov		al,1
				shl		al,cl
				not		al
				add		esi,[vcpi_alloc_bitmap_phys]
				int		3				; <- FIXME: Remove debug b.p. when you verify this works
				and		[es:esi],al
				int_exit
.not_found:			mov		int_EAX,0x8A00
				mov		eax,0xABCD1234			; <- DEBUG: remove when you verify this works
				int		3
				int_exit

; AX=0xDE06 VCPI get physical address of 4K page
vcpi06_get_phys_addr:		mov		eax,int_ECX
				and		eax,0xFFFF
				shl		eax,12
				mov		int_EDX,eax
				mov		int_EAX,0
				int_exit

; AX=0xDE0A VCPI get Interrupt Vector Mappings
vcpi0A_8259A_mapping:		mov		int_EAX,0
				mov		int_EBX,IRQ_BASE_INT	; 1st vector for master PIC
				mov		int_ECX,IRQ_BASE_INT+8	; 1st vector for slave PIC
				int_exit

; AX=0xDE0C VCPI enter protected mode
vcpi0C_enter_pm:		call		interrupt_routine_halt_prepare
				mov		edx,67h
				mov		esi,str_vcpi_pm_not_supported
				jmp		fault_jmp_unhandled

; AH=0x40 Get Status
emm40_get_status:		mov		int_EAX,0	; we're OK
				int_exit

emm41_get_pfa:			mov		int_EAX,0	; OK
				mov		int_EBX,0xC000	; FIXME: We don't provide ANY pages, just make the VGA BISO the "page frame"
				int_exit

emm42_get_unalloc_page_count:	mov		int_EAX,0	; OK
				mov		int_EBX,0	; no pages free
				mov		int_EDX,0	; no pages at all
				int_exit

emm43_alloc_pages:		mov		int_EAX,0x8700	; not enough memory
				mov		int_EDX,0	; handle=0
				int_exit

emm44_map_handle_page:		mov		int_EAX,0x8A00	; out of range
				int_exit

emm45_dealloc_pages:		mov		int_EAX,0	; uhhhh... OK
				int_exit

emm46_get_version:		mov		int_EAX,0x40	; version 4.0
				int_exit

emm47_save_page_map:		jmp	emm_unhandled
emm48_restore_page_map:		jmp	emm_unhandled
emm4B_get_handle_count:		jmp	emm_unhandled
emm4C_get_handle_pages:		jmp	emm_unhandled
emm4D_get_all_handle_pages:	jmp	emm_unhandled
emm4E_combo:			jmp	emm_unhandled
emm4F_combo:			jmp	emm_unhandled
emm50_combo:			jmp	emm_unhandled
emm51_realloc_pages:		jmp	emm_unhandled
emm52_combo:			jmp	emm_unhandled
emm53_combo:			jmp	emm_unhandled
emm54_combo:			jmp	emm_unhandled
emm55_combo:			jmp	emm_unhandled
emm56_combo:			jmp	emm_unhandled
emm57_combo:			jmp	emm_unhandled

emm58_combo:			mov	eax,int_EAX
				cmp	al,0	
				jz	.func_5800
				jmp	emm_unhandled
.func_5800: ; ========================== INT 67h AX=5800h
				mov	int_EAX,0		; OK
				mov	int_ECX,0		; the returned array is zero entries in length
				int_exit

emm59_combo:			jmp	emm_unhandled
emm5A_combo:			jmp	emm_unhandled
emm5B_combo:			jmp	emm_unhandled
emm5C_prepare_warmboot:		jmp	emm_unhandled
emm5D_combo:			jmp	emm_unhandled


%unmacro int_entry 0
%unmacro int_exit 0
%undef		int_ES
%undef		int_DS
%undef		int_EDI
%undef		int_ESI
%undef		int_EBP
%undef		int_ESP
%undef		int_EBX
%undef		int_EDX
%undef		int_ECX
%undef		int_EAX
%undef		int_INTNUM
%undef		int_INTNUM_b
%undef		int_ERRCODE
%undef		int_EIP_w
%undef		int_EIP
%undef		int_CS
%undef		int_EFLAGS
%undef		int_v86_SS
%undef		int_v86_ES
%undef		int_v86_DS
%undef		int_v86_FS
%undef		int_v86_GS

; ========== FAULT COLLECTION ROUTINE. SS:ESP should point to fault. If the exception does not push an error code,
;    then the caller must push a dummy error code
; if privilege escalation was involved (stack switching) then retrieve SS:ESP at fault from the stack frame.
; else retrieve from actual SS:ESP registers
fault_jmp_unhandled:
		jmp		CODE16_SEL:.thunk16
		bits		16
.thunk16:	mov		ax,DATA16_SEL
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		sp,stack_init
		jmp		unhandled_fault_errcode
		bits		32

; ============= cleanup, exit to DOS (from 32-bit protected mode)
_exit_from_prot32:
		jmp		CODE16_SEL:.entry16
		bits		16
.entry16:	mov		ax,DATA16_SEL
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		ss,ax
		mov		esp,stack_init
		
; ============= cleanup, exit to DOS (from 16-bit protected mode)
_exit_from_prot16:
		mov		ax,DATA16_SEL
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		ss,ax

		; overwrite the far jmp's segment value
		mov		ax,[my_realmode_seg]
		mov		word [.real_hackme+3],ax
		mov		esp,stack_init

		mov		eax,0x00000010
		mov		cr0,eax

		lidt		[idtr_real]
		lgdt		[gdtr_real]

.real_hackme:	jmp		0:.real_entry
.real_entry:	mov		ax,[cs:my_realmode_seg]
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		ss,ax
		mov		sp,stack_init

; reprogram the PIC back to what normal DOS expects: IRQ 0-7 => INT 8-15
		mov		al,0x10			; ICW1 A0=0
		out		20h,al
		mov		al,0x08			; ICW2 A0=1
		out		21h,al
		mov		al,0x04			; ICW3 A0=1 slave on IRQ 2
		out		21h,al

		mov		al,0x10			; ICW1 A0=0
		out		0xA0,al
		mov		al,0x70			; ICW2 A0=1
		out		0xA1,al
		mov		al,0x02			; ICW3 A0=1
		out		0xA1,al

; remove our INT 66h API
		xor		ax,ax
		mov		es,ax
		mov		word [es:(RM_INT_API*4)],ax
		mov		word [es:(RM_INT_API*4)+2],ax

; free HIMEM.SYS blocks
		test		word [himem_sys_buffer_handle],0
		jz		.no_himem_sys
		mov		ah,0Dh			; HIMEM.SYS function 0Dh unlock memory block
		mov		dx,word [himem_sys_buffer_handle]
		call far	word [himem_sys_entry]
		mov		ah,0Ah			; HIMEM.SYS function 0Ah free memory block
		mov		dx,word [himem_sys_buffer_handle]
		call far	word [himem_sys_entry]
.no_himem_sys:

; if we already exited as a TSR...
		test		byte [i_am_tsr],1
		jnz		.tsr_exit

; time to exit to DOS
		mov		dx,str_exit_to_dos
		jmp		_exit_with_msg

; ============= ALTERNATE EXIT IF WE ALREADY EXITED AS TSR
.tsr_exit:	
		mov		ax,cs
		mov		es,ax			; ES = our code segment which is also our PSP segment
		mov		ah,0x49			; function 49h free memory block
		clc
		int		21h
		jnc		.tsr_exit_free_ok
		mov		dx,str_cannot_free_self
		call		dos_puts
.tsr_exit_free_ok:
		cli
		mov		ax,[cs:unload_int_stk+0]	; offset
		add		ax,6				; discard prior frame
		mov		sp,ax
		mov		ax,[cs:unload_int_stk+2]	; segment
		mov		ss,ax

		mov		dx,str_exit_to_dos
		call		dos_puts

		cmp		word [himem_sys_buffer_handle],0 ; if there is a handle to free, then do it
		jz		.no_handle
		mov		ah,0Dh			; HIMEM.SYS function 0Dh unlock memory block
		mov		dx,word [himem_sys_buffer_handle]
		call far	word [himem_sys_entry]
		mov		ah,0Ah			; HIMEM.SYS function 0Ah free memory block
		mov		dx,word [himem_sys_buffer_handle]
		call far	word [himem_sys_entry]
.no_handle:

		cli

		mov		al,020h

		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al

		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al

		in		al,61h
		in		al,61h
		in		al,61h
		in		al,61h

		sti

		jmp far		word [cs:unload_int_ret]

; ============= UNHANDLED FAULT HANDLER (16-bit code)
;   input: EDX = Number of interrupt
;          SI = Textual string of fault
;                  
unhandled_fault_errcode:
		cli
		mov		ax,DATA16_SEL
		mov		ds,ax

		mov		ax,[cs:my_realmode_seg]
		mov		word [.real16jmp+3],ax

		mov		ax,FLAT16_SEL
		mov		ds,ax
		mov		es,ax
		mov		ss,ax

		lgdt		[cs:gdtr_real]
		lidt		[cs:idtr_real]

		; crash-thunk to real mode
		mov		eax,0x00000010
		mov		cr0,eax
.real16jmp:	jmp		0:.real16
.real16:	mov		ax,[cs:my_realmode_seg]
		mov		ds,ax
		mov		ss,ax
		xor		ax,ax
		mov		es,ax

		mov		ax,3
		int		10h

		cld
		mov		ax,0x4E20
		mov		ecx,80*25
		mov		edi,0xB8000
		a32 rep		stosw

		; print exception name on screen
		mov		edi,0xB8000
		call		.unhandled_print
		mov		al,' '		; +space plus AH still contains upper byte from .unhandled_print
		a32 stosw

		; then the number (in EDX) write to DS:DI
		mov		eax,edx
		push		edi
		mov		edi,scratch_str
		call		eax_to_hex_16_dos
		lea		si,[di+6]	; only the last two hex digits
		pop		edi
		call		.unhandled_print

		; print the registers.
		; during this loop: SI = print list  EDI = location on screen to draw   [ESP] = location on screen of row start
		mov		edi,0xB8000+(160*2)	; two lines down
		push		edi
		mov		si,printlist_32
.regprint32:	lodsw
		or		ax,ax
		jz		.regprint32e		; AX=0 STOP
		dec		ax
		jz		.regprint32nl		; AX=1 GO TO NEW LINE
		push		si
		mov		si,ax			; SI=AX=address of variable name
		inc		si
		call		.unhandled_print
		pop		si
		mov		ax,0x4E00 | (':')
		a32 stosw
		lodsw					; SI=address of variable
		push		si
		mov		si,ax
		mov		eax,[si]
		push		edi
		mov		edi,scratch_str
		call		eax_to_hex_16_dos
		mov		esi,edi
		pop		edi
		call		.unhandled_print
		pop		si
		mov		ax,0x4E00 | (' ')
		a32 stosw
		jmp		.regprint32
.regprint32nl:	pop		edi
		add		edi,160			; move to next line, save back to [ESP]
		push		edi
		jmp		.regprint32
.regprint32e:	pop		edi

		add		edi,160			; next line...

		mov		si,printlist_16
.regprint16:	lodsw
		or		ax,ax
		jz		.regprint16e		; AX=0 STOP
		dec		ax
		jz		.regprint16nl		; AX=1 GO TO NEW LINE
		push		si
		mov		si,ax			; SI=AX=address of variable name
		inc		si
		call		.unhandled_print
		pop		si
		mov		ax,0x4E00 | (':')
		a32 stosw
		lodsw					; SI=address of variable
		push		si
		mov		si,ax
		xor		eax,eax
		mov		ax,[si]
		push		edi
		mov		edi,scratch_str
		call		eax_to_hex_16_dos
		lea		esi,[edi+4]
		pop		edi
		call		.unhandled_print
		pop		si
		mov		ax,0x4E00 | (' ')
		a32 stosw
		jmp		.regprint16
.regprint16nl:	pop		edi
		add		edi,160			; move to next line, save back to [ESP]
		push		edi
		jmp		.regprint16
.regprint16e:	mov		si,str_mode_prot	; CPU mode
		test		dword [unhandled_fault_var_eflags],0x20000
		jz		.regprint_cpu_mode_not_v86
		mov		si,str_mode_v86
.regprint_cpu_mode_not_v86:
		call		.unhandled_print
		pop		edi

		mov		al,020h

		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al

		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al

		sti
		jmp		short $
; ===== print on screen from DS:SI to ES:EDI
.unhandled_print:
		lodsb
		cmp		al,'$'
		jz		.unhandled_printe
		mov		ah,0x4E
		a32 stosw
		jmp		.unhandled_print
.unhandled_printe:
		ret

; ============= Entry point (virtual 8086 mode)
vm86_entry:	mov		ax,ds		; *DEBUG*
		mov		ss,ax

		cli				; make sure the v86 monitor handles CLI
		sti				; ...and STI
		pushf				; ...and PUSHF
		popf				; ...and POPF
		pushfd				; ...32-bit PUSHF
		popfd				; ...32-bit POPF
		in		al,21h		; ...IN?
		out		21h,al		; ...OUT?

		; can I clear vm86 mode by clearing the VM bit?
		pushfd
		pop		eax
		and		eax,~0x20000
		push		eax
		popfd

		; NOW MAKE SURE PUSHF/POPF STORE THE VALUE ON-STACK LIKE THEY'RE SUPPOSED TO
		mov		bx,sp
		mov		word [ss:bx-2],0x5A5A
		pushf
		mov		bx,sp
		cmp		word [ss:bx],0x5A5A
		jnz		.pushf_ok	; if the value DIDN'T CHANGE then the monitor failed to write FLAGS to stack
		mov		ax,0
		jmp		vm86_errcode
.pushf_ok:

		; DOES POPF WORK?
		mov		ax,0x492
		push		ax
		popf
		pushf
		pop		ax
		and		ax,0xFD6
		cmp		ax,0x492
		jz		.popf_ok
		mov		ax,1
		jmp		vm86_errcode
.popf_ok:

		; TEST 32-bit PUSHF
		mov		bx,sp
		mov		dword [ss:bx-4],0x5A5A5A5A
		pushfd
		mov		bx,sp
		cmp		dword [ss:bx],0x5A5A5A5A
		jnz		.pushfd_ok	; if the value DIDN'T CHANGE then the monitor failed to write FLAGS to stack
		mov		ax,2
		jmp		vm86_errcode
.pushfd_ok:

		; DOES POPFD WORK?
		mov		eax,0x492
		push		eax
		popfd
		pushfd
		pop		eax
		and		eax,0xFD6
		cmp		eax,0x492
		jz		.popfd_ok
		mov		ax,3
		jmp		vm86_errcode
.popfd_ok:

		; IF I CLEAR INTERRUPT (CLI) AND THEN EXECUTE AN INTERRUPT, DOES IT COME BACK ENABLED?
		cli
		mov		ah,0x0F			; INT 10 AH=0x0F which has no visisible effect
		int		10h
		pushf
		pop		ax
		test		ax,0x200
		jz		.int_doesnt_enable
		mov		ax,4
		jmp		vm86_errcode
.int_doesnt_enable:

		; HELLO WORLD!
		mov		si,str_vm86_hello
		call		bios_puts

		; TEST DEFERRED IRQ MECHANISM BY DELIBERATLEY HALTING FOR AWHILE
		cli
		mov		ecx,0x1000000		; delibrate slow countdown loop
.l1:		dec		ecx
		jnz		.l1
		sti

		; for my next trick, I will exit to DOS as a TSR
		; and allow the user to run the whole DOS kernel this way :)
		mov		es,[cs:0x2C]		; locate our environment block and free it
		mov		ah,0x49			; function 49h free memory block
		int		21h
		jnc		.env_free_ok
		mov		ax,4
		jmp		vm86_errcode
.env_free_ok:	mov		word [cs:0x2C],0	; rub out the ENV block

		; setup our INT 66h API
		xor		ax,ax
		mov		es,ax
		mov		word [es:(RM_INT_API*4)],realmode_api_entry
		mov		ax,cs
		mov		word [es:(RM_INT_API*4)+2],ax

		; setup our INT 67h vector
		; REMINDER: On exit you need to restore this vector
		mov		ax,cs
		mov		bx,rm_int_67_entry
		shr		bx,4
		add		ax,bx
		mov		word [es:(0x67*4)+2],ax
		xor		ax,ax
		mov		word [es:(0x67*4)+0],ax

		; finally, terminate and stay resident
		mov		byte [i_am_tsr],1
		mov		edx,the_end		; DX = memory in paragraphs to save
		add		edx,15
		shr		edx,4
		add		edx,0x11		; <-- FIXME: IS THIS NECESSARY?
		mov		ah,0x31			; function 31h terminate and stay resident
		int		21h

; ============= "Secret Handshake" to exit back into the v86 monitor and shutdown the program (virtual 8086 mode)
; TODO: Remove this, call into RM_INT_API instead
vm86_exit:	mov		eax,0xAABBAABB
		int		RM_INT_API
		hlt

; ============= If any of our self-test fails, we draw DIRECTLY ON VGA RAM and hike back into the vm86 monitor ASAP.
;   if self-tests fail chances are calling the BIOS/DOS will cause major problems. AX=CODE
vm86_errcode:	mov		bx,0xB800
		mov		es,bx
		and		ax,0xF
		or		ax,0x4E30	; AX = VGA alphanumeric code for that number
		mov		[es:160],ax
		jmp		vm86_exit

; ============= Real-mode API entry (reflect to v86 monitor by executing an INT)
;    this would allow the trick to work even for programs that direct-call instead
realmode_api_entry:
		int		RM_INT_API
		iret

; ============= Parse command line (from PSP segment)
parse_argv:	cld
		mov		si,81h
.scan:		lodsb
		or		al,al
		jz		.done
		cmp		al,0Dh
		jz		.done
		cmp		al,20h
		jz		.scan
		cmp		al,'-'
		jz		.switch
		cmp		al,'/'
		jz		.switch
		; FALL THROUGH WITH ZF=0 to return
.done:		ret
		; AT THIS POINT: SI = just after the / or - in the switch
.switch:	lodsb
		cmp		al,'?'
		jz		.help
		cmp		al,'A'
		jb		.unknown_switch
		cmp		al,'Z'
		ja		.unknown_switch
		; the A-Z switches are allowed to have "=NNNN" after them where N is some integer in hex or decimal
		sub		al,'A'
		mov		bl,al
		xor		bh,bh		; BX = index into lookup table
		add		bx,bx
		jmp		word [bx+.switch_az]
.fail:		mov		al,1
.help:		or		al,al		; AL != 0 => ZF=0
		ret
.unknown_switch:mov		dx,str_unknown_switch
		call		dos_puts
		lea		dx,[si-2]	; step back two chars
		mov		byte [si],'$'
		call		dos_puts
		mov		dx,str_crlf
		call		dos_puts
		jmp		.fail
; ========== Switches CALL here if they need a numeric value to follow.
; returns to caller if so, parsing as 16-bit integer returned in EAX. Else,
; it discards the return address and jumps to the 'needs param' error message.
.switch_needs_equ_check:
		cmp		byte [si],'='
		jnz		.switch_needs_equ_check_fail
		inc		si
		cli
		xor		eax,eax
		call		ax_strtol_16
		ret
.switch_needs_equ_check_fail:
		add		sp,2		; fall through
.switch_needs_equ:
		mov		dx,str_needs_equals
		call		dos_puts
		jmp		.fail
; ========== /B=<number>
.switch_buffer_size:
		call		.switch_needs_equ_check
		shl		eax,10
		mov		[himem_sys_buffer_size],eax
		jmp		.scan
; ========== /U
.switch_unload:	mov		byte [user_req_unload],1
		jmp		.scan
; ========== /I
.switch_iopl:	mov		byte [user_req_iopl],0
		jmp		.scan
; ========== /C
.switch_cache_disable:
		or		dword [cr0_more],0x40000000
		jmp		.scan
; ========== /W
.switch_writeback_disable:
		or		dword [cr0_more],0x20000000
		jmp		.scan
; switch A-Z jump table
.switch_az:	dw		.unknown_switch			; /A
		dw		.switch_buffer_size		; /B=<number>
		dw		.switch_cache_disable		; /C
		dw		.unknown_switch			; /D
		dw		.unknown_switch			; /E
		dw		.unknown_switch			; /F
		dw		.unknown_switch			; /G
		dw		.unknown_switch			; /H
		dw		.switch_iopl			; /I
		dw		.unknown_switch			; /J
		dw		.unknown_switch			; /K
		dw		.unknown_switch			; /L
		dw		.unknown_switch			; /M
		dw		.unknown_switch			; /N
		dw		.unknown_switch			; /O
		dw		.unknown_switch			; /P
		dw		.unknown_switch			; /Q
		dw		.unknown_switch			; /R
		dw		.unknown_switch			; /S
		dw		.unknown_switch			; /T
		dw		.switch_unload			; /U
		dw		.unknown_switch			; /V
		dw		.switch_writeback_disable	; /W
		dw		.unknown_switch			; /X
		dw		.unknown_switch			; /Y
		dw		.unknown_switch			; /Z

; register print list
printlist_32:	dw		str_eax,	unhandled_fault_var_eax
		dw		str_ebx,	unhandled_fault_var_ebx
		dw		str_ecx,	unhandled_fault_var_ecx
		dw		str_edx,	unhandled_fault_var_edx
		dw		str_esi,	unhandled_fault_var_esi
		dw		str_edi,	unhandled_fault_var_edi
		dw		1
		dw		str_ebp,	unhandled_fault_var_ebp
		dw		str_esp,	unhandled_fault_var_esp
		dw		str_eip,	unhandled_fault_var_eip
		dw		str_eflags,	unhandled_fault_var_eflags
		dw		str_errcode,	unhandled_fault_var_errcode
		dw		str_cr0,	unhandled_fault_var_cr0
		dw		1
		dw		str_cr3,	unhandled_fault_var_cr3
		dw		str_cr4,	unhandled_fault_var_cr4
		dw		str_opcode,	unhandled_fault_var_opcode
		dw		0
printlist_16:	dw		str_cs,		unhandled_fault_var_cs
		dw		str_ds,		unhandled_fault_var_ds
		dw		str_es,		unhandled_fault_var_es
		dw		str_fs,		unhandled_fault_var_fs
		dw		str_gs,		unhandled_fault_var_gs
		dw		str_ss,		unhandled_fault_var_ss
		dw		0

; ============= bios_puts (print $-terminated string at DS:SI)
bios_puts:	cli
		cld
		push		ax
		push		bx
.putsloop:	lodsb
		cmp		al,'$'
		jz		.putsend
		mov		ah,0x0E
		xor		bx,bx
		int		10h
		jmp		.putsloop
.putsend:	pop		bx
		pop		ax
		ret

; ============= dos_puts (print $-terminated string at DS:DX)
dos_puts:	mov		ah,09h
		int		21h
		ret

; ============= read one digit from DS:SI return in AX (16-bit code)
ax_strtol_16_single:mov		al,[si]
		cmp		al,'0'
		jb		.no
		cmp		al,'9'
		ja		.no
		sub		al,'0'
		xor		ah,ah
		inc		si
		clc
		ret
.no:		stc
		ret

; ============= read from DS:SI and convert numerical string to integer value return in AX (16-bit code)
ax_strtol_16:	xor		cx,cx
.loop:		push		cx
		call		ax_strtol_16_single
		pop		cx
		jc		.done
		mov		bx,cx
		add		bx,bx
		shl		cx,3		; BX = CX * 2,  CX *= 8
		add		cx,bx		; CX = (CX * 8) + (CX * 2) = CX * 10
		add		cx,ax		; CX += new digit
		jmp		.loop
.done:		mov		ax,cx
		ret

; ============= take AX and write to buffer (DS:SI) as hexadecimal string (16-bit code)
al_to_hex_16_dos:mov		byte [di+2],'$'
		jmp		al_to_hex_16
al_to_hex_16_nul:mov		byte [di+2],0
al_to_hex_16:	push		di
		push		bx
		push		ax
		xor		bh,bh
		mov		ah,al
		and		al,0xF
		mov		bl,al
		mov		al,[bx+str_hex]		; AL' = str_hex[al]
		shr		ah,4
		mov		bl,ah
		mov		ah,[bx+str_hex]		; AH' = str_hex[ah]
		mov		[di+0],ah
		mov		[di+1],al
		pop		ax
		pop		bx
		pop		di
		ret

; ============= take AX and write to buffer (DS:SI) as hexadecimal string (16-bit code)
ax_to_hex_16_dos:mov		byte [di+4],'$'
		jmp		ax_to_hex_16
ax_to_hex_16_nul:mov		byte [di+4],0
ax_to_hex_16:	push		di
		push		ax
		mov		al,ah
		call		al_to_hex_16
		pop		ax
		add		di,2
		call		al_to_hex_16
		pop		di
		ret

; ============= take EAX and write to buffer (DS:DI) as hexadecimal string (16-bit code)
eax_to_hex_16_dos:mov		byte [di+8],'$'
		jmp		eax_to_hex_16
eax_to_hex_16_nul:mov		byte [di+8],0
eax_to_hex_16:	push		di
		push		eax
		shr		eax,16
		call		ax_to_hex_16
		pop		eax
		add		di,4
		call		ax_to_hex_16
		pop		di
		ret

; ============= /U Unloading the resident copy of this program
unload_this_program:
		smsw		ax
		test		al,1
		jnz		.v86_active
		mov		dx,str_not_loaded
		jmp		_exit_with_msg
.v86_active:
		xor		ax,ax
		mov		es,ax
		mov		bx,[es:(RM_INT_API*4)]
		or		cx,[es:(RM_INT_API*4)+2]
		cmp		cx,0		; if pointer is 0000:0000
		jz		.v86_not_me
		mov		eax,0xAABBAA55
		int		RM_INT_API
		cmp		eax,0xBBAABB33
		jnz		.v86_not_me
.v86_is_me:	mov		ax,cs
		mov		ds,ax
		mov		es,ax
		mov		fs,ax
		mov		gs,ax
		mov		dx,str_removing_self
		call		dos_puts
; instruct it to remove itself
		mov		eax,0xAABBAABB
		int		RM_INT_API
; exit, having done our job
		mov		dx,str_crlf
		call		dos_puts
		mov		dx,str_unloaded
		jmp		_exit_with_msg
.v86_not_me:	mov		dx,str_v86_but_not_me
		jmp		_exit_with_msg

; ============= DATA: THESE EXIST IN THE .COM BINARY IMAGE
		section		.data align=2
himem_sys_buffer_size:dd	(256*1024)	; DWORD [amount of extended memory to allocate]
cr0_more:	dd		0x00000000	; cache disable, not write through
str_require_386:db		'386 or higher required$'
str_removing_self:db		'Removing from memory',13,10,'$'
str_v86_detected:db		'Virtual 8086 mode already active$'
str_v86_but_not_me:db		'Virtual 8086 active, and its not me$'
str_not_loaded:	db		'Not resident in memory$'
str_cannot_free_self:db		'Cannot free self from memory$'
str_need_himem_sys:db		'HIMEM.SYS not installed$'
str_himem_a20_error:db		'HIMEM.SYS failed to enable A20$'
str_himem_alloc_err:db		'Unable to alloc extended memory$'
str_himem_lock_err:db		'Unable to lock extended memory$'
str_buffer_too_small:db		'Buffer too small$'
str_buffer_too_large:db		'Buffer too large$'
str_unloaded:	db		'Unloaded',13,10,'$'
str_buffer_at:	db		'Buffer at: $'
str_crlf:	db		13,10,'$'
str_hex:	db		'0123456789ABCDEF'
str_help:	db		'V86KERN [options]',13,10
		db		'Demonstration Virtual 8086 kernel/monitor',13,10
		db		13,10
		db		'Options start with - or /',13,10
		db		'  /?      Show this help',13,10
		db		'  /B=...  Set buffer size (in KB)',13,10
		db		'  /U      Unload the kernel',13,10
		db		'  /I      Run with IOPL=3 (trap CLI/STI/etc)',13,10
		db		'  /C      Disable memory cache',13,10
		db		'  /W      Disable write-back cache',13,10
		db		'$'
str_unknown_switch:db		'Unknown switch $'
str_needs_equals:db		'Switch missing =...$'
str_eax:	db		'EAX$'
str_ebx:	db		'EBX$'
str_ecx:	db		'ECX$'
str_edx:	db		'EDX$'
str_esi:	db		'ESI$'
str_edi:	db		'EDI$'
str_ebp:	db		'EBP$'
str_esp:	db		'ESP$'
str_eip:	db		'EIP$'
str_errcode:	db		'ERR$'
str_eflags:	db		'FLG$'
str_cr0:	db		'CR0$'
str_cr3:	db		'CR3$'
str_cr4:	db		'CR4$'
str_opcode:	db		'OPC$'
str_cs:		db		'CS$'
str_ds:		db		'DS$'
str_es:		db		'ES$'
str_fs:		db		'FS$'
str_gs:		db		'GS$'
str_ss:		db		'SS$'
str_mode_prot:	db		'Protected mode$'
str_mode_v86:	db		'Virtual 8086 mode$'
str_vm86_hello:	db		'This text was printed by the Virtual 8086 mode component of this program',13,10,'$'
str_fault_0x00:	db		'Divide by Zero$'
str_fault_0x01:	db		'Debug$'
str_fault_0x02:	db		'NMI$'
str_fault_0x03:	db		'Breakpoint$'
str_fault_0x04:	db		'Overflow$'
str_fault_0x05:	db		'Boundary Check$'
str_fault_0x06:	db		'Invalid Opcode$'
str_fault_0x07:	db		'Coprocessor N/A$'
str_fault_0x08:	db		'Double Fault$'
str_fault_0x09:	db		'Coprocessor Segment Overrun$'
str_fault_0x0A:	db		'Invalid TSS$'
str_fault_0x0B:	db		'Segment Not Present$'
str_fault_0x0C:	db		'Stack Fault$'
str_fault_0x0D:	db		'General Protection Fault$'
str_fault_0x0E:	db		'Page Fault$'
str_fault_0x0F:	db		'Exception F$'
str_fault_0x10:	db		'FPU Error$'
str_fault_0x11:	db		'Alignment Check$'
str_fault_0x12:	db		'Machine Check$'
str_fault_0x13:	db		'SIMD/SSE Exception$'
str_fault_unknown:db		'Unknown exception$'
str_v86_unknown:db		'Unknown instruction in v86 mode$'
str_v86_hlt_cli:db		'v86 halt with interrupts disabled$'
str_v86_secret:	db		'Inappropriate use of v86 secret handshake$'
str_exit_to_dos:db		'Shutdown successful, exiting to DOS$'
str_unknown_irq:db		'Unknown IRQ$'
str_irq_deferred:db		'Deferred IRQ$'
str_irq_1:	db		'IRQ #1$'
str_stack_overflow:db		'Stack overflow$'
str_himem_corruption:db		'Extended memory corruption$'
str_unexpected_int_not_v86:db	'Unexpected: IRQ not from within v86$'
str_unknown_emm_vcpi_call:db	'Unknown EMM/VCPI call$'
str_unknown_emm_vcpi_pm_call:db	'Unknown VCPI entry point call$'
str_vcpi_alloc_page_bug:db	'VCPI page allocation bug check$'
str_vcpi_pm_not_supported:db	'VCPI protected mode not implemented$'

; ============= EMM/VCPI entry point redirection and "signature"
		align		16
rm_int_67_entry:mov		ah,0x84		; AH=0x84 means "not defined"
		iret
		nop
		nop
		nop
		nop
		nop
		nop
		nop
rm_int_67_sig:	db		'EMMXXXX0',0	; DOS programs look for this signature relative to INT 67h segment

; ============= VARIABLES: THESE DO NOT EXIST IN THE .COM FILE THEY EXIST IN MEMORY FOLLOWING THE BINARY IMAGE
		section		.bss align=2
		align		8
; ---------------------- STACK
stack_base:	resb		4096		; char[4096+4]
stack_init:	resd		1		; DWORD
stack_top:
scratch_str:	resb		64		; char[64]
; ---------------------- STACK
stack_base_vm86:resb		4096		; char[4096+4]
stack_init_vm86:resd		2		; DWORD
stack_top_vm86:
; ---------------------- HIMEM.SYS state
himem_sys_entry:resd		1		; FAR POINTER
himem_sys_buffer_phys:resd	1		; DWORD [physical memory address]
himem_sys_buffer_handle:resw	1		; WORD [HIMEM.SYS handle]
; ---------------------- my real mode segment
my_realmode_seg:resw		1		; WORD
my_phys_base:	resd		1		; DWORD
tss_phys_base:	resd		1		; DWORD base logical address of TSS
tss_vm86_phys_base:resd		1		; DWORD base logical address of TSS
v86_raw_opcode:	resd		1		; DWORD
; ---------------------- GDTR/IDTR
gdtr_pmode:	resq		1		; LIMIT. BASE
gdtr_real:	resq		1		; LIMIT, BASE
idtr_pmode:	resq		1		; LIMIT, BASE
idtr_real:	resq		1		; LIMIT, BASE
; ---------------------- STATE
irq_pending:	resw		1
user_req_unload:resb		1
v86_if:		resb		1
; ---------------------- GLOBAL DESCRIPTOR TABLE
		resd		1
		align		8
gdt:		resq		(MAX_SEL/8)	; 16 GDT entries
; ---------------------- INTERRUPT DESCRIPTOR TABLE
		align		8
idt:		resq		256		; all 256
; ---------------------- STATE
user_req_iopl:	resb		1
i_am_tsr:	resb		1
unload_int_ret:	resd		1
unload_int_stk:	resd		1
; ---------------------- VCPI allocation map and pages
vcpi_alloc_bitmap_phys:resd		1
vcpi_alloc_bitmap_size:resd		1
vcpi_alloc_pages_phys:resd		1
; ---------------------- WHEN DISPLAYING THE UNHANDLED FAULT DIALOG
unhandled_fault_var_errcode:resd	1
unhandled_fault_var_eax:resd		1
unhandled_fault_var_ebx:resd		1
unhandled_fault_var_ecx:resd		1
unhandled_fault_var_edx:resd		1
unhandled_fault_var_esi:resd		1
unhandled_fault_var_edi:resd		1
unhandled_fault_var_ebp:resd		1
unhandled_fault_var_esp:resd		1
unhandled_fault_var_eip:resd		1
unhandled_fault_var_eflags:resd		1
unhandled_fault_var_cr0:resd		1
unhandled_fault_var_cr3:resd		1
unhandled_fault_var_cr4:resd		1
unhandled_fault_var_cs:resw		1
unhandled_fault_var_ds:resw		1
unhandled_fault_var_es:resw		1
unhandled_fault_var_fs:resw		1
unhandled_fault_var_gs:resw		1
unhandled_fault_var_ss:resw		1
unhandled_fault_var_opcode:resd		1
; ---------------------- TSS
tss_main:	resb		128
; ---------------------- VM86 TSS
tss_vm86:	resb		8192+128
; ---------------------------------------------------------------------
;                       END POINTER
; ---------------------------------------------------------------------
padding:	resq		2		; SAFETY PADDING
the_end:
