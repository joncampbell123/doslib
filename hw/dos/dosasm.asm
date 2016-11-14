; dosasm.asm
;
; Assembly language support routines for dos.c
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

; extern defs for *.c code
%define this_is dosasm
%include "dos.inc"

; ---------- CODE segment -----------------
%include "_segcode.inc"

; NASM won't do it for us... make sure "retnative" is defined
%ifndef retnative
 %error retnative not defined
%endif

%ifndef TARGET_WINDOWS
 %if TARGET_MSDOS == 16

; make them global
global l_dpmi_mode, l_dpmi_rm_entry, l_dpmi_pm_entry, l_dpmi_segs, this_process_psp

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

; WARNING: Tiny model compile NOT YET TESTED

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
%ifndef TINYMODE
	mov		ax,seg _dpmi_entered
	mov		ds,ax
%else
    mov     ax,cs
    mov     ds,ax
%endif
	xor		ax,ax
	mov		bl,byte [_dpmi_entered]
	mov		byte [cs:l_dpmi_mode],bl ; the protected mode side of the function needs this
	cmp		bl,32
	jnz		.not32_ax
	or		al,1		; indicate 32-bit DPMI connection
.not32_ax:
	; so: AX=0 if 16-bit setup, AX=1 if 32-bit setup. Now for simplicity set DS==CS
%ifndef TINYMODE
	mov		bx,seg _dpmi_private_data_segment	; NTS may be zero if DPMI doesn't need it
	mov		es,bx
	mov		es,[es:_dpmi_private_data_segment]	; NTS: ES = DPMI private data. Do not modify between here and call to DPMI entry
%else
	mov		es,[_dpmi_private_data_segment]	    ; NTS: ES = DPMI private data. Do not modify between here and call to DPMI entry
%endif

%ifndef TINYMODE
	mov		bx,seg _dpmi_entry_point
	mov		ds,bx
%endif
	mov		bx,word [_dpmi_entry_point+0]
	mov		word [cs:l_dpmi_rm_entry+0],bx
	mov		bx,word [_dpmi_entry_point+2]
	mov		word [cs:l_dpmi_rm_entry+2],bx

	mov		bx,cs
	mov		ds,bx
	call far	word [cs:l_dpmi_rm_entry]
	jnc		.entry_ok
	; ENTRY FAILED. Set entered flag to zero and return
%ifndef TINYMODE
	mov		ax,seg _dpmi_entered
	mov		ds,ax
%endif
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
%ifndef TINYMODE
	mov		cx,seg _dpmi_pm_entry
	mov		ds,cx
%endif
	mov		word [_dpmi_pm_entry+0],ax
	mov		word [_dpmi_pm_entry+2],bx

	mov		ax,word [cs:l_dpmi_rm_entry]
	mov		bx,word [cs:l_dpmi_rm_entry+2]
	mov		cx,word [cs:l_dpmi_rm_entry+4]
%ifndef TINYMODE
	mov		dx,seg _dpmi_rm_entry
	mov		ds,dx
%endif
	mov		word [_dpmi_rm_entry+0],ax
	mov		word [_dpmi_rm_entry+2],bx
	mov		word [_dpmi_rm_entry+4],cx

	mov		ax,word [cs:l_dpmi_segs+0]
%ifndef TINYMODE
	mov		dx,seg _dpmi_pm_cs
	mov		ds,dx
%endif
	mov		word [_dpmi_pm_cs],ax

	mov		ax,word [cs:l_dpmi_segs+2]
%ifndef TINYMODE
	mov		dx,seg _dpmi_pm_ds
	mov		ds,dx
%endif
	mov		word [_dpmi_pm_ds],ax

	mov		ax,word [cs:l_dpmi_segs+4]
%ifndef TINYMODE
	mov		dx,seg _dpmi_pm_es
	mov		ds,dx
%endif
	mov		word [_dpmi_pm_es],ax

	mov		ax,word [cs:l_dpmi_segs+6]
%ifndef TINYMODE
	mov		dx,seg _dpmi_pm_ss
	mov		ds,dx
%endif
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

; make them global
global l_lin2fm_params,l_lin2fm_param_dst,l_lin2fm_param_lsrc,l_lin2fm_param_sz,l_rm_ret,l_rm_reentry,l_lin2fm_src_sel,l_lin2fm_dst_sel

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

 %endif
%endif

; we must explicitly defined _DATA and _TEXT to become part of the program's code and data,
; else this code will not work correctly
group DGROUP _DATA

