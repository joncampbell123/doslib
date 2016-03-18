; flatmode.asm
;
; Flat real mode support routines
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

; ---------- CODE segment -----------------
%include "_segcode.inc"

; NASM won't do it for us... make sure "retnative" is defined
%ifndef retnative
 %error retnative not defined
%endif

%if TARGET_MSDOS == 16
; int flatrealmode_test()
global flatrealmode_test_
flatrealmode_test_:
		pushf
		push		ds
		push		si
		push		cx

		; clear interrupts, to ensure IRQ 5 is not mistaken for a GP fault
		cli

		; set DS=0 to carry out the test, and zero AX
		xor		ax,ax
		mov		ds,ax

		; hook interrupt 0x0D (general protection fault and IRQ 5)
		mov		si,0x0D * 4
		mov		cx,[si]			; offset
		mov		word [si],_flatrealmode_test_fail
		push		cx
		mov		cx,[si+2]
		mov		word [si+2],cs
		push		cx

		; now try it. either we'll make it through unscathed or it will cause a GP fault and branch to our handler
		mov		esi,0xFFFFFFF8
		mov		esi,[esi]

		; either nothing happened, or control jmp'd here from the exception handler (who also set AX=1)
		; restore interrupt routine and clean up
_flatrealmode_test_conclude:
		mov		si,0x0D * 4
		pop		cx
		mov		word [si+2],cx
		pop		cx
		mov		word [si],cx

		pop		cx
		pop		si
		pop		ds
		popf
		retnative
_flatrealmode_test_fail:
		add		sp,6			; throw away IRETF address (IP+CS+FLAGS)
		inc		ax			; make AX nonzero
		jmp short	_flatrealmode_test_conclude
%endif

%if TARGET_MSDOS == 16
; void __cdecl flatrealmode_force_datasel(void *ptr);
global _flatrealmode_force_datasel
_flatrealmode_force_datasel:
		push		bp
		mov		bp,sp
		pushf
		pusha

		mov		ax,cs
		mov		word [cs:_flatrealmode_force_datasel_j2_hackme+3],ax		; overwrite segment portion of JMP FAR instruction

		xor		esi,esi
		mov		si,[bp+cdecl_param_offset]
		xor		eax,eax
 %ifdef MMODE_DATA_VAR_DEF_FAR
		; LARGE memory model version (we're given a FAR pointer)
		; caller passes us the address of the constructed GDT (near ptr)
		mov		ax,[bp+cdecl_param_offset+2]
 %else		; SMALL memory model version (we're given a NEAR pointer)
		; caller passes us the address of the constructed GDT (near ptr)
		mov		ax,ds
 %endif
		; now convert to physical addr
		shl		eax,4
		add		eax,esi

		; disable interrupts, we're going to fuck with the CPU mode and thunk into protected mode
		cli

		; allocate room on the stack for the GDT and store the original GDTR
		sub		sp,8
		mov		bx,sp
		sgdt		[bx]

		; and another for the new GDT
		sub		sp,8
		mov		di,sp
		mov		word [di],0xFF
		mov		dword [di+2],eax

		; load the new GDT
		lgdt		[di]

		; save previous segment regs
		mov		bx,ds
		mov		cx,es

		; enable protected mode
		mov		eax,cr0
		or		al,1
		mov		cr0,eax

		; now load the segment registers
		mov		ax,8			; GDT selector value
		mov		ds,ax
		mov		es,ax

		; force CPU to go into protected mode, clear cache
		jmp		16:word _flatrealmode_force_datasel_j1
_flatrealmode_force_datasel_j1:

		; disable protected mode
		mov		eax,cr0
		and		al,~1
		mov		cr0,eax

		; jump to force it
		; opcode format: 0xEA <ofs> <seg>
_flatrealmode_force_datasel_j2_hackme:
		jmp		0:word _flatrealmode_force_datasel_j2
_flatrealmode_force_datasel_j2:

		; reload sane realmode selectors
		mov		ds,bx
		mov		es,cx

		; discard the new GDT
		add		sp,8

		; restore old GDT
		mov		bx,sp
		lgdt		[bx]

		; discard old GDT
		add		sp,8

		popa
		popf
		pop		bp
		retnative
%endif
