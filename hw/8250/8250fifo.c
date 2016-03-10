 
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

void uart_8250_disable_FIFO(struct info_8250 *uart) {
	if (uart->type <= TYPE_8250_IS_16550) return;
	outp(uart->port+PORT_8250_FCR,7); /* enable and flush */
	outp(uart->port+PORT_8250_FCR,0); /* then disable */
}

void uart_8250_set_FIFO(struct info_8250 *uart,uint8_t flags) {
	if (uart->type <= TYPE_8250_IS_16550) return;
	outp(uart->port+PORT_8250_FCR,flags | 7);
	outp(uart->port+PORT_8250_FCR,flags);
}

