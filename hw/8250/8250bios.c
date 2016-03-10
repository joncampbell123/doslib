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

unsigned char		bios_8250_ports = 0;

int probe_8250_bios_ports() {
	uint16_t eqw;

	/* read the BIOS equipment word[11-9]. how many serial ports? */
#if TARGET_MSDOS == 32
	eqw = *((uint16_t*)(0x400 + 0x10));
#else
	eqw = *((uint16_t far*)MK_FP(0x40,0x10));
#endif
	bios_8250_ports = (eqw >> 9) & 7;
	if (bios_8250_ports > 4) bios_8250_ports = 4;
	return (bios_8250_ports > 0) ? 1 : 0;
}

uint16_t get_8250_bios_port(unsigned int index) {
	if (index >= (unsigned int)bios_8250_ports)
		return 0;

#if TARGET_MSDOS == 32
	return *((uint16_t*)(0x400 + (index*2)));
#else
	return *((uint16_t far*)MK_FP(0x40,index*2));
#endif
}

