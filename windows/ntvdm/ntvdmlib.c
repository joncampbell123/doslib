
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>
#include <i86.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>
#include <windows/ntvdm/ntvdmlib.h>

#if !defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
static uint16_t		ntvdm_rm_code_sel = 0;		/* 1024 bytes long */
static unsigned char*	ntvdm_rm_code_ptr = NULL;

int ntvdm_rm_code_alloc() {
	if (ntvdm_rm_code_sel != 0)
		return 1;

	ntvdm_rm_code_ptr = dpmi_alloc_dos(1024,&ntvdm_rm_code_sel);
	if (ntvdm_rm_code_ptr == NULL)
		return 0;

	return 1;
}

void ntvdm_rm_code_call(struct dpmi_realmode_call *rc) {
	rc->cs = ((size_t)ntvdm_rm_code_ptr) >> 4UL;
	rc->ip = ((size_t)ntvdm_rm_code_ptr) & 0xFUL;

	if (dpmi_no_0301h > 0) {
		/* Fuck you DOS4/GW! */
		dpmi_alternate_rm_call(rc);
	}
	else {
		__asm {
			mov	ax,0x0301
			xor	bx,bx
			xor	cx,cx
			mov	edi,rc		; we trust Watcom has left ES == DS
			int	0x31		; call DPMI
		}
	}
}
#endif

#if !defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
void ntvdm_DispatchCall_dpmi(uint16_t handle,struct dpmi_realmode_call *rc) {
	unsigned char *p = ntvdm_rm_code_ptr;

	if (p == NULL) {
		fprintf(stderr,"ERROR: Attempt to call NTVDM.EXE dispatch call to real mode without allocating first!\n");
		abort(); /* Sorry, this is a serious problem! */
	}

	*p++ = 0xC4;	/* NTVDM.EXE RegisterModule */
	*p++ = 0xC4;
	*p++ = 0x58;
	*p++ = 0x02;
	*p++ = 0xCB;	/* RETF */

	rc->eax = handle;
	ntvdm_rm_code_call(rc);
}
#endif

void ntvdm_UnregisterModule(uint16_t handle) {
#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
	/* No! */
#elif defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
	/* the NTVDM.EXE doscall thing doesn't really work well from Win16 */
#elif TARGET_MSDOS == 32
	/* write the code */
	{
		struct dpmi_realmode_call rc={0};
		unsigned char *p = ntvdm_rm_code_ptr;
		*p++ = 0xC4;	/* NTVDM.EXE RegisterModule */
		*p++ = 0xC4;
		*p++ = 0x58;
		*p++ = 0x01;
		*p++ = 0xCB;	/* RETF */

		rc.eax = handle;
		ntvdm_rm_code_call(&rc);
	}
#else
	__asm {
		mov		ax,handle

		; NTVDM.EXE call: UnregisterModule()
		db		0xC4,0xC4,0x58,0x01
	}
#endif
}

uint16_t ntvdm_RegisterModule(const char *dll_name,const char *dll_init_routine,const char *dll_dispatch_routine) {
	uint16_t retx = ntvdm_RM_ERR_DLL_NOT_FOUND;

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
	/* Not applicable */
	return ntvdm_RM_ERR_NOT_AVAILABLE;
#elif defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
	/* Don't. It seems to only cause a GPF in KRNL386.EXE anyway. Besides, as a Win16
	 * app under NTVDM.EXE you can use CallProcEx32W() if you need to call Win32 code */
	return ntvdm_RM_ERR_NOT_AVAILABLE;
#elif TARGET_MSDOS == 32
	/* problem: NTVDM.EXE handles the BOP secret opcode just as it does in real mode,
	 *          except that if used in protected mode the BOP doesn't do anything.
	 * solution: make a temporary DOS segment, put the BOP in it (and an IRET) and
	 *           use DPMI to call the real-mode "procedure" we just made up to carry
	 *           out the action */
	if (!ntvdm_rm_code_alloc())
		return ntvdm_RM_ERR_NOT_AVAILABLE;

	/* write the code */
	{
		uint16_t a,b,c,l;
		struct dpmi_realmode_call rc={0};
		unsigned char *f = ntvdm_rm_code_ptr + 1023;
		unsigned char *p = ntvdm_rm_code_ptr;
		*p++ = 0xC4;	/* NTVDM.EXE RegisterModule */
		*p++ = 0xC4;
		*p++ = 0x58;
		*p++ = 0x00;
		*p++ = 0xCB;	/* RETF */

		/* followed by the strings */
		a = (uint16_t)(p - ntvdm_rm_code_ptr);
		l = strlen(dll_name);
		if ((l+p+1) < f) {
			memcpy(p,dll_name,l+1);
			p += l+1;
		}
		else {
			*p++ = 0;
		}

		if (dll_init_routine) {
			b = (uint16_t)(p - ntvdm_rm_code_ptr);
			l = strlen(dll_init_routine);
			if ((l+p+1) < f) {
				memcpy(p,dll_init_routine,l+1);
				p += l+1;
			}
			else {
				*p++ = 0;
			}
		}
		else {
			b = 0;
		}

		c = (uint16_t)(p - ntvdm_rm_code_ptr);
		l = strlen(dll_dispatch_routine);
		if ((l+p+1) < f) {
			memcpy(p,dll_dispatch_routine,l+1);
			p += l+1;
		}
		else {
			*p++ = 0;
		}

		rc.ds = ((size_t)ntvdm_rm_code_ptr) >> 4UL;
		rc.es = dll_init_routine != NULL ? rc.ds : 0;
		rc.esi = a;
		rc.edi = b;
		rc.ebx = c;
		rc.ecx = 0x32323232; /* this is a 32-bit build */
		ntvdm_rm_code_call(&rc);

		if (rc.flags & 1)
			retx = (uint16_t)(-(rc.eax&0xFFFFUL));
		else
			retx = (uint16_t)rc.eax;
	}
#elif defined(__LARGE__) || defined(__COMPACT__)
	/* problem: all three pointers above could be in different segments.
	 * solution: make a temporary buffer (one segment), copy the strings in */
	{
# define MAX 4096
		uint16_t s,b,c,l;
		unsigned char far *tmp = _fmalloc(MAX);
		if (tmp == NULL) return 0xFFFFU;

		s = 0;
		l = _fstrlen(dll_name);
		if ((l+s+1) < MAX) {
			_fmemcpy(tmp+s,dll_name,l+1);
			s += l+1;
		}
		else {
			tmp[s++] = 0;
		}

		if (dll_init_routine) {
			b = s;
			l = _fstrlen(dll_init_routine);
			if ((l+s+1) < MAX) {
				_fmemcpy(tmp+s,dll_init_routine,l+1);
				s += l+1;
			}
			else {
				tmp[s++] = 0;
			}
		}
		else {
			b = 0;
		}

		c = s;
		l = _fstrlen(dll_dispatch_routine);
		if ((l+s+1) < MAX) {
			_fmemcpy(tmp+s,dll_dispatch_routine,l+1);
			s += l+1;
		}
		else {
			tmp[s++] = 0;
		}

		__asm {
			.386p
			pusha
			push		ds
			push		es
			clc
			lds		si,tmp			/* DLL name */

			xor		di,di
			mov		es,di

			test		b,0xFFFF
			jz		no_init
			mov		ax,ds
			mov		es,ax
			mov		di,b
			add		di,si
no_init:

			mov		bx,c
			add		bx,si
			xor		ax,ax
			mov		cx,ax

			; NTVDM.EXE call: RegisterModule()
			db		0xC4,0xC4,0x58,0x00

			pop		es
			pop		ds
			jnc		noerr
			neg		ax			/* 1 becomes -1, 2 becomes -2, etc. */
noerr:			mov		retx,ax
			popa
		}

		_ffree(tmp);
	}
# undef MAX
#else
	/* DS=ES and the strings are in the data segment */
	__asm {
		.386p
		pusha
		push		es
		clc
		mov		si,word ptr [dll_name]

		xor		di,di
		mov		es,di
		test		word ptr [dll_init_routine],0xFFFF
		jz		no_init
		mov		ax,ds
		mov		es,ax
		mov		di,word ptr [dll_init_routine]
no_init:

		mov		bx,word ptr [dll_dispatch_routine]
		xor		ax,ax
		mov		cx,ax

		; NTVDM.EXE call: RegisterModule()
		db		0xC4,0xC4,0x58,0x00

		pop		es
		jnc		noerr
		neg		ax			/* 1 becomes -1, 2 becomes -2, etc. */
noerr:		mov		retx,ax
		popa
	}
#endif

	return retx;
}

