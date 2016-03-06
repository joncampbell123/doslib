
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>

#include <hw/cpu/cpu.h>
#include <hw/8254/8254.h>

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

