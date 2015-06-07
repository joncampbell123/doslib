/* sse.c
 *
 * Test program: Detecting and using the SSE extensions
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

#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/cpu/cpusse.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

static unsigned char temp[2048];
static unsigned char *tempa;

int main() {
	/* all we need is a 16-byte aligned pointer */
	/* NOTE: This alignment calculation works properly in both
	         16-bit real mode and 32-bit flat mode */
	tempa = temp; tempa += (16UL - (((unsigned long)tempa) & 15UL)) & 15UL;

	cpu_probe();
	probe_dos();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));
	if (cpu_v86_active) {
		printf(" - Your CPU is currently running me in virtual 8086 mode\n");
	}

	if (!cpu_check_sse_is_usable() && cpu_sse_usable_can_turn_on) {
		printf("SSE not available yet, but I can switch it on. Doing so now...\n");
		printf("Reason: %s\n",cpu_sse_unusable_reason);
		printf("Detection method: %s\n",cpu_sse_usable_detection_method);

		if (!cpu_sse_enable()) {
			printf("Nope, cannot enable.\n");
			return 1;
		}
	}

	if (!cpu_check_sse_is_usable()) {
		printf("SSE not available.\n");
		printf("Reason: %s\n",cpu_sse_unusable_reason);
		printf("Detection method: %s\n",cpu_sse_usable_detection_method);
		printf("can turn off: %d\n",cpu_sse_usable_can_turn_off);
		printf("can turn on: %d\n",cpu_sse_usable_can_turn_on);
		return 1;
	}

	printf("SSE available, detection method: %s\n",cpu_sse_usable_detection_method);
	printf("can turn off: %d\n",cpu_sse_usable_can_turn_off);
	printf("can turn on: %d\n",cpu_sse_usable_can_turn_on);

	/* NOTE: Contrary to first impressions, you can enable
	         and execute SSE instructions from 16-bit real
		 mode. Just be careful you don't modify the CR4
		 register or use 32-bit addressing when EMM386.EXE
		 is resident and active. */

	*((uint64_t*)(tempa+0)) = 0x0011223344556677ULL;
	*((uint64_t*)(tempa+8)) = 0x8899AABBCCDDEEFFULL;
	*((uint64_t*)(tempa+16)) = 0x1111111111111111ULL;
	*((uint64_t*)(tempa+24)) = 0x1111111111111111ULL;

#if TARGET_MSDOS == 32
	__asm {
		.686p
		.xmm
		mov	esi,tempa
		movaps	xmm0,[esi]
		andps	xmm0,[esi+16]
		movaps	[esi+32],xmm0
	}
#elif defined(__LARGE__) || defined(__COMPACT__)
	__asm {
		.686p
		.xmm
		push	es
		les	si,tempa
		movaps	xmm0,es:[si]
		andps	xmm0,es:[si+16]
		movaps	es:[si+32],xmm0
		pop	es
	}
#else
	__asm {
		.686p
		.xmm
		mov	si,tempa
		movaps	xmm0,[si]
		andps	xmm0,[si+16]
		movaps	[si+32],xmm0
	}
#endif

	printf("ANDPS: %016llX%016llX & %016llX%016llX =\n       %016llX%016llX\n",
		*((uint64_t*)(tempa+8)),
		*((uint64_t*)(tempa+0)),
		*((uint64_t*)(tempa+24)),
		*((uint64_t*)(tempa+16)),
		*((uint64_t*)(tempa+40)),
		*((uint64_t*)(tempa+32)));

/* second test: the first revision of SSE has heavy emphasis on floating
   point, so let's do some test floating point math */
	((float*)(tempa))[0] = 2.0;
	((float*)(tempa))[1] = 3.0;
	((float*)(tempa))[2] = 5.0;
	((float*)(tempa))[3] = 7.0;
	
	((float*)(tempa))[4] = 3.0;
	((float*)(tempa))[5] = 2.0;
	((float*)(tempa))[6] = 1.5;
	((float*)(tempa))[7] = 1.0;

#if TARGET_MSDOS == 32
	__asm {
		.686p
		.xmm
		mov	esi,tempa
		movaps	xmm0,[esi]
		mulps	xmm0,[esi+16]
		movaps	[esi+32],xmm0
	}
#elif defined(__LARGE__) || defined(__COMPACT__)
	__asm {
		.686p
		.xmm
		push	es
		les	si,tempa
		movaps	xmm0,es:[si]
		mulps	xmm0,es:[si+16]
		movaps	es:[si+32],xmm0
		pop	es
	}
#else
	__asm {
		.686p
		.xmm
		mov	si,tempa
		movaps	xmm0,[si]
		mulps	xmm0,[si+16]
		movaps	[si+32],xmm0
	}
#endif

	printf("MULPS: %.1f * %.1f, %.1f * %.1f, %.1f * %.1f, %.1f * %.1f\n",
		((float*)(tempa))[0],((float*)(tempa))[4],
		((float*)(tempa))[1],((float*)(tempa))[5],
		((float*)(tempa))[2],((float*)(tempa))[6],
		((float*)(tempa))[3],((float*)(tempa))[7]);
	printf("   = %.1f %.1f %.1f %.1f\n",
		((float*)(tempa))[ 8],((float*)(tempa))[ 9],
		((float*)(tempa))[10],((float*)(tempa))[11]);

	return 0;
}

