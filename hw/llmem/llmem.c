/* NOTES:
 *    This code works perfectly on most test systems I have.
 *    Some systems do provide interesting insight though when they do fail.
 *
 *    Test: Oracle VirtualBox 4.1.8 with 64-bit guest and AMD VT-X acceleration:
 *    Result: VirtualBox reports PAE and PSE, and 32-bit test program works perfectly.
 *            The 16-bit real mode builds however, cause the virtual machine to hang
 *            when attempting to use the PSE method. This hang does not occur when
 *            AMD VT-x is disabled.
 *
 *    Test: Dell netbook with Intel Atom processor
 *    Result: Processor reports via CPUID that it has 32-bit linear and 32-bit physical
 *            address space. And it means it. The minute this program steps beyond 4GB,
 *            the CPU faults and the system resets. This is true regardless of whether
 *            the 16-bit real mode or the 32-bit protected mode version is used. This is
 *            true whether you use PSE-36 or PAE.
 *
 *            So apparently, they don't do what 386/486/pentium systems USED to do and just
 *            silently wrap the addresses, eh?
 *
 *            That means this code should make use of the "extended CPUID" to read those
 *            limits and then llmemcpy() should enforce them. */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/llmem/llmem.h>
#include <hw/cpu/cpuidext.h>
#include <hw/dos/doswin.h>

unsigned char		llmem_probed = 0;
unsigned char		llmem_meth_pse = 0;
unsigned char		llmem_meth_pae = 0;
unsigned char		llmem_available = 0;
uint64_t		llmem_phys_limit = 0ULL;
uint64_t		llmem_pse_limit = 0ULL;
const char*		llmem_reason = NULL; /* non-NULL if probing failed */

/* page tables created on-demand for the llmemcpy() function.
 * they are created once and retained so llmemcpy() is faster.
 * they will be freed and recreated if we have to do so.
 * you can do llmemcpy_free() to free up memory */
volatile void FAR*	llmemcpy_pagetables = NULL;
volatile void FAR*	llmemcpy_pagetables_raw = NULL;
size_t			llmemcpy_pagetables_size = 0;
unsigned char		llmemcpy_pagefmt = 0;
#if TARGET_MSDOS == 16
uint32_t far*		llmemcpy_gdt = NULL;
uint16_t		llmemcpy_gdtr[4];
uint16_t		llmemcpy_idtr[4];
uint32_t		llmemcpy_vcpi[0x20];
uint32_t		llmemcpy_vcpi_return[2];
uint8_t			llmemcpy_vcpi_tss[108];
#endif

#if TARGET_MSDOS == 16
void llmem_memcpy_16_inner_pae(void);
void llmem_memcpy_16_inner_pse(void);
void __cdecl llmem_memcpy_16_inner_pae_vcpi(uint32_t dst,uint32_t src,uint32_t cpy);
void __cdecl llmem_memcpy_16_inner_pse_vcpi(uint32_t dst,uint32_t src,uint32_t cpy);
#endif

int llmem_init() {
	if (!llmem_probed) {
		llmem_probed = 1;
		if (cpu_basic_level < 0) cpu_probe();
		if (cpu_basic_level < 4) {
			llmem_reason = "Requires a 486 or higher processor";
			return 0; /* 486 or higher */
		}

		if (!(cpu_flags & CPU_FLAG_CPUID)) {
			llmem_reason = "Requires CPUID";
			return 0; /* The CPU must support CPUID */
		}

		if (!(cpu_flags & CPU_FLAG_CPUID_EXT) || cpu_cpuid_ext_info == NULL)
			cpu_extended_cpuid_probe();

		probe_dos();
		detect_windows();
#if TARGET_MSDOS == 32
		probe_dpmi();
#endif
		probe_vcpi();

#if TARGET_MSDOS == 32
		/* if we can't use the LTP library, then we can't do the trick */
		if (!dos_ltp_probe()) {
			llmem_reason = "Runtime environment does not permit knowing physical<->linear page table translation";
			return 0;
		}
#endif

		/* if we're running under Windows, then we definitely can't do the trick */
		if (windows_mode != WINDOWS_NONE) {
			llmem_reason = "LLMEM cannot operate from within Windows";
			return 0;
		}

#if TARGET_MSDOS == 16
		/* 16-bit real mode: If we're running in virtual 8086 mode, and VCPI is not available, then we can't do it */
		if (cpu_v86_active && !vcpi_present) {
			llmem_reason = "16-bit LLMEM cannot operate while virtual 8086 mode is active (no VCPI services available)";
			return 0;
		}
#endif

		/* decide the PSE access limit. Older Pentium III systems are 36 bit limited.
		 * Newer AMD64 systems allow up to 40 bits. */
		/* FIXME: There is a way to query through CPUID the maximum physical memory address supported, use it.
		 *        Ref [https://www.sandpile.org/x86/cpuid.htm#level_8000_0008h]*/
		llmem_pse_limit = 0xFFFFFFFFFFULL; /* 40 bits by default */

		/* by default, assume the physical limit is the PSE limit */
		llmem_phys_limit = llmem_pse_limit;

		/* but if CPUID provides more information, then use it.
		 * this is important to know, modern CPUs will actually treat any attempt to read past it's physical memory limit as a fault (or at least Intel chips do).
		 * it's not like the 386/486/Pentium systems of old that silently wrapped addresses */
		if (cpu_cpuid_ext_info_has_longmode) {
			unsigned char c = (unsigned char)(cpu_cpuid_ext_info->longmode.a.raw[0] & 0xFFUL);
			if (c < 32) c = 32;
			llmem_phys_limit = (1ULL << ((uint64_t)c)) - 1ULL;
		}

		/* PSE & PSE-36 support */
		if (	/*PSE*/(cpu_cpuid_features.a.raw[2/*EDX*/]&(1UL<<3UL)) &&
			/*PSE-36*/(cpu_cpuid_features.a.raw[2/*EDX*/]&(1UL<<17UL)))
			llmem_meth_pse = 1;

		/* PAE support */
		if (/*PAE*/(cpu_cpuid_features.a.raw[2/*EDX*/]&(1UL<<6UL)))
			llmem_meth_pae = 1;

		if (llmem_meth_pse || llmem_meth_pae) {
			llmem_available = 1;
		}
		else {
			llmem_reason = "LLMEM requires that the CPU support PSE or PAE";
			llmem_available = 0;
		}
	}

	return llmem_available;
}

/* returns 0xFFFFFFFFFFFFFFFF if unmappable */
#if TARGET_MSDOS == 16
uint64_t llmem_ptr2ofs(unsigned char far *ptr) {
	uint16_t seg = FP_SEG(ptr),ofs = FP_OFF(ptr);

	if (cpu_v86_active) {
		uint32_t vofs = ((uint32_t)seg << 4UL) + (uint32_t)ofs,pofs;
		if (vcpi_present) {
			pofs = dos_linear_to_phys_vcpi(vofs>>12UL);
			if (pofs != 0xFFFFFFFFUL) return pofs + (vofs & 0xFFFUL);
		}
		return (0xFFFFFFFFFFFFFFFFULL);
	}

	return (((uint64_t)seg << 4ULL) + (uint64_t)ofs);
}

/* if copying from your virtual address space (using llmem_ptr2ofs) the maximum
 * amount of data you should copy before translating again. */
uint32_t llmem_virt_phys_recommended_copy_size() {
	if (cpu_v86_active)
		return 0x1000UL; /* Paging enabled. EMM386.EXE might remap some memory. Max copy 4KB */

	return 0x100000UL; /* 1MB */
}
#else
uint64_t llmem_ptr2ofs(unsigned char *ptr) {
	if (dos_ltp_info.paging)
		return dos_linear_to_phys((uint32_t)ptr);

	return ((size_t)ptr);
}

/* if copying from your virtual address space (using llmem_ptr2ofs) the maximum
 * amount of data you should copy before translating again. */
uint32_t llmem_virt_phys_recommended_copy_size() {
	if (dos_ltp_info.paging)
		return 0x1000UL; /* Paging enabled. Max copy 4KB */

	return 0x100000UL; /* 1MB */
}
#endif

void llmemcpy_free() {
	if (llmemcpy_pagetables) {
#if TARGET_MSDOS == 16
		_ffree((void FAR*)llmemcpy_pagetables_raw);
#else
		free((void*)llmemcpy_pagetables_raw);
#endif
		llmemcpy_pagetables_size = 0;
		llmemcpy_pagetables_raw = NULL;
		llmemcpy_pagetables = NULL;
	}
#if TARGET_MSDOS == 16
	if (llmemcpy_gdt) {
		_ffree(llmemcpy_gdt);
		llmemcpy_gdt = NULL;
	}
#endif
}

int llmemcpy_alloc(size_t len,unsigned char typ) {
	if (len == 0)
		return 0;
	if (llmemcpy_pagetables != NULL && len <= llmemcpy_pagetables_size && typ == llmemcpy_pagefmt)
		return 1;

	llmemcpy_free();

	llmemcpy_pagetables_size = (len + 0xFFFUL) & (~0xFFFUL);
#if TARGET_MSDOS == 16
	llmemcpy_pagetables_raw = _fmalloc(llmemcpy_pagetables_size + 4096UL + 32UL);
#else
	llmemcpy_pagetables_raw = malloc(llmemcpy_pagetables_size + 4096UL);
#endif
	if (llmemcpy_pagetables_raw != NULL) {
		/* the table itself must be 4K aligned, so... */
#if TARGET_MSDOS == 16
		unsigned int seg = FP_SEG(llmemcpy_pagetables_raw) + (FP_OFF(llmemcpy_pagetables_raw) >> 4UL);
		seg = (seg + 0xFFUL) & (~0xFFUL);
		llmemcpy_pagetables = MK_FP(seg,0);
#else
		llmemcpy_pagetables = (void*)(((size_t)llmemcpy_pagetables_raw + 0xFFFUL) & (~0xFFFUL));
#endif
		llmemcpy_pagefmt = typ;
	}

	return (llmemcpy_pagetables != NULL);
}

/* NTS: src and dst must be PHYSICAL addresses. the routine will deal with conversion if it has to */
size_t llmemcpy(uint64_t dst,uint64_t src,size_t len) {
	if (!llmem_available || len == 0) return 0;
	if (dst == 0xFFFFFFFFFFFFFFFFULL) return 0;
	if (src == 0xFFFFFFFFFFFFFFFFULL) return 0;

	/* do not attempt to exceed the CPU's physical memory limits. doing so will (likely) trigger a fault */
	/* this prevents the 0-4GB boundary test from causing the Intel Atom processor (32-bit phys limit) to hard-reset */
	if (dst > llmem_phys_limit || (dst+(uint64_t)len) > llmem_phys_limit) return 0;
	if (src > llmem_phys_limit || (src+(uint64_t)len) > llmem_phys_limit) return 0;

#if TARGET_MSDOS == 16
	if (cpu_v86_active) {
		if ((src|dst) < (1ULL << 52ULL) && llmem_meth_pae) { /* if both are below 52 bits, and PAE is an option */
			register uint32_t FAR *p32;
			register uint64_t FAR *p;
			uint32_t psrc,pdst;
			uint32_t cpy = (uint32_t)min(((1ULL << 52ULL) - max(src,dst)),0x1000000ULL),i,j; /* we have to max() it to ensure the result is < 4GB, something that will fit into size_t on 32-bit */
			if (cpy > len) cpy = len;

			/* limit memory copy to 2M or less */
			if (cpy > 0x200000UL) cpy = 0x200000UL;

			/* make temporary page tables, switch on PSE, switch to page tables, and do the copy operation.
			 * This code takes advantage of the Page Size bit to make only the first level (4M pages)
			 * and that modern processors interpret bits 16...13 as bits 35...32 of the physical address. */
			if (llmemcpy_pagetables == NULL || llmemcpy_pagefmt != LLMEMCPY_PF_PAE_2M_VCPI) {
				if (!llmemcpy_alloc(36*1024,LLMEMCPY_PF_PAE_2M_VCPI)) { /* 4KB PDPTE + 16KB Page directories + 8KB VCPI compat + 4KB 32-bit page0 32-bit + 4KB page dir 32-bit */
					llmem_reason = "llmemcpy: no memory available (36KB needed) for 2M PAE VCPI page table";
					return 0;
				}

				/* 32-bit page dir */
				psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));
				p32 = (uint32_t FAR*)((char FAR*)llmemcpy_pagetables + (28*1024));
				p32[0] = (psrc + 0x8000UL) | 1UL; /* first page directory -> second level is at the second 4KB block */
				for (i=1;i < 1024;i++) p32[i] = (i << 22UL) | (1 << 7UL) | 7UL; /* every other 4MB range is mapped 1:1 */

				/* level 1: PDPTE */
				psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));
				p = (uint64_t FAR*)llmemcpy_pagetables;
				for (i=0;i < 4;i++) p[i] = ((uint64_t)psrc + 0x1000UL + (i<<12UL)) | 1UL;
				for (;i < 512;i++) p[i] = 0;

				/* level 2: page directories */
				for (j=0;j < 4;j++) {
					p = (uint64_t FAR*)((char FAR*)llmemcpy_pagetables + 0x1000UL + (j<<12UL));
					for (i=0;i < 512;i++) p[i] = ((j << 30UL) | (i << 21UL)) | (1UL << 7UL) | 7UL;
				}

				/* page dir 0, page 0+1 need a 3rd level, for the (translated) copy of the VCPI server's page tables */
				psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables)) + 0x1000UL + 0x4000UL;
				p = (uint64_t FAR*)((char FAR*)llmemcpy_pagetables + 0x1000UL);
				p[0] = ((uint32_t)psrc & 0xFFFFF000UL) | 1UL;
				p[1] = (((uint32_t)psrc + 0x1000UL) & 0xFFFFF000UL) | 1UL;
			}

			if (llmemcpy_gdt == NULL) {
				if ((llmemcpy_gdt = _fmalloc(8*(5+4))) == NULL) {
					llmem_reason = "llmemcpy: no memory available for GDT";
					return 0;
				}
			}

			__asm {
				.386
				xor	eax,eax
				mov	ax,cs
				shl	eax,4
				mov	psrc,eax

				xor	eax,eax
				mov	ax,ds
				shl	eax,4
				mov	pdst,eax
			}

			/* NULL selector */
			llmemcpy_gdt[0*2 + 0] = 0;
			llmemcpy_gdt[0*2 + 1] = 0;
			/* code (16-bit) limit=0xFFFF */
			llmemcpy_gdt[1*2 + 0] = 0xFFFFUL | (psrc << 16UL);
			llmemcpy_gdt[1*2 + 1] = ((psrc >> 16UL) & 0xFFUL) | (0x9AUL << 8UL) | (0x0FUL << 16UL);
			/* data (16-bit) limit=0xFFFF */
			llmemcpy_gdt[2*2 + 0] = 0xFFFFUL | (pdst << 16UL);
			llmemcpy_gdt[2*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x92UL << 8UL) | (0x0FUL << 16UL);
			/* data (16-bit) limit=0xFFFFFFFF base=0 */
			llmemcpy_gdt[3*2 + 0] = 0xFFFFUL;
			llmemcpy_gdt[3*2 + 1] = (0x92UL << 8UL) | (0x8FUL << 16UL);

			__asm {
				.386
				xor	eax,eax
				mov	ax,ss
				shl	eax,4
				mov	pdst,eax
			}

			/* data (16-bit) limit=0xFFFF */
			llmemcpy_gdt[4*2 + 0] = 0xFFFFUL | (pdst << 16UL);
			llmemcpy_gdt[4*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x92UL << 8UL) | (0x0FUL << 16UL);

			/* 5, 6, and 7 are assigned to VCPI */

			/* TSS */
			pdst = ((uint32_t)FP_SEG(llmemcpy_vcpi_tss) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_vcpi_tss));
			llmemcpy_gdt[8*2 + 0] = 0xFFFFUL | (pdst << 16UL);
			llmemcpy_gdt[8*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x89UL << 8UL) | (0x0FUL << 16UL);

			psrc = (((uint32_t)FP_SEG(llmemcpy_gdt)) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_gdt));
			llmemcpy_gdtr[0] = ((5+4)*8) - 1;
			llmemcpy_gdtr[1] = psrc;
			llmemcpy_gdtr[2] = psrc >> 16UL;
			llmemcpy_gdtr[3] = 0;

			llmemcpy_idtr[0] = (8*256) - 1;
			llmemcpy_idtr[1] = 0;
			llmemcpy_idtr[2] = 0;
			llmemcpy_idtr[3] = 0;

			llmemcpy_vcpi_return[0] = 0;
			llmemcpy_vcpi_return[1] = (5 << 3);

			psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));

			/* we want to point to the upper 4GB to remap it */
			p = (uint64_t FAR*)((char FAR*)llmemcpy_pagetables + 0x1000UL + (3UL<<12UL));

			/* modify the page entries for the topmost 2MB x 4 = 8MB of the 4GB range.
			 * that is where we will map the source and destination buffers, each 2MB x 2.
			 * Notice this is the reason we limit the memcpy to 2MB or less.
			 *
			 * The DOS extender might put our code anywhere within the 2GB, but it certaintly
			 * wouldn't put us up there with the BIOS, not even in flat 32-bit mode with no paging,
			 * on a system with >= 4GB of RAM.
			 *
			 * Starting from 4GB - 8MB:
			 * [0,1] = source
			 * [2,3] = dest */
			p[508] = (src & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
			p[509] = ((src + 0x200000ULL) & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
			p[510] = (dst & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
			p[511] = ((dst + 0x200000ULL) & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;

			/* compute flat addr of page tables.
			 * we do this HERE because if done further down
			 * we may get weird random values and crash, god damn Watcom optimizer */
			psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));

			/* compute flat addr of page tables.
			 * we do this HERE because if done further down
			 * we may get weird random values and crash, god damn Watcom optimizer */
			psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));
			llmemcpy_vcpi[0] = psrc + 0x7000;

			psrc = ((uint32_t)FP_SEG(llmemcpy_gdtr) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_gdtr));
			llmemcpy_vcpi[1] = psrc;

			psrc = ((uint32_t)FP_SEG(llmemcpy_idtr) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_idtr));
			llmemcpy_vcpi[2] = psrc;

			llmemcpy_vcpi[3] = ((8UL << 3UL) << 16UL);	/* LDTR=0 TR=8 */

			llmemcpy_vcpi[4] = 0;

			llmemcpy_vcpi[5] = 1UL << 3UL;

			/* compute the 32-bit flat pointers we will use */
			psrc = (3UL << 30UL) + (508UL << 21UL) + ((uint32_t)src & 0x1FFFFFUL);
			pdst = (3UL << 30UL) + (510UL << 21UL) + ((uint32_t)dst & 0x1FFFFFUL);

			llmem_memcpy_16_inner_pae_vcpi(pdst,psrc,cpy);

			return cpy;
		}
		else if ((src|dst) < 0x1000000000ULL && llmem_meth_pse) { /* if both are below 64GB (36 bits) and PSE-36 is an option */
			uint32_t psrc,pdst;
			register uint32_t FAR *p;
			uint32_t cpy = (uint32_t)min((0x1000000000ULL - max(src,dst)),0x1000000ULL),i; /* we have to max() it to ensure the result is < 4GB, something that will fit into size_t on 32-bit */
			if (cpy > len) cpy = len;

			/* limit memory copy to 4M or less */
			if (cpy > 0x400000UL) cpy = 0x400000UL;

			/* switch to protected mode using VCPI. Let the VCPI server define it's page 0 in the second 4KB page,
			 * and then we'll define paging that is mostly 4M pages except for the first 4M (in the first 4KB page).
			 * everyone (including VCPI) is happy */
			if (llmemcpy_pagetables == NULL || llmemcpy_pagefmt != LLMEMCPY_PF_PSE_4M_VCPI) {
				if (!llmemcpy_alloc(8*1024,LLMEMCPY_PF_PSE_4M_VCPI)) {
					llmem_reason = "llmemcpy: no memory available (8KB needed) for 4M PSE VCPI page table";
					return 0;
				}

				psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));

				/* generate the page table contents to map the entire space as 1:1 (so this code can
				 * continue to execute properly) */
				/* base address i * 4MB, PS=1, U/W/P=1 */
				p = (uint32_t FAR*)llmemcpy_pagetables;
				p[0] = (psrc + 0x1000UL) | 1UL; /* first page directory -> second level is at the second 4KB block */
				for (i=1;i < 1024;i++) p[i] = (i << 22UL) | (1 << 7UL) | 7UL; /* every other 4MB range is mapped 1:1 */
			}
			else {
				p = (uint32_t FAR*)llmemcpy_pagetables;
			}

			if (llmemcpy_gdt == NULL) {
				if ((llmemcpy_gdt = _fmalloc(8*(5+4))) == NULL) {
					llmem_reason = "llmemcpy: no memory available for GDT";
					return 0;
				}
			}

			__asm {
				.386
				xor	eax,eax
				mov	ax,cs
				shl	eax,4
				mov	psrc,eax

				xor	eax,eax
				mov	ax,ds
				shl	eax,4
				mov	pdst,eax
			}

			/* NULL selector */
			llmemcpy_gdt[0*2 + 0] = 0;
			llmemcpy_gdt[0*2 + 1] = 0;
			/* code (16-bit) limit=0xFFFF */
			llmemcpy_gdt[1*2 + 0] = 0xFFFFUL | (psrc << 16UL);
			llmemcpy_gdt[1*2 + 1] = ((psrc >> 16UL) & 0xFFUL) | (0x9AUL << 8UL) | (0x0FUL << 16UL);
			/* data (16-bit) limit=0xFFFF */
			llmemcpy_gdt[2*2 + 0] = 0xFFFFUL | (pdst << 16UL);
			llmemcpy_gdt[2*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x92UL << 8UL) | (0x0FUL << 16UL);
			/* data (16-bit) limit=0xFFFFFFFF base=0 */
			llmemcpy_gdt[3*2 + 0] = 0xFFFFUL;
			llmemcpy_gdt[3*2 + 1] = (0x92UL << 8UL) | (0x8FUL << 16UL);

			__asm {
				.386
				xor	eax,eax
				mov	ax,ss
				shl	eax,4
				mov	pdst,eax
			}
		
			/* data (16-bit) limit=0xFFFF */
			llmemcpy_gdt[4*2 + 0] = 0xFFFFUL | (pdst << 16UL);
			llmemcpy_gdt[4*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x92UL << 8UL) | (0x0FUL << 16UL);

			/* 5, 6, and 7 are assigned to VCPI */

			/* TSS */
			pdst = ((uint32_t)FP_SEG(llmemcpy_vcpi_tss) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_vcpi_tss));
			llmemcpy_gdt[8*2 + 0] = 0xFFFFUL | (pdst << 16UL);
			llmemcpy_gdt[8*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x89UL << 8UL) | (0x0FUL << 16UL);

			psrc = (((uint32_t)FP_SEG(llmemcpy_gdt)) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_gdt));
			llmemcpy_gdtr[0] = ((5+4)*8) - 1;
			llmemcpy_gdtr[1] = psrc;
			llmemcpy_gdtr[2] = psrc >> 16UL;
			llmemcpy_gdtr[3] = 0;

			llmemcpy_idtr[0] = (8*256) - 1;
			llmemcpy_idtr[1] = 0;
			llmemcpy_idtr[2] = 0;
			llmemcpy_idtr[3] = 0;

			llmemcpy_vcpi_return[0] = 0;
			llmemcpy_vcpi_return[1] = (5 << 3);

			/* modify the page entries for the topmost 4MB x 4 = 16MB of the 4GB range.
			 * that is where we will map the source and destination buffers, each 4MB x 2.
			 * Notice this is the reason we limit the memcpy to 4MB or less.
			 *
			 * The DOS extender might put our code anywhere within the 4GB, but it certaintly
			 * wouldn't put us up there with the BIOS, not even in flat 32-bit mode with no paging,
			 * on a system with >= 4GB of RAM.
			 *
			 * Starting from 4GB - 16MB:
			 * [0,1] = source
			 * [2,3] = dest */
			p[1020] = (uint32_t)(((uint32_t)src & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
					((((uint32_t)(src >> 32ULL)) & 0xFUL) << 13UL));
			p[1021] = (uint32_t)(((uint32_t)(src+0x400000ULL) & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
					((((uint32_t)((src+0x400000ULL) >> 32ULL)) & 0xFUL) << 13UL));

			p[1022] = (uint32_t)(((uint32_t)dst & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
					((((uint32_t)(dst >> 32ULL)) & 0xFUL) << 13UL));
			p[1023] = (uint32_t)(((uint32_t)(dst+0x400000ULL) & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
					((((uint32_t)((dst+0x400000ULL) >> 32ULL)) & 0xFUL) << 13UL));

			/* compute flat addr of page tables.
			 * we do this HERE because if done further down
			 * we may get weird random values and crash, god damn Watcom optimizer */
			psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));
			llmemcpy_vcpi[0] = psrc;

			psrc = ((uint32_t)FP_SEG(llmemcpy_gdtr) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_gdtr));
			llmemcpy_vcpi[1] = psrc;

			psrc = ((uint32_t)FP_SEG(llmemcpy_idtr) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_idtr));
			llmemcpy_vcpi[2] = psrc;

			llmemcpy_vcpi[3] = ((8UL << 3UL) << 16UL);	/* LDTR=0 TR=8 */

			llmemcpy_vcpi[4] = 0;

			llmemcpy_vcpi[5] = 1UL << 3UL;

			/* compute the 32-bit flat pointers we will use */
			psrc = (1020UL << 22UL) + ((uint32_t)src & 0x3FFFFFUL);
			pdst = (1022UL << 22UL) + ((uint32_t)dst & 0x3FFFFFUL);

			llmem_memcpy_16_inner_pse_vcpi(pdst,psrc,cpy);

			return cpy;
		}
	}
	else if (src < 0x100000ULL && dst < 0x100000ULL) { /* aka both src and dst below < 1MB boundary (non v86) */
		size_t cpy = 0x100000UL - (size_t)max(src,dst),scpy;
		if (cpy > len) cpy = len;
		scpy = cpy;

		while (scpy >= 4096UL) {
			_fmemcpy(MK_FP(dst>>4,dst&0xFUL),MK_FP(src>>4,src&0xFUL),4096);
			scpy -= 4096UL;
			src += 4096ULL;
			dst += 4096ULL;
		}
		if (scpy > 0UL) {
			_fmemcpy(MK_FP(dst>>4,dst&0xFUL),MK_FP(src>>4,src&0xFUL),scpy);
		}

		return cpy;
	}
	/* this is the preferred option, especially for modern CPUs */
	else if ((src|dst) < (1ULL << 52ULL) && llmem_meth_pae) { /* if both are below 52 bits, and PAE is an option */
		register uint64_t FAR *p;
		uint32_t psrc,pdst;
		uint32_t cpy = (uint32_t)min(((1ULL << 52ULL) - max(src,dst)),0x1000000ULL),i,j; /* we have to max() it to ensure the result is < 4GB, something that will fit into size_t on 32-bit */
		if (cpy > len) cpy = len;

		/* limit memory copy to 2M or less */
		if (cpy > 0x200000UL) cpy = 0x200000UL;

		if (llmemcpy_gdt == NULL) {
			if ((llmemcpy_gdt = _fmalloc(8*5)) == NULL) {
				llmem_reason = "llmemcpy: no memory available for GDT";
				return 0;
			}
		}

		__asm {
			.386
			xor	eax,eax
			mov	ax,cs
			shl	eax,4
			mov	psrc,eax

			xor	eax,eax
			mov	ax,ds
			shl	eax,4
			mov	pdst,eax
		}

		/* NULL selector */
		llmemcpy_gdt[0*2 + 0] = 0;
		llmemcpy_gdt[0*2 + 1] = 0;
		/* code (16-bit) limit=0xFFFF */
		llmemcpy_gdt[1*2 + 0] = 0xFFFFUL | (psrc << 16UL);
		llmemcpy_gdt[1*2 + 1] = ((psrc >> 16UL) & 0xFFUL) | (0x9AUL << 8UL) | (0x0FUL << 16UL);
		/* data (16-bit) limit=0xFFFF */
		llmemcpy_gdt[2*2 + 0] = 0xFFFFUL | (pdst << 16UL);
		llmemcpy_gdt[2*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x92UL << 8UL) | (0x0FUL << 16UL);
		/* data (16-bit) limit=0xFFFFFFFF base=0 */
		llmemcpy_gdt[3*2 + 0] = 0xFFFFUL;
		llmemcpy_gdt[3*2 + 1] = (0x92UL << 8UL) | (0x8FUL << 16UL);

		__asm {
			.386
			xor	eax,eax
			mov	ax,ss
			shl	eax,4
			mov	pdst,eax
		}
		
		/* data (16-bit) limit=0xFFFF */
		llmemcpy_gdt[4*2 + 0] = 0xFFFFUL | (pdst << 16UL);
		llmemcpy_gdt[4*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x92UL << 8UL) | (0x0FUL << 16UL);

		psrc = (((uint32_t)FP_SEG(llmemcpy_gdt)) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_gdt));
		llmemcpy_gdtr[0] = (5*8) - 1;
		llmemcpy_gdtr[1] = psrc;
		llmemcpy_gdtr[2] = psrc >> 16UL;
		llmemcpy_gdtr[3] = 0;

		/* make temporary page tables, switch on PSE, switch to page tables, and do the copy operation.
		 * This code takes advantage of the Page Size bit to make only the first level (4M pages)
		 * and that modern processors interpret bits 16...13 as bits 35...32 of the physical address. */
		if (llmemcpy_pagetables == NULL || llmemcpy_pagefmt != LLMEMCPY_PF_PAE_2M) {
			if (!llmemcpy_alloc(20*1024,LLMEMCPY_PF_PAE_2M)) { /* 4KB PDPTE + 16KB Page directories */
				llmem_reason = "llmemcpy: no memory available (20KB needed) for 2M PAE page table";
				return 0;
			}

			/* we need the phys addr of the table */
			psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));

			/* level 1: PDPTE */
			p = (uint64_t FAR*)llmemcpy_pagetables;
			for (i=0;i < 4;i++) p[i] = ((uint64_t)psrc + 0x1000UL + (i<<12UL)) | 1UL;
			for (;i < 512;i++) p[i] = 0;

			/* level 2: page directories */
			for (j=0;j < 4;j++) {
				p = (uint64_t FAR*)((char FAR*)llmemcpy_pagetables + 0x1000UL + (j<<12UL));
				for (i=0;i < 512;i++) p[i] = ((j << 30UL) | (i << 21UL)) | (1UL << 7UL) | 7UL;
			}
		}

		psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));

		/* we want to point to the upper 4GB to remap it */
		p = (uint64_t FAR*)((char FAR*)llmemcpy_pagetables + 0x1000UL + (3UL<<12UL));

		/* modify the page entries for the topmost 2MB x 4 = 8MB of the 4GB range.
		 * that is where we will map the source and destination buffers, each 2MB x 2.
		 * Notice this is the reason we limit the memcpy to 2MB or less.
		 *
		 * The DOS extender might put our code anywhere within the 2GB, but it certaintly
		 * wouldn't put us up there with the BIOS, not even in flat 32-bit mode with no paging,
		 * on a system with >= 4GB of RAM.
		 *
		 * Starting from 4GB - 8MB:
		 * [0,1] = source
		 * [2,3] = dest */
		p[508] = (src & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
		p[509] = ((src + 0x200000ULL) & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
		p[510] = (dst & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
		p[511] = ((dst + 0x200000ULL) & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;

		/* compute flat addr of page tables.
		 * we do this HERE because if done further down
		 * we may get weird random values and crash, god damn Watcom optimizer */
		psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));
		__asm {
			.386p

			cli

			; enable PSE
			mov	eax,cr4
			or	eax,0x30
			mov	cr4,eax

			; load CR3
			mov	eax,psrc
			mov	cr3,eax
		}

		/* load the GDTR here. again, the goddamn optimizer. */
		__asm {
			.386p

			push	es
			mov	ax,seg llmemcpy_gdtr
			mov	es,ax
			lgdt fword ptr [es:llmemcpy_gdtr]
			pop	es
		}

		/* compute the 32-bit flat pointers we will use */
		psrc = (3UL << 30UL) + (508UL << 21UL) + ((uint32_t)src & 0x1FFFFFUL);
		pdst = (3UL << 30UL) + (510UL << 21UL) + ((uint32_t)dst & 0x1FFFFFUL);

		__asm {
			.386p

			; load the params we need NOW. we can't load variables during this brief
			; hop into protected mode.
			mov	ecx,cpy
			mov	esi,psrc
			mov	edi,pdst

			; inner code
			call	llmem_memcpy_16_inner_pae

			; switch off PSE
			mov	eax,cr4
			and	eax,0xFFFFFFCF
			mov	cr4,eax

			; clear CR3
			xor	eax,eax
			mov	cr3,eax
		}

		llmemcpy_gdtr[0] = 0xFFFF;
		llmemcpy_gdtr[1] = 0;
		llmemcpy_gdtr[2] = 0;

		__asm {
			.386p

			push	es
			mov	ax,seg llmemcpy_gdtr
			mov	es,ax
			lgdt fword ptr [es:llmemcpy_gdtr]
			pop	es
		}

		__asm {
			sti
		}

		return cpy;
	}
	/* FIXME: Known bug:
	 *
	 *       Build:       16-bit real mode (large model)
	 *       Run under:   Oracle Virtual Box 4.1.8 configured for 64-bit guest and AMD VT-x acceleration
	 *       Bug:         Though Virtual Box reports PSE and PAE extensions, the PSE-36 code here crashes and hangs the
	 *                    virtual machine. Turning off 64-bit guest and AMD VT-x resolves the crash and both PSE-36 and PAE
	 *                    code paths here (16-bit real mode) execute without problems. Perhaps Virtual Box has a problem
	 *                    with PSE page tables and AMD VT-x?
	 *       Workaround:  Don't use PSE mode, use PAE mode. or,
	 *                    Switch off 64-bit guest and AMD VT-x in Virtual Box (which causes Virtual Box to report only PSE) */
	else if ((src|dst) <= llmem_pse_limit && llmem_meth_pse) { /* if both are below 64GB (36 bits) and PSE-36 is an option */
		uint32_t psrc,pdst;
		register uint32_t FAR *p;
		uint32_t cpy = (uint32_t)min(((llmem_pse_limit+1ULL) - max(src,dst)),0x1000000ULL),i; /* we have to max() it to ensure the result is < 4GB, something that will fit into size_t on 32-bit */
		if (cpy > len) cpy = len;

		/* limit memory copy to 4M or less */
		if (cpy > 0x400000UL) cpy = 0x400000UL;

		/* make temporary page tables, switch on PSE, switch to page tables, and do the copy operation.
		 * This code takes advantage of the Page Size bit to make only the first level (4M pages)
		 * and that modern processors interpret bits 16...13 as bits 35...32 of the physical address. */
		if (llmemcpy_pagetables == NULL || llmemcpy_pagefmt != LLMEMCPY_PF_PSE_4M) {
			if (!llmemcpy_alloc(4*1024,LLMEMCPY_PF_PSE_4M)) {
				llmem_reason = "llmemcpy: no memory available (4KB needed) for 4M PSE page table";
				return 0;
			}

			/* generate the page table contents to map the entire space as 1:1 (so this code can
			 * continue to execute properly) */
			/* base address i * 4MB, PS=1, U/W/P=1 */
			p = (uint32_t FAR*)llmemcpy_pagetables;
			for (i=0;i < 1024;i++) p[i] = (i << 22UL) | (1 << 7UL) | 7UL;
		}
		else {
			p = (uint32_t FAR*)llmemcpy_pagetables;
		}

		if (llmemcpy_gdt == NULL) {
			if ((llmemcpy_gdt = _fmalloc(8*5)) == NULL) {
				llmem_reason = "llmemcpy: no memory available for GDT";
				return 0;
			}
		}

		__asm {
			.386
			xor	eax,eax
			mov	ax,cs
			shl	eax,4
			mov	psrc,eax

			xor	eax,eax
			mov	ax,ds
			shl	eax,4
			mov	pdst,eax
		}

		/* NULL selector */
		llmemcpy_gdt[0*2 + 0] = 0;
		llmemcpy_gdt[0*2 + 1] = 0;
		/* code (16-bit) limit=0xFFFF */
		llmemcpy_gdt[1*2 + 0] = 0xFFFFUL | (psrc << 16UL);
		llmemcpy_gdt[1*2 + 1] = ((psrc >> 16UL) & 0xFFUL) | (0x9AUL << 8UL) | (0x0FUL << 16UL);
		/* data (16-bit) limit=0xFFFF */
		llmemcpy_gdt[2*2 + 0] = 0xFFFFUL | (pdst << 16UL);
		llmemcpy_gdt[2*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x92UL << 8UL) | (0x0FUL << 16UL);
		/* data (16-bit) limit=0xFFFFFFFF base=0 */
		llmemcpy_gdt[3*2 + 0] = 0xFFFFUL;
		llmemcpy_gdt[3*2 + 1] = (0x92UL << 8UL) | (0x8FUL << 16UL);

		__asm {
			.386
			xor	eax,eax
			mov	ax,ss
			shl	eax,4
			mov	pdst,eax
		}
		
		/* data (16-bit) limit=0xFFFF */
		llmemcpy_gdt[4*2 + 0] = 0xFFFFUL | (pdst << 16UL);
		llmemcpy_gdt[4*2 + 1] = ((pdst >> 16UL) & 0xFFUL) | (0x92UL << 8UL) | (0x0FUL << 16UL);

		psrc = (((uint32_t)FP_SEG(llmemcpy_gdt)) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_gdt));
		llmemcpy_gdtr[0] = (5*8) - 1;
		llmemcpy_gdtr[1] = psrc;
		llmemcpy_gdtr[2] = psrc >> 16UL;
		llmemcpy_gdtr[3] = 0;

		/* modify the page entries for the topmost 4MB x 4 = 16MB of the 4GB range.
		 * that is where we will map the source and destination buffers, each 4MB x 2.
		 * Notice this is the reason we limit the memcpy to 4MB or less.
		 *
		 * The DOS extender might put our code anywhere within the 4GB, but it certaintly
		 * wouldn't put us up there with the BIOS, not even in flat 32-bit mode with no paging,
		 * on a system with >= 4GB of RAM.
		 *
		 * Starting from 4GB - 16MB:
		 * [0,1] = source
		 * [2,3] = dest */
		p[1020] = (uint32_t)(((uint32_t)src & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
				((((uint32_t)(src >> 32ULL)) & 0xFFUL) << 13UL));
		p[1021] = (uint32_t)(((uint32_t)(src+0x400000ULL) & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
				((((uint32_t)((src+0x400000ULL) >> 32ULL)) & 0xFFUL) << 13UL));

		p[1022] = (uint32_t)(((uint32_t)dst & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
				((((uint32_t)(dst >> 32ULL)) & 0xFFUL) << 13UL));
		p[1023] = (uint32_t)(((uint32_t)(dst+0x400000ULL) & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
				((((uint32_t)((dst+0x400000ULL) >> 32ULL)) & 0xFFUL) << 13UL));

		/* compute flat addr of page tables.
		 * we do this HERE because if done further down
		 * we may get weird random values and crash, god damn Watcom optimizer */
		psrc = ((uint32_t)FP_SEG(llmemcpy_pagetables) << 4UL) + ((uint32_t)FP_OFF(llmemcpy_pagetables));
		__asm {
			.386p

			cli

			; enable PSE
			mov	eax,cr4
			or	eax,0x10
			mov	cr4,eax

			; load CR3
			mov	eax,psrc
			mov	cr3,eax
		}

		/* load the GDTR here. again, the goddamn optimizer. */
		__asm {
			.386p

			push	es
			mov	ax,seg llmemcpy_gdtr
			mov	es,ax
			lgdt fword ptr [es:llmemcpy_gdtr]
			pop	es
		}

		/* compute the 32-bit flat pointers we will use */
		psrc = (1020UL << 22UL) + ((uint32_t)src & 0x3FFFFFUL);
		pdst = (1022UL << 22UL) + ((uint32_t)dst & 0x3FFFFFUL);

		__asm {
			.386p

			; load the params we need NOW. we can't load variables during this brief
			; hop into protected mode.
			mov	ecx,cpy
			mov	esi,psrc
			mov	edi,pdst

			; inner code
			call	llmem_memcpy_16_inner_pse

			; switch off PSE
			mov	eax,cr4
			and	eax,0xFFFFFFEF
			mov	cr4,eax

			; clear CR3
			xor	eax,eax
			mov	cr3,eax
		}

		llmemcpy_gdtr[0] = 0xFFFF;
		llmemcpy_gdtr[1] = 0;
		llmemcpy_gdtr[2] = 0;

		__asm {
			.386p

			push	es
			mov	ax,seg llmemcpy_gdtr
			mov	es,ax
			lgdt fword ptr [es:llmemcpy_gdtr]
			pop	es
		}

		__asm {
			sti
		}

		return cpy;
	}
#else /* TARGET_MSDOS == 32 */
	if (dos_ltp_info.paging) {
		/* TODO: If we're not running in Ring-0, then do VCPI jailbreak to get to Ring-0 */

		{
			uint16_t v=0;
			__asm {
				mov	ax,cs
				mov	v,ax
			}

			if ((v&3) != 0) {
				llmem_reason = "DPMI/VCPI server is running this code with paging, not at Ring 0";
				return 0;
			}
		}

		llmem_reason = "32-bit with paging already enabled under DPMI/VCPI not implemented";
		return 0;
	}
	else {
		/* paging is not enabled. if we're Ring 0 we may be able to play with the control registers */
		{
			uint16_t v=0;
			__asm {
				mov	ax,cs
				mov	v,ax
			}

			if ((v&3) != 0) {
				llmem_reason = "DPMI/VCPI server is running this code without paging, but not at Ring 0";
				return 0;
			}
		}

		if ((src|dst) < 0x100000000ULL) { /* if both are below 4GB (32 bits) */
			/* paging disabled, we can just memcpy() */
			size_t cpy = (size_t)(0x100000000ULL - max(src,dst));
			if (cpy > len) cpy = len;

			memcpy((void*)((uint32_t)dst),(void const*)((uint32_t)src),cpy);
			return cpy;
		}
		/* this is the preferred option, especially for modern CPUs */
		else if ((src|dst) < (1ULL << 52ULL) && llmem_meth_pae) { /* if both are below 52 bits, and PAE is an option */
			size_t old_cr4=0;
			register uint64_t *p;
			volatile unsigned char *psrc,*pdst;
			size_t cpy = (size_t)min(((1ULL << 52ULL) - max(src,dst)),0x1000000ULL),i,j; /* we have to max() it to ensure the result is < 4GB, something that will fit into size_t on 32-bit */
			if (cpy > len) cpy = len;

			/* limit memory copy to 2M or less */
			if (cpy > 0x200000UL) cpy = 0x200000UL;

			/* make temporary page tables, switch on PSE, switch to page tables, and do the copy operation.
			 * This code takes advantage of the Page Size bit to make only the first level (4M pages)
			 * and that modern processors interpret bits 16...13 as bits 35...32 of the physical address. */
			if (llmemcpy_pagetables == NULL || llmemcpy_pagefmt != LLMEMCPY_PF_PAE_2M) {
				if (!llmemcpy_alloc(20*1024,LLMEMCPY_PF_PAE_2M)) { /* 4KB PDPTE + 16KB Page directories */
					llmem_reason = "llmemcpy: no memory available (20KB needed) for 2M PAE page table";
					return 0;
				}

				/* level 1: PDPTE */
				p = (uint64_t*)llmemcpy_pagetables;
				for (i=0;i < 4;i++) p[i] = ((uint64_t)llmemcpy_pagetables + 0x1000UL + (i<<12UL)) | 1UL;
				for (;i < 512;i++) p[i] = 0;

				/* level 2: page directories */
				for (j=0;j < 4;j++) {
					p = (uint64_t*)((char*)llmemcpy_pagetables + 0x1000UL + (j<<12UL));
					for (i=0;i < 512;i++) p[i] = ((j << 30UL) | (i << 21UL)) | (1UL << 7UL) | 7UL;
				}
			}

			/* we want to point to the upper 4GB to remap it */
			p = (uint64_t*)((char*)llmemcpy_pagetables + 0x1000UL + (3UL<<12UL));

			/* modify the page entries for the topmost 2MB x 4 = 8MB of the 4GB range.
			 * that is where we will map the source and destination buffers, each 2MB x 2.
			 * Notice this is the reason we limit the memcpy to 2MB or less.
			 *
			 * The DOS extender might put our code anywhere within the 2GB, but it certaintly
			 * wouldn't put us up there with the BIOS, not even in flat 32-bit mode with no paging,
			 * on a system with >= 4GB of RAM.
			 *
			 * Starting from 4GB - 8MB:
			 * [0,1] = source
			 * [2,3] = dest */
			p[508] = (src & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
			p[509] = ((src + 0x200000ULL) & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
			p[510] = (dst & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;
			p[511] = ((dst + 0x200000ULL) & 0xFFFFFFFE00000ULL) | (1ULL << 7ULL) | 7ULL;

			/* we're going to fuck with the page tables, we don't want interrupts to cause problems */
			_cli();

			/* save the old value of CR4, and then write to enable PSE. also read/write cr0 to enable paging */
			__asm {
				mov		eax,cr4
				mov		old_cr4,eax
				or		eax,0x30		; set bit 4 (PSE) and bit 5 (PAE)
				mov		cr4,eax

				mov		eax,llmemcpy_pagetables
				mov		cr3,eax

				mov		eax,cr0
				or		eax,0x80000001		; set bit 31 (paging)
				mov		cr0,eax
			}

			/* commence the memcpy */
			{
				psrc = (volatile unsigned char*)((3UL << 30UL) + (508UL << 21UL) + ((size_t)src & 0x1FFFFFUL));
				pdst = (volatile unsigned char*)((3UL << 30UL) + (510UL << 21UL) + ((size_t)dst & 0x1FFFFFUL));
				memcpy((void*)pdst,(void*)psrc,cpy);
			}

			/* restore CR4, disable paging */
			__asm {
				mov		eax,cr0
				and		eax,0x7FFFFFFF		; clear bit 31 (paging)
				mov		cr0,eax

				mov		eax,old_cr4
				mov		cr4,eax

				xor		eax,eax
				mov		cr3,eax			; erase CR3
			}

			_sti();
			return cpy;
		}
		else if ((src|dst) <= llmem_pse_limit && llmem_meth_pse) { /* if both are below 64GB (36 bits) and PSE-36 is an option */
			size_t old_cr4=0;
			register uint32_t *p;
			volatile unsigned char *psrc,*pdst;
			size_t cpy = (size_t)min(((llmem_pse_limit+1ULL) - max(src,dst)),0x1000000ULL),i; /* we have to max() it to ensure the result is < 4GB, something that will fit into size_t on 32-bit */
			if (cpy > len) cpy = len;

			/* limit memory copy to 4M or less */
			if (cpy > 0x400000UL) cpy = 0x400000UL;

			/* make temporary page tables, switch on PSE, switch to page tables, and do the copy operation.
			 * This code takes advantage of the Page Size bit to make only the first level (4M pages)
			 * and that modern processors interpret bits 16...13 as bits 35...32 of the physical address. */
			if (llmemcpy_pagetables == NULL || llmemcpy_pagefmt != LLMEMCPY_PF_PSE_4M) {
				if (!llmemcpy_alloc(4*1024,LLMEMCPY_PF_PSE_4M)) {
					llmem_reason = "llmemcpy: no memory available (4KB needed) for 4M PSE page table";
					return 0;
				}

				/* generate the page table contents to map the entire space as 1:1 (so this code can
				 * continue to execute properly) */
				/* base address i * 4MB, PS=1, U/W/P=1 */
				p = (uint32_t*)llmemcpy_pagetables;
				for (i=0;i < 1024;i++) p[i] = (i << 22UL) | (1 << 7UL) | 7UL;
			}
			else {
				p = (uint32_t*)llmemcpy_pagetables;
			}

			/* modify the page entries for the topmost 4MB x 4 = 16MB of the 4GB range.
			 * that is where we will map the source and destination buffers, each 4MB x 2.
			 * Notice this is the reason we limit the memcpy to 4MB or less.
			 *
			 * The DOS extender might put our code anywhere within the 4GB, but it certaintly
			 * wouldn't put us up there with the BIOS, not even in flat 32-bit mode with no paging,
			 * on a system with >= 4GB of RAM.
			 *
			 * Starting from 4GB - 16MB:
			 * [0,1] = source
			 * [2,3] = dest */
			p[1020] = (uint32_t)(((uint32_t)src & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
				((((uint32_t)(src >> 32ULL)) & 0xFFUL) << 13UL));
			p[1021] = (uint32_t)(((uint32_t)(src+0x400000ULL) & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
				((((uint32_t)((src+0x400000ULL) >> 32ULL)) & 0xFFUL) << 13UL));

			p[1022] = (uint32_t)(((uint32_t)dst & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
				((((uint32_t)(dst >> 32ULL)) & 0xFFUL) << 13UL));
			p[1023] = (uint32_t)(((uint32_t)(dst+0x400000ULL) & 0xFFC00000ULL) | (1 << 7UL) | 7UL |
				((((uint32_t)((dst+0x400000ULL) >> 32ULL)) & 0xFFUL) << 13UL));

			/* we're going to fuck with the page tables, we don't want interrupts to cause problems */
			_cli();

			/* save the old value of CR4, and then write to enable PSE. also read/write cr0 to enable paging */
			__asm {
				mov		eax,cr4
				mov		old_cr4,eax
				or		eax,0x10		; set bit 4 (PSE)
				mov		cr4,eax

				mov		eax,llmemcpy_pagetables
				mov		cr3,eax

				mov		eax,cr0
				or		eax,0x80000001		; set bit 31 (paging)
				mov		cr0,eax
			}

			/* commence the memcpy */
			{
				psrc = (volatile unsigned char*)((1020UL << 22UL) + ((size_t)src & 0x3FFFFFUL));
				pdst = (volatile unsigned char*)((1022UL << 22UL) + ((size_t)dst & 0x3FFFFFUL));
				memcpy((void*)pdst,(void*)psrc,cpy);
			}

			/* restore CR4, disable paging */
			__asm {
				mov		eax,cr0
				and		eax,0x7FFFFFFF		; clear bit 31 (paging)
				mov		cr0,eax

				mov		eax,old_cr4
				mov		cr4,eax

				xor		eax,eax
				mov		cr3,eax			; erase CR3
			}

			_sti();
			return cpy;
		}
	}
#endif

	/* pick a method, based on what is available */

	llmem_reason = "llmemcpy: no methods available";
	return 0;
}

