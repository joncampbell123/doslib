 
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

void uart_toggle_xmit_ien(struct info_8250 *uart) {
	/* apparently if the XMIT buffer is empty, you can trigger
	 * another TX empty interrupt event by toggling bit 2 in
	 * the IER register. */
	unsigned char c = inp(uart->port+PORT_8250_IER);
	outp(uart->port+PORT_8250_IER,c & ~(1 << 2));
	outp(uart->port+PORT_8250_IER,c);
}

