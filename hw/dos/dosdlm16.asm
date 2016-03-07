
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

%if TARGET_MSDOS == 16
 %ifndef TARGET_WINDOWS

extern l_lin2fm_params
extern l_lin2fm_param_dst
extern l_lin2fm_param_lsrc
extern l_lin2fm_param_sz
extern l_rm_ret
extern l_rm_reentry
extern l_lin2fm_src_sel
extern l_lin2fm_dst_sel

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

