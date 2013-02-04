
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

%if TARGET_MSDOS == 16
; ext vars
; uint32_t far* near	llmemcpy_gdt = NULL;
extern _llmemcpy_gdt
; uint16_t near		llmemcpy_gdtr[4];
extern _llmemcpy_gdtr
; uint16_t near		llmemcpy_idtr[4];
extern _llmemcpy_idtr
; uint32_t near		llmemcpy_vcpi[0x20];
extern _llmemcpy_vcpi
; uint32_t near		llmemcpy_vcpi_return[2];
extern _llmemcpy_vcpi_return
; volatile void FAR*	llmemcpy_pagetables = NULL;
extern _llmemcpy_pagetables
%endif

; NTS: Help NASM put the code segment in the right place for Watcom to link it in properly
segment text public align=1 class=code

%if TARGET_MSDOS == 16

global llmem_memcpy_16_inner_pae_
llmem_memcpy_16_inner_pae_:
		; save sregs
		push	es
		push	ds
		push	ss

		; patch exit jmp
		mov	ax,cs
		mov	word [cs:exit_pae_patch+1+2],ax

		; jump into protected mode, with paging
		mov	eax,cr0
		or	eax,0x80000001
		mov	cr0,eax
		jmp	0x08:entry_pae
entry_pae:

		; load the data selectors
		mov	ax,0x10
		mov	ds,ax
		mov	ax,0x18
		mov	es,ax
		mov	ax,0x20
		mov	ss,ax

		; do the memcpy
		mov	edx,ecx
		and	edx,3
		shr	ecx,2
		cld

		es rep	a32 movsd

		mov	ecx,edx
		es rep	a32 movsb

		; get out of protected mode
		mov	eax,cr0
		and	eax,0x7FFFFFFE
		mov	cr0,eax
exit_pae_patch:
		jmp	0:exit_pae
exit_pae:

		; restore sregs
		pop	ss
		pop	ds
		pop	es

		retnative

global llmem_memcpy_16_inner_pse_
llmem_memcpy_16_inner_pse_:
		; save sregs
		push	es
		push	ds
		push	ss

		; patch exit jmp
		mov	ax,cs
		mov	word [cs:exit_pse_patch+1+2],ax

		; jump into protected mode, with paging
		mov	eax,cr0
		or	eax,0x80000001
		mov	cr0,eax
		jmp	0x08:entry_pse
entry_pse:

		; load the data selectors
		mov	ax,0x10
		mov	ds,ax
		mov	ax,0x18
		mov	es,ax
		mov	ax,0x20
		mov	ss,ax

		; do the memcpy
		mov	edx,ecx
		and	edx,3
		shr	ecx,2
		cld

		es rep	a32 movsd

		mov	ecx,edx
		es rep	a32 movsb

		; get out of protected mode
		mov	eax,cr0
		and	eax,0x7FFFFFFE
		mov	cr0,eax
exit_pse_patch:
		jmp	0:exit_pse
exit_pse:

		; restore sregs
		pop	ss
		pop	ds
		pop	es

		retnative

; alternate version to do PSE llmemcpy when VCPI/EMM386.EXE is active.
; void __cdecl llmem_memcpy_16_inner_pse_vcpi(uint32_t dst,uint32_t src,uint32_t cpy);
global _llmem_memcpy_16_inner_pse_vcpi
_llmem_memcpy_16_inner_pse_vcpi:
		push	bp
; extra vars
		push	cs	; +14
		push	ds	; +12
		push	es	; +10
		push	ss	; +8

; we need to store _llmemcpy_vcpi_return on the stack. once we're in protected mode
; the FAR pointers given by Watcom are not usable.
		mov	si,seg _llmemcpy_vcpi_return
		mov	fs,si
		mov	si,_llmemcpy_vcpi_return
		mov	eax,[fs:si+4] ; segment
		push	eax	; +4
		xor	eax,eax
		push	eax	; +0

%define extra		16	; +16
; end extra
		mov	bp,sp

		mov	ax,seg _llmemcpy_pagetables
		mov	fs,ax
		xor	edi,edi
		les	di,[fs:_llmemcpy_pagetables]
		add	edi,0x1000

		mov	ax,seg _llmemcpy_gdt
		mov	fs,ax
		xor	esi,esi
		lds	si,[fs:_llmemcpy_gdt]
		add	esi,5 << 3

		; so: DS:SI = First GDT available to VCPI server
		;     ES:DI = Page dir 0 page 0 4KB page
		mov	ax,0xDE01
		int	67h
		cmp	ah,0
		jz	.info_ok
; ==ERROR==
		mov	eax,0xAABBCC01
		jmp	short $
; ==END ERROR==
.info_ok:	; we need EBX, the return entry point offset
		mov	[bp+0],ebx

		; now enter VCPI protected mode
		mov	bx,seg _llmemcpy_vcpi
		mov	fs,bx
		mov	dword [fs:_llmemcpy_vcpi+0x10],.vcpi_entry

		xor	esi,esi
		mov	si,seg _llmemcpy_vcpi
		shl	esi,4
		add	esi,_llmemcpy_vcpi

		mov	ax,0xDE0C
		int	67h
		hlt		; <- BRICK WALL in case of errant VCPI server

.vcpi_entry:	mov	ax,2 << 3
		mov	ds,ax

		mov	ax,3 << 3
		mov	es,ax
		mov	fs,ax
		mov	gs,ax

		mov	ax,4 << 3
		mov	ss,ax

		; switch on PSE. note we couldn't do this from the real-mode side
		; since the v86 monitor would likely not allow that
		mov	eax,cr4
		or	eax,0x10
		mov	cr4,eax

		mov	ecx,[bp+cdecl_param_offset+extra+8]	; cpy
		mov	esi,[bp+cdecl_param_offset+extra+4]	; src
		mov	edi,[bp+cdecl_param_offset+extra+0]	; dst

		cld
		push	ecx
		shr	ecx,2
		a32 rep	es movsd
		pop	ecx
		and	ecx,3
		a32 rep	es movsb

		; switch off PSE. once we're back in v86 mode we can't touch control regs
		mov	eax,cr4
		and	eax,~0x10
		mov	cr4,eax

		; set up return to v86 mode
		and	esp,0xFFFF	; <--- THIS IS VERY IMPORTANT ON RETURN FROM VCPI, UPPER BITS OF ESP CAN BE NONZERO
		xor	eax,eax
		mov	ax,[bp+12]	; DS
		push	eax		; SS:ESP+0x28 GS
		push	eax		; SS:ESP+0x24 FS
		push	eax		; SS:ESP+0x20 DS
		mov	ax,[bp+10]
		push	eax		; SS:ESP+0x1C ES
		mov	ax,[bp+8]
		push	eax		; SS:ESP+0x18 SS
		mov	eax,ebp
		push	eax		; SS:ESP+0x14 ESP
		pushfd			; SS:ESP+0x10 EFLAGS
		mov	ax,[bp+14]
		push	eax		; SS:ESP+0x0C CS
		push	dword .vcpi_exit; SS:ESP+0x08 EIP
		mov	eax,[bp+4]	; VCPI code segment
		push	eax		; SS:ESP+0x04 VCPI code segment
		mov	eax,[bp+0]	; VCPI offset
		push	eax
		mov	eax,0xDE0C	; switch back to v86
		jmp far	dword [esp]	; <--- 32-bit address mode required for direct use of SP, but only if we refer to ESP
.vcpi_exit:

		add	sp,extra
		pop	bp
		retnative
%undef extra

; alternate version to do PAE llmemcpy when VCPI/EMM386.EXE is active.
; void __cdecl llmem_memcpy_16_inner_pae_vcpi(uint32_t dst,uint32_t src,uint32_t cpy);
global _llmem_memcpy_16_inner_pae_vcpi
_llmem_memcpy_16_inner_pae_vcpi:
		push	bp
; extra vars

		xor	esi,esi
		xor	edi,edi
		mov	si,seg _llmemcpy_pagetables
		mov	fs,si
		mov	si,[fs:_llmemcpy_pagetables+2]
		shl	esi,4
		mov	di,[fs:_llmemcpy_pagetables]
		add	esi,edi
		push	esi	; +16

		push	cs	; +14
		push	ds	; +12
		push	es	; +10
		push	ss	; +8

; we need to store _llmemcpy_vcpi_return on the stack. once we're in protected mode
; the FAR pointers given by Watcom are not usable.
		mov	si,seg _llmemcpy_vcpi_return
		mov	fs,si
		mov	si,_llmemcpy_vcpi_return
		mov	eax,[fs:si+4] ; segment
		push	eax	; +4
		xor	eax,eax
		push	eax	; +0

%define extra		20	; +20
; end extra
		mov	bp,sp

		; we're going to give the VCPI server the last 4KB page in the 36KB
		; buffer allocated by the function, set aside for that purpose.
		; what we're then going to do is copy and translate that page table
		; to the 64-bit form required by PAE, initially starting from the
		; 32-bit form in the last 8KB
		mov	ax,seg _llmemcpy_pagetables
		mov	fs,ax
		xor	edi,edi
		les	di,[fs:_llmemcpy_pagetables]
		add	edi,0x8000	; +32KB

		mov	ax,seg _llmemcpy_gdt
		mov	fs,ax
		xor	esi,esi
		lds	si,[fs:_llmemcpy_gdt]
		add	esi,5 << 3

		; so: DS:SI = First GDT available to VCPI server
		;     ES:DI = Page dir 0 page 0 4KB page
		mov	ax,0xDE01
		int	67h
		cmp	ah,0
		jz	.info_ok
; ==ERROR==
		mov	eax,0xAABBCC01
		jmp	short $
; ==END ERROR==
.info_ok:	; we need EBX, the return entry point offset
		mov	[bp+0],ebx

		; now enter VCPI protected mode
		mov	bx,seg _llmemcpy_vcpi
		mov	fs,bx
		mov	dword [fs:_llmemcpy_vcpi+0x10],.vcpi_entry

		xor	esi,esi
		mov	si,seg _llmemcpy_vcpi
		shl	esi,4
		add	esi,_llmemcpy_vcpi

		mov	ax,0xDE0C
		int	67h
		hlt		; <- BRICK WALL in case of errant VCPI server

.vcpi_entry:	mov	ax,2 << 3
		mov	ds,ax

		mov	ax,3 << 3
		mov	es,ax
		mov	fs,ax
		mov	gs,ax

		mov	ax,4 << 3
		mov	ss,ax

		; switch on PSE. note we couldn't do this from the real-mode side
		; since the v86 monitor would likely not allow that
		mov	eax,cr4
		or	eax,0x10
		mov	cr4,eax

		; copy the first 4MB of 32-bit page tables and translate to 64-bit
		mov	eax,[bp+16]				; _llmemcpy_pagetables
		lea	esi,[eax+0x8000]			; source: 32-bit VCPI page zero
		lea	edi,[eax+0x5000]			; dest: 64-bit page zero and one
		mov	ecx,1024				; 1024 x 32-bit -> 1024 x 64-bit (4KB -> 8KB)
		cld
.xlate_loop:	a32 es	movsd					; lower 32 bits -> 64 bits with upper bits zero
		xor	eax,eax
		a32 stosd
		loop	.xlate_loop

		; switch on PAE, reload CR3. Temporarily shut down paging to accomplish that.
		; most likely: as a DOS program in the 1MB area we're not remapped and it won't affect us.
		mov	ecx,cr0
		and	ecx,0x7FFFFFFF
		mov	cr0,ecx					; CR0=Disable PE
		mov	ebx,[bp+16]				; _llmemcpy_pagetables
		mov	cr3,ebx					; CR3=new 64-bit page table
		mov	eax,cr4
		or	eax,0x30
		mov	cr4,eax					; CR4=PSE and PAE
		or	ecx,0x80000000
		mov	cr0,ecx					; CR0=Enable PE

		mov	ecx,[bp+cdecl_param_offset+extra+8]	; cpy
		mov	esi,[bp+cdecl_param_offset+extra+4]	; src
		mov	edi,[bp+cdecl_param_offset+extra+0]	; dst

		cld
		push	ecx
		shr	ecx,2
		a32 rep	es movsd
		pop	ecx
		and	ecx,3
		a32 rep	es movsb

		; switch on PAE, reload CR3. Temporarily shut down paging to accomplish that.
		; most likely: as a DOS program in the 1MB area we're not remapped and it won't affect us.
		mov	ecx,cr0
		and	ecx,0x7FFFFFFF
		mov	cr0,ecx					; CR0=Disable PE
		mov	ebx,[bp+16]				; _llmemcpy_pagetables
		add	ebx,0x7000				; point at 32-bit tables
		mov	cr3,ebx					; CR3=new 64-bit page table
		mov	eax,cr4
		and	eax,~0x30
		mov	cr4,eax					; CR4=Disable PSE and PAE
		or	ecx,0x80000000
		mov	cr0,ecx					; CR0=Enable PE

		; set up return to v86 mode
		and	esp,0xFFFF	; <--- THIS IS VERY IMPORTANT ON RETURN FROM VCPI, UPPER BITS OF ESP CAN BE NONZERO
		xor	eax,eax
		mov	ax,[bp+12]	; DS
		push	eax		; SS:ESP+0x28 GS
		push	eax		; SS:ESP+0x24 FS
		push	eax		; SS:ESP+0x20 DS
		mov	ax,[bp+10]
		push	eax		; SS:ESP+0x1C ES
		mov	ax,[bp+8]
		push	eax		; SS:ESP+0x18 SS
		mov	eax,ebp
		push	eax		; SS:ESP+0x14 ESP
		pushfd			; SS:ESP+0x10 EFLAGS
		mov	ax,[bp+14]
		push	eax		; SS:ESP+0x0C CS
		push	dword .vcpi_exit; SS:ESP+0x08 EIP
		mov	eax,[bp+4]	; VCPI code segment
		push	eax		; SS:ESP+0x04 VCPI code segment
		mov	eax,[bp+0]	; VCPI offset
		push	eax
		mov	eax,0xDE0C	; switch back to v86
		jmp far	dword [esp]	; <--- 32-bit address mode required for direct use of SP, but only if we refer to ESP
.vcpi_exit:

		add	sp,extra
		pop	bp
		retnative
%undef extra

%endif
