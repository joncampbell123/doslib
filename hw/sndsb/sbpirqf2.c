
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

static unsigned char sb_irq_test = 0;
static volatile unsigned char sb_irq_happened = 0;
static void (interrupt *sb_irq_old)() = NULL;
static void interrupt sb_probe_irq_handler() {
	sb_irq_happened++;
	p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
}

/* Probe for Sound Blaster IRQ using DSP command 0xF2 (generate interrupt).
 * Works with most Sound Blaster cards. If it doesn't work, then you should
 * use sndsb_probe_irq2() to use method #2. Note that for safety reasons, we
 * only test against IRQ's that the original Sound Blaster would use */
static unsigned char test_irqs[] = {2,3,5,7};
void sndsb_probe_irq(struct sndsb_ctx *cx) {
	unsigned char irqn=0,iter,expect;
	unsigned char masked;

	if (cx->irq >= 0) return;

	while (irqn < sizeof(test_irqs)) {
		sb_irq_test = test_irqs[irqn];

		_cli();
		expect = 0;
		sb_irq_happened = 0;
		sb_irq_old = _dos_getvect(0x08 + sb_irq_test);
		_dos_setvect(0x08 + sb_irq_test,sb_probe_irq_handler);
		masked = p8259_is_masked(sb_irq_test);
		p8259_unmask(sb_irq_test);
		_sti();

		for (iter=0;iter < 10;iter++) {
			/* trigger an IRQ */
			_cli();
			sndsb_interrupt_ack(cx,sndsb_interrupt_reason(cx));
			sndsb_write_dsp(cx,0xF2);
			_sti();

			/* wait 5ms */
			t8254_wait(t8254_us2ticks(5000));

			/* any interrupt? */
			++expect;
			sndsb_interrupt_ack(cx,sndsb_interrupt_reason(cx));
			if (expect != sb_irq_happened) break;
		}

		if (iter == 10)
			cx->irq = sb_irq_test;

		_cli();
		if (masked) p8259_mask(sb_irq_test);
		_dos_setvect(0x08 + sb_irq_test,sb_irq_old);
		irqn++;
		_sti();
	}
}

void sndsb_probe_dma8(struct sndsb_ctx *cx) {
	// TODO
}

void sndsb_probe_dma16(struct sndsb_ctx *cx) {
	// TODO
}

