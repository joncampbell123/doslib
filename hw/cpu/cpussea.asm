
%if TARGET_MSDOS == 16
 %ifndef TARGET_WINDOWS
extern _dpmi_pm_cs
extern _dpmi_pm_ds
extern _dpmi_pm_ss
extern _dpmi_pm_es
extern _dpmi_entered
extern _dpmi_pm_entry
extern _dpmi_rm_entry
 %endif
%endif

global cpu_basic_probe_
extern _cpu_flags
extern _cpu_tmp1
extern _cpu_cpuid_max
; char cpu_v86_active
; extern _cpu_v86_active
; char cpu_cpuid_vendor[13];
extern _cpu_cpuid_vendor
; struct cpu_cpuid_feature cpu_cpuid_features;
extern _cpu_cpuid_features
; NTS: Do NOT define variables here, Watcom or NASM is putting them in the wrong places (like at 0x000!)

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
; unsigned int _cdecl cpu_dpmi_win9x_sse_test();
; NOTE: The caller should have first checked dpmi_present, and that dpmi_pm_entry, and dpmi_rm_entry
; are valid. We assume they are.
global _cpu_dpmi_win9x_sse_test
_cpu_dpmi_win9x_sse_test:
	push	ds
	push	es
	push	ss

; copy return vector
	mov	ax,seg _dpmi_rm_entry
	mov	ds,ax
	mov	ax,[_dpmi_rm_entry+0]
	mov	word [cs:_cpu_dpmi_return_vec+0],ax
	mov	ax,[_dpmi_rm_entry+2]
	mov	word [cs:_cpu_dpmi_return_vec+2],ax
	mov	ax,[_dpmi_rm_entry+4]
	mov	word [cs:_cpu_dpmi_return_vec+4],ax

; save some key registers
	mov	word [cs:_cpu_dpmi_saved_cs],cs
	mov	word [cs:_cpu_dpmi_saved_ss],ss
	mov	ax,sp
	mov	word [cs:_cpu_dpmi_saved_sp],ax

	mov	ax,seg _dpmi_entered
	mov	ds,ax
	mov	al,[_dpmi_entered]
	mov	byte [cs:_cpu_dpmi_mode],al
	mov	byte [cs:_cpu_dpmi_result],0

; enter protected mode
	mov	ax,seg _dpmi_pm_ds
	mov	ds,ax
	mov	ax,[_dpmi_pm_ds]
	mov	cx,ax

	mov	dx,seg _dpmi_pm_ss
	mov	ds,dx
	mov	dx,[_dpmi_pm_ss]

	mov	si,seg _dpmi_pm_cs
	mov	ds,si
	mov	si,[_dpmi_pm_cs]

	mov	bx,seg _dpmi_pm_entry
	mov	es,bx
	mov	bx,sp
	mov	di,.enterpm
	jmp far word [es:_dpmi_pm_entry]
.enterpm:
; remember: when the DOS code first entered, DS == CS so DS is an alias to CS

; the path we take depends on 16-bit or 32-bit dpmi!
	cmp	byte [cs:_cpu_dpmi_mode],32
	jz	.path32
; 16-bit path
	mov	ax,0x0202		; get current exception vector for #UD
	mov	bl,0x06
	int	31h
	push	cx
	push	dx			; save on stack

	mov	ax,0x0203		; set exception vector for #UD to our test routine
	mov	bl,0x06
	mov	cx,cs
	mov	edx,_cpu_dpmi_win9x_sse_test_except16
	int	31h

	; the exception handler will set DS:_cpu_dpmi_result = 1
	xorps	xmm0,xmm0

	pop	dx			; restore exception vector for #UD
	pop	cx
	mov	ax,0x0203
	mov	bl,0x06
	int	31h

	; now return to real mode
	mov	ax,word [_cpu_dpmi_saved_cs]
	mov	cx,ax
	mov	si,ax
	mov	dx,word [_cpu_dpmi_saved_ss]
	mov	bx,sp
	mov	di,.exitpm
	jmp far word [_cpu_dpmi_return_vec]
; 32-bit path
.path32:
	mov	ax,0x0202		; get current exception vector for #UD
	mov	bl,0x06
	int	31h
	push	ecx
	push	edx			; save on stack

	mov	ax,0x0203		; set exception vector for #UD to our test routine
	mov	bl,0x06
	mov	cx,cs
	mov	edx,_cpu_dpmi_win9x_sse_test_except32
	int	31h

	; the exception handler will set DS:_cpu_dpmi_result = 1
	xorps	xmm0,xmm0

	pop	edx			; restore exception vector for #UD
	pop	ecx
	mov	ax,0x0203
	mov	bl,0x06
	int	31h

	; now return to real mode
	mov	ax,word [_cpu_dpmi_saved_cs]
	mov	cx,ax
	mov	si,ax
	mov	dx,word [_cpu_dpmi_saved_ss]
	mov	bx,sp
	mov	di,.exitpm
	jmp far dword [_cpu_dpmi_return_vec]

.exitpm:
; exit point
	pop	ss
	pop	es
	pop	ds
	movzx	result,byte [cs:_cpu_dpmi_result]
	retnative

; exception handler. skip the offending instruction by directly modifying "IP" on the stack
_cpu_dpmi_win9x_sse_test_except32:
	push	bp
	mov	bp,sp
	mov	byte [_cpu_dpmi_result],1
	add	dword [bp+2+12],3	; 'xorps xmm0,xmm0' is 3 bytes long
	pop	bp
	o32 retf

; exception handler. skip the offending instruction by directly modifying "IP" on the stack
_cpu_dpmi_win9x_sse_test_except16:
	push	bp
	mov	bp,sp
	mov	byte [_cpu_dpmi_result],1
	add	word [bp+2+6],3	; 'xorps xmm0,xmm0' is 3 bytes long
	pop	bp
	retf

_cpu_dpmi_saved_cs	dw	0
_cpu_dpmi_saved_ss	dw	0
_cpu_dpmi_saved_sp	dw	0
_cpu_dpmi_mode		db	0
_cpu_dpmi_result	db	0
_cpu_dpmi_return_vec	dd	0
			dw	0

 %endif
%endif

