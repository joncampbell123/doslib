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

/* Probe the 8042 for the AUX port. This verifies that AUX is present, it does NOT
 * necessarily mean that a PS/2 mouse is attached to it. */
int k8042_probe_aux() {
	int r=0;

	k8042_flags &= ~K8042_F_AUX;
	k8042_disable_keyboard();
	k8042_drain_buffer();

	/* write command byte to enable the AUX and KEYB interrupts */
	if (k8042_write_command_byte(0x47)) { /* XLAT=1, enable aux, enable keyboard, SYS=1, enable IRQ12, enable IRQ1 */
		/* Check for 8024 AUX support by stuffing something into it's output register */
		k8042_enable_aux();
		k8042_flags |= K8042_F_AUX;
		if (k8042_write_command(0xD3) && k8042_write_data(0xAA)) {
			if (k8042_read_output_wait() == 0xAA && k8042_output_was_aux()) { /* NTS: We demand that the byte come back WITH the indication that it's from AUX */
				if (k8042_write_command(0xD3) && k8042_write_data(0x55)) {
					if (k8042_read_output_wait() == 0x55 && k8042_output_was_aux()) {
						r = 1; /* Aux works for me! */
					}
				}
			}
		}
	}

	if (r) {
		k8042_enable_aux();
	}
	else {
		k8042_flags &= ~K8042_F_AUX;
	}

	k8042_enable_keyboard();
	return r;
}

/* write a Synaptics touchpad command. caller is expected to read back 3 bytes */
int k8042_write_aux_synaptics_mode(unsigned char c) {
	if (k8042_write_aux(0xE7) && k8042_read_output_wait() == 0xFA && /* set scaling 2:1 */
		k8042_write_aux(0xE8) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux((c>>6)&3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0xE8) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux((c>>4)&3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0xE8) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux((c>>2)&3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0xE8) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux((c>>0)&3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0x14) && k8042_read_output_wait() == 0xFA) { /* 0xE9 status request */
		return 1;
	}

	return 0;
}

/* write a Synaptics touchpad command. caller is expected to read back 3 bytes */
int k8042_write_aux_synaptics_E8(unsigned char c) {
	if (k8042_write_aux(0xE7) && k8042_read_output_wait() == 0xFA && /* set scaling 2:1 */
		k8042_write_aux(0xE8) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux((c>>6)&3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0xE8) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux((c>>4)&3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0xE8) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux((c>>2)&3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0xE8) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux((c>>0)&3) && k8042_read_output_wait() == 0xFA &&
		k8042_write_aux(0xE9) && k8042_read_output_wait() == 0xFA) { /* 0xE9 status request */
		return 1;
	}

	return 0;
}

