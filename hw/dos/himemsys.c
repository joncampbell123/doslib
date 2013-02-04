/* himemsys.c
 *
 * Support calls to use HIMEM.SYS
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

/* HIMEM.SYS api for DOS programs library
 *
 *
 *
 *
 * Testing:
 *
 * System/configuration                 Works?   Supports >= 64MB?
 * DOSBox 0.74                          YES      NO, BUGS
 * DOSBox 0.74 +
 *   Microsoft Windows 3.0
 *     Real mode                        YES      --
 *     Standard mode                    NO       --                      Reports 0KB free memory (why?)
 *     386 Enhanced mode                YES      --
 *   Microsoft Windows 3.1
 *     Standard mode                    NO       --                      Reports 0KB free memory (why?)
 *     386 Enhanced mode                YES      --
 *   Microsoft Windows 3.11
 *     Standard mode                    NO       --                      Reports 0KB free memory (why?)
 *     386 Enhanced mode                YES      --
 * QEMU +
 *   Microsoft Windows 95 (4.00.950)
 *     Normal mode                      YES      YES                     Allows my program to request more memory than available, then triggers the "needs MS-DOS mode" warning (PIF: XMS memory setting on "auto")
 *     Normal mode (PIF: XMS=2MB)       YES      YES                     This program's attempts to alloc > 1MB fail (correctly). It still triggers the "needs MS-DOS mode" dialog
 *     Safe mode                        YES      YES                     Allows my program to request more memory than available, then triggers the "needs MS-DOS mode" warning (PIF: XMS memory setting on "auto")
 *     MS-DOS mode (official)           YES      YES
 *     MS-DOS mode (gui=0)              YES      YES
 *       * NOTE: I also noticed that within the DOS box the Windows kernel denies all requests to lock a handle
 *   Microsoft Windows 98 (4.10.1998)
 *     Normal mode                      YES      YES                     Same problem as Windows 95
 *     MS-DOS mode (gui=0)              YES      YES
 *   Microsoft Windows ME (4.90.3000)
 *     Normal mode                      YES      YES                     Same problem as Windows 95, triggers "needs MS-DOS mode" warning----Hey wait, Windows ME doesn't have a "DOS mode". A hilarious oversight by Microsoft.
 *   Microsoft Windows 2000 Professional
 *     Normal mode                      YES      NO                      NTVDM is very conservative about HIMEM.SYS allocation, listing the largest block size as 919KB. So apparently the default is that MS-DOS
 *                                                                       applications are allowed up to 1MB of extended memory? The usual MS-DOS configuration options are there, suggesting that in reality the
 *                                                                       program should have NO extended memory (?). Apparently when you say "none" what it really means is "1MB". Locking the handle is permitted though.
 *                                                                       The highest value you can enter in the PIF through the GUI is 65534. Setting to 65535 somehow triggers internally the "auto" setting, and is
 *                                                                       the highest value the editor will let you type in.
 *   Microsoft Windows XP Professional
 *     Normal mode                      YES      NO                      Same problems as Windows 2000
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/himemsys.h>

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/*===================================== MS-DOS only ===================================*/

unsigned long himem_sys_largest_free = 0;
unsigned long himem_sys_total_free = 0;
unsigned char himem_sys_present = 0;
unsigned int himem_sys_version = 0;
unsigned long himem_sys_entry = 0;
unsigned char himem_sys_flags = 0;

#if TARGET_MSDOS == 32
static void himem_sys_realmode_2F_call(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x002F
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}

/* WARNING: If this code is run under DOS4/GW it will silently fail.
            If the HIMEM.SYS test program spouts nonsense about a
	    HIMEM.SYS that is v0.00 and has some random amount of memory
	    open, that's why. Make sure you link with dos32a. If that's
	    not possible, then run your program with dos32 like this:

	    dos32a <program> */
static void himem_sys_realmode_entry_call(struct dpmi_realmode_call *rc) {
	rc->ip = himem_sys_entry & 0xFFFF;
	rc->cs = (himem_sys_entry >> 16UL);

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

int probe_himem_sys() {
	struct dpmi_realmode_call rc={0};
	union REGS regs;

	himem_sys_present = 0;

#if TARGET_MSDOS == 32
	/* WAIT!!! We might be running under DOS4/GW. Make sure we can
	   call real-mode subroutines with the DPMI server. */
	if (dpmi_no_0301h < 0) probe_dpmi();
#endif

	regs.w.ax = 0x4300;
	int386(0x2F,&regs,&regs);
	if (regs.h.al != 0x80) return 0;
	himem_sys_present = 1;

	/* use the realmode DPMI call to ensure the DPMI server does not screw up (translate) the segment register */
	rc.eax = 0x4310;
	himem_sys_realmode_2F_call(&rc);
	himem_sys_entry =
		((unsigned long)rc.es << 16UL) |
		((unsigned long)rc.ebx & 0xFFFFUL);

	/* get version info, and autodetect whether it supports extended functions */
	rc.eax = 0;
	himem_sys_realmode_entry_call(&rc);
	himem_sys_version = rc.eax & 0xFFFF;
	himem_sys_flags = (rc.edx & 1) ? HIMEM_F_HMA : 0; /* FIXME: Am I crazy, or does HIMEM.SYS suddenly stop mentioning HMA when we call from protected mode? */

	rc.ebx = 0;
	rc.eax = 0x8800;
	himem_sys_realmode_entry_call(&rc);
	himem_sys_flags = (rc.ebx & 0xFF == 0x80) ? 0 : HIMEM_F_4GB;

	return 1;
}

int himem_sys_global_a20(int enable) {
	struct dpmi_realmode_call rc={0};
	if (!himem_sys_present) return 0;
	rc.eax = ((enable > 0) ? 3 : 4) << 8;
	himem_sys_realmode_entry_call(&rc);
	return rc.eax;
}

int himem_sys_local_a20(int enable) {
	struct dpmi_realmode_call rc={0};
	if (!himem_sys_present) return 0;
	rc.eax = ((enable > 0) ? 5 : 6) << 8;
	himem_sys_realmode_entry_call(&rc);
	return rc.eax;
}

int himem_sys_query_a20() {
	struct dpmi_realmode_call rc={0};
	if (!himem_sys_present) return 0;
	rc.eax = 7 << 8;
	himem_sys_realmode_entry_call(&rc);
	return rc.eax;
}

/* NTS: This function will likely set largest & free variables to zero,
 *      because most 32-bit DOS extenders take up all extended memory to do their work */
void himem_sys_update_free_memory_status() {
	struct dpmi_realmode_call rc={0};
	if (!himem_sys_present) return;

	if (himem_sys_flags & HIMEM_F_4GB) {
		rc.eax = 0x88 << 8;
		himem_sys_realmode_entry_call(&rc);
		himem_sys_largest_free = rc.eax;
		himem_sys_total_free = rc.edx;
	}
	else {
		rc.eax = 8 << 8;
		himem_sys_realmode_entry_call(&rc);
		himem_sys_largest_free = rc.eax & 0xFFFF;
		himem_sys_total_free = rc.edx & 0xFFFF;
	}
}

int __cdecl himem_sys_alloc(unsigned long size/* in KB---not bytes*/) {
	struct dpmi_realmode_call rc={0};
	int handle = -1;

	if (himem_sys_present) {
		if (himem_sys_flags & HIMEM_F_4GB) {
			rc.eax = 0x89 << 8;
			rc.edx = size;
			himem_sys_realmode_entry_call(&rc);
			if ((rc.eax & 0xFFFF) == 1) handle = rc.edx & 0xFFFF;
		}
		else {
			if (size >= 65535UL) return -1;
			rc.eax = 9 << 8;
			rc.edx = size & 0xFFFF;
			himem_sys_realmode_entry_call(&rc);
			if ((rc.eax & 0xFFFF) == 1) handle = rc.edx & 0xFFFF;
		}
	}

	return handle;
}

int himem_sys_free(int handle) {
	struct dpmi_realmode_call rc={0};
	rc.eax = 10 << 8;
	rc.edx = handle & 0xFFFF;
	himem_sys_realmode_entry_call(&rc);
	return (int)(rc.eax & 0xFFFF);
}

int himem_sys_move(unsigned int dst_handle,uint32_t dst_offset,unsigned int src_handle,uint32_t src_offset,uint32_t length) {
	struct dpmi_realmode_call rc={0};
	unsigned char *tmp;
	uint16_t tmpsel=0;
	int retv = 0;

	if ((tmp = (unsigned char*)dpmi_alloc_dos(16,&tmpsel)) == NULL)
		return 0;

	if (himem_sys_present) {
		/* for src or dest references with handle == 0 the HIMEM.SYS driver actually
		 * takes SEG:OFFSET but we allow the caller to give us a physical memory addr. */
		if (src_handle == 0)
			src_offset = ((src_offset << 12) & 0xFFFF0000UL) | (src_offset & 0xFUL);
		if (dst_handle == 0)
			dst_offset = ((dst_offset << 12) & 0xFFFF0000UL) | (dst_offset & 0xFUL);

		*((uint32_t*)(tmp+0x0)) = length;
		*((uint16_t*)(tmp+0x4)) = src_handle;
		*((uint32_t*)(tmp+0x6)) = src_offset;
		*((uint16_t*)(tmp+0xA)) = dst_handle;
		*((uint32_t*)(tmp+0xC)) = dst_offset;
		{
			const uint16_t ofsv = (uint16_t)tmp & 0xFUL;
			const uint16_t segv = (uint16_t)((size_t)tmp >> 4UL);
			rc.eax = 0x0B << 8;
			rc.esi = ofsv;
			rc.ds = segv;
			rc.es = segv;
			himem_sys_realmode_entry_call(&rc);
			retv = rc.eax & 0xFFFF;
		}
	}

	dpmi_free_dos(tmpsel);
	return retv;
}

uint32_t himem_sys_lock(unsigned int handle) {
	struct dpmi_realmode_call rc={0};
	uint32_t o = 0UL;

	if (himem_sys_present) {
		rc.eax = 0x0C << 8;
		rc.edx = handle & 0xFFFF;
		himem_sys_realmode_entry_call(&rc);
		if (rc.eax & 1) o = ((rc.edx & 0xFFFF) << 16) | (rc.ebx & 0xFFFF);
	}

	return o;
}

int himem_sys_unlock(unsigned int handle) {
	struct dpmi_realmode_call rc={0};
	int retv = 0;

	if (himem_sys_present) {
		rc.eax = 0x0D << 8;
		rc.edx = handle & 0xFFFF;
		himem_sys_realmode_entry_call(&rc);
		retv = rc.eax & 0xFFFF;
	}

	return retv;
}

int himem_sys_realloc(unsigned int handle,unsigned long size/* in KB---not bytes*/) {
	struct dpmi_realmode_call rc={0};
	int retv = 0;

	if (himem_sys_present) {
		if (himem_sys_flags & HIMEM_F_4GB) {
			rc.eax = 0x8F << 8;
			rc.ebx = size;
			rc.edx = handle & 0xFFFF;
			himem_sys_realmode_entry_call(&rc);
			retv = rc.eax & 0xFFFF;
		}
		if (retv == 0) {
			if (size >= 65535UL) return 0;
			rc.eax = 0x0F << 8;
			rc.ebx = size;
			rc.edx = handle & 0xFFFF;
			himem_sys_realmode_entry_call(&rc);
			retv = rc.eax & 0xFFFF;
		}
	}

	return retv;
}

int himem_sys_get_handle_info(unsigned int handle,struct himem_block_info *b) {
	struct dpmi_realmode_call rc={0};
	int retv = 0;

	if (himem_sys_present) {
		if (himem_sys_flags & HIMEM_F_4GB) {
			rc.eax = 0x8E << 8;
			rc.edx = handle & 0xFFFF;
			himem_sys_realmode_entry_call(&rc);
			b->block_length_kb = rc.edx;
			b->lock_count = (rc.ebx >> 8) & 0xFF;
			b->free_handles = rc.ebx & 0xFF;
			retv = rc.eax & 0xFFFF;
		}
		if (retv == 0) {
			rc.eax = 0x0E << 8;
			rc.edx = handle & 0xFFFF;
			himem_sys_realmode_entry_call(&rc);
			b->block_length_kb = rc.edx & 0xFFFF;
			b->lock_count = (rc.ebx >> 8) & 0xFF;
			b->free_handles = rc.ebx & 0xFF;
			retv = rc.eax & 0xFFFF;
		}
	}

	return retv;
}
#else /* 16-bit real mode */
int probe_himem_sys() {
	struct SREGS sregs;
	union REGS regs;

	himem_sys_present = 0;
	/* NTS: If this is an 8086, then there is no extended memory, and therefore no reason to call HIMEM.SYS */
	if (cpu_basic_level < 0) cpu_probe();
	if (cpu_basic_level < 2) return 0;

	regs.w.ax = 0x4300;
	int86(0x2F,&regs,&regs);
	if (regs.h.al != 0x80) return 0;
	himem_sys_present = 1;

	regs.w.ax = 0x4310;
	int86x(0x2F,&regs,&regs,&sregs);
	himem_sys_entry =
		((unsigned long)sregs.es << 16UL) |
		((unsigned long)regs.w.bx);

	__asm {
		xor	ah,ah		; function 0x00
		call	[himem_sys_entry]
		mov	himem_sys_version,ax
		and	dl,1		; DX=1 if HMA present, else 0 if not. Your HIMEM.SYS is noncompliant if any other values were put here
		mov	himem_sys_flags,dl ; this maps to HIMEM_F_HMA
	}

	/* does this HIMEM.SYS support the extended functions to address more than 64MB of memory? */
	__asm {
		mov	ah,0x88		; function 0x88: query any free memory
		mov	bl,0x80
		call	[himem_sys_entry]
		cmp	bl,0x80		; BL=0x80 if error (unsupported)
		jz	label1
		or	himem_sys_flags,2 ; <- HIMEM_F_4GB
label1:
	}

	/* Unfortunately, there are HIMEM.SYS implementations that will respond to the extended commands, but fail
           to read or make use of the upper 16 bits of the registers. These broken implementations are easy to check
           for: just allocate a block that is 64MB in size (DX == 0 but EDX == 0x00010000) and if the allocation
	   succeeds, use the Get Block Info command to verify that it is in fact 64MB in size. The broken implementation
	   will create a zero-length block (which is legal in the HIMEM.SYS standard) and will say so when we ask.
	  
	   Known HIMEM.SYS broken emulation:
	        DOSBox 0.74:
	            - Responds to extended commands as if newer HIMEM.SYS but ignores upper 16 bits. You might as well
		      just call the original API functions you'll get just as far. DOSBox doesn't emulate more than 64MB
		      anyway. */
	if (himem_sys_flags & HIMEM_F_4GB) {
		int h = himem_sys_alloc(0x10000UL);
		if (h != -1) {
			struct himem_block_info binf;
			if (himem_sys_get_handle_info(h,&binf)) {
				if (binf.block_length_kb == 0 || binf.block_length_kb == 1) {
					/* Nope. Our 64MB allocation was mis-interpreted as a zero-length allocation request */
					himem_sys_flags &= ~HIMEM_F_4GB;
				}
			}
			himem_sys_free(h);
		}
	}

	return 1;
}

int himem_sys_global_a20(int enable) {
	int retv=0;

	if (!himem_sys_present) return 0;
	enable = (enable > 0) ? 3 : 4;

	__asm {
		mov	ah,byte ptr enable
		call	[himem_sys_entry]
		mov	retv,ax
	}

	return retv;
}

int himem_sys_local_a20(int enable) {
	int retv=0;

	if (!himem_sys_present) return 0;
	enable = (enable > 0) ? 5 : 6;

	__asm {
		mov	ah,byte ptr enable
		call	[himem_sys_entry]
		mov	retv,ax
	}

	return retv;
}

int himem_sys_query_a20() {
	int retv=0;

	if (!himem_sys_present) return 0;

	__asm {
		mov	ah,7
		call	[himem_sys_entry]
		mov	retv,ax
	}

	return retv;
}

void himem_sys_update_free_memory_status() {
	if (!himem_sys_present) return;

	if (himem_sys_flags & HIMEM_F_4GB) {
		__asm {
			mov	ah,0x88
			call	[himem_sys_entry]

			mov	word ptr himem_sys_largest_free,ax
			db	0x66,0xC1,0xE8,0x10 ; shr eax,16
			mov	word ptr himem_sys_largest_free+2,ax

			mov	word ptr himem_sys_total_free,dx
			db	0x66,0xC1,0xEA,0x10 ; shr edx,16
			mov	word ptr himem_sys_total_free+2,dx
		}
	}
	else {
		__asm {
			mov	ah,8
			call	[himem_sys_entry]

			mov	word ptr himem_sys_largest_free,ax
			mov	word ptr himem_sys_largest_free+2,0

			mov	word ptr himem_sys_total_free,dx
			mov	word ptr himem_sys_total_free+2,0
		}
	}
}

/* WARNING: do not remove the __cdecl declaration, the hack below relies on it.
 *          Watcom's native register protocol will copy the long value to a 16-bit
 *          word on stack and then we won't get the full value. */
int __cdecl himem_sys_alloc(unsigned long size/* in KB---not bytes*/) {
	int handle = -1;

	if (himem_sys_present) {
		if (himem_sys_flags & HIMEM_F_4GB) {
			__asm {
				mov	ah,0x89
				db	0x66
				mov	dx,word ptr size ; the 0x66 makes it 'mov edx,size'
				call	[himem_sys_entry]
				test	al,1
				jnz	alloc_ok
				xor	dx,dx
				dec	dx
alloc_ok:			mov	handle,dx
			}
		}
		if (handle == -1) {
			if (size >= 65535UL) return -1;

			__asm {
				mov	ah,9
				mov	dx,word ptr size
				call	[himem_sys_entry]
				test	al,1
				jnz	alloc_ok
				xor	dx,dx
				dec	dx
alloc_ok:			mov	handle,dx
			}
		}
	}

	return handle;
}

int himem_sys_free(int handle) {
	int retv = 0;

	if (himem_sys_present) {
		__asm {
			mov	ah,10
			mov	dx,handle
			call	[himem_sys_entry]
			mov	retv,ax
		}
	}

	return retv;
}

int himem_sys_move(unsigned int dst_handle,uint32_t dst_offset,unsigned int src_handle,uint32_t src_offset,uint32_t length) {
	unsigned char tmp[16]; /* struct */
	int retv = 0;

	if (himem_sys_present) {
		/* for src or dest references with handle == 0 the HIMEM.SYS driver actually
		 * takes SEG:OFFSET but we allow the caller to give us a physical memory addr. */
		if (src_handle == 0)
			src_offset = ((src_offset << 12) & 0xFFFF0000UL) | (src_offset & 0xFUL);
		if (dst_handle == 0)
			dst_offset = ((dst_offset << 12) & 0xFFFF0000UL) | (dst_offset & 0xFUL);

		*((uint32_t*)(tmp+0x0)) = length;
		*((uint16_t*)(tmp+0x4)) = src_handle;
		*((uint32_t*)(tmp+0x6)) = src_offset;
		*((uint16_t*)(tmp+0xA)) = dst_handle;
		*((uint32_t*)(tmp+0xC)) = dst_offset;
		{
			const void far *x = (void far*)tmp;
			const uint16_t ofsv = FP_OFF(x);
			const uint16_t segv = FP_SEG(x);
			const uint16_t dsseg = 0;
			__asm {
				mov ax,ds
				mov dsseg,ax
			}
			assert(segv == dsseg);
			__asm {
				mov	ah,11
				mov	si,ofsv
				call	[himem_sys_entry]
				mov	retv,ax
			}
		}
	}

	return retv;
}

uint32_t himem_sys_lock(unsigned int handle) {
	uint32_t o = 0UL;

	if (himem_sys_present) {
		__asm {
			mov	ah,12
			mov	dx,handle
			call	[himem_sys_entry]
			test	al,1
			jnz	lockend
			xor	bx,bx
			mov	dx,bx
lockend:		mov	word ptr o,bx
			mov	word ptr o+2,dx
		}
	}

	return o;
}

int himem_sys_unlock(unsigned int handle) {
	int retv = 0;

	if (himem_sys_present) {
		__asm {
			mov	ah,13
			mov	dx,handle
			call	[himem_sys_entry]
			mov	retv,ax
		}
	}

	return retv;
}

int __cdecl himem_sys_realloc(unsigned int handle,unsigned long size/* in KB---not bytes*/) {
	int retv = 0;

	if (himem_sys_present) {
		if (himem_sys_flags & HIMEM_F_4GB) {
			__asm {
				mov	ah,0x8F
				db	0x66
				mov	bx,word ptr size ; the 0x66 makes it 'mov ebx,size'
				mov	dx,handle
				call	[himem_sys_entry]
				mov	retv,ax
			}
		}
		if (retv == 0) {
			if (size >= 65535UL) return 0;

			__asm {
				mov	ah,15
				mov	bx,word ptr size
				mov	dx,handle
				call	[himem_sys_entry]
				mov	retv,ax
			}
		}
	}

	return retv;
}

int himem_sys_get_handle_info(unsigned int handle,struct himem_block_info *b) {
	int retv = 0;

	if (himem_sys_present) {
		if (himem_sys_flags & HIMEM_F_4GB) {
			__asm {
				mov	ah,0x8E
				mov	dx,handle
				call	[himem_sys_entry]
				mov	si,word ptr b
				db	0x66
				mov	word ptr [si],dx	; becomes dword ptr [esi]
				mov	byte ptr [si+4],bh	; lock count
				mov	byte ptr [si+5],bl	; free handles
				mov	retv,ax
			}
		}
		if (retv == 0) {
			__asm {
				mov	ah,14
				mov	dx,handle
				call	[himem_sys_entry]
				mov	si,word ptr b
				mov	word ptr [si],dx
				mov	word ptr [si+2],0
				mov	byte ptr [si+4],bh	; lock count
				mov	byte ptr [si+5],bl	; free handles
				mov	retv,ax
			}
		}
	}

	return retv;
}
#endif

#endif /* !defined(TARGET_WINDOWS) && !defined(TARGET_OS2) */

