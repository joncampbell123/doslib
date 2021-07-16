/* dos.c
 *
 * Code to detect the surrounding DOS/Windows environment and support routines to work with it
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#ifdef TARGET_WINDOWS
# include <windows.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosntvdm.h>

/* TODO: Since VCPI/EMM386.EXE can affect 16-bit real mode, why not enable this API for 16-bit real mode too? */
/* TODO: Also why not enable this function for 16-bit protected mode under Windows 3.1? */
#if TARGET_MSDOS == 32
struct dos_linear_to_phys_info dos_ltp_info;
unsigned char dos_ltp_info_init=0;

/* WARNING: Caller must have called probe_dos() and detect_windows() */
int dos_ltp_probe() {
	if (!dos_ltp_info_init) {
		memset(&dos_ltp_info,0,sizeof(dos_ltp_info));

		/* part of our hackery needs to know what CPU we're running under */
		if (cpu_basic_level < 0)
			cpu_probe();

		probe_dos();

#if defined(TARGET_WINDOWS)
		/* TODO: Careful analsys of what version and mode Windows we're running under */
		/* start with the assumption that we don't know where we are and we can't translate to physical. */
		dos_ltp_info.vcpi_xlate = 0; /* TODO: It is said Windows 3.0 has VCPI at the core. Can we detect that? Use it? */
		dos_ltp_info.paging = (windows_mode <= WINDOWS_STANDARD ? 0 : 1); /* paging is not used in REAL or STANDARD modes */
		dos_ltp_info.dos_remap = dos_ltp_info.paging;
		dos_ltp_info.should_lock_pages = 1;
		dos_ltp_info.cant_xlate = 1;
		dos_ltp_info.using_pae = 0; /* TODO: Windows XP SP2 and later can and do use PAE. How to detect that? */
		dos_ltp_info.dma_dos_xlate = 0;

# if TARGET_MSDOS == 32
# else
		/* TODO: Use GetWinFlags() and version info */
# endif
#else
/* ================ MS-DOS specific =============== */
		/* we need to know if VCPI is present */
		probe_vcpi();

		/* NTS: Microsoft Windows 3.0/3.1 Enhanced mode and Windows 95/98/ME all trap access to the control registers.
		 *      But then they emulate the instruction in such a way that we get weird nonsense values.
		 *
		 *      Windows 95/98/ME: We get CR0 = 2. Why??
		 *      Windows 3.0/3.1: We get CR0 = 0.
		 *
		 *      So basically what Windows is telling us... is that we're 32-bit protected mode code NOT running in
		 *      protected mode? What? */
		if (windows_mode == WINDOWS_ENHANCED) {
			/* it's pointless, the VM will trap and return nonsense for control register contents */
			dos_ltp_info.cr0 = 0x80000001UL;
			dos_ltp_info.cr3 = 0x00000000UL;
			dos_ltp_info.cr4 = 0x00000000UL;
		}
		else if (windows_mode == WINDOWS_NT) {
			/* Windows NTVDM will let us read CR0, but CR3 and CR4 come up blank. So what's the point then? */
			uint32_t r0=0;

			__asm {
				xor	eax,eax
				dec	eax

				mov	eax,cr0
				mov	r0,eax
			}
			dos_ltp_info.cr0 = r0 | 0x80000001UL; /* paging and protected mode are ALWAYS enabled, even if NTVDM should lie to us */
			dos_ltp_info.cr3 = 0x00000000UL;
			dos_ltp_info.cr4 = 0x00000000UL;
		}
		else {
			/* FIXME: Reading CR0 here doesn't bother DOS32A or DOS4/GW but CWSDPMI (the standard ring-3 version) doesn't like it */
			uint32_t r0=0,r3=0,r4=0;
			__asm {
				xor	eax,eax
				dec	eax

				mov	eax,cr0
				mov	r0,eax

				mov	eax,cr3
				mov	r3,eax
			}

			if (cpu_flags & CPU_FLAG_CR4_EXISTS) {
				__asm {
					mov	eax,cr4
					mov	r4,eax
				}
			}

			dos_ltp_info.cr0 = r0;
			dos_ltp_info.cr3 = r3;
			dos_ltp_info.cr4 = r4;
		}

		dos_ltp_info.vcpi_xlate = vcpi_present?1:0;			/* if no other methods available, try asking the VCPI server */
		dos_ltp_info.paging = (dos_ltp_info.cr0 >> 31)?1:0;		/* if bit 31 of CR0 is set, the extender has paging enabled */
		dos_ltp_info.dos_remap = vcpi_present?1:0;			/* most DOS extenders map 1:1 the lower 1MB, but VCPI can violate that */
		dos_ltp_info.should_lock_pages = dos_ltp_info.paging;		/* it's a good assumption if paging is enabled the extender probably pages to disk and may move things around */
		dos_ltp_info.cant_xlate = dos_ltp_info.paging;			/* assume we can't translate addresses yet if paging is enabled */
		dos_ltp_info.using_pae = (dos_ltp_info.cr4 & 0x20)?1:0;		/* take note if PAE is enabled */
		dos_ltp_info.dma_dos_xlate = dos_ltp_info.paging;		/* assume the extender translates DMA if paging is enabled */

		if (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT) {
			dos_ltp_info.should_lock_pages = 1;		/* Windows is known to page to disk (the swapfile) */
			dos_ltp_info.dma_dos_xlate = 1;			/* Windows virtualizes the DMA controller */
			dos_ltp_info.cant_xlate = 1;			/* Windows provides us no way to determine the physical memory address from linear */
			dos_ltp_info.dos_remap = 1;			/* Windows remaps the DOS memory area. This is how it makes multiple DOS VMs possible */
			dos_ltp_info.vcpi_xlate = 0;			/* Windows does not like VCPI */
			dos_ltp_info.paging = 1;			/* Windows uses paging. Always */
		}

		/* this code is ill prepared for PAE modes for the time being, since PAE makes page table entries 64-bit
		 * wide instead of 32-bit wide. Then again, 99% of DOS out there probably couldn't handle PAE well either. */
		if (dos_ltp_info.using_pae)
			dos_ltp_info.cant_xlate = 1;

		/* if NOT running under Windows, and the CR3 register shows something, then we can translate by directly peeking at the page tables */
		if (windows_mode == WINDOWS_NONE && dos_ltp_info.cr3 >= 0x1000)
			dos_ltp_info.cant_xlate = 0;
#endif

		dos_ltp_info_init = 1;
	}

	return 1;
}
#endif

#if TARGET_MSDOS == 32
/* WARNINGS: Worst case scanario this function cannot translate anything at all.
 *           It will return 0xFFFFFFFFUL if it cannot determine the address.
 *           If paging is disabled, the linear address will be returned.
 *           If the environment requires you to lock pages, then you must do so
 *           before calling this function. Failure to do so will mean erratic
 *           behavior when the page you were working on moves out from under you! */

/* "There is no way in a DPMI environment to determine the physical address corresponding to a given linear address. This is part of the design of DPMI. You must design your application accordingly."
 *
 * Fuck you.
 * I need the damn physical address and you're not gonna stop me! */

/* NOTES:
 *
 *     QEMU + Windows 95 + EMM386.EXE:
 *
 *        I don't know if the DOS extender is doing this, or EMM386.EXE is enforcing it, but
 *        a dump of the first 4MB in the test program reveals our linear address space is
 *        randomly constructed from 16KB pages taken from all over extended memory. Some of
 *        them, the pages are arranged BACKWARDS. Yeah, backwards.
 *
 *        Anyone behind DOS4/GW and DOS32a care to explain that weirdness?
 *
 *        Also revealed, DOS4/GW follows the DPMI spec and refuses to map pages from conventional
 *        memory. So if DOS memory is not mapped 1:1 and the page tables are held down there we
 *        literally cannot read them! */
/* NOTE: The return value is 64-bit so that in the future, if we ever run under DOS with PAE or
 *       PSE-36 trickery, we can still report the correct address even if above 4GB. The parameter
 *       supplied however remains 32-bit, because as a 32-bit DOS program there's no way any
 *       linear address will ever exceed 4GB. */
uint64_t dos_linear_to_phys(uint32_t linear) {
	uint32_t off,ent;
	uint64_t ret = DOS_LTP_FAILED;
	unsigned char lock1=0,lock2=0;
	uint32_t *l1=NULL,*l2=NULL;

	if (!dos_ltp_info.paging)
		return linear;
	if (linear < 0x100000UL && !dos_ltp_info.dos_remap) /* if below 1MB boundary and DOS is not remapped, then no translation */
		return linear;

	/* if VCPI translation is to be used, and lower DOS memory is remapped OR the page requested is >= 1MB (or in adapter ROM/RAM), then call the VCPI server and ask */
	if (dos_ltp_info.vcpi_xlate && (dos_ltp_info.dos_remap || linear >= 0xC0000UL)) {
		ent = dos_linear_to_phys_vcpi(linear>>12);
		if (ent != 0xFFFFFFFFUL) return ent;
		/* Most likely requests for memory >= 1MB will fail, because VCPI is only required to
		 * provide the system call for lower 1MB DOS conventional memory */
	}

/* if we can't use VCPI and cannot translate, then give up. */
/* also, this code does not yet support PAE */
	if (dos_ltp_info.using_pae || dos_ltp_info.cant_xlate)
		return ret;

/* re-read control reg because EMM386, etc. is free to change it at any time */
	{
		uint32_t r3=0,r4=0;
		__asm {
			xor	eax,eax
			dec	eax

			mov	eax,cr3
			mov	r3,eax
		}

		if (cpu_flags & CPU_FLAG_CR4_EXISTS) {
			__asm {
				mov	eax,cr4
				mov	r4,eax
			}
		}

		dos_ltp_info.cr3 = r3;
		dos_ltp_info.cr4 = r4;
	}

	/* OK then, we have to translate */
	off = linear & 0xFFFUL;
	linear >>= 12UL;
	if (dos_ltp_info.cr3 < 0x1000) /* if the contents of CR3 are not available, then we cannot translate */
		return ret;

	/* VCPI possibility: the page table might reside below 1MB in DOS memory, and remain unmapped. */
	if (dos_ltp_info.dos_remap && dos_ltp_info.vcpi_xlate && dos_ltp_info.cr3 < 0x100000UL) {
		lock1 = 0;
		if (dos_linear_to_phys_vcpi(dos_ltp_info.cr3>>12) == (dos_ltp_info.cr3&0xFFFFF000UL)) /* if VCPI says it's a 1:1 mapping down there, then it's OK */
			l1 = (uint32_t*)(dos_ltp_info.cr3 & 0xFFFFF000UL);
	}
	/* DOS4/GW and DOS32A Goodie: the first level of the page table is in conventional memory... and the extender maps DOS 1:1 :) */
	else if (!dos_ltp_info.dos_remap && dos_ltp_info.cr3 < 0x100000UL) {
		lock1 = 0;
		l1 = (uint32_t*)(dos_ltp_info.cr3 & 0xFFFFF000UL);
	}
	else {
		/* well, then we gotta map it */
		l1 = (uint32_t*)dpmi_phys_addr_map(dos_ltp_info.cr3 & 0xFFFFF000UL,4096);
		if (l1 != NULL) lock1 = 1;
	}

	if (l1 != NULL) {
		/* level 1 lookup */
		ent = l1[linear >> 10UL];
		if (ent & 1) { /* if the page is actually present... */
			/* if the CPU supports PSE (Page Size Extensions) and has them enabled (via CR4) and the page is marked PS=1 */
			if ((cpu_cpuid_features.a.raw[2/*EDX*/] & (1 << 3)) && (dos_ltp_info.cr4 & 0x10) && (ent & 0x80)) {
				/* it's a 4MB page, and we stop searching the page hierarchy here */
				ret = ent & 0xFFC00000UL; /* bits 31-22 are the actual page address */

				/* but wait: if the CPU supports PSE-36, then we need to readback address bits 35...32,
				 * or if an AMD64 processor, address bits 39...32 */
				/* FIXME: So, we can assume if the CPU supports 64-bit long mode, that we can use bits 39...32?
				 *        Perhaps this is a more in-depth check that we should be doing in the ltp_probe function? */
				if (cpu_cpuid_features.a.raw[2/*E2X*/] & (1 << 17)) { /* If PSE support exists */
					if (cpu_cpuid_features.a.raw[3/*ECX*/] & (1 << 29)) { /* If CPU supports 64-bit long mode */
						/* AMD64 compatible, up to 40 bits */
						ret |= ((uint64_t)((ent >> 13UL) & 0xFFUL)) << 32ULL;
					}
					else { /* else, assume Pentium III compatible, up to 36 bits */
						ret |= ((uint64_t)((ent >> 13UL) & 0xFUL)) << 32ULL;
					}
				}
			}
			else {
				/* VCPI possibility: the page table might reside below 1MB in DOS memory, and remain unmapped. */
				if (dos_ltp_info.dos_remap && dos_ltp_info.vcpi_xlate && ent < 0x100000UL) {
					lock2 = 0;
					if (dos_linear_to_phys_vcpi(ent>>12) == (ent&0xFFFFF000UL)) /* if VCPI says it's a 1:1 mapping down there, then it's OK */
						l2 = (uint32_t*)(ent & 0xFFFFF000UL);
				}
				/* again the second level is usually in DOS memory where we can assume 1:1 mapping */
				else if (!dos_ltp_info.dos_remap && !dos_ltp_info.dos_remap && ent < 0x100000UL) {
					lock2 = 0;
					l2 = (uint32_t*)(ent & 0xFFFFF000UL);
				}
				else {
					/* well, then we gotta map it */
					l2 = (uint32_t*)dpmi_phys_addr_map(ent & 0xFFFFF000UL,4096);
					if (l2 != NULL) lock2 = 1;
				}
			}
		}
	}

	if (l2 != NULL) {
		/* level 2 lookup */
		ent = l2[linear & 0x3FF];
		if (ent & 1) { /* if the page is actually present... */
			ret = ent & 0xFFFFF000UL;
		}
	}

	if (lock2) dpmi_phys_addr_free((void*)l2);
	if (lock1) dpmi_phys_addr_free((void*)l1);
	return ret;
}
#endif

