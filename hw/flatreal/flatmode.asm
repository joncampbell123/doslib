
; NTS: We use NASM to achieve our goals here because WASM sucks donkey balls
;      Maybe when they bother to implement a proper conditional macro system, I'll consider it...

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

; NTS: Associate our data with Watcom's data segment
segment .data public align=4 class=data

; NTS: Help NASM put the code segment in the right place for Watcom to link it in properly
segment text public align=1 class=code

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

; void __cdecl flatrealmode_force_datasel(void *ptr);
global _flatrealmode_force_datasel
_flatrealmode_force_datasel:
		push		bp
		mov		bp,sp
		pushf
		pusha

		mov		ax,cs
		mov		word [cs:_flatrealmode_force_datasel_j2_hackme+3],ax		; overwrite segment portion of JMP FAR instruction

; LARGE and COMPACT memory models: a far pointer is passed onto the stack
%ifidni MMODE,l
%define FARPTR_
%else
 %ifidni MMODE,c
 %define FARPTR_
 %endif
%endif

%ifdef FARPTR_
 %undef FARPTR_
		; LARGE memory model version (we're given a FAR pointer)
		; caller passes us the address of the constructed GDT (near ptr)
		xor		esi,esi
		mov		si,[bp+cdecl_param_offset]

		; now convert to physical addr
		xor		eax,eax
		mov		ax,[bp+cdecl_param_offset+2]
		shl		eax,4
		add		eax,esi
%else		; SMALL memory model version (we're given a NEAR pointer)
		; caller passes us the address of the constructed GDT (near ptr)
		xor		esi,esi
		mov		si,[bp+cdecl_param_offset]

		; now convert to physical addr
		xor		eax,eax
		mov		ax,ds
		shl		eax,4
		add		eax,esi
%endif

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
