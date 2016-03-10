
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>

#include <hw/cpu/cpu.h>
#include <hw/8254/8254.h>

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

