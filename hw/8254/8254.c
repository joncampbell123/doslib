/* 8254.c
 *
 * 8254 programmable interrupt timer control library.
 * (C) 2008-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * The 8254 Programmable Interrupt Timer is used on the PC platform for
 * several purposes. 3 Timers are provided, which are used as:
 *
 *  Timer 0 - System timer. This is tied to IRQ 0 and under normal operation
 *            provides the usual IRQ 0 18.2 ticks per second. DOS programs that
 *            need higher resolution reprogram this timer to get it.
 *
 *  Timer 1 - Misc, varies. On older PC hardware this was tied to DRAM refresh.
 *            Modern hardware cycles it for whatever purpose. Don't assume it's function.
 *
 *  Timer 2 - PC Speaker. This timer is configured to run as a square wave and it's
 *            output is gated through the 8042 before going directly to a speaker in the
 *            PC's computer case. When used, this timer allows your program to generate
 *            audible beeps.
 *
 * On modern hardware this chip is either integrated into the core motherboard chipset or
 * emulated for backwards compatibility.
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>

#include <hw/cpu/cpu.h>
#include <hw/8254/8254.h>

uint32_t t8254_counter[3] = {0x10000,0x10000,0x10000};
int8_t probed_8254_result = -1;

int probe_8254() {
	if (probed_8254_result >= 0)
		return (int)probed_8254_result;

	/* NTS: Reading port 0x43 does nothing. Intel's datasheet even mentions this in one of the tables.
	 *      Actual hardware experience tells me some motherboards DO return something but the correct
	 *      response (as seen in emulators in DOSBox) is to ignore, returning 0xFF */
	{
		/* read timer 0 and see if it comes back non-0xFF */
		/* NTS: We MUST use the read_8254 function to read it in order. The previous
		 *      version of this code read it byte-wise. For some reason it seems,
		 *      some emulators including DOSBox don't take that well and they fail
		 *      to reset the MSB/LSB flip-flop. When that happens our timer counter
		 *      readings come out byte-swapped and we cannot provide proper timing
		 *      and sleep routines. Symptoms: A DOS program using our timing code
		 *      would have proper timing only on every other run. */
		unsigned int patience = 128,cc;
		unsigned short c;
		do {
			c = read_8254(0);
			if (c == 0xFFFF) c = read_8254(0);
			if (c != 0xFFFF) break;
			for (cc=0;cc != 0xFFFFU;) cc++; /* delay */
		} while (patience-- > 0);

		if (c == 0xFF)
			return (probed_8254_result=0);
	}

	return (probed_8254_result=1);
}

unsigned long t8254_us2ticks(unsigned long a) {
	/* FIXME: can you write a version that doesn't require 64-bit integers? */
	uint64_t b;
	if (a == 0) return 0;
	b = (uint64_t)a * (uint64_t)T8254_REF_CLOCK_HZ;
	return (unsigned long)(b / 1000000ULL);
}

unsigned long t8254_us2ticksr(unsigned long a,unsigned long *rem) {
	/* FIXME: can you write a version that doesn't require 64-bit integers? */
	uint64_t b;
	if (a == 0) return 0;
	b = (uint64_t)a * (uint64_t)T8254_REF_CLOCK_HZ;
	*rem = (unsigned long)(b % 1000000ULL);
	return (unsigned long)(b / 1000000ULL);
}

void t8254_wait(unsigned long ticks) {
	uint16_t dec;
	t8254_time_t pr,cr;
	if (ticks <= 1) return;
	ticks--;
	cr = read_8254(0);
	do {
		pr = cr;
		cr = read_8254(0);
		if (cr > pr)
			dec = (pr + (uint16_t)t8254_counter[0] - cr);
		else
			dec = (pr - cr);

		ticks -= dec;
	} while ((signed long)ticks >= 0L);
}

