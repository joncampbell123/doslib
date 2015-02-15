
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
#include <hw/flatreal/flatreal.h>

/*=====move to cpu lib=====*/
unsigned char			apic_flags = 0;
uint32_t			apic_base = 0;
const char*			apic_error_str = NULL;

#define APIC_FLAG_PRESENT		(1U << 0U)
#define APIC_FLAG_GLOBAL_ENABLE		(1U << 1U)
#define APIC_FLAG_PROBE_ON_BOOT_CPU	(1U << 2U)
#define APIC_FLAG_CANT_DETECT		(1U << 6U)
#define APIC_FLAG_PROBED		(1U << 7U)

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

void forget_apic() {
	apic_flags = 0; /* to permit re-probing */
}

int probe_apic() {
	if (apic_flags == 0) {
		uint32_t reg;

		/* FIXME: Some say the APIC-like interface appeared in the late 486 era, though as a separate chip,
		 *        unlike the Pentium and later that put the APIC on-chip. How do we detect those? */
		apic_flags = APIC_FLAG_PROBED;

		cpu_probe();
		if (cpu_basic_level < CPU_586) {
			apic_error_str = "APIC support requires at least a 586/Pentium class system";
			return 0;
		}
		detect_windows();
		if (windows_mode >= WINDOWS_STANDARD) {
			apic_error_str = "I will not attempt to play with the APIC from within Windows";
			apic_flags |= APIC_FLAG_CANT_DETECT;
			return 0;
		}
		if (cpu_v86_active) {
			apic_error_str = "I will not attempt to play with the APIC from virtual 8086 mode";
			apic_flags |= APIC_FLAG_CANT_DETECT;
			return 0;
		}
		if (!(cpu_flags & CPU_FLAG_CPUID)) {
			apic_error_str = "APIC detection requires CPUID";
			return 0;
		}
		if (!(cpu_cpuid_features.a.raw[2/*EDX*/] & (1UL << 9UL))) {
			apic_error_str = "CPU does not have on-chip APIC";
			return 0;
		}
		if (!(cpu_cpuid_features.a.raw[2/*EDX*/] & (1UL << 5UL))) {
			apic_error_str = "CPU does have on-chip APIC but no support for RDMSR/WRMSR";
			return 0;
		}

		reg = cpu_rdmsr(0x0000001B); /* hopefully, we do not crash */
		apic_base = (unsigned long)(reg & 0xFFFFF000UL);
		apic_flags |= APIC_FLAG_PRESENT;
		apic_flags |= (reg & (1UL << 11UL)) ? APIC_FLAG_GLOBAL_ENABLE : 0;
		apic_flags |= (reg & (1UL << 8UL)) ? APIC_FLAG_PROBE_ON_BOOT_CPU : 0;
	}

	return (apic_flags & APIC_FLAG_PRESENT)?1:0;
}

/*=========================*/

/* APIC read/write functions.
 * WARNING: You are responsible for making sure APIC was detected before using these functions!
 * Also, Intel warns about some newer registers requiring aligned access to work properly OR ELSE. */
#if TARGET_MSDOS == 32
static inline uint32_t apic_readd(uint32_t offset) {
	return *((volatile uint32_t*)(apic_base+offset));
}
#else
/* 16-bit real mode versions that rely on Flat Real Mode to work */
static inline uint32_t apic_readd(uint32_t offset) {
	if (flatrealmode_test()) flatrealmode_setup(FLATREALMODE_4GB);
	return flatrealmode_readd(apic_base+offset);
}
#endif

int main(int argc,char **argv) {
	if (!probe_apic()) {
		printf("APIC not detected. Reason: %s\n",apic_error_str);
		return 1;
	}

	printf("APIC base address:        0x%08lx\n",(unsigned long)apic_base);
	printf("APIC global enable:       %u\n",(unsigned int)(apic_flags&APIC_FLAG_GLOBAL_ENABLE?1:0));
	printf("Read from bootstrap CPU:  %u\n",(unsigned int)(apic_flags&APIC_FLAG_PROBE_ON_BOOT_CPU?1:1));

#if TARGET_MSDOS == 32
	dos_ltp_probe();
	/* TODO: If LTP probe indicates we shouldn't assume physical<->linear addresses (i.e paging) then bail out */
#else
	probe_dos();
	detect_windows();
	if (!flatrealmode_setup(FLATREALMODE_4GB)) {
		printf("Cannot enter flat real mode\n");
		return 1;
	}
#endif

	{
		unsigned int i;

		/* NTS: For safe access on Intel processors always read on DWORD aligned boundary */
		printf("APIC dump:\n");
		for (i=0;i < 0x400;i += 4) {
			if ((i&0x1F) == 0)
				printf("0x%03x:",i);

			printf("%08lx ",(unsigned long)apic_readd(i));
			if ((i&0x1F) == 0x1C)
				printf("\n");
		}
	}

	return 0;
}

