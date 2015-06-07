/* 8042.c
 *
 * Intel 8042 keyboard controller library.
 * (C) 2008-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * On "IBM compatible" hardware the keyboard controller is responsible for
 * receiving scan codes from the keyboard. On newer (post 1990) systems,
 * the controller also handles PS/2 mouse input and communication (AUX).
 *
 * Note that on recent hardware (1999 or later) it's entirely possible the
 * BIOS is using System Management Mode and/or a USB mouse & keyboard to
 * fake 8042 I/O ports and communications for backwards compatibility. */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>

#include <hw/cpu/cpu.h>
#include <hw/8042/8042.h>

unsigned char		k8042_flags = 0;
unsigned char		k8042_last_status = 0;

/* drain the 8042 buffer entirely */
void k8042_drain_buffer() {
	unsigned char c;

	do {
		inp(K8042_DATA);
		c = inp(K8042_STATUS);
	} while (c&3);
	k8042_last_status = c;
}

/* probe for the existence of the 8042 controller I/O ports.
 * obviously as a DOS program the ports are there. but someday...
 * it's possible the newer BIOSes will stop emulating it sooner or later. */
int k8042_probe() {
	if (inp(K8042_STATUS) == 0xFF)
		return 0;

	return 1;
}

