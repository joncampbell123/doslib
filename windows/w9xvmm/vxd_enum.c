/* library for DOS & Windows programs to enumerate resident VxD drivers
 * from within Windows 95/98/ME.
 *
 * Supported environments:
 *    Host:
 *       Windows 3.0/3.1/3.11
 *       Windows 95/98/ME
 *    Compiled as:
 *       Windows 9x application
 *       Win32s application
 *       Win16 application
 *       32-bit DOS application
 *       16-bit DOS application
 *
 * Be warned: This code relies on the fact that under every possible environment
 * within Windows 9x, that the 9x kernel is visible at 0xC0000000 and the VMM
 * VxD linked list can be traversed that way. This code should not be used in
 * environments that are unrelated to Windows 3.x/9x/ME. In it's current form,
 * it may crash if it is lied to about running under Windows 9x (FIXME: Until
 * you get page fault handlers set up to catch it!)
 */

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
#include <hw/dos/dosbox.h>
#include <windows/w9xvmm/vxd_enum.h>

#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
unsigned char far *w9x_vmm_vxd_ddb_scan_ptr(struct w9x_vmm_DDB_scan *vxdscan,uint32_t addr) {
	if (vxdscan->flat_sel == 0)
		return NULL;

	if (addr < vxdscan->flat_sel_base ||
		addr >= (vxdscan->flat_sel_base+0xF000UL)) {
		uint32_t nab = addr & 0xFFFFF000UL;
		/* need to adjust */
		if (SetSelectorBase(vxdscan->flat_sel,nab) == 0) {
			/* it failed */
			return NULL;
		}

		vxdscan->flat_sel_base = nab;
	}

	return (unsigned char far*)MK_FP(
		vxdscan->flat_sel,
		(unsigned short)(addr - vxdscan->flat_sel_base));
}
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
unsigned char far *w9x_vmm_vxd_ddb_scan_ptr(struct w9x_vmm_DDB_scan *vxdscan,uint32_t addr) {
	if (vxdscan->flat_sel == 0)
		return NULL;

	return (unsigned char far*)MK_FP(vxdscan->flat_sel,addr);
}
#endif

void w9x_vmm_last_vxd(struct w9x_vmm_DDB_scan *vxdscan) {
#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	if (vxdscan->flat_sel != 0) {
		FreeSelector(vxdscan->flat_sel);
		vxdscan->flat_sel = 0;
	}
	vxdscan->flat_sel_base = 0;
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
	if (vxdscan->flat_sel != 0) {
		dpmi_free_descriptor(vxdscan->flat_sel);
		vxdscan->flat_sel = 0;
	}
#endif
}

/* NTS: The VXD chain we're going through is rarely in
 *      order, though always in the upper area. */
int w9x_vmm_next_vxd(struct w9x_vmm_DDB_scan *vxdscan) {
	if (vxdscan->found_at == 0)
		return 0;
	if (vxdscan->ddb.next == 0)
		return 0;
	if (vxdscan->ddb.next < vxdscan->lowest ||
		vxdscan->ddb.next > vxdscan->highest)
		return 0;

#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
	/* this may crash */
	__try {
		vxdscan->found_at = vxdscan->ddb.next;
		memcpy(&vxdscan->ddb,(char*)(vxdscan->found_at-vxdscan->linear_bias),
			sizeof(vxdscan->ddb));
		return 1;
	}
	__except(1) {
		/* safely break out */
	}
	
	vxdscan->found_at = 0;
#elif TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	{
		unsigned char far *fp;
		vxdscan->found_at = vxdscan->ddb.next;
		fp = w9x_vmm_vxd_ddb_scan_ptr(vxdscan,vxdscan->found_at);
		if (fp == NULL) return 0;
		_fmemcpy(&vxdscan->ddb,fp,sizeof(vxdscan->ddb));
		return 1;
	}
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) /* 32-bit DOS */
	{
		unsigned char far *fp;
		vxdscan->found_at = vxdscan->ddb.next;
		fp = w9x_vmm_vxd_ddb_scan_ptr(vxdscan,vxdscan->found_at);
		if (fp == NULL) return 0;
		_fmemcpy(&vxdscan->ddb,fp,sizeof(vxdscan->ddb));
		return 1;
	}
#else
	if (dpmi_present && dpmi_entered != 0) {
		vxdscan->found_at = vxdscan->ddb.next;
		if (dpmi_lin2fmemcpy((unsigned char far*)(&vxdscan->ddb),vxdscan->found_at,sizeof(vxdscan->ddb)) == 0)
			return 0;
		return 1;
	}
	
	vxdscan->found_at = 0;
#endif

	return 0;
}

int w9x_vmm_first_vxd(struct w9x_vmm_DDB_scan *vxdscan) {
	uint32_t start = 0xC0000000UL+0x1000UL;
	uint32_t stop_at = start + 0x100000UL; /* scan for up to 16MB */

	/* init the struct */
	memset(vxdscan,0,sizeof(*vxdscan));

	/* this won't work on anything less than a 386 */
	if (cpu_basic_level < 0)
		cpu_probe();
	if (cpu_basic_level < 3)
		return 0;

	/* uhhh this won't work under NT */
	if (windows_mode == WINDOWS_NT)
		return 0;
	/* Windows 9x/ME or Windows 3.1 in 386 enhanced mode ONLY */
	if (windows_mode != WINDOWS_ENHANCED)
		return 0;
	/* WINE (Wine Is Not an Emulator) does not emulate the Win9x VxD chain */
	if (windows_emulation == WINEMU_WINE)
		return 0;

	if (windows_version < 0x35F) {
		/* Windows 3.x has it at 0x80000000 not 0xC0000000 */
		start = 0x80000000+0x1000UL;
		stop_at = start + 0x100000UL;
#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
		/* and for whatever reason, the flat code/data segment is set to base 0xFFFF0000, not 0 */
		vxdscan->linear_bias = 0xFFFF0000;
#endif
	}

#if TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS)
	if (windows_version < 0x30A) { /* Anything earlier than Windows 3.1 */
		/* FIXME: Windows 3.0: Our DPMI entry hackery doesn't work, it just causes our
		 *        program to hang. Do not attempt. But contrary to the anecdotal documentation
		 *        floating around on the 'net, Windows 3.0 does have a DPMI server built in. */
		return 0;
	}
#endif

#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	{
		uint16_t myds=0;

		__asm {
			mov	ax,ds
			mov	myds,ax
		}

		vxdscan->flat_sel = AllocSelector(myds);
		if (vxdscan->flat_sel == 0) {
			w9x_vmm_last_vxd(vxdscan);
			return 0;
		}
	}

	vxdscan->flat_sel_base = start;
	if (SetSelectorBase(vxdscan->flat_sel,vxdscan->flat_sel_base) == 0) {
		w9x_vmm_last_vxd(vxdscan);
		return 0;
	}
	/* NTS: Do NOT attempt to set limit beyond 0xFFFF Windows 9x
	 *      will hard-crash (because our data segment is 16-bit) */
	SetSelectorLimit(vxdscan->flat_sel,0xFFFFUL);
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
	vxdscan->flat_sel = dpmi_alloc_descriptor();
	if (vxdscan->flat_sel == 0) {
		w9x_vmm_last_vxd(vxdscan);
		return 0;
	}

	{
		uint16_t my_ds=0;

		__asm {
			mov	ax,ds
			mov	my_ds,ax
		}

		dpmi_set_segment_access(vxdscan->flat_sel,0x90|((my_ds&3)<<5)|0x02); /* P=1 DPL=(same as DS) data exp-up r/w not accessed */
		dpmi_set_segment_base(vxdscan->flat_sel,0UL);
		dpmi_set_segment_limit(vxdscan->flat_sel,0xFFFFFFFFUL);
	}
#endif

	vxdscan->lowest = start;
	vxdscan->highest = 0xFFFF0000;

#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
	__try { /* this may pagefault */
		while ((start+sizeof(struct w9x_vmm_DDB)) < stop_at) {
			if (!memcmp((char*)(start-vxdscan->linear_bias),"VMM     ",8)) {
				/* TODO: Check if No == 1 */
				start -= 12;
				vxdscan->found_at = start;
				memcpy(&vxdscan->ddb,(char*)(start-vxdscan->linear_bias),sizeof(vxdscan->ddb));
				return 1;
			}

			/* keep looking */
			start++;
		}
	}
	__except(1) {
		/* safely break out */
	}
#elif TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	/* FIXME: You should install a DPMI exception handler or ToolHelp handler in case this causes a page fault */
	{
		unsigned char far *fp;

		while ((start+sizeof(struct w9x_vmm_DDB)) < stop_at) {
			fp = w9x_vmm_vxd_ddb_scan_ptr(vxdscan,start);
			if (fp == NULL) {
				printf("Unable to scan ptr\n");
				break;
			}

			if (!_fmemcmp(fp,"VMM     ",8)) {
				/* TODO: Check if No == 1 */
				start -= 12;
				fp = w9x_vmm_vxd_ddb_scan_ptr(vxdscan,start);
				if (fp == NULL) break;
				vxdscan->found_at = start;
				_fmemcpy(&vxdscan->ddb,fp,sizeof(vxdscan->ddb));
				return 1;
			}

			/* keep looking */
			start++;
		}
	}
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) /* 32-bit DOS */
	/* FIXME: You should install a DPMI exception handler in case this causes a page fault */
	{/*Bwahahahahha under Windows 9x this actually works----32-bit DOS can see Windows 9x kernel structs! :) */
		unsigned char far *fp;

		while ((start+sizeof(struct w9x_vmm_DDB)) < stop_at) {
			fp = w9x_vmm_vxd_ddb_scan_ptr(vxdscan,start);
			if (fp == NULL) {
				printf("Unable to scan ptr\n");
				break;
			}

			if (!_fmemcmp(fp,"VMM     ",8)) {
				/* TODO: Check if No == 1 */
				start -= 12;
				fp = w9x_vmm_vxd_ddb_scan_ptr(vxdscan,start);
				if (fp == NULL) break;
				vxdscan->found_at = start;
				_fmemcpy(&vxdscan->ddb,fp,sizeof(vxdscan->ddb));
				return 1;
			}

			/* keep looking */
			start++;
		}
	}
#elif TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS) /* 16-bit DOS */
	if (dpmi_lin2fmemcpy_init()) {
		unsigned char far *tmp;
		unsigned int tmpsz,i;

		/* there is a LOT of overhead in switching between v86 and DPMI protected mode
		 * for each memcpy. help speed up the code, make it worthwhile by DPMI memcpying
		 * large chunks and doing a scan within each chunk. */
		tmpsz = 32768;
		tmp = _fmalloc(tmpsz);
		if (tmp == NULL)
			return 0;

		while ((start+sizeof(struct w9x_vmm_DDB)) < stop_at) {
			if (dpmi_lin2fmemcpy(tmp,start,tmpsz) == 0)
				break;

			for (i=0;(i+8U) <= tmpsz;i++) {
				if (!_fmemcmp(tmp+i,"VMM     ",8)) {
					/* TODO: Check if No == 1 */
					start += (uint32_t)i;
					start -= 12;
					vxdscan->found_at = start;
					_ffree(tmp);
					if (dpmi_lin2fmemcpy((unsigned char far*)(&vxdscan->ddb),start,sizeof(vxdscan->ddb)) == 0)
						return 0;

					return 1;
				}
			}

			/* keep looking */
			start += tmpsz - 8;
		}

		_ffree(tmp);
	}
#endif

	w9x_vmm_last_vxd(vxdscan);
	return 0;
}

