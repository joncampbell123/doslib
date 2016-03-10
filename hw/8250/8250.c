/* 8250.c
 *
 * 8250/16450/16550/16750 serial port UART library.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * On PCs that have them, the UART is usually a 8250/16450/16550 or compatible chip,
 * or it is emulated on modern hardware to act like one. At a basic programming level
 * the UART is very simple to talk to.
 *
 * The best way to play with this code, is to obtain a null-modem cable, connect two
 * PCs together, and run this program on either end. On most PC hardware, this code
 * should be able to run at a full baud rate sending and receiving without issues.
 *
 * For newer (post 486) systems with PnP serial ports and PnP aware programs, this
 * library offers a PnP aware additional library that can be linked to. For some
 * late 1990's hardware, the PnP awareness is required to correctly identify the
 * IRQ associated with the device, such as on older Toshiba laptops that emulate
 * a serial port using the IR infared device on the back. */
 
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

const char *type_8250_strs[TYPE_8250_MAX] = {
	"8250",
	"16450",
	"16550",
	"16550A",
	"16750"
};

uint16_t		base_8250_port[MAX_8250_PORTS];
struct info_8250	info_8250_port[MAX_8250_PORTS];
unsigned int		base_8250_ports;
char			inited_8250=0;

int already_got_8250_port(uint16_t port) {
	unsigned int i;

	for (i=0;i < (unsigned int)base_8250_ports;i++) {
		if (base_8250_port[i] == port)
			return 1;
	}

	return 0;
}

int init_8250() {
	if (!inited_8250) {
		memset(base_8250_port,0,sizeof(base_8250_port));
		base_8250_ports = 0;
		bios_8250_ports = 0;
		inited_8250 = 1;
	}

	return 1;
}

void uart_8250_enable_interrupt(struct info_8250 *uart,uint8_t mask) {
	uint8_t c;

	outp(uart->port+PORT_8250_LCR,inp(uart->port+PORT_8250_LCR) & 0x7F);

	/* the mask is written as-is to the IER. we assume the DLAB latch == 0 */
	outp(uart->port+PORT_8250_IER,mask);

	/* on PC platforms, we also have to diddle with the AUX 2 line (FIXME: why?) */
	c = inp(uart->port+PORT_8250_MCR);
	if (mask != 0) c |= 8;	/* AUX 2 output line */
	else           c &= ~8;
	outp(uart->port+PORT_8250_MCR,c);
}

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

uint16_t uart_8250_baud_to_divisor(struct info_8250 *uart,unsigned long rate) {
	if (rate == 0) return 0;
	return (uint16_t)(115200UL / rate);
}

unsigned long uart_8250_divisor_to_baud(struct info_8250 *uart,uint16_t rate) {
	if (rate == 0) return 1;
	return (unsigned long)(115200UL / (unsigned long)rate);
}

void uart_8250_get_config(struct info_8250 *uart,unsigned long *baud,unsigned char *bits,unsigned char *stop_bits,unsigned char *parity) {
	uint16_t dlab;
	uint8_t c = inp(uart->port+PORT_8250_LCR);
	*bits = (c & 3) + 5;
	*stop_bits = (c & 4) ? 2 : 1;
	*parity = (c >> 3) & 7;

	/* then switch on DLAB to get divisor */
	outp(uart->port+PORT_8250_LCR,c | 0x80);

	/* read back divisor */
	dlab = inp(uart->port+PORT_8250_DIV_LO);
	dlab |= (uint16_t)inp(uart->port+PORT_8250_DIV_HI) << 8;

	/* then switch off DLAB */
	outp(uart->port+PORT_8250_LCR,c & 0x7F);

	*baud = uart_8250_divisor_to_baud(uart,dlab);
}

const char *type_8250_parity(unsigned char parity) {
	if (parity & 1) {
		switch (parity >> 1) {
			case 0:	return "odd parity";
			case 1: return "even parity";
			case 2:	return "odd sticky parity";
			case 3: return "even sticky parity";
		};
	}

	return "no parity";
}

void uart_toggle_xmit_ien(struct info_8250 *uart) {
	/* apparently if the XMIT buffer is empty, you can trigger
	 * another TX empty interrupt event by toggling bit 2 in
	 * the IER register. */
	unsigned char c = inp(uart->port+PORT_8250_IER);
	outp(uart->port+PORT_8250_IER,c & ~(1 << 2));
	outp(uart->port+PORT_8250_IER,c);
}

