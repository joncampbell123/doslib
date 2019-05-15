
#if !defined(TARGET_PC98)

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <dos/dos.h>
#include <isapnp/isapnp.h>

#include <hw/8254/8254.h>		/* 8254 timer */

#if TARGET_MSDOS == 32 /* 32-bit functions that thunk to the 16-bit code in the PnP BIOS */

static unsigned int isa_pnp_thunk_and_call() { /* private, don't export */
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	if (isa_pnp_pm_use) {
		unsigned char *callfunc = (unsigned char*)isa_pnp_rm_call_area + 0x8;
		unsigned short ret_ax = ~0U,my_cs = 0;

		__asm {
			mov		ax,cs
			mov		my_cs,ax
		}

		/* 32-bit far pointer used in call below */
		*((uint32_t*)(callfunc+0+0)) = 0x08+0x8;
		*((uint32_t*)(callfunc+0+4)) = isa_pnp_rm_call_area_code_sel;
		/* 386 assembly language: CALL <proc> */
		*((uint8_t *)(callfunc+8+0)) = 0x9A;
		*((uint16_t*)(callfunc+8+1)) = (uint16_t)isa_pnp_info.pm_ent_offset;
		*((uint16_t*)(callfunc+8+3)) = (uint16_t)isa_pnp_pm_code_seg;
		/* 386 assembly language: from 16-bit segment: [32-bit] call far <32-bit code segment:offset of label "asshole"> */
		*((uint8_t *)(callfunc+8+5)) = 0x66;
		*((uint8_t *)(callfunc+8+6)) = 0x67;
		*((uint8_t *)(callfunc+8+7)) = 0xEA;
		*((uint32_t*)(callfunc+8+8)) = 0;
		*((uint32_t*)(callfunc+8+12)) = my_cs;

		/* protected mode call */
		if (isa_pnp_pm_win95_mode) {
			/* Windows 95 mode: call just like below, but DON'T switch stacks (leave SS:ESP => 32-bit stack).
			 * This is what Windows 95 does and it's probably why the Bochs implementation of Plug & Play BIOS
			 * doesn't work, since everything in the PnP spec implies 16-bit FAR pointers */
			__asm {
				pushad
				push		fs
				cli

				; These stupid acrobatics are necessary because Watcom has a hang-up
				; about inline assembly referring to stack variables and doesn't like
				; inline assembly referring to label addresses
				call		acrobat1			; near call to put label address on stack
				jmp		asshole				; relative JMP address to the 'asshole' label below
acrobat1:			pop		eax				; retrieve the address of the 'acrobat1' label
				add		eax,dword ptr [eax+1]		; sum it against the relative 32-bit address portion of the JMP instruction (NTS: JMP = 0xEA <32-bit rel addr>)
				mov		esi,callfunc
				mov		dword ptr [esi+8+8],eax

;				mov		cx,ss
				mov		dx,ds
				mov		ebx,esp
;				mov		esi,ebp
;				xor		ebp,ebp
				mov		esp,isa_pnp_rm_call_area
				add		esp,0x1F0
				mov		ax,isa_pnp_rm_call_area_sel
				mov		fs,ax
				mov		ax,isa_pnp_pm_data_seg
				mov		ds,ax
				mov		es,ax

				; call
				jmpf		fword ptr fs:0x8

				; WARNING: do NOT remove these NOPs
				nop
				nop
				nop
				nop
				nop
				nop
asshole:
				nop
				nop
				nop
				nop
				nop
				nop
				nop

				; restore stack (NTS: PnP BIOSes are supposed to restore ALL regs except AX)
				cli
;				mov		ebp,esi
				mov		esp,ebx
;				mov		ss,cx
				mov		ds,dx
				mov		es,dx

				mov		ret_ax,ax

				sti
				pop		fs
				popad
			}
		}
		else {
			__asm {
				pushad
				cli

				; These stupid acrobatics are necessary because Watcom has a hang-up
				; about inline assembly referring to stack variables and doesn't like
				; inline assembly referring to label addresses
				call		acrobat1			; near call to put label address on stack
				jmp		asshole				; relative JMP address to the 'asshole' label below
acrobat1:			pop		eax				; retrieve the address of the 'acrobat1' label
				add		eax,dword ptr [eax+1]		; sum it against the relative 32-bit address portion of the JMP instruction (NTS: JMP = 0xEA <32-bit rel addr>)
				mov		esi,callfunc
				mov		dword ptr [esi+8+8],eax

				mov		cx,ss
				mov		dx,ds
				mov		ebx,esp
				mov		esi,ebp
				xor		ebp,ebp
				mov		esp,0x1F0
				mov		ax,isa_pnp_rm_call_area_sel
				mov		ss,ax
				mov		ax,isa_pnp_pm_data_seg
				mov		ds,ax
				mov		es,ax

				; call
				jmpf		fword ptr ss:0x8

				; WARNING: do NOT remove these NOPs
				nop
				nop
				nop
				nop
				nop
				nop
asshole:
				nop
				nop
				nop
				nop
				nop
				nop
				nop

				; restore stack (NTS: PnP BIOSes are supposed to restore ALL regs except AX)
				cli
				mov		ebp,esi
				mov		esp,ebx
				mov		ss,cx
				mov		ds,dx
				mov		es,dx

				mov		ret_ax,ax

				sti
				popad
			}
		}

		return ret_ax;
	}
	else {
		/* real-mode call via DPMI */
		/* ..but wait---we might be running under DOS4/G which doesn't
		   provide the "real mode call" function! */
		struct dpmi_realmode_call d={0};
		unsigned int x = (unsigned int)(&d); /* NTS: Work around Watcom's "cannot take address of stack symbol" error */
		d.cs = isa_pnp_info.rm_ent_segment;
		d.ip = isa_pnp_info.rm_ent_offset;
		d.ss = sgm;				/* our real-mode segment is also the stack during the call */
		d.sp = 0x1F0;

		if (dpmi_no_0301h < 0) probe_dpmi();

		if (dpmi_no_0301h > 0) {
			/* Fuck you DOS4/GW! */
			dpmi_alternate_rm_call_stacko(&d);
		}
		else {
			__asm {
				pushad

				push		ds
				pop		es

				mov		eax,0x0301		; DPMI call real-mode far routine
				xor		ebx,ebx
				xor		ecx,ecx
				mov		edi,x
				int		0x31

				popad
			}
		}

		if (!(d.flags & 1))
			return d.eax&0xFF;
	}

	return ~0U;
}

unsigned int isa_pnp_bios_number_of_sysdev_nodes(unsigned char far *a,unsigned int far *b) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a
	 * SEG:0x0004 = b
	 * SEG:0x00F0 = stack for passing params */
	unsigned int *rb_a = (unsigned int*)((unsigned char*)isa_pnp_rm_call_area);
	unsigned int *rb_b = (unsigned int*)((unsigned char*)isa_pnp_rm_call_area + 4);
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	unsigned int ret_ax = ~0;

	*rb_a = ~0UL; *rb_b = ~0UL;

	/* build the stack */
	stk[5] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */
	stk[4] = sgm;
	stk[3] = 4;	/* offset of "b" (segment will be filled in separately) */
	stk[2] = sgm;
	stk[1] = 0;	/* offset of "a" (ditto) */
	stk[0] = 0;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		*a = (unsigned char)(*rb_a);
		*b = *rb_b & 0xFFFF;
		return ret_ax&0xFF;
	}

	return ~0;
}

unsigned int isa_pnp_bios_get_sysdev_node(unsigned char far *a,unsigned char far *b,unsigned int c) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a (node)
	 * SEG:0x00F0 = stack for passing params
	 * SEG:0x0100 = b (buffer for xfer)
	 * SEF:0x0FFF = end */
	unsigned char *rb_a = (unsigned char*)((unsigned char*)isa_pnp_rm_call_area);
	unsigned char *rb_b = (unsigned char*)((unsigned char*)isa_pnp_rm_call_area + 0x200);
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	unsigned int ret_ax = ~0;

	*rb_a = *a;
	rb_b[0] = rb_b[1] = 0;

	stk[6] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */
	stk[5] = c;
	stk[4] = sgm;	/* offset of "b" */
	stk[3] = 0x200;
	stk[2] = sgm;	/* offset of "a" */
	stk[1] = 0;
	stk[0] = 1;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		unsigned int len = *((uint16_t*)rb_b);	/* read back the size of the device node */
		*a = (unsigned char)(*rb_a);
		if ((0x100+len) > ISA_PNP_RM_DOS_AREA_SIZE) {
			fprintf(stderr,"Whoahhhh..... the returned device struct is too large! len=%u\n",len);
			return 0xFF;
		}
		else {
			if ((ret_ax&0xFF) == 0) _fmemcpy(b,rb_b,len);
		}
		return ret_ax&0xFF;
	}

	return ~0;
}

unsigned int isa_pnp_bios_get_static_alloc_resinfo(unsigned char far *a) {
	return ~0;
}

unsigned int isa_pnp_bios_read_escd(unsigned char far *a) {
	return ~0;
}

unsigned int isa_pnp_bios_get_pnp_isa_cfg(unsigned char far *a) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a (node)
	 * SEG:0x00F0 = stack for passing params
	 * SEG:0x0100 = b (buffer for xfer)
	 * SEF:0x0FFF = end */
	unsigned char *rb_a = (unsigned char*)((unsigned char*)isa_pnp_rm_call_area + 0x200);
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	unsigned int ret_ax = ~0;

	stk[3] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */
	stk[2] = sgm;	/* offset of "a" */
	stk[1] = 0x200;
	stk[0] = 0x40;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		if ((ret_ax&0xFF) == 0) _fmemcpy(a,rb_a,sizeof(struct isapnp_pnp_isa_cfg));
		return ret_ax&0xFF;
	}

	return ~0;
}

/* UPDATE 2011/05/27: Holy shit, found an ancient Pentium Pro system who's BIOS implements the ESCD functions! */
unsigned int isa_pnp_bios_get_escd_info(unsigned int far *min_escd_write_size,unsigned int far *escd_size,unsigned long far *nv_storage_base) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a (node)
	 * SEG:0x00F0 = stack for passing params
	 * SEG:0x0100 = b (buffer for xfer)
	 * SEF:0x0FFF = end */
	unsigned short *rb_a = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x200);
	unsigned short *rb_b = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x204);
	unsigned long  *rb_c = (unsigned long*)((unsigned char*)isa_pnp_rm_call_area + 0x208);
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	unsigned int ret_ax = ~0;

	*rb_a = ~0UL;
	*rb_b = ~0UL;
	*rb_c = ~0UL;

	stk[7] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */

	stk[6] = sgm;	/* offset of "c" */
	stk[5] = 0x208;

	stk[4] = sgm;	/* offset of "b" */
	stk[3] = 0x204;

	stk[2] = sgm;	/* offset of "a" */
	stk[1] = 0x200;

	stk[0] = 0x41;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		if ((ret_ax&0xFF) == 0) {
			*min_escd_write_size = *rb_a;
			*escd_size = *rb_b;
			*nv_storage_base = *rb_c;
		}
		return ret_ax&0xFF;
	}

	return ~0;
}

unsigned int isa_pnp_bios_send_message(unsigned int msg) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a (node)
	 * SEG:0x00F0 = stack for passing params
	 * SEG:0x0100 = b (buffer for xfer)
	 * SEF:0x0FFF = end */
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned int ret_ax = ~0;

	stk[2] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */
	stk[1] = msg;

	stk[0] = 0x04;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		return ret_ax&0xFF;
	}

	return ~0U;
}

#endif

#endif //!defined(TARGET_PC98)
