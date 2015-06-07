/* gdt_enum.c
 *
 * Library for reading the Global Descriptor Table if possible.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS
 *   - Windows 3.0/3.1/95/98/ME
 *   - Windows NT 3.1/3.51/4.0/2000/XP/Vista/7
 *   - OS/2 16-bit
 *   - OS/2 32-bit
 *
 * If the host OS is loose, or unprotected, this library can abuse the
 * loophole to read the contents of the Global Descriptor Table. Note
 * that this is possible under Windows 3.0/3.1/95/98/ME because they
 * leave it out in the open. Windows NT does NOT let us do this!! Neither
 * does OS/2!
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/cpu/gdt_enum.h>

unsigned char				cpu_gdtlib_can_read = 0,cpu_gdtlib_can_write = 0,cpu_gdtlib_result = 0xFF;
uint16_t				cpu_gdtlib_ldtr = 0;
struct x86_gdtr				cpu_gdtlib_gdtr;
struct cpu_gdtlib_entry			cpu_gdtlib_ldt_ent;
#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
uint16_t				cpu_gdtlib_gdt_sel = 0;	/* selector for reading GDT table */
uint16_t				cpu_gdtlib_ldt_sel = 0;	/* selector for reading LDT table */
#endif

#if TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
uint32_t				cpu_gdtlib_lin_bias = 0UL;
#endif

unsigned int cpu_gdtlib_gdt_entries(struct x86_gdtr *r) {
	if (r->limit == 0xFFFFU) return 0x2000;
	return ((unsigned int)r->limit + 1U) >> 3;
}

unsigned int cpu_gdtlib_ldt_entries(struct cpu_gdtlib_entry *r) {
	return ((unsigned int)(r->limit + 1UL)) >> 3UL;
}

int cpu_gdtlib_read_ldtr(uint16_t *sel) {
	if (!cpu_gdtlib_can_read)
		return 0;

#if TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
	/* in what is likely a strange inversion of priorities, the Win 9x kernel allows
	 * DOS programs like us to read the GDTR register. But... attempts to read the LDTR
	 * are not permitted. */
	if (windows_mode == WINDOWS_ENHANCED)
		return 0;
#endif

#if TARGET_MSDOS == 16
# if defined(__LARGE__) || defined(__COMPACT__)
	__asm {
		.286p
		push	ds
		push	si
		lds	si,sel
		sldt	[si]
		pop	si
		pop	ds
	}
# else
	__asm {
		.286p
		push	si
		mov	si,sel
		sldt	[si]
		pop	si
	}
# endif
#else
	__asm {
		push	eax
		mov	eax,sel
		sldt	[eax]
		pop	eax
	}
#endif
	return 1;
}

int cpu_gdtlib_read_gdtr(struct x86_gdtr *raw) {
	if (!cpu_gdtlib_can_read)
		return 0;

#if TARGET_MSDOS == 16
# if defined(__LARGE__) || defined(__COMPACT__)
	__asm {
		.286p
		push	ds
		push	si
		lds	si,raw
		sgdt	[si]
		pop	si
		pop	ds
	}
# else
	__asm {
		.286p
		push	si
		mov	si,raw
		sgdt	[si]
		pop	si
	}
# endif
#else
	__asm {
		push	eax
		mov	eax,raw
		sgdt	[eax]
		pop	eax
	}
#endif
	return 1;
}

int cpu_gdtlib_init() {
	if (cpu_gdtlib_result == 0xFF) {
		cpu_gdtlib_ldtr = 0;
		cpu_gdtlib_can_read = cpu_gdtlib_can_write = 0;
		if (cpu_basic_level < 0) cpu_probe();
		if (cpu_basic_level < 3) return (cpu_gdtlib_result=0);
		probe_dos();
		detect_windows();

#if defined(TARGET_OS2)
		/* OS/2 16-bit or 32-bit: Not gonna happen */
		return (cpu_gdtlib_result=0);
#elif TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS)
	/* 16-bit MS-DOS:
	 *    Either:
	 *       - We're in real mode, and there is no such GDT installed
	 *       - We're in v86 mode, we might be able to locate the GDT and read it
	 *       - We're in v86 mode and VCPI is present: uhhhmmmm.... we'll we could enter via VCPI but that only installs our OWN GDT tables, so...
	 *       - We're in v86 mode under Windows 3.x/9x/ME: DPMI is likely available, we could take advantage of the 9x kernel's permissiveness to read the GDT
	 *       - We're in v86 mode under Windows NT/2000/XP: don't try, not gonna happen. */
		if (windows_mode == WINDOWS_NT)
			return (cpu_gdtlib_result=0);
		if (windows_mode == WINDOWS_REAL)
			return (cpu_gdtlib_result=0);
		if (!cpu_v86_active) /* if there's no v86 mode, then there's no protected mode GDT table */
			return (cpu_gdtlib_result=0);
		if (!dpmi_lin2fmemcpy_init())
			return (cpu_gdtlib_result=0);

		/* FIXME: Windows 3.0: This code hangs */
		if (windows_mode == WINDOWS_ENHANCED && windows_version < 0x30A) /* Anything less than Windows 3.1 */
			return (cpu_gdtlib_result=0);

		cpu_gdtlib_can_read = 1;
		return (cpu_gdtlib_result=1);
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
	/* 32-bit MS-DOS:
	 *    Either:
	 *       - Pure DOS mode is active, the DOS extender did not enable paging, and we're ring 0. we can just read the GDT directly. No problem.
	 *       - Pure DOS mode with EMM386.EXE. The DOS extender used VCPI to enter protected mode, we just have to locate the GDT somehow.
	 *         Usually it ends up in the first 1MB (DOS area) where 1:1 mapping is still retained. We can read it
	 *       - Windows 3.x/9x/ME DOS Box: We're running at ring 3, but we can take advantage of the 9x kernel's apparent permissiveness to
	 *         locate the GDT and read it.
	 *       - Windows NT/2000/XP: Not gonna happen. Even reading the control register will fault our application. */
		if (windows_mode == WINDOWS_NT)
			return (cpu_gdtlib_result=0);

		cpu_gdtlib_can_read = 1;
		return (cpu_gdtlib_result=1);
#elif TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	/* 16-bit Win16 application:
	 *    Either:
	 *       - Windows 3.x/9x/ME: As a Win16 app we can use Win16 functions like AllocateSelector(), etc. to give ourself access to the GDT.
	 *       - Windows NT/2000/XP: We might be able to use AllocateSelector(), etc. NTVDM.EXE allows "SGDT" but result seems to point to garbage
	 *
	 * For obvious reasons, we do not attempt to read the GDT in real-mode Windows 3.0 because there is no GDT */
		if (windows_mode == WINDOWS_NT)
			return (cpu_gdtlib_result=0);
		if (windows_mode == WINDOWS_REAL)
			return (cpu_gdtlib_result=0);

		cpu_gdtlib_can_read = 1;
		return (cpu_gdtlib_result=1);
#elif TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
	/* 32-bit Windows application:
	 *    Either:
	 *       - Windows 3.1/9x/ME: We can use "sgdt" to read the GDTR, and then just refer to that location in memory.
	 *       - Windows NT/2000/XP: Not gonna happen */
		if (windows_mode == WINDOWS_NT)
			return (cpu_gdtlib_result=0);

# ifdef WIN386
		{
			unsigned short s=0;

			__asm {
				mov	ax,ds
				mov	s,ax
			}

			/* #1: We need to know what Watcom Win386's data selector bias is */
			cpu_gdtlib_lin_bias = GetSelectorBase(s);

			/* #2: Whatever the base is, Win386 usually sets the limit to 0xFFFFFFFF.
			 *     The base is usually something like 0x80302330, and the GDT is usually
			 *     something like 0x80112000, so if we have to rollover BACKWARDS we
			 *     want to make sure the limit is 0xFFFFFFFF */
			if (GetSelectorLimit(s) != 0xFFFFFFFFUL)
				return (cpu_gdtlib_result=0);
		}
# else
		/* Windows 3.1 with Win32s sets the flat selectors to base = 0xFFFF0000 for whatever reason.
		 * So to properly read the GDT we have to compensate for that */
		if (windows_mode == WINDOWS_ENHANCED && windows_version < 0x35F)
			cpu_gdtlib_lin_bias = 0xFFFF0000UL;
# endif

		cpu_gdtlib_can_read = 1;
		return (cpu_gdtlib_result=1);
#endif
	}

	return cpu_gdtlib_result;
}

void cpu_gdtlib_free() {
#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	if (cpu_gdtlib_gdt_sel != 0) {
		FreeSelector(cpu_gdtlib_gdt_sel);
		cpu_gdtlib_gdt_sel=0;
	}
	if (cpu_gdtlib_ldt_sel != 0) {
		FreeSelector(cpu_gdtlib_ldt_sel);
		cpu_gdtlib_ldt_sel=0;
	}
#endif
}

int cpu_gdtlib_ldt_read_entry(struct cpu_gdtlib_entry *e,unsigned int i) {
	if (i >= cpu_gdtlib_ldt_entries(&cpu_gdtlib_ldt_ent))
		return 0;
	if (!cpu_gdtlib_can_read)
		return 0;

#if defined(TARGET_OS2)
	return 0; /* NOPE */
#elif TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS)
	{ /* 16-bit real mode DOS */
		unsigned char tmp[8];
	
		if (dpmi_lin2fmemcpy(tmp,cpu_gdtlib_ldt_ent.base + (i << 3),8) == 0)
			return 0;

		e->limit = (uint32_t)( *((uint16_t*)(tmp+0)) );
		e->base = (uint32_t)( *((uint32_t*)(tmp+2)) & 0xFFFFFFUL );
		e->access = tmp[5];
		e->granularity = tmp[6];
		e->base |= ((uint32_t)tmp[7]) << 24UL;
		e->limit |= ((uint32_t)e->granularity & 0xFUL) << 16UL;
	}
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
	{ /* 32-bit protected mode DOS */
		unsigned char *p = (unsigned char*)cpu_gdtlib_ldt_ent.base + (i << 3);
		e->limit = (uint32_t)( *((uint16_t*)(p+0)) );
		e->base = (uint32_t)( *((uint32_t*)(p+2)) & 0xFFFFFFUL );
		e->access = p[5];
		e->granularity = p[6];
		e->base |= ((uint32_t)p[7]) << 24UL;
		e->limit |= ((uint32_t)e->granularity & 0xFUL) << 16UL;
	}
#elif TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	{ /* 16-bit Win16 app */
		unsigned char far *p;

		if (cpu_gdtlib_ldt_sel == 0) {
			uint16_t myds=0;
			__asm mov myds,ds
			cpu_gdtlib_ldt_sel = AllocSelector(myds);
			if (cpu_gdtlib_ldt_sel == 0) return 0;
			SetSelectorLimit(cpu_gdtlib_ldt_sel,(DWORD)cpu_gdtlib_ldt_ent.limit);
			SetSelectorBase(cpu_gdtlib_ldt_sel,(DWORD)cpu_gdtlib_ldt_ent.base);
		}

		if (cpu_gdtlib_ldt_sel != 0) {
			p = MK_FP(cpu_gdtlib_ldt_sel,i << 3);
			e->limit = (uint32_t)( *((uint16_t far*)(p+0)) );
			e->base = (uint32_t)( *((uint32_t far*)(p+2)) & 0xFFFFFFUL );
			e->access = p[5];
			e->granularity = p[6];
			e->base |= ((uint32_t)p[7]) << 24UL;
			e->limit |= ((uint32_t)e->granularity & 0xFUL) << 16UL;
		}
	}
#elif TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
	{ /* 32-bit Windows app */
		unsigned char *p = (unsigned char*)cpu_gdtlib_ldt_ent.base + (i << 3) - cpu_gdtlib_lin_bias;
		e->limit = (uint32_t)( *((uint16_t*)(p+0)) );
		e->base = (uint32_t)( *((uint32_t*)(p+2)) & 0xFFFFFFUL );
		e->access = p[5];
		e->granularity = p[6];
		e->base |= ((uint32_t)p[7]) << 24UL;
		e->limit |= ((uint32_t)e->granularity & 0xFUL) << 16UL;

	}
#endif

#if !defined(TARGET_OS2)
	if (e->granularity & 0x80)
		e->limit = (e->limit << 12UL) | 0xFFFUL;

	return 1;
#endif
}

int cpu_gdtlib_gdt_read_entry(struct cpu_gdtlib_entry *e,unsigned int i) {
	if (i >= cpu_gdtlib_gdt_entries(&cpu_gdtlib_gdtr))
		return 0;
	if (!cpu_gdtlib_can_read)
		return 0;

#if defined(TARGET_OS2)
	return 0; /* NOPE */
#elif TARGET_MSDOS == 16 && !defined(TARGET_WINDOWS)
	{ /* 16-bit real mode DOS */
		unsigned char tmp[8];
	
		if (dpmi_lin2fmemcpy(tmp,cpu_gdtlib_gdtr.base + (i << 3),8) == 0)
			return 0;

		e->limit = (uint32_t)( *((uint16_t*)(tmp+0)) );
		e->base = (uint32_t)( *((uint32_t*)(tmp+2)) & 0xFFFFFFUL );
		e->access = tmp[5];
		e->granularity = tmp[6];
		e->base |= ((uint32_t)tmp[7]) << 24UL;
		e->limit |= ((uint32_t)e->granularity & 0xFUL) << 16UL;
	}
#elif TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
	{ /* 32-bit protected mode DOS */
		unsigned char *p = (unsigned char*)cpu_gdtlib_gdtr.base + (i << 3);
		e->limit = (uint32_t)( *((uint16_t*)(p+0)) );
		e->base = (uint32_t)( *((uint32_t*)(p+2)) & 0xFFFFFFUL );
		e->access = p[5];
		e->granularity = p[6];
		e->base |= ((uint32_t)p[7]) << 24UL;
		e->limit |= ((uint32_t)e->granularity & 0xFUL) << 16UL;
	}
#elif TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	{ /* 16-bit Win16 app */
		unsigned char far *p;

		if (cpu_gdtlib_gdt_sel == 0) {
			uint16_t myds=0;
			__asm mov myds,ds
			cpu_gdtlib_gdt_sel = AllocSelector(myds);
			if (cpu_gdtlib_gdt_sel == 0) return 0;
			SetSelectorLimit(cpu_gdtlib_gdt_sel,(DWORD)cpu_gdtlib_gdtr.limit);
			SetSelectorBase(cpu_gdtlib_gdt_sel,(DWORD)cpu_gdtlib_gdtr.base);
		}

		if (cpu_gdtlib_gdt_sel != 0) {
			p = MK_FP(cpu_gdtlib_gdt_sel,i << 3);
			e->limit = (uint32_t)( *((uint16_t far*)(p+0)) );
			e->base = (uint32_t)( *((uint32_t far*)(p+2)) & 0xFFFFFFUL );
			e->access = p[5];
			e->granularity = p[6];
			e->base |= ((uint32_t)p[7]) << 24UL;
			e->limit |= ((uint32_t)e->granularity & 0xFUL) << 16UL;
		}
	}
#elif TARGET_MSDOS == 32 && defined(TARGET_WINDOWS)
	{ /* 32-bit Windows app */
		unsigned char *p = (unsigned char*)cpu_gdtlib_gdtr.base + (i << 3) - cpu_gdtlib_lin_bias;
		e->limit = (uint32_t)( *((uint16_t*)(p+0)) );
		e->base = (uint32_t)( *((uint32_t*)(p+2)) & 0xFFFFFFUL );
		e->access = p[5];
		e->granularity = p[6];
		e->base |= ((uint32_t)p[7]) << 24UL;
		e->limit |= ((uint32_t)e->granularity & 0xFUL) << 16UL;

	}
#endif

#if !defined(TARGET_OS2)
	if (e->granularity & 0x80)
		e->limit = (e->limit << 12UL) | 0xFFFUL;

	return 1;
#endif
}

int cpu_gdtlib_empty_gdt_entry(struct cpu_gdtlib_entry *e) {
	if (e->limit == 0 && e->base == 0 && e->access == 0 && e->granularity == 0)
		return 1;

	return 0;
}

#define cpu_gdtlib_empty_ldt_entry cpu_gdtlib_empty_gdt_entry

int cpu_gdtlib_entry_is_special(struct cpu_gdtlib_entry *e) {
	return !(e->access & 0x10);
}

int cpu_gdtlib_entry_is_executable(struct cpu_gdtlib_entry *e) {
	return (e->access & 0x08);
}

int cpu_gdtlib_read_current_regs() {
#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
	if (cpu_gdtlib_ldt_sel != 0) {
		FreeSelector(cpu_gdtlib_ldt_sel);
		cpu_gdtlib_ldt_sel=0;
	}
#endif

	/* it matters if we can't read the GDTR */
	if (!cpu_gdtlib_read_gdtr(&cpu_gdtlib_gdtr))
		return 0;

	/* but if we can't read the LDTR, we don't care */
	memset(&cpu_gdtlib_ldt_ent,0,sizeof(cpu_gdtlib_ldt_ent));
	cpu_gdtlib_ldtr = 0;
	cpu_gdtlib_read_ldtr(&cpu_gdtlib_ldtr);

	return 1;
}

int cpu_gdtlib_prepare_to_read_ldt() {
	if (cpu_gdtlib_ldtr == 0) return 0;
	if (cpu_gdtlib_ldt_ent.limit == 0 && cpu_gdtlib_ldt_ent.access == 0) {
		if (cpu_gdtlib_gdt_read_entry(&cpu_gdtlib_ldt_ent,cpu_gdtlib_ldtr>>3) == 0)
			return 0;
		if (cpu_gdtlib_empty_gdt_entry(&cpu_gdtlib_ldt_ent))
			return 0;
	}

	return 1;
}

