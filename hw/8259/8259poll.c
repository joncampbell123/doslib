
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8259/8259.h>

/* NTS: bit 7 is set if there was an interrupt */
/* WARNING: This code crashes DOSBox 0.74 with "PIC poll not handled" error message */
unsigned char p8259_poll(unsigned char c) {
	/* issue poll command to read and ack an interrupt */
	p8259_OCW3(c,0x0C);	/* OCW3 = POLL=1 SMM=0 RR=0 */
	return inp(p8259_irq_to_base_port(c,0));
}

