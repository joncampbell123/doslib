
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>

#include <hw/cpu/cpu.h>
#include <hw/8254/8254.h>

unsigned long t8254_us2ticks(unsigned long a) {
	/* FIXME: can you write a version that doesn't require 64-bit integers? */
	uint64_t b;
	if (a == 0) return 0;
	b = (uint64_t)a * (uint64_t)T8254_REF_CLOCK_HZ;
	return (unsigned long)(b / 1000000ULL);
}

