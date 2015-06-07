
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
#include <hw/dos/doswin.h>
#include <hw/flatreal/flatreal.h>

#include <hw/cpu/apiclib.h>

unsigned char			apic_flags = 0;
uint32_t			apic_base = 0;
const char*			apic_error_str = NULL;

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

