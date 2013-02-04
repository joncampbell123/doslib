/* mmx.c
 *
 * Test program: Proof of concept detection and use of the MMX extensions
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

static unsigned char temp[2048];
static unsigned char *tempa;

int main() {
	/* all we need is a 8-byte aligned pointer */
	/* NOTE: This alignment calculation works properly in both
	         16-bit real mode and 32-bit flat mode */
	tempa = temp; tempa += (8UL - (((unsigned long)tempa) & 7UL)) & 7UL;

	cpu_probe();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));
	if (cpu_v86_active) {
		printf(" - Your CPU is currently running me in virtual 8086 mode\n");
	}
	if (!(cpu_flags & CPU_FLAG_CPUID)) {
		printf(" - No CPUID present. How can you have MMX extensions without CPUID?\n");
		return 1;
	}
	if (!(cpu_cpuid_features.a.raw[2] & (1UL << 23UL))) {
		printf(" - Your CPU does not support MMX extensions\n");
		return 1;
	}

	/* NOTE: You can execute MMX instructions in real mode.
	         You can also use them within the v8086 environment
		 provided by EMM386.EXE. Just don't use 32-bit addressing
		 when in v86 mode and you won't crash */

	*((uint64_t*)(tempa+0)) = 0x0011223344556677ULL;
	*((uint64_t*)(tempa+8)) = 0x1111111111111111ULL;

#if TARGET_MSDOS == 32
	__asm {
		.686p
		.mmx
		mov	esi,tempa
		movq	mm0,[esi+8]
		paddb	mm0,[esi+0]
		movq	[esi+16],mm0
		psubusb	mm0,[esi+8]
		movq	[esi+24],mm0
		psubusb	mm0,[esi+8]
		movq	[esi+32],mm0
	}
#elif defined(__LARGE__) || defined(__COMPACT__)
	__asm {
		.686p
		.mmx
		push	ds
		lds	si,tempa
		movq	mm0,[si+8]
		paddb	mm0,[si+0]
		movq	[si+16],mm0
		psubusb	mm0,[si+8]
		movq	[si+24],mm0
		psubusb	mm0,[si+8]
		movq	[si+32],mm0
		pop	ds
	}
#else
	__asm {
		.686p
		.mmx
		mov	si,tempa
		movq	mm0,[si+8]
		paddb	mm0,[si+0]
		movq	[si+16],mm0
		psubusb	mm0,[si+8]
		movq	[si+24],mm0
		psubusb	mm0,[si+8]
		movq	[si+32],mm0
	}
#endif
	__asm {
		.686p
		.mmx
		emms
	}

	printf("PADDB: %016llX + %016llX = %016llX\n",
		*((uint64_t*)(tempa+0)),
		*((uint64_t*)(tempa+8)),
		*((uint64_t*)(tempa+16)));
	printf("PSUBUSB: %016llX - %016llX = %016llX - %016llX = %016llX\n",
		*((uint64_t*)(tempa+16)),
		*((uint64_t*)(tempa+8)),
		*((uint64_t*)(tempa+24)),
		*((uint64_t*)(tempa+8)),
		*((uint64_t*)(tempa+32)));

	return 0;
}

