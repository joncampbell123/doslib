
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>

#include <hw/cpu/cpu.h>
#include <hw/8254/8254.h>

void t8254_wait(unsigned long ticks) {
	t8254_time_t pr,cr;
	uint16_t dec;

	if (ticks <= 1) return;
	ticks--;

	cr = read_8254(0);
	do {
		/* NTS: remember the 8254 counts downward, not upward */
		pr = cr;
		cr = read_8254(0);
		if (cr > pr) /* when counter reaches zero it resets to the original counter value and begins counting again */
			dec = (pr + (uint16_t)t8254_counter[0] - cr);
		else
			dec = (pr - cr);

		ticks -= dec;
	} while ((signed long)ticks >= 0L);
}

