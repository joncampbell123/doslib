/* rdtsc.h
 *
 * Library of functions to read the Pentium TSC register.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#ifndef __HW_CPU_RDTSC_H
#define __HW_CPU_RDTSC_H

#include <stdint.h>

typedef uint64_t rdtsc_t;

#if TARGET_MSDOS == 32
static inline uint64_t cpu_rdtsc();
#pragma aux cpu_rdtsc = \
	".586p" \
	"rdtsc" \
	value [edx eax]

static inline void cpu_rdtsc_write(const uint64_t val);
#pragma aux cpu_rdtsc_write = \
	".586p" \
	"mov ecx,0x10" \
	"wrmsr" \
	parm [edx eax] \
	modify [ecx]
#else
/* This is too much code to inline insert everywhere---unless you want extra-large EXEs.
 * It's better to conform to Watcom's register calling convention and make it a function.
 * Note that if you look at the assembly language most of the code is shuffling the values
 * around to convert EDX:EAX to AX:BX:CX:DX and disabling interrupts during the call. */
uint64_t cpu_rdtsc();
void cpu_rdtsc_write(const uint64_t val);
#endif

#endif /* __HW_CPU_RDTSC_H */

