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

void readback_8254(unsigned char what,struct t8254_readback_t *t) {
	t8254_time_t x;
	unsigned int i;

	if (what & 0x30) { /* if anything to readback */
		unsigned int cpu_flags = get_cpu_flags();

		_cli(); /* do not interrupt me */
		for (i=0;i <= 2;i++) {
			if (what & (T8254_READBACK_TIMER_0 << i)) { /* if asked to read back the timer... */
				/* NTS: Intel docs say we're supposed to be able to latch multiple counters in one I/O command then read them back,
				 *      but that doesn't seem to be 100% reliable on actual hardware or in most emulators (DOSBox only allows the
				 *      one read them unlatches them all). So just latch one at a time and read. */
				outp(T8254_CONTROL_PORT,0xC0 | ((what & 0x30) ^ 0x30) | (T8254_READBACK_TIMER_0 << i));	/* read-back command D5=COUNT D4=STATUS D3=COUNTER 2 D2=COUNTER 1 D1=COUNTER 0 */
				if (what & T8254_READBACK_STATUS) {
					t->timer[i].status = inp(T8254_TIMER_PORT(i));
				}

				if (what & T8254_READBACK_COUNT) {
					x  = (t8254_time_t)inp(T8254_TIMER_PORT(i));
					x |= (t8254_time_t)inp(T8254_TIMER_PORT(i)) << 8;
					t->timer[i].count = x;
				}
			}
		}

		/* restore caller's IF bit */
		_sti_if_flags(cpu_flags);
	}
}

