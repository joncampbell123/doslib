
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <setjmp.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>

/*=====move to cpu lib=====*/

#if TARGET_MSDOS == 32
static inline uint64_t cpu_rdmsr(const uint32_t idx);
#pragma aux cpu_rdmsr = \
	".586p" \
	"rdmsr" \
	parm [ecx] \
	value [edx eax]

static inline void cpu_wrmsr(const uint32_t idx,const uint64_t val);
#pragma aux cpu_wrmsr = \
	".586p" \
	"wrmsr" \
	parm [ecx] [edx eax]
#else
/* This is too much code to inline insert everywhere---unless you want extra-large EXEs.
 * It's better to conform to Watcom's register calling convention and make it a function.
 * Note that if you look at the assembly language most of the code is shuffling the values
 * around to convert EDX:EAX to AX:BX:CX:DX and disabling interrupts during the call. */
/* see CPUASM.ASM */
uint64_t cpu_rdmsr(const uint32_t idx);
void cpu_wrmsr(const uint32_t idx,const uint64_t val);
#endif

/*=========================*/

int main(int argc,char **argv) {
	uint32_t reg;

	cpu_probe();
	probe_dos();
	detect_windows();
	probe_dpmi();
	probe_vcpi();

	/* FIXME: Some say the APIC-like interface appeared in the late 486 era, though as a separate chip,
	 *        unlike the Pentium and later that put the APIC on-chip. How do we detect those? */
	if (cpu_basic_level < CPU_586) {
		printf("APIC support requires at least a 586/Pentium class system\n");
		return 1;
	}
	if (windows_mode >= WINDOWS_STANDARD) {
		printf("I will not attempt to play with the APIC from within Windows\n");
		return 1;
	}
	if (cpu_v86_active) {
		printf("I will not attempt to play with the APIC from virtual 8086 mode\n");
		return 1;
	}
	if (!(cpu_flags & CPU_FLAG_CPUID)) {
		printf("APIC detection requires CPUID\n");
		return 1;
	}
	if (!(cpu_cpuid_features.a.raw[2/*EDX*/] & (1UL << 9UL))) {
		printf("CPU does not have on-chip APIC\n");
		return 1;
	}
	if (!(cpu_cpuid_features.a.raw[2/*EDX*/] & (1UL << 5UL))) {
		printf("CPU does have on-chip APIC but no support for RDMSR/WRMSR\n");
		return 1;
	}
	reg = cpu_rdmsr(0x0000001B); /* hopefully, we do not crash */
	printf("Raw contents of MSR 0x1B: 0x%08lx\n",(unsigned long)reg);
	printf("APIC base address:        0x%08lx\n",(unsigned long)reg & 0xFFFFF000UL);	/* reg[31:12] = APIC base address */
	printf("APIC global enable:       %u\n",(unsigned int)((reg >> 11UL) & 1UL));
	printf("Read from bootstrap CPU:  %u\n",(unsigned int)((reg >> 8UL) & 1UL));

	return 0;
}

