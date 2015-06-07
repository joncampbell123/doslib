
#ifndef __HW_CPU_CPUIDEXT_H
#define __HW_CPU_CPUIDEXT_H

#include <stdint.h>
#include <dos.h>

#if !defined(FAR)
# if defined(TARGET_WINDOWS)
#  include <windows.h>
# else
#  if TARGET_MSDOS == 32
#   define FAR
#  else
#   define FAR far
#  endif
# endif
#endif

/* has L1 TLB/CACHE info */
#define cpu_cpuid_ext_info_has_cache_tlb_l1 (cpu_cpuid_ext_info && cpu_cpuid_ext_info->cpuid_max >= 0x80000005UL)

/* has L2 TLB/CACHE info */
#define cpu_cpuid_ext_info_has_cache_tlb_l2 (cpu_cpuid_ext_info && cpu_cpuid_ext_info->cpuid_max >= 0x80000006UL)

/* has APM info */
#define cpu_cpuid_ext_info_has_apm (cpu_cpuid_ext_info && cpu_cpuid_ext_info->cpuid_max >= 0x80000007UL)

/* has Long mode info */
#define cpu_cpuid_ext_info_has_longmode (cpu_cpuid_ext_info && cpu_cpuid_ext_info->cpuid_max >= 0x80000008UL)

struct cpu_cpuid_ext_info {
	char					brand[49];
	uint32_t				cpuid_max;
	struct cpu_cpuid_ext_features		features;
	struct cpu_cpuid_ext_cache_tlb		cache_tlb;
	struct cpu_cpuid_ext_cache_tlb_l2	cache_tlb_l2;
	struct cpu_cpuid_ext_apm		apm;
	struct cpu_cpuid_ext_longmode		longmode;
};

extern struct cpu_cpuid_ext_info*	cpu_cpuid_ext_info;

int cpu_extended_cpuid_probe();
void cpu_extended_cpuid_info_free();

#endif /* __HW_CPU_CPUIDEXT_H */

