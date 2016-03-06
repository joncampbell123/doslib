
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8259/8259.h>

void p8259_ICW(unsigned char a,unsigned char b,unsigned char c,unsigned char d) {
	outp(p8259_irq_to_base_port(c,0),a | 0x10);	/* D4 == 1 */
	outp(p8259_irq_to_base_port(c,1),b);
	outp(p8259_irq_to_base_port(c,1),c);
	if (a & 1) outp(p8259_irq_to_base_port(c,1),d);
}

