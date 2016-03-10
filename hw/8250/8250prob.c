 
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

/* this is used to probe for ports in standard locations, when we really don't know if it's there */
int probe_8250(uint16_t port) {
	unsigned char ier,dlab1,dlab2,c,fcr;
	struct info_8250 *inf;

	if (already_got_8250_port(port))
		return 0;
	if (base_8250_full())
		return 0;

	inf = &info_8250_port[base_8250_ports];
	inf->type = TYPE_8250_IS_8250;
	inf->port = port;
	inf->irq = -1;
	if (windows_mode == WINDOWS_NONE || windows_mode == WINDOWS_REAL) {
		/* in real-mode DOS we can play with the UART to our heart's content. so we play with the
		 * DLAB select and interrupt enable registers to detect the UART in a manner non-destructive
		 * to the hardware state. */

		/* there's no way to autodetect the COM port's IRQ, we have to guess */
		if (port == 0x3F8 || port == 0x3E8)
			inf->irq = 4;
		else if (port == 0x2F8 || port == 0x2E8)
			inf->irq = 3;

		/* switch registers 0+1 back to RX/TX and interrupt enable, and then test the Interrupt Enable register */
		_cli();
		outp(port+3,inp(port+3) & 0x7F);
		if (inp(port+3) == 0xFF) { _sti(); return 0; }
		ier = inp(port+1);
		outp(port+1,0);
		if (inp(port+1) == 0xFF) { _sti(); return 0; }
		outp(port+1,ier);
		if ((inp(port+1) & 0xF) != (ier & 0xF)) { _sti(); return 0; }
		/* then switch 0+1 to DLAB (divisor registers) and see if values differ from what we read the first time */
		outp(port+3,inp(port+3) | 0x80);
		dlab1 = inp(port+0);
		dlab2 = inp(port+1);
		outp(port+0,ier ^ 0xAA);
		outp(port+1,ier ^ 0x55);
		if (inp(port+1) == ier || inp(port+0) != (ier ^ 0xAA) || inp(port+1) != (ier ^ 0x55)) {
			outp(port+0,dlab1);
			outp(port+1,dlab2);
			outp(port+3,inp(port+3) & 0x7F);
			_sti();
			return 0;
		}
		outp(port+0,dlab1);
		outp(port+1,dlab2);
		outp(port+3,inp(port+3) & 0x7F);

		/* now figure out what type */
		fcr = inp(port+2);
		outp(port+2,0xE7);	/* write FCR */
		c = inp(port+2);	/* read IIR */
		if (c & 0x40) { /* if FIFO */
			if (c & 0x80) {
				if (c & 0x20) inf->type = TYPE_8250_IS_16750;
				else inf->type = TYPE_8250_IS_16550A;
			}
			else {
				inf->type = TYPE_8250_IS_16550;
			}
		}
		else {
			unsigned char oscratch = inp(port+7);

			/* no FIFO. try the scratch register */
			outp(port+7,0x55);
			if (inp(port+7) == 0x55) {
				outp(port+7,0xAA);
				if (inp(port+7) == 0xAA) {
					outp(port+7,0x00);
					if (inp(port+7) == 0x00) {
						inf->type = TYPE_8250_IS_16450;
					}
				}
			}

			outp(port+7,oscratch);
		}

		outp(port+2,fcr);
		_sti();
	}
	else {
		unsigned int i;

		/* if we were to actually do our self-test in a VM, Windows would mistakingly assume we
		 * were trying to use it and would allocate the port. we're just enumerating at this point.
		 * play it safe and assume it works if the port is listed as one of the BIOS ports.
		 * we also don't use interrupts. */
		for (i=0;i < bios_8250_ports && port != get_8250_bios_port(i);) i++;
		if (i >= bios_8250_ports) return 0;
	}

	base_8250_port[base_8250_ports++] = port;
	return 1;
}

