; dosasm.asm
;
; Assembly language support routines for dos.c
; (C) 2011-2012 Jonathan Campbell.
; Hackipedia DOS library.
;
; This code is licensed under the LGPL.
; <insert LGPL legal text here>

extern _dpmi_entered ; BYTE
extern _dpmi_entry_point ; DWORD
extern _dpmi_private_data_segment ; word
extern _dpmi_rm_entry ; qword
extern _dpmi_pm_entry ; dword
extern _dpmi_pm_cs,_dpmi_pm_ds,_dpmi_pm_es,_dpmi_pm_ss

section .text class=CODE

; NTS: If we code 'push ax' and 'popf' for the 16-bit tests in 32-bit protected mode we will screw up the stack pointer and crash
;      so we avoid duplicate code by defining 'native' pushf/popf functions and 'result' to ax or eax depending on CPU mode
%if TARGET_MSDOS == 32
 %define point_s esi
 %define result eax
 %define pushfn pushfd
 %define popfn popfd
use32
%else
 %define point_s si
 %define result ax
 %define pushfn pushf
 %define popfn popf
use16
%endif

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

%ifndef TARGET_WINDOWS
 %if TARGET_MSDOS == 16
; cheap coding: put some variables here in the code segment. as real-mode
; code there's nothing to stop us from leaving DS == CS on entry and letting
; DPMI build an alias to our own code segment
l_dpmi_mode		db	0
l_dpmi_rm_entry		dd	0	; also re-used to call entry!
			dw	0
l_dpmi_pm_entry		dd	0
; we also need to record the segments given to us by the DPMI server so
; that we can re-enter protected mode
l_dpmi_segs		dw	0,0,0,0
this_process_psp	dw	0

; void __cdecl dpmi_enter_core(); /* Watcom's inline assembler is too limiting to carry out the DPMI entry and switch back */
global _dpmi_enter_core
_dpmi_enter_core:
	; 16-bit or 32-bit?
	pushf
	pusha
	push		ds
	push		es
	push		cs
	push		ss
	cli
	mov		ax,seg _dpmi_entered
	mov		ds,ax
	xor		ax,ax
	mov		bl,byte [_dpmi_entered]
	mov		byte [cs:l_dpmi_mode],bl ; the protected mode side of the function needs this
	cmp		bl,32
	jnz		.not32_ax
	or		al,1		; indicate 32-bit DPMI connection
.not32_ax:
	; so: AX=0 if 16-bit setup, AX=1 if 32-bit setup. Now for simplicity set DS==CS
	mov		bx,seg _dpmi_private_data_segment	; NTS may be zero if DPMI doesn't need it
	mov		es,bx
	mov		es,[es:_dpmi_private_data_segment]	; NTS: ES = DPMI private data. Do not modify between here and call to DPMI entry

	mov		bx,seg _dpmi_entry_point
	mov		ds,bx
	mov		bx,word [_dpmi_entry_point+0]
	mov		word [cs:l_dpmi_rm_entry+0],bx
	mov		bx,word [_dpmi_entry_point+2]
	mov		word [cs:l_dpmi_rm_entry+2],bx

	mov		bx,cs
	mov		ds,bx
	call far	word [cs:l_dpmi_rm_entry]
	jnc		.entry_ok
	; ENTRY FAILED. Set entered flag to zero and return
	mov		ax,seg _dpmi_entered
	mov		ds,ax
	mov		byte [_dpmi_entered],0
	add		sp,4 ; discard saved CS+SS
	pop		es
	pop		ds
	popa
	popf
	retnative
; HERE: Entry succeeded. Get DPMI PM/RM entry points and then switch back to real mode.
; note that because we entered with DS == CS the DPMI server should have CS != DS but both
; refer to the same segment, as aliases. That makes our job simpler as we can use local storage
; privately in the code segment.
.entry_ok:
	mov		ax,0x0306
	int		31h

	; BX:CX real to protected mode entry point
	mov		word [l_dpmi_pm_entry+0],cx
	mov		word [l_dpmi_pm_entry+2],bx

	; save the selectors preallocated by DPMI
	mov		word [l_dpmi_segs+0],cs
	mov		word [l_dpmi_segs+2],ds
	mov		word [l_dpmi_segs+4],es
	mov		word [l_dpmi_segs+6],ss

	; SI:DI (16-bit) or SI:EDI (32-bit) protected mode to real mode entry point
	cmp		byte [l_dpmi_mode],32
	jnz		.store_16
	; 32-bit storage, and return
	mov		dword [l_dpmi_rm_entry+0],edi
	mov		word [l_dpmi_rm_entry+4],si
	pop		dx		; restore SS into DX. DX will become SS
	pop		ax		; restore CS into AX. AX will become DS
	mov		cx,ax		; CX will become ES
	mov		si,ax		; SI will become CS
	mov		bx,sp		; BX will become SP
	mov		di,.exit_ok	; DI will become IP, so direct it at the exit point below
	jmp far		dword [l_dpmi_rm_entry]
.store_16:
	; 16-bit storage, and return
	mov		word [l_dpmi_rm_entry+0],di
	mov		word [l_dpmi_rm_entry+2],si
	pop		dx		; restore SS into DX. DX will become SS
	pop		ax		; restore CS into AX. AX will become DS
	mov		cx,ax		; CX will become ES
	mov		si,ax		; SI will become CS
	mov		bx,sp		; BX will become SP
	mov		di,.exit_ok	; DI will become IP, so direct it at the exit point below
	jmp far		word [l_dpmi_rm_entry]
; jump back to realmode here
.exit_ok:

; copy results to host variables
	mov		ax,word [cs:l_dpmi_pm_entry]
	mov		bx,word [cs:l_dpmi_pm_entry+2]
	mov		cx,seg _dpmi_pm_entry
	mov		ds,cx
	mov		word [_dpmi_pm_entry+0],ax
	mov		word [_dpmi_pm_entry+2],bx

	mov		ax,word [cs:l_dpmi_rm_entry]
	mov		bx,word [cs:l_dpmi_rm_entry+2]
	mov		cx,word [cs:l_dpmi_rm_entry+4]
	mov		dx,seg _dpmi_rm_entry
	mov		ds,dx
	mov		word [_dpmi_rm_entry+0],ax
	mov		word [_dpmi_rm_entry+2],bx
	mov		word [_dpmi_rm_entry+4],cx

	mov		ax,word [cs:l_dpmi_segs+0]
	mov		dx,seg _dpmi_pm_cs
	mov		ds,dx
	mov		word [_dpmi_pm_cs],ax

	mov		ax,word [cs:l_dpmi_segs+2]
	mov		dx,seg _dpmi_pm_ds
	mov		ds,dx
	mov		word [_dpmi_pm_ds],ax

	mov		ax,word [cs:l_dpmi_segs+4]
	mov		dx,seg _dpmi_pm_es
	mov		ds,dx
	mov		word [_dpmi_pm_es],ax

	mov		ax,word [cs:l_dpmi_segs+6]
	mov		dx,seg _dpmi_pm_ss
	mov		ds,dx
	mov		word [_dpmi_pm_ss],ax

	; now that DPMI is active, we have to hook real-mode INT 21h
	; to catch program termination, so we can forward that to the DPMI
	; server for proper DPMI cleanup
	call		dpmi_hook_int21

	pop		es
	pop		ds
	popa
	popf
	retnative
 %endif
%endif

; INT 21h hook:
; We use DPMI entry and thunking back to real mode to let the host
; program remain 16-bit. BUT: there's a problem. if the host program
; exits normally with INT 21h via real mode, the DPMI server never gets
; the message and it remains stuck running in the background. To make
; DPMI exit normally, we have to hook INT 21h and reflect AH=0x4C to
; protected mode.
%ifndef TARGET_WINDOWS
 %if TARGET_MSDOS == 16
old_int21h		dd	0
dpmi_hook_int21:
	push		es
	push		ax
	push		bx
	push		cx
	xor		ax,ax
	mov		es,ax
	mov		ax,cs
	mov		bx,word [es:(0x21*4)]
	mov		cx,word [es:(0x21*4)+2]
	mov		word [es:(0x21*4)],dpmi_int21_hook_exit
	mov		word [es:(0x21*4)+2],ax
	mov		word [cs:old_int21h+0],bx
	mov		word [cs:old_int21h+2],cx

	; also keep track of this process's PSP segment, so we can readily
	; identify WHO is calling INT 21h AH=0x4C and forward to DPMI only
	; for our process, not any other process.
	mov		ah,0x62
	int		21h
	mov		word [cs:this_process_psp],bx

	pop		cx
	pop		bx
	pop		ax
	pop		es
	ret

; Our INT 21h hook. We're looking for any INT 21h AH=0x4C call coming from
; this process. If the call comes from any other program in memory, the call
; is forwarded without modification, so that DPMI does not prematurely exit
; when subprocesses started by this program terminate.
; 
; This hack seems silly but apparently most DPMI servers do not monitor real-mode
; INT 21h for the AH=0x4C call. If they never see the termination call from
; protected mode, then they never clean up for this process and in most cases
; (especially Windows) end up leaking selectors and other resources. So to avoid
; memory leaks, we must forward INT 21h AH=0x4C to the protected mode side of
; the DPMI server.
;
; TODO: This hook should also catch INT 21h AH=31 Terminate and Stay Resident,
;       DPMI needs to keep those too!
;
; FIXME: How will this code catch cases where the calling program calls INT 21h
;        from protected mode to terminate? Worst case scenario: DPMI cleans up
;        and we never get a chance to remove our INT 21h hook.
dpmi_int21_hook_exit:
	cmp		ah,0x4C
	jz		.catch_exit
	jmp far		word [cs:old_int21h]
.catch_exit:
	; this is our process terminating, not some subprocess, right?
	; we want to forward termination only for this process, not anyone else.
	push		ax
	push		bx
	mov		ah,0x62		; get PSP segment
	int		21h
	cmp		bx,word [cs:this_process_psp]
	jz		.catch_exit_psp
	pop		bx
	pop		ax
	jmp far		word [cs:old_int21h]
.catch_exit_psp:
	pop		bx
	pop		ax
	; restore the old vector
	push		es
	push		ax
	push		bx
	push		cx
	xor		ax,ax
	mov		es,ax
	mov		ax,cs
	mov		bx,word [cs:old_int21h+0]
	mov		cx,word [cs:old_int21h+2]
	mov		word [es:(0x21*4)],bx
	mov		word [es:(0x21*4)+2],cx
	pop		cx
	pop		bx
	pop		ax
	pop		es
	; OK. Switch into protected mode.
	; use the segment values given to us by the DPMI server.
	cli
	mov		bp,ax				; save AX
	mov		ax,word [cs:l_dpmi_segs+2]	; AX becomes DS (so load DS from DPMI env)
	mov		cx,ax				; CX becomes ES
	mov		dx,ax				; DX becomes SS (doesn't matter)
	mov		bx,sp				; BX becomes SP (doesn't matter)
	mov		si,word [cs:l_dpmi_segs+0]	; SI becomes CS (so load CS from DPMI env)
	mov		di,.catch_exit_pmode		; DI becomes IP
	jmp far		word [cs:l_dpmi_pm_entry]
.catch_exit_pmode:
	mov		ax,bp
	mov		ah,0x4C
	int		21h			; now issue INT 21h AH=0x4C where the DPMI server can see it
	hlt
 %endif
%endif

%if TARGET_MSDOS == 16
 %ifndef TARGET_WINDOWS

; WARNING: The caller must have ensured we are running on a 386 or higher, and that
;          the DPMI entry points were obtained

; __cdecl: right-to-left argument passing (meaning: caller does "push sz", "push lsrc", "push dst"...)
l_lin2fm_params:
l_lin2fm_param_dst:	dd	0	; unsigned char far *dst
l_lin2fm_param_lsrc:	dd	0	; uint32_t lsrc
l_lin2fm_param_sz:	dd	0	; uint32_t sz
			; = 12 bytes

l_rm_ret		dw	0
l_rm_reentry		dd	0
			dw	0

; TODO: Export these so they are visible as C variables
; we need these selectors for copy operation
l_lin2fm_src_sel	dw	0
l_lin2fm_dst_sel	dw	0

; dpmi_pm_cs,dpmi_pm_ds,dpmi_pm_es,dpmi_pm_ss
; int __cdecl dpmi_lin2fmemcpy_32(unsigned char far *dst,uint32_t lsrc,uint32_t sz);
global _dpmi_lin2fmemcpy_32
_dpmi_lin2fmemcpy_32:
	push	bp
	mov	bp,sp

	; copy params, we need them in protected mode
	mov	eax,dword [bp+cdecl_param_offset+0]
	mov	dword [cs:l_lin2fm_params+0],eax
	mov	eax,dword [bp+cdecl_param_offset+4]
	mov	dword [cs:l_lin2fm_params+4],eax
	mov	eax,dword [bp+cdecl_param_offset+8]
	mov	dword [cs:l_lin2fm_params+8],eax

	pusha			; save all regs

	push	ds
	push	es

	push	cs		; realmode re-entry needs this
	push	ss		; realmode re-entry needs this
	push	ds		; realmode re-entry needs this

	mov	ax,seg _dpmi_pm_entry
	mov	ds,ax

	xor	ax,ax
	mov	word [cs:l_rm_ret],ax

	mov	eax,dword [_dpmi_rm_entry+0]
	mov	dword [cs:l_rm_reentry+0],eax

	mov	ax,word [_dpmi_rm_entry+4]
	mov	word [cs:l_rm_reentry+4],ax

	mov	ax,word [_dpmi_pm_ds]
	mov	cx,ax
	mov	dx,word [_dpmi_pm_ss]
	mov	bx,sp
	mov	si,word [_dpmi_pm_cs]
	mov	di,.entry_pm
	call far word [_dpmi_pm_entry]
	; didn't make it. error return
	add	sp,6		; do not restore SS+CS+DS, just discard
	pop	es
	pop	ds
	popa
	pop	bp
	xor	ax,ax		; return 0 == no copy made
	retnative
.entry_pm:

	; we need to allocate two selectors to do the copy operation with
	cmp	word [l_lin2fm_src_sel],0
	jnz	.sel_avail		; if != 0, then skip code
	; allocate two descriptors
	xor	ax,ax
	mov	cx,2
	int	31h
	jnc	.sel_alloced		; if carry clear, continue
	jmp	.go_to_exit_pm		; else return to RM with retval == 0
.sel_alloced:
	; we got two descriptors, store them
	mov	word [l_lin2fm_src_sel],ax
	add	ax,8			; obviously...
	mov	word [l_lin2fm_dst_sel],ax
	; we need to make them data selectors
	mov	ax,0x0009		; DPMI Set Descriptor Access Rights
	mov	bx,word [l_lin2fm_src_sel]
	mov	cl,0xF0			; P=1 DPL=3 data expand-up r/o. I know DPMI says it must equal our level, but Windows always runs us Ring-3 so we can assume
	xor	ch,ch			; 16-bit selector (we are 16-bit code!)
	int	31h
	jc	short $			; FIXME:For now, hang if the request failed
	mov	ax,0x0008		; DPMI Set Selector Limit
	mov	bx,word [l_lin2fm_src_sel]
	xor	cx,cx
	xor	dx,dx
	dec	dx			; CX:DX = 0000:FFFF
	int	31h
	; and the other one
	mov	ax,0x0009		; DPMI Set Descriptor Access Rights
	mov	bx,word [l_lin2fm_dst_sel]
	mov	cl,0xF2			; P=1 DPL=3 data expand-up r/w. I know DPMI says it must equal our level, but Windows always runs us Ring-3 so we can assume
	xor	ch,ch			; 16-bit selector (we are 16-bit code!)
	int	31h
	jc	short $			; FIXME:For now, hang if the request failed
	mov	ax,0x0008		; DPMI Set Selector Limit
	mov	bx,word [l_lin2fm_dst_sel]
	xor	cx,cx
	xor	dx,dx
	dec	dx			; CX:DX = 0000:FFFF
	int	31h
.sel_avail:
	; OK, pull in source address (flat) from param and set the selector base
	mov	ax,0x0007
	mov	bx,word [l_lin2fm_src_sel]
	mov	dx,word [l_lin2fm_param_lsrc+0]	; CX:DX = base
	mov	cx,word [l_lin2fm_param_lsrc+2]
	int	31h
	; and the dest address (realmode seg:off) too
	movzx	eax,word [l_lin2fm_param_dst+2]
	shl	eax,4
	movzx	ebx,word [l_lin2fm_param_dst+0]
	add	eax,ebx	
	mov	dx,ax
	shr	eax,16
	mov	cx,ax
	mov	bx,word [l_lin2fm_dst_sel]
	mov	ax,0x0007
	int	31h
	; alright then, do the memcpy
	mov	cx,word [l_lin2fm_param_sz]
	mov	word [l_rm_ret],cx		; set return value too
	push	ds
	push	es
	cld
	mov	ax,word [l_lin2fm_src_sel]
	mov	bx,word [l_lin2fm_dst_sel]
	mov	ds,ax
	mov	es,bx
	xor	si,si
	mov	di,si
	rep	movsb			; ES:DI <- DS:SI
	pop	es
	pop	ds
.go_to_exit_pm:
	; NTS: when dpmi_enter_core() did it's job it made sure DS == CS
	; so the DPMI server would make DS an alias of CS in protected mode
	pop	ax		; AX = realmode DS
	mov	cx,ax
	pop	dx		; DX = realmode SS
	pop	si		; SI = realmode CS
	mov	bx,sp
	mov	di,.exit_pm
	call far dword [l_rm_reentry] ; NTS: We're using the 32-bit DPMI server, the RM entry point is 16:32 format
.exit_pm: ; NTS: Don't forget CS+DS+SS was pushed but the PM part popped them as part of returning
	pop	es
	pop	ds
	popa

	pop	bp
	mov	ax,word [cs:l_rm_ret]
	retnative

; NOTE: This version of the code is written to work with 16-bit DPMI servers,
;       and to work within the constraint that we could be run on a 286 where
;       32-bit registers are not available.
; dpmi_pm_cs,dpmi_pm_ds,dpmi_pm_es,dpmi_pm_ss
; int __cdecl dpmi_lin2fmemcpy_16(unsigned char far *dst,uint32_t lsrc,uint32_t sz);
global _dpmi_lin2fmemcpy_16
_dpmi_lin2fmemcpy_16:
	push	bp
	mov	bp,sp

	; copy params, we need them in protected mode
	mov	ax,word [bp+cdecl_param_offset+0]
	mov	word [cs:l_lin2fm_params+0],ax
	mov	ax,word [bp+cdecl_param_offset+2]
	mov	word [cs:l_lin2fm_params+2],ax
	mov	ax,word [bp+cdecl_param_offset+4]
	mov	word [cs:l_lin2fm_params+4],ax
	mov	ax,word [bp+cdecl_param_offset+6]
	mov	word [cs:l_lin2fm_params+6],ax
	mov	ax,word [bp+cdecl_param_offset+8]
	mov	word [cs:l_lin2fm_params+8],ax
	mov	ax,word [bp+cdecl_param_offset+10]
	mov	word [cs:l_lin2fm_params+10],ax

	pusha			; save all regs

	push	ds
	push	es

	push	cs		; realmode re-entry needs this
	push	ss		; realmode re-entry needs this
	push	ds		; realmode re-entry needs this

	mov	ax,seg _dpmi_pm_entry
	mov	ds,ax

	xor	ax,ax
	mov	word [cs:l_rm_ret],ax

	mov	eax,dword [_dpmi_rm_entry+0]
	mov	dword [cs:l_rm_reentry+0],eax

	mov	ax,word [_dpmi_rm_entry+4]
	mov	word [cs:l_rm_reentry+4],ax

	mov	ax,word [_dpmi_pm_ds]
	mov	cx,ax
	mov	dx,word [_dpmi_pm_ss]
	mov	bx,sp
	mov	si,word [_dpmi_pm_cs]
	mov	di,.entry_pm
	call far word [_dpmi_pm_entry]
	; didn't make it. error return
	add	sp,6		; do not restore SS+CS+DS, just discard
	pop	es
	pop	ds
	popa
	pop	bp
	xor	ax,ax		; return 0 == no copy made
	retnative
.entry_pm:

	; we need to allocate two selectors to do the copy operation with
	cmp	word [l_lin2fm_src_sel],0
	jnz	.sel_avail		; if != 0, then skip code
	; allocate two descriptors
	xor	ax,ax
	mov	cx,2
	int	31h
	jnc	.sel_alloced		; if carry clear, continue
	jmp	.go_to_exit_pm		; else return to RM with retval == 0
.sel_alloced:
	; we got two descriptors, store them
	mov	word [l_lin2fm_src_sel],ax
	add	ax,8			; obviously...
	mov	word [l_lin2fm_dst_sel],ax
	; we need to make them data selectors
	mov	ax,0x0009		; DPMI Set Descriptor Access Rights
	mov	bx,word [l_lin2fm_src_sel]
	mov	cl,0xF0			; P=1 DPL=3 data expand-up r/o. I know DPMI says it must equal our level, but Windows always runs us Ring-3 so we can assume
	xor	ch,ch			; 16-bit selector (we are 16-bit code!)
	int	31h
	jc	short $			; FIXME:For now, hang if the request failed
	mov	ax,0x0008		; DPMI Set Selector Limit
	mov	bx,word [l_lin2fm_src_sel]
	xor	cx,cx
	xor	dx,dx
	dec	dx			; CX:DX = 0000:FFFF
	int	31h
	; and the other one
	mov	ax,0x0009		; DPMI Set Descriptor Access Rights
	mov	bx,word [l_lin2fm_dst_sel]
	mov	cl,0xF2			; P=1 DPL=3 data expand-up r/w. I know DPMI says it must equal our level, but Windows always runs us Ring-3 so we can assume
	xor	ch,ch			; 16-bit selector (we are 16-bit code!)
	int	31h
	jc	short $			; FIXME:For now, hang if the request failed
	mov	ax,0x0008		; DPMI Set Selector Limit
	mov	bx,word [l_lin2fm_dst_sel]
	xor	cx,cx
	xor	dx,dx
	dec	dx			; CX:DX = 0000:FFFF
	int	31h
.sel_avail:
	; OK, pull in source address (flat) from param and set the selector base
	mov	ax,0x0007
	mov	bx,word [l_lin2fm_src_sel]
	mov	dx,word [l_lin2fm_param_lsrc+0]	; CX:DX = base
	mov	cx,word [l_lin2fm_param_lsrc+2]
	int	31h
	; and the dest address (realmode seg:off) too
	mov	dx,word [l_lin2fm_param_dst+2]	; DX = (seg << 4) + offset, CX = (seg >> 12) + (carry flag result of computing DX)
	mov	cx,dx
	shr	cx,12
	shl	dx,4
	add	dx,word [l_lin2fm_param_dst+0]
	adc	cx,0				; CX:DX = 32-bit linear address
	mov	bx,word [l_lin2fm_dst_sel]
	mov	ax,0x0007
	int	31h
	; alright then, do the memcpy
	mov	cx,word [l_lin2fm_param_sz]
	mov	word [l_rm_ret],cx		; set return value too
	push	ds
	push	es
	cld
	mov	ax,word [l_lin2fm_src_sel]
	mov	bx,word [l_lin2fm_dst_sel]
	mov	ds,ax
	mov	es,bx
	xor	si,si
	mov	di,si
	rep	movsb			; ES:DI <- DS:SI
	pop	es
	pop	ds
.go_to_exit_pm:
	; NTS: when dpmi_enter_core() did it's job it made sure DS == CS
	; so the DPMI server would make DS an alias of CS in protected mode
	pop	ax		; AX = realmode DS
	mov	cx,ax
	pop	dx		; DX = realmode SS
	pop	si		; SI = realmode CS
	mov	bx,sp
	mov	di,.exit_pm
	call far word [l_rm_reentry] ; NTS: We're using the 16-bit DPMI server, the RM entry point is 16:16 format
.exit_pm: ; NTS: Don't forget CS+DS+SS was pushed but the PM part popped them as part of returning
	pop	es
	pop	ds
	popa

	pop	bp
	mov	ax,word [cs:l_rm_ret]
	retnative

 %endif
%endif
