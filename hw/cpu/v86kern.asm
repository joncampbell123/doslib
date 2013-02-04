; v86kern.asm
;
; Test program: Proof-of-concept minimalist virtual 8086 "monitor"
; (C) 2010-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>
;
; MODE: 16-bit real mode MS-DOS .COM executable
; *THIS CODE IS OBSOLETE*
;       Assumes DS == ES == SS

; NOTES:
;   - This works... for the most part.
;   - Somehow this works even with DOSBox's funky ROM-based interrupt emulation
;   - The emulation provided is sufficient for real-mode exceptions including INT 3h debug and
;     INT 1h trace. It also handles the correct exception to permit v86 programs to use the
;     FPU if present.
; FIXME:
;   - Privileged instructions like mov cr0,<reg> trigger an exception and this program makes no
;     attempt to emulate those instructions.
;   - This code makes no attempt to emulate the LDT manipulation that most BIOS implementations
;     apparently like to do when handling INT 15H extended memory copy. Programs that use extended
;     memory via HIMEM.SYS or via INT 15H will crash.
;   - For reasons unknown to me, running this under Windows 95 pure DOS mode is crashy. It will run
;     for awhile but eventually, things will hang. Under QEMU, running another program or a 3rd
;     will trigger a sudden reset, which mirrors behavior seen on an actual Pentium system. For
;     other unknown reasons, Bochs and DOSBox run this code just fine.
;   - Whatever the BIOS does in response to CTRL+ALT+DEL it doesn't work well when we are active.
;
; This code manages virtual 8086 mode in a suboptimal way. A better implementation is provided in
; v86kern2.asm
;
; FIXME: Okay now this is crashing. Why?

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
IRQ_BASE_INT	equ		0x68
RM_INT_API	equ		0x66

; Extensible virtual 8086 mode kernel for DOS
; (C) 2011 Jonathan Campbell

		bits		16
		section		.code
		[map		v86kern.map]
		org		0x100

; ============= ENTRY POINT
		mov		ax,cs
		mov		word [my_realmode_seg],ax
		mov		bp,stack_base
		mov		sp,stack_init			; SP is normally at 0xFFF0 so move it back down
		mov		word [himem_sys_buffer_handle],0
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
		mov		dx,str_need_himem_sys
		jmp		_exit_with_msg
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

; choose where things go within the buffer
;        = 104 bytes for main TSS
		mov		eax,[himem_sys_buffer_phys]
		mov		[tss_phys_base],eax
		add		eax,128
;        = 8192+104 bytes for VM86 TSS
		mov		[tss_vm86_phys_base],eax
		add		eax,8192+128
;        = 4096 for kernel 32 stack
		mov		[kern32_stack_base],eax
		add		eax,4096
		lea		ebx,[eax-8]
		mov		[kern32_stack_top],ebx
;        = store it for future allocation
		mov		[buffer_alloc],eax

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
		mov		eax,1
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
		mov		eax,[kern32_stack_top]			; TSS+0x04 = ESP for CPL0
		sub		eax,[my_phys_base]
		stosd
		mov		eax,DATA32_SEL				; TSS+0x08 = SS for CPL0
		stosd
		mov		eax,[kern32_stack_top]			; TSS+0x0C = ESP for CPL1
		sub		eax,[my_phys_base]
		stosd
		mov		eax,DATA32_SEL				; TSS+0x10 = SS for CPL1
		stosd
		mov		eax,[kern32_stack_top]			; TSS+0x14 = ESP for CPL2
		sub		eax,[my_phys_base]
		stosd
		mov		eax,DATA32_SEL				; TSS+0x18 = SS for CPL2
		stosd
		xor		eax,eax					; TSS+0x1C = CR3
		stosd
		mov		eax,vm86_entry				; TSS+0x20 = EIP
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
		mov		eax,stack_init_vm86			; TSS+0x38 = ESP
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
		mov		eax,[kern32_stack_top]			; TSS+0x04 = ESP for CPL0
		sub		eax,[my_phys_base]
		stosd
		mov		eax,DATA32_SEL				; TSS+0x08 = SS for CPL0
		stosd
		mov		eax,[kern32_stack_top]			; TSS+0x0C = ESP for CPL1
		sub		eax,[my_phys_base]
		stosd
		mov		eax,DATA32_SEL				; TSS+0x10 = SS for CPL1
		stosd
		mov		eax,[kern32_stack_top]			; TSS+0x14 = ESP for CPL2
		sub		eax,[my_phys_base]
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
		mov		eax,stack_init				; TSS+0x38 = ESP
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
		mov		eax,(104 << 16)				; TSS+0x64 = I/O map base
		stosd
		xor		eax,eax
		mov		ecx,8192 >> 2				; TSS+0x68 = I/O permission map (pre-set to all open)
		rep		stosd

; set up the IDT
		cld

		mov		ecx,0x100
		mov		edi,idt
.idtdef:	mov		ax,fault_no_int		; no interrupt assigned procedure
		stosw					; base[15:0]
		mov		ax,CODE32_SEL
		stosw
		mov		ax,0x8E00		; DPL=3
		stosw
		xor		ax,ax
		stosw
		loop		.idtdef

		mov		esi,fault_routines
		mov		ecx,0x20
		mov		edi,idt
.idtsetup:	lodsw
		stosw					; base[15:0]
		mov		ax,CODE32_SEL
		stosw
		mov		ax,0x8E00		; DPL=3
		stosw
		xor		ax,ax
		stosw
		loop		.idtsetup

		cld
		mov		esi,irq_routines
		mov		ecx,0x10
		mov		edi,idt + (IRQ_BASE_INT*8)
.idtsetup2:	lodsw
		stosw					; base[15:0]
		mov		ax,CODE32_SEL
		stosw
		mov		ax,0x8E00		; you must set DPL=3
		stosw
		xor		ax,ax
		stosw
		loop		.idtsetup2

; next we need to reprogram the PIC so that IRQ 0-7 do not conflict with the CPU exceptions.
; note for stability we only reprogram first PIC, since we do not relocate the 2nd PIC.
		mov		al,0x10			; ICW1 A0=0
		out		20h,al
		mov		al,IRQ_BASE_INT		; ICW2 A0=1
		out		21h,al
		mov		al,0x04			; ICW3 A0=1 slave on IRQ 2
		out		21h,al

; jump into virtual 8086 mode
		jmp		TSS_VM86_SEL:0

; =============== IRQ handler code

int_rm_map:	db		0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F
		db		0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77

irq_priority:	db		0,1,8,9,10,11,12,13,14,15,2,3,4,5,6,7
	; ^ 16 entries, reflecting IRQ 8-15 -> IRQ 2 (slave PIC) and IRQ 0-7 (master PIC) cascade

; EAX = IRQ that fired
;  [ESP+0] = old EAX
;  [ESP+4] = EIP
;  [ESP+8] = CS
;  [ESP+12] = EFLAGS
;  [ESP+16] = ESP
;  [ESP+20] = SS
;  [ESP+24] = ES
;  [ESP+28] = DS
;  [ESP+32] = FS
;  [ESP+36] = GS
irq_general:	test		dword [esp+12],0x20000	; did the interrupt happen while in v86 mode?
		jnz		.reflect_v86
; this happened while NOT in v86 mode. We should still reflect it back to v86 mode.
;   ----------------------TODO--------------------
		call		fault_collect_regs
		mov		edx,0x00
		mov		esi,str_irq_1
		jmp		fault_jmp_unhandled
;   ----------------------------------------------
; CPU was in v86 mode. Modify stack pointer to reflect the interrupt.
.reflect_v86:	test		dword [esp+12],0x200	; are interrupts enabled in the vm?
		jz		.reflect_v86_pending	; if not, then we need to note it and reflect later
		push		ebx
		push		ecx
		mov		bx,FLAT32_SEL		; NTS: Don't worry about saving ES. The CPU saved
		mov		ds,bx			; it as part of the v86 -> interrupt transition.
		xor		ebx,ebx
		mov		ecx,ebx
		mov		bl,[cs:int_rm_map+eax]	; IRQ -> interrupt
		mov		ebx,[ebx*4]		; interrupt -> realmode vector
		mov		ax,[esp+20+8]		; fetch SS
		shl		eax,4			; EAX = SS*16
		mov		cx,[esp+16+8]		; fetch SP
		sub		cx,6			; SP -= 6
		add		eax,ecx			; EAX = SS*16 + SP
		mov		[esp+16+8],cx		; store modified SP back
; EBX = realmode interrupt vector
; EAX = stack pointer (physical mem addr)
		mov		cx,[esp+4+8]		; fetch EIP
		mov		word [eax+0],cx		; SS:SP = offset
		mov		cx,[esp+8+8]		; fetch CS
		mov		word [eax+2],cx		; SS:SP+2 = segment
		mov		cx,[esp+12+8]		; fetch FLAGS
		mov		word [eax+4],cx		; SS:SP+4 = flags
		mov		[esp+4+8],bx		; overwrite IP = offset of vector
		shr		ebx,16
		mov		[esp+8+8],bx		; overwrite CS = segment of vector

; *DEBUG*
		inc		word [0xB8000]

		pop		ecx
		pop		ebx
		pop		eax
		iret
; v86 mode, but interrupts are disabled
.reflect_v86_pending:
		push		ebx
		push		ecx
		mov		bx,DATA32_SEL
		mov		ds,bx
		mov		cl,al			; CL = IRQ
		mov		eax,1
		shl		eax,cl			; EAX = 1 << IRQ
		or		word [irq_pending],ax	; irq_pending |= 1 << IRQ
		pop		ecx
		pop		ebx
		pop		eax
		iret

irq_0:		push		eax
		mov		eax,0
		jmp		irq_general

irq_1:		push		eax
		mov		eax,1
		jmp		irq_general

irq_2:		push		eax
		mov		eax,2
		jmp		irq_general

irq_3:		push		eax
		mov		eax,3
		jmp		irq_general

irq_4:		push		eax
		mov		eax,4
		jmp		irq_general

irq_5:		push		eax
		mov		eax,5
		jmp		irq_general

irq_6:		push		eax
		mov		eax,6
		jmp		irq_general

irq_7:		push		eax
		mov		eax,7
		jmp		irq_general

irq_8:		push		eax
		mov		eax,8
		jmp		irq_general

irq_9:		push		eax
		mov		eax,9
		jmp		irq_general

irq_10:		push		eax
		mov		eax,10
		jmp		irq_general

irq_11:		push		eax
		mov		eax,11
		jmp		irq_general

irq_12:		push		eax
		mov		eax,12
		jmp		irq_general

irq_13:		push		eax
		mov		eax,13
		jmp		irq_general

irq_14:		push		eax
		mov		eax,14
		jmp		irq_general

irq_15:		push		eax
		mov		eax,15
		jmp		irq_general

; =============== GENERAL PURPOSE "NO INTERRUPT ASSIGNED" HANDLER
fault_no_int:	iret

fault_x86_vector:db		0
; =============== REFLECT EXCEPTION TO REAL MODE
fault_v86_reflect:
		push		eax
		push		ebx
		push		ecx
		mov		bx,FLAT32_SEL		; NTS: Don't worry about saving ES. The CPU saved
		mov		ds,bx			; it as part of the v86 -> interrupt transition.
		xor		ebx,ebx
		mov		ecx,ebx
		mov		bl,[cs:fault_x86_vector]; what interrupt is involved?
		mov		ebx,[ebx*4]		; interrupt -> realmode vector
		mov		ax,[esp+20+8]		; fetch SS
		shl		eax,4			; EAX = SS*16
		mov		cx,[esp+16+8]		; fetch SP
		sub		cx,6			; SP -= 6
		add		eax,ecx			; EAX = SS*16 + SP
		mov		[esp+16+8],cx		; store modified SP back
; EBX = realmode interrupt vector
; EAX = stack pointer (physical mem addr)
		mov		cx,[esp+4+8]		; fetch EIP
		mov		word [eax+0],cx		; SS:SP = offset
		mov		cx,[esp+8+8]		; fetch CS
		mov		word [eax+2],cx		; SS:SP+2 = segment
		mov		cx,[esp+12+8]		; fetch FLAGS
		mov		word [eax+4],cx		; SS:SP+4 = flags
		mov		[esp+4+8],bx		; overwrite IP = offset of vector
		shr		ebx,16
		mov		[esp+8+8],bx		; overwrite CS = segment of vector
; if this is INT 0x01 we also need to clear the TF bit
		cmp		byte [cs:fault_x86_vector],1
		jnz		.not_int1
		and		word [esp+12+8],~0x100	; clear TF
.not_int1:

; *DEBUG*
		inc		word [0xB8002]

		pop		ecx
		pop		ebx
		pop		eax
		iret

; =============== FAULT HANDLER CODE
fault_0x00:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x00
		mov		esi,str_fault_0x00
		jmp		fault_jmp_unhandled

fault_0x01:	test		dword [esp+8],0x20000	; did it happen from within v86 mode?
		jnz		.reflect_v86
		push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x01
		mov		esi,str_fault_0x01
		jmp		fault_jmp_unhandled
.reflect_v86:	mov		byte [ss:fault_x86_vector],0x01 ; reflect to INT 0x01
		jmp		fault_v86_reflect

fault_0x02:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x02
		mov		esi,str_fault_0x02
		jmp		fault_jmp_unhandled

fault_0x03:	test		dword [esp+8],0x20000	; did it happen from within v86 mode?
		jnz		.reflect_v86
		push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x03
		mov		esi,str_fault_0x03
		jmp		fault_jmp_unhandled
.reflect_v86:	mov		byte [ss:fault_x86_vector],0x03 ; reflect to INT 0x03
		jmp		fault_v86_reflect

fault_0x04:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x04
		mov		esi,str_fault_0x04
		jmp		fault_jmp_unhandled

fault_0x05:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x05
		mov		esi,str_fault_0x05
		jmp		fault_jmp_unhandled

fault_0x06:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x06
		mov		esi,str_fault_0x06
		jmp		fault_jmp_unhandled

fault_0x07:	push		eax
		mov		eax,cr0
		test		eax,0x08		; is this a result of CR0.TS being set?
		pop		eax
		jz		.not_cr0_ts
; very likely a real-mode DOS application executed floating point instructions, and
; the task switch into vm86 mode left bit 3 (CR0.TS) set. Clear it and return. This
; is necessary to allow the DOS system to use floating point, even on 486/Pentium and
; higher systems where the FPU is integral to the CPU. Even for simple instructions
; like FSTSW/FNSTSW.
		clts
		iret
; if the exception did NOT involve that bit, then yes, it's something to halt on
.not_cr0_ts:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x07
		mov		esi,str_fault_0x07
		jmp		fault_jmp_unhandled

fault_0x08:	call		fault_collect_regs
		mov		edx,0x08
		mov		esi,str_fault_0x08
		jmp		fault_jmp_unhandled

fault_0x09:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x09
		mov		esi,str_fault_0x09
		jmp		fault_jmp_unhandled

fault_0x0A:	call		fault_collect_regs
		mov		edx,0x0A
		mov		esi,str_fault_0x0A
		jmp		fault_jmp_unhandled

fault_0x0B:	call		fault_collect_regs
		mov		edx,0x0B
		mov		esi,str_fault_0x0B
		jmp		fault_jmp_unhandled

fault_0x0C:	call		fault_collect_regs
		mov		edx,0x0C
		mov		esi,str_fault_0x0C
		jmp		fault_jmp_unhandled

fault_0x0E:	call		fault_collect_regs
		mov		edx,0x0E
		mov		esi,str_fault_0x0E
		jmp		fault_jmp_unhandled

fault_0x0F:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x0F
		mov		esi,str_fault_0x0F
		jmp		fault_jmp_unhandled

fault_0x10:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x10
		mov		esi,str_fault_0x10
		jmp		fault_jmp_unhandled

fault_0x11:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x11
		mov		esi,str_fault_0x11
		jmp		fault_jmp_unhandled

fault_0x12:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x12
		mov		esi,str_fault_0x12
		jmp		fault_jmp_unhandled

fault_0x13:	push		dword 0 ; ERROR CODE
		call		fault_collect_regs
		mov		edx,0x13
		mov		esi,str_fault_0x13
		jmp		fault_jmp_unhandled

fault_0x14:	call		fault_collect_regs
		mov		edx,0x14
		jmp		fault_jmp_unhandled_unknown

fault_0x15:	call		fault_collect_regs
		mov		edx,0x15
		jmp		fault_jmp_unhandled_unknown

fault_0x16:	call		fault_collect_regs
		mov		edx,0x16
		jmp		fault_jmp_unhandled_unknown

fault_0x17:	call		fault_collect_regs
		mov		edx,0x17
		jmp		fault_jmp_unhandled_unknown

fault_0x18:	call		fault_collect_regs
		mov		edx,0x18
		jmp		fault_jmp_unhandled_unknown

fault_0x19:	call		fault_collect_regs
		mov		edx,0x19
		jmp		fault_jmp_unhandled_unknown

fault_0x1A:	call		fault_collect_regs
		mov		edx,0x1A
		jmp		fault_jmp_unhandled_unknown

fault_0x1B:	call		fault_collect_regs
		mov		edx,0x1B
		jmp		fault_jmp_unhandled_unknown

fault_0x1C:	call		fault_collect_regs
		mov		edx,0x1C
		jmp		fault_jmp_unhandled_unknown

fault_0x1D:	call		fault_collect_regs
		mov		edx,0x1D
		jmp		fault_jmp_unhandled_unknown

fault_0x1E:	call		fault_collect_regs
		mov		edx,0x1E
		jmp		fault_jmp_unhandled_unknown

fault_0x1F:	call		fault_collect_regs
		mov		edx,0x1F
		jmp		fault_jmp_unhandled_unknown

fault_jmp_unhandled_unknown:
		mov		esi,str_fault_unknown
		jmp		fault_jmp_unhandled

; ============= EXCEPTION HANDLER: INT 0x0D GENERAL PROTECTION FAULT
; If caused by v8086 mode:
;  [ESP+0] = error code
;  [ESP+4] = EIP
;  [ESP+8] = CS
;  [ESP+12] = EFLAGS
;  [ESP+16] = ESP
;  [ESP+20] = SS
;  [ESP+24] = ES
;  [ESP+28] = DS
;  [ESP+32] = FS
;  [ESP+36] = GS
; Else, only the error code, EIP, CS, EFLAGS fields are present.
; If not from ring 0, then ESP, SS are as well.
fault_0x0D:	test		dword [esp+12],0x20000			; [ESP+12] = EFLAGS. Is bit 17 (VM) set?
		jz		.not_vm86_related

; at this point, we know this is the processor trapping CLI/STI or anything that a v86 monitor needs to know.
; so the next thing we do is examine the opcode at CS:IP to determine what the code is trying to do, and how
; to emulate it. Note the CS:IP off stack are REAL MODE addresses.
		pushad
		mov		ax,DATA32_SEL
		mov		ds,ax
		mov		es,ax

		xor		eax,eax
		mov		ebx,eax
		mov		ax,[esp+0x20+8]				; CS ON STACK
		shl		eax,4
		add		bx,[esp+0x20+4]				; EIP ON STACK
		add		eax,ebx
		sub		eax,[my_phys_base]			; REMEMBER our data segment is relative to the COM.
									; ALSO KNOW most x86 processors will wrap addresses like
									; 0xFFFFF000 back around to 0 (32-bit overflow) with nonzero
									; segment bases.

		mov		eax,[eax]				; fetch 4 bytes at CS:IP
		mov		[v86_raw_opcode],eax			; store for reference

		cmp		al,0xFA					; CLI?
		jz		.v86_cli
		cmp		al,0xFB					; STI?
		jz		.v86_sti
		cmp		al,0xF4					; HLT? (apparently, yes, that causes a GPF from v86 mode)
		jz		.v86_hlt
		cmp		al,0xCD					; INT X (AH=interrupt)
		jz		.v86_int
		cmp		al,0xCC					; INT 3
		jz		.v86_int3
		cmp		al,0xCF					; IRET
		jz		.v86_iret
		cmp		al,0x9C					; PUSHF 16-bit
		jz		.v86_pushf
		cmp		al,0x9D					; POPF 16-bit
		jz		.v86_popf
		cmp		ax,0x9C66				; PUSHFD 32-bit
		jz		.v86_pushfd
		cmp		ax,0x9D66				; POPFD 32-bit
		jz		.v86_popfd
		jmp		.v86_unknown

.v86_complete_and_check_pending_irq: ; <----------- COMPLETION, PLUS CHECK IF INTERRUPTS ENABLED, PENDING IRQs
		test		word [esp+0x20+12],0x200		; are interrupts enabled (IF=1)
		jz		.v86_complete
; interrupts enabled, are there pending IRQs?
		cmp		word [irq_pending],0
		jz		.v86_complete
; for each pending IRQ, stuff the stack with an interrupt frame.
; this must be done in the order the PIC would do based on IRQ priority.
		cld
		mov		ecx,16
		mov		esi,irq_priority
.v86_complete_and_check_pending_irq_scan:
		xor		eax,eax
		lodsb
		mov		ebx,1
		push		ecx
		mov		cl,al	; <- NTS: bits 8-31 should be zero because we inited ECX == 16
		shl		ebx,cl
		pop		ecx
		test		word [irq_pending],bx			; if (irq_pending & (1 << AL)) ...
		jnz		.v86_complete_and_check_pending_irq_found
		loop		.v86_complete_and_check_pending_irq_scan
; FALL THROUGH TO COMPLETION
.v86_complete:	popad
.v86_complete_no_popad:
		add		esp,4					; dump error code (usually zero)
		iret

; we found a pending IRQ. EBX = 1 << IRQ, EAX = IRQ
.v86_complete_and_check_pending_irq_found:
		push		ecx
		xor		word [irq_pending],bx			; clear the bit

		movzx		ebx,al					; EBX = interrupt number
		mov		eax,FLAT32_SEL
		mov		es,ax					; we'll need flat mode for this
		mov		bl,[int_rm_map+ebx]			; IRQ -> interrupt
		; store CS:IP and FLAGS on 16-bit stack, decrement stack pointer. DO NOT MODIFY EBX
		sub		word [esp+0x20+4+16],6			; (E)SP -= 6
		mov		ax,word [esp+0x20+4+20]			; AX = SS (upper bits should be zero)
		shl		eax,4					; AX *= 16
		xor		ecx,ecx
		mov		cx,word [esp+0x20+4+16]			; CX = SP
		add		eax,ecx					; AX += SP  AX = (SS*16)+SP
		mov		cx,word [esp+0x20+4+4]			; IP
		mov		word [es:eax],cx			; SS:SP+0 = IP
		mov		cx,word [esp+0x20+4+8]			; CS
		mov		word [es:eax+2],cx			; SS:SP+2 = CS
		mov		cx,word [esp+0x20+4+12]			; FLAGS
		mov		word [es:eax+4],cx			; SS:SP+4 = FLAGS
		; replace CS:IP with values from real-mode interrupt table (EBX = interrupt vector)
		mov		ax,[es:(ebx*4)]				; read from real-mode interrupt table (offset)
		mov		word [esp+0x20+4+4],ax			; replace EIP
		mov		ax,[es:(ebx*4)+2]			;  .... (segment)
		mov		word [esp+0x20+4+8],ax			; replace CS

		pop		ecx
		dec		ecx
		jz		.v86_complete
		jmp		.v86_complete_and_check_pending_irq_scan

;   EXCEPTION HANDLING REACHES HERE IF IT TURNS OUT VM86 MODE WAS NOT INVOLVED
.not_vm86_related:
		call		fault_collect_regs
		mov		edx,0x0D				; INT 0x0D General Protection Fault
		mov		esi,str_fault_0x0D
		jmp		fault_jmp_unhandled
;   V86 IRET
.v86_iret:	mov		eax,FLAT32_SEL
		mov		es,ax					; we'll need flat mode for this
		; retrieve CS:IP and FLAGS from 16-bit stack, increment stack pointer
		mov		ax,word [esp+0x20+20]			; AX = SS (upper bits should be zero)
		shl		eax,4					; AX *= 16
		xor		ecx,ecx
		mov		cx,word [esp+0x20+16]			; CX = SP
		add		eax,ecx					; AX += SP  AX = (SS*16)+SP

		mov		cx,word [es:eax]			; IP = SS:SP+0
		mov		word [esp+0x20+4],cx			; IP

		mov		cx,word [es:eax+2]			; CS = SS:SP+2
		mov		word [esp+0x20+8],cx			; CS

		mov		cx,word [es:eax+4]			; FLAGS = SS:SP+4
		mov		word [esp+0x20+12],cx			; FLAGS

		add		word [esp+0x20+16],6			; (E)SP += 6
		jmp		.v86_complete
;   V86 INT 66h
.v86_int_api:	popad
		cmp		eax,0xAABBAA55
		jz		.v86_int_api_detect
		cmp		eax,0xAABBAABB
		jz		.v86_int_api_unload
		int		3
.v86_int_api_detect:
		mov		eax,0xBBAABB33
		jmp		.v86_complete_no_popad
.v86_int_api_unload:
		mov		ax,word [esp+4]				; save IP
		mov		bx,word [esp+8]				; save CS
		mov		[unload_int_ret+0],ax
		mov		[unload_int_ret+2],bx

		mov		ax,word [esp+16]			; save SP
		mov		bx,word [esp+20]			; save SS
		mov		[unload_int_stk+0],ax
		mov		[unload_int_stk+2],bx

		jmp		v86_api_exit
;   V86 INT 3 (AL = 0xCC)
.v86_int3:	mov		ah,0x03					; convert to INT 3 (CD 03)
		inc		dword [esp+0x20+4]			; step past (EIP++)
		jmp		short .v86_int_n
;   V86 INT x (AL = 0xCD   AH = N)
.v86_int:	add		dword [esp+0x20+4],2			; EIP += 2
;   V86 INT REFLECTION TO REAL MODE
.v86_int_n:	movzx		ebx,ah					; EBX = interrupt number
		; *DEBUG*
		cmp		bl,RM_INT_API
		jz		.v86_int_api
		; *END DEBUG*
		mov		eax,FLAT32_SEL
		mov		es,ax					; we'll need flat mode for this
		; store CS:IP and FLAGS on 16-bit stack, decrement stack pointer. DO NOT MODIFY EBX
		sub		word [esp+0x20+16],6			; (E)SP -= 6
		mov		ax,word [esp+0x20+20]			; AX = SS (upper bits should be zero)
		shl		eax,4					; AX *= 16
		xor		ecx,ecx
		mov		cx,word [esp+0x20+16]			; CX = SP
		add		eax,ecx					; AX += SP  AX = (SS*16)+SP
		mov		cx,word [esp+0x20+4]			; IP
		mov		word [es:eax],cx			; SS:SP+0 = IP
		mov		cx,word [esp+0x20+8]			; CS
		mov		word [es:eax+2],cx			; SS:SP+2 = CS
		mov		cx,word [esp+0x20+12]			; FLAGS
		mov		word [es:eax+4],cx			; SS:SP+4 = FLAGS
		; replace CS:IP with values from real-mode interrupt table (EBX = interrupt vector)
		mov		ax,[es:(ebx*4)]				; read from real-mode interrupt table (offset)
		mov		word [esp+0x20+4],ax			; replace EIP
		mov		ax,[es:(ebx*4)+2]			;  .... (segment)
		mov		word [esp+0x20+8],ax			; replace CS
		jmp		.v86_complete
;   V86 CLI
.v86_cli:	inc		dword [esp+0x20+4]			; step past (EIP++)
		and		word [esp+0x20+12],~0x200
		jmp		.v86_complete_and_check_pending_irq
;   V86 STI
.v86_sti:	inc		dword [esp+0x20+4]			; step past (EIP++)
		or		word [esp+0x20+12],0x200
		jmp		.v86_complete_and_check_pending_irq
;   V86 HLT
.v86_hlt:	inc		dword [esp+0x20+4]			; step past (EIP++)
		test		word [esp+0x20+12],0x200
		jz		.v86_hlt_with_cli
		jmp		.v86_complete_and_check_pending_irq
;   V86 HLT with interrupts disabled
.v86_hlt_with_cli:
		popad							; undo v86 check stack
		call		fault_collect_regs
		mov		edx,0x0D
		mov		esi,str_v86_hlt_cli
		jmp		fault_jmp_unhandled
;   V86 PUSHF
.v86_pushf:	mov		eax,FLAT32_SEL
		mov		es,ax
		inc		dword [esp+0x20+4]			; step past (EIP++)
		sub		word [esp+0x20+16],2			; (E)SP -= 2
		mov		ax,word [esp+0x20+20]			; AX = SS (upper bits should be zero)
		shl		eax,4					; AX *= 16
		xor		ecx,ecx
		mov		cx,word [esp+0x20+16]			; CX = SP
		add		eax,ecx					; AX += SP  AX = (SS*16)+SP
		mov		cx,word [esp+0x20+12]			; FLAGS
		mov		word [es:eax],cx			; SS:SP+0 = FLAGS
		jmp		.v86_complete
;   V86 PUSHFD
.v86_pushfd:	mov		eax,FLAT32_SEL
		mov		es,ax
		add		dword [esp+0x20+4],2			; step past (EIP += 2)
		sub		word [esp+0x20+16],4			; (E)SP -= 4
		mov		ax,word [esp+0x20+20]			; AX = SS (upper bits should be zero)
		shl		eax,4					; AX *= 16
		xor		ecx,ecx
		mov		cx,word [esp+0x20+16]			; CX = SP
		add		eax,ecx					; AX += SP  AX = (SS*16)+SP
		mov		ecx,dword [esp+0x20+12]			; EFLAGS
		mov		dword [es:eax],ecx			; SS:SP+0 = FLAGS
		jmp		.v86_complete
;   V86 POPF
.v86_popf:	mov		eax,FLAT32_SEL
		mov		es,ax
		inc		dword [esp+0x20+4]			; step past (EIP++)
		mov		ax,word [esp+0x20+20]			; AX = SS (upper bits should be zero)
		shl		eax,4					; AX *= 16
		xor		ecx,ecx
		mov		cx,word [esp+0x20+16]			; CX = SP
		add		eax,ecx					; AX += SP  AX = (SS*16)+SP
		mov		cx,word [es:eax]			; FLAGS = SS:SP+0
		mov		word [esp+0x20+12],cx			; FLAGS
		add		word [esp+0x20+16],2			; (E)SP += 2
		jmp		.v86_complete_and_check_pending_irq
;   V86 POPFD
.v86_popfd:	mov		eax,FLAT32_SEL
		mov		es,ax
		add		dword [esp+0x20+4],2			; step past (EIP += 2)
		mov		ax,word [esp+0x20+20]			; AX = SS (upper bits should be zero)
		shl		eax,4					; AX *= 16
		xor		ecx,ecx
		mov		cx,word [esp+0x20+16]			; CX = SP
		add		eax,ecx					; AX += SP  AX = (SS*16)+SP
		mov		ecx,dword [es:eax]			; EFLAGS = SS:SP+0
		or		ecx,0x20000				; make sure the VM bit is set
		mov		dword [esp+0x20+12],ecx			; EFLAGS
		add		word [esp+0x20+16],4			; (E)SP += 4
		jmp		.v86_complete_and_check_pending_irq
;   UNKNOWN OPCODE AT CS:IP in V8086 MODE
.v86_unknown:	popad							; undo v86 check stack
		add		esp,4					; toss real error code
		push		dword [v86_raw_opcode]			; the "ERROR CODE" are the 4 bytes at CS:IP
		call		fault_collect_regs
		mov		edx,0x0D
		mov		esi,str_v86_unknown
		jmp		fault_jmp_unhandled
;   API CALL TO SHUTDOWN VM86 MONITOR
v86_api_exit:	mov		ax,FLAT32_SEL
		mov		es,ax
		mov		ax,DATA32_SEL
		mov		ds,ax
; FIXME: I give up... why does JMPing to TSS_SEL:0 cause random crashes in VirtualBox?
		jmp		_exit_from_prot32			; and then begin shutdown of this program

; ========== FAULT COLLECTION ROUTINE. SS:ESP should point to fault. If the exception does not push an error code,
;    then the caller must push a dummy error code
fault_collect_regs:
		push		ds
		push		eax
		push		ebx
		mov		ax,ds
		mov		bx,ax
		mov		ax,DATA32_SEL
		mov		ds,ax
		mov		word [unhandled_fault_var_ds],bx
		pop		ebx

		mov		eax,[esp+4+8+0]				; ERROR CODE ON STACK +2 DWORDs PUSHED
		mov		dword [unhandled_fault_var_errcode],eax

		mov		eax,[esp+4+8+4]				; EIP ON STACK
		mov		dword [unhandled_fault_var_eip],eax

		mov		eax,[esp+4+8+8]				; CS ON STACK
		mov		word [unhandled_fault_var_cs],ax

		mov		eax,[esp+4+8+12]			; EFLAGS ON STACK
		mov		dword [unhandled_fault_var_eflags],eax

		call		.retr_stack_ptr

		pop		eax

		mov		dword [unhandled_fault_var_eax],eax
		mov		dword [unhandled_fault_var_ebx],ebx
		mov		dword [unhandled_fault_var_ecx],ecx
		mov		dword [unhandled_fault_var_edx],edx
		mov		dword [unhandled_fault_var_esi],esi
		mov		dword [unhandled_fault_var_edi],edi
		mov		dword [unhandled_fault_var_ebp],ebp

		mov		eax,cr0
		mov		dword [unhandled_fault_var_cr0],eax
		mov		eax,cr3
		mov		dword [unhandled_fault_var_cr3],eax
		mov		eax,cr4
		mov		dword [unhandled_fault_var_cr4],eax
		mov		ax,es
		mov		word [unhandled_fault_var_es],ax
		mov		ax,fs
		mov		word [unhandled_fault_var_fs],ax
		mov		ax,gs
		mov		word [unhandled_fault_var_gs],ax
		pop		ds
		ret
; if privilege escalation was involved (stack switching) then retrieve SS:ESP at fault from the stack frame.
; else retrieve from actual SS:ESP registers
.retr_stack_ptr:
		test		word [unhandled_fault_var_cs],3		; if code segment is nonzero
		jz		.retr_stack_ptr_ring_0

		mov		eax,[esp+4+4+8+16]			; ESP ON STACK
		mov		dword [unhandled_fault_var_esp],eax

		mov		eax,[esp+4+4+8+20]			; SS ON STACK
		mov		word [unhandled_fault_var_ss],ax

		ret
.retr_stack_ptr_ring_0:
		lea		eax,[esp+4+4+8+16]			; +4 our call frame, +8 PUSH DS,EAX +16 GPF stack frame
		mov		dword [unhandled_fault_var_esp],eax

		mov		eax,ss					; SS ON STACK
		mov		word [unhandled_fault_var_ss],ax

		ret

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
		mov		sp,stack_init
		; overwrite the far jmp's segment value
		mov		ax,[my_realmode_seg]
		mov		word [.real_hackme+3],ax
		lidt		[idtr_real]
		lgdt		[gdtr_real]
		xor		eax,eax
		mov		cr0,eax
.real_hackme:	jmp		0:.real_entry
.real_entry:	mov		ax,[my_realmode_seg]
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

; remove our INT 66h API
		xor		ax,ax
		mov		es,ax
		mov		word [es:(RM_INT_API*4)],ax
		mov		word [es:(RM_INT_API*4)+2],ax

; free HIMEM.SYS blocks
		mov		ah,0Dh			; HIMEM.SYS function 0Dh unlock memory block
		mov		dx,word [himem_sys_buffer_handle]
		call far	word [himem_sys_entry]
		mov		ah,0Ah			; HIMEM.SYS function 0Ah free memory block
		mov		dx,word [himem_sys_buffer_handle]
		call far	word [himem_sys_entry]

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

		jmp far		word [cs:unload_int_ret]

; ============= UNHANDLED FAULT HANDLER (16-bit code)
;   input: EDX = Number of interrupt
;          DS:SI = Textual string of fault
;          ESP = Stack containing:
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
		xor		eax,eax
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

		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al
		out		20h,al

		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al
		out		0A0h,al

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
vm86_entry:	cli				; make sure the v86 monitor handles CLI
		sti				; ...and STI
		pushf				; ...and PUSHF
		popf				; ...and POPF
		pushfd				; ...32-bit PUSHF
		popfd				; ...32-bit POPF
		in		al,21h		; ...IN?
		out		21h,al		; ...OUT?

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

		; finally, terminate and stay resident
		mov		byte [i_am_tsr],1
		mov		edx,the_end		; DX = memory in paragraphs to save
		add		edx,15
		shr		edx,4
		add		edx,16			; <-- FIXME: IS THIS NECESSARY
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
; switch A-Z jump table
.switch_az:	dw		.unknown_switch			; /A
		dw		.switch_buffer_size		; /B=<number>
		dw		.unknown_switch			; /C
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
		dw		.unknown_switch			; /W
		dw		.unknown_switch			; /X
		dw		.unknown_switch			; /Y
		dw		.unknown_switch			; /Z

irq_routines:	dw		irq_0
		dw		irq_1
		dw		irq_2
		dw		irq_3
		dw		irq_4
		dw		irq_5
		dw		irq_6
		dw		irq_7
		dw		irq_8
		dw		irq_9
		dw		irq_10
		dw		irq_11
		dw		irq_12
		dw		irq_13
		dw		irq_14
		dw		irq_15

fault_routines:	dw		fault_0x00
		dw		fault_0x01
		dw		fault_0x02
		dw		fault_0x03
		dw		fault_0x04
		dw		fault_0x05
		dw		fault_0x06
		dw		fault_0x07
		dw		fault_0x08
		dw		fault_0x09
		dw		fault_0x0A
		dw		fault_0x0B
		dw		fault_0x0C
		dw		fault_0x0D
		dw		fault_0x0E
		dw		fault_0x0F
		dw		fault_0x10
		dw		fault_0x11
		dw		fault_0x12
		dw		fault_0x13
		dw		fault_0x14
		dw		fault_0x15
		dw		fault_0x16
		dw		fault_0x17
		dw		fault_0x18
		dw		fault_0x19
		dw		fault_0x1A
		dw		fault_0x1B
		dw		fault_0x1C
		dw		fault_0x1D
		dw		fault_0x1E
		dw		fault_0x1F


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
str_irq_deferred:db		'Deferred IRQ$'
str_irq_1:	db		'IRQ #1$'

; ============= VARIABLES: THESE DO NOT EXIST IN THE .COM FILE THEY EXIST IN MEMORY FOLLOWING THE BINARY IMAGE
		section		.bss align=2
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
buffer_alloc:	resd		1		; DWORD
kern32_stack_base:resd		1		; DWORD
kern32_stack_top:resd		1		; DWORD
v86_raw_opcode:	resd		1		; DWORD
		resd		1		; *PADDING*
; ---------------------- GDTR/IDTR
gdtr_pmode:	resq		1		; LIMIT. BASE
gdtr_real:	resq		1		; LIMIT, BASE
idtr_pmode:	resq		1		; LIMIT, BASE
idtr_real:	resq		1		; LIMIT, BASE
; ---------------------- GLOBAL DESCRIPTOR TABLE
		align		8
gdt:		resq		(MAX_SEL/8)	; 16 GDT entries
; ---------------------- INTERRUPT DESCRIPTOR TABLE
		align		8
idt:		resq		256		; all 256
; ---------------------- STATE
irq_pending:	resw		1
v86_if:		resb		1
user_req_unload:resb		1
user_req_iopl:	resb		1
i_am_tsr:	resb		1
unload_int_ret:	resd		1
unload_int_stk:	resd		1
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
; ---------------------------------------------------------------------
;                       END POINTER
; ---------------------------------------------------------------------
padding:	resq		2		; SAFETY PADDING
the_end:
