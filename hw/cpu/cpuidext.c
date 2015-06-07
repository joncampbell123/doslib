
#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
/* Win16: We're probably on a 386, but we could be on a 286 if Windows 3.1 is in standard mode.
 *        If the user manages to run us under Windows 3.0, we could also run in 8086 real mode.
 *        We still do the tests so the Windows API cannot deceive us, but we still need GetWinFlags
 *        to tell between 8086 real mode + virtual8086 mode and protected mode. */
# include <windows.h>
# include <toolhelp.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/cpu/cpuidext.h>

/* DEBUG: Flush out calls that aren't there */
#ifdef TARGET_OS2
# define int86 ___EVIL___
# define ntvdm_RegisterModule ___EVIL___
# define ntvdm_UnregisterModule ___EVIL___
# define _dos_getvect ___EVIL___
# define _dos_setvect ___EVIL___
#endif

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 16
# include <hw/dos/winfcon.h>
#endif

struct cpu_cpuid_ext_info*	cpu_cpuid_ext_info = NULL;

void cpu_extended_cpuid_info_free() {
	if (cpu_cpuid_ext_info) free(cpu_cpuid_ext_info);
	cpu_cpuid_ext_info = NULL;
}

int cpu_extended_cpuid_probe() {
	struct cpuid_result r={0};

	cpu_extended_cpuid_info_free();

	cpu_cpuid_ext_info = malloc(sizeof(struct cpu_cpuid_ext_info));
	if (cpu_cpuid_ext_info == NULL) return 0;
	memset(cpu_cpuid_ext_info,0,sizeof(struct cpu_cpuid_ext_info));

	if (cpu_flags & CPU_FLAG_CPUID) {
		/* probe for extended CPUID */
		cpu_cpuid(0x80000000UL,&r);
		if (r.eax >= 0x80000001UL) {
			cpu_cpuid_ext_info->cpuid_max = r.eax;
			cpu_flags |= CPU_FLAG_CPUID_EXT;

			cpu_cpuid(0x80000001UL,&r);
			cpu_cpuid_ext_info->features.a.raw[0] = r.eax;
			cpu_cpuid_ext_info->features.a.raw[1] = r.ebx;
			cpu_cpuid_ext_info->features.a.raw[2] = r.ecx;
			cpu_cpuid_ext_info->features.a.raw[3] = r.edx;

			if (cpu_cpuid_ext_info->cpuid_max >= 0x80000004UL) {
				cpu_cpuid(0x80000002UL,&r);
				((uint32_t*)cpu_cpuid_ext_info->brand)[0] = r.eax;
				((uint32_t*)cpu_cpuid_ext_info->brand)[1] = r.ebx;
				((uint32_t*)cpu_cpuid_ext_info->brand)[2] = r.ecx;
				((uint32_t*)cpu_cpuid_ext_info->brand)[3] = r.edx;

				cpu_cpuid(0x80000003UL,&r);
				((uint32_t*)cpu_cpuid_ext_info->brand)[4] = r.eax;
				((uint32_t*)cpu_cpuid_ext_info->brand)[5] = r.ebx;
				((uint32_t*)cpu_cpuid_ext_info->brand)[6] = r.ecx;
				((uint32_t*)cpu_cpuid_ext_info->brand)[7] = r.edx;

				cpu_cpuid(0x80000004UL,&r);
				((uint32_t*)cpu_cpuid_ext_info->brand)[8] = r.eax;
				((uint32_t*)cpu_cpuid_ext_info->brand)[9] = r.ebx;
				((uint32_t*)cpu_cpuid_ext_info->brand)[10] = r.ecx;
				((uint32_t*)cpu_cpuid_ext_info->brand)[11] = r.edx;

				cpu_cpuid_ext_info->brand[48] = 0;
			}

			if (cpu_cpuid_ext_info->cpuid_max >= 0x80000005UL) {
				cpu_cpuid(0x80000005UL,&r);
				cpu_cpuid_ext_info->cache_tlb.a.raw[0] = r.eax;
				cpu_cpuid_ext_info->cache_tlb.a.raw[1] = r.ebx;
				cpu_cpuid_ext_info->cache_tlb.a.raw[2] = r.ecx;
				cpu_cpuid_ext_info->cache_tlb.a.raw[3] = r.edx;
			}

			if (cpu_cpuid_ext_info->cpuid_max >= 0x80000006UL) {
				cpu_cpuid(0x80000006UL,&r);
				cpu_cpuid_ext_info->cache_tlb_l2.a.raw[0] = r.eax;
				cpu_cpuid_ext_info->cache_tlb_l2.a.raw[1] = r.ebx;
				cpu_cpuid_ext_info->cache_tlb_l2.a.raw[2] = r.ecx;
				cpu_cpuid_ext_info->cache_tlb_l2.a.raw[3] = r.edx;
			}

			if (cpu_cpuid_ext_info->cpuid_max >= 0x80000007UL) {
				cpu_cpuid(0x80000007UL,&r);
				cpu_cpuid_ext_info->apm.a.raw[0] = r.eax;
			}

			if (cpu_cpuid_ext_info->cpuid_max >= 0x80000008UL) {
				cpu_cpuid(0x80000008UL,&r);
				cpu_cpuid_ext_info->longmode.a.raw[0] = r.eax;
				cpu_cpuid_ext_info->longmode.a.raw[1] = r.ebx;
				cpu_cpuid_ext_info->longmode.a.raw[2] = r.ecx;
				cpu_cpuid_ext_info->longmode.a.raw[3] = r.edx;
			}
		}
	}

	return (cpu_flags & CPU_FLAG_CPUID_EXT)?1:0;
}

