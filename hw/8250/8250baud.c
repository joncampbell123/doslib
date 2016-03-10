 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8250/8250.h>
#include <hw/dos/doswin.h>

void uart_8250_set_baudrate(struct info_8250 *uart,uint16_t dlab) {
	uint8_t c;

	/* enable access to the divisor */
	c = inp(uart->port+PORT_8250_LCR);
	outp(uart->port+PORT_8250_LCR,c | 0x80);
	/* set rate */
	outp(uart->port+PORT_8250_DIV_LO,dlab);
	outp(uart->port+PORT_8250_DIV_HI,dlab >> 8);
	/* disable access to the divisor */
	outp(uart->port+PORT_8250_LCR,c & 0x7F);
}

