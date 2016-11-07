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

#include <string.h>

#include <hw/8250/8250.h>

uint16_t            base_8250_port[MAX_8250_PORTS];
struct info_8250    info_8250_port[MAX_8250_PORTS];
unsigned int        base_8250_ports;
char                inited_8250=0;

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

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */
