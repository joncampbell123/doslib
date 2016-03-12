
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>
#include <hw/dos/tgusmega.h>
#include <hw/dos/tgussbos.h>

/* Windows 9x VxD enumeration */
#include <windows/w9xvmm/vxd_enum.h>

/* this is intended for DOS programs that want to make sure the IRQ found works,
 * or for DOS games that read DMA and IRQ configuration from a file or environment
 * that want to make sure it works. */
int sndsb_irq_test(struct sndsb_ctx *cx) {
	uint32_t pirq_counter = cx->irq_counter,cirq_counter;
	unsigned int patience = 1000; // 1 sec
	int result = 0,count = 0;

	if (cx->dsp_playing) return 0; // ARE YOU NUTS??
	if (cx->irq < 0) return 1; // Um, OK. It succeeded.

	if (!sndsb_write_dsp(cx,0xF2/*Generate interrupt*/))
		return 0;

	do {
		if (--patience == 0) break;
		t8254_wait(t8254_us2ticks(1000)); // 1ms

		cirq_counter = cx->irq_counter;
		if (cirq_counter != pirq_counter) {
			if (++count == 3) {
				result = 1;
				break;
			}

			pirq_counter = cirq_counter;
			if (!sndsb_write_dsp(cx,0xF2/*Generate interrupt*/))
				return 0;
		}
	} while (1);

	return result;
}

