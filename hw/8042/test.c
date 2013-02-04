/* test.c
 *
 * Intel 8042 keyboard controller library test program.
 * (C) 2008-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * This program allows you (the curious programmer) to play with your
 * keyboard controller and experiment to see what it is capable of,
 * and to get a better idea of what is involved in writing software
 * to talk to it.
 *
 * WARNING: This code can put your keyboard controller in a state where
 *          the keyboard stops responding temporarily until you reboot,
 *          or is in the wrong scan code set. As far as I know, this code
 *          cannot damage your keyboard or keyboard controller in any
 *          way. In the unlikely chance it does, this code carries no
 *          warranty express or implied. */

/* NTS: This should work well on all systems I have, except for one 386 motherboard that I know has
 *      an annoying quirk with it's controller: when the controller sets the "OUTPUT FULL" bit
 *      it leaves it set for about 30-40ms after we read it. If we blindly rely on reading output
 *      whenever that's set we end up with many duplicate characters. So while this test code is
 *      probably the simplest way to do it, the best way to do it is to hook IRQ 1 and read data
 *      THERE, not poll. */
/* NTS: I can also attest from programming experience that most PC/AT and modern PC hardware is
 *      FAR from ideal when it comes to implementing or emulating the 8042 keyboard controller.
 *      Many implementations have bugs or corner cases that might cause problems with this
 *      program. The most common one it seems is that certain commands like Set/Reset LEDs are
 *      prone to leave the keyboard in a disabled state if the user is mashing keys while triggering
 *      the command. */
/* NTS: Emulator testing: Bochs and VirtualBox are fine for testing this code, but DO NOT USE DOSBox.
 *      DOSBox 0.74's emulation of the 8042 is pitifully minimalist and a lot of what we play with
 *      here is not implemented at all. At best you can verify that, yes, DOSBox 0.74 is sending us
 *      scan codes. That's it. */
/* NTS: Don't forget what we play with here is a limited subset of what most keyboards and keyboard
 *      controllers are capable of. However such commands could easily put the keyboard in a state
 *      that makes our UI unusable, especially the ones that change scan code conversion or switch
 *      scan code modes. To fully play around with this system and understand what you are doing
 *      you need to download or obtain a 8042 and keyboard programming reference. */
/* NTS: Apparently on newer hardware, when faking 8042 via "Legacy USB support" in the BIOS, there
 *      is a very real danger that issuing "self-test" command could completely sever the connection
 *      within the BIOS between the fakery and the USB keyboard, making your USB keyboard unusable!
 *      The USB fakery is especially fragile on some nVidia nforce + AMD systems I have that were
 *      bought around 2003-ish, sometimes for no reason at all keyboard input can just go dead!
 *         Also: Set LEDs command doesn't work
 *               Diagnostic echo doesn't work
 *               Keyboard ID doesn't work
 *               Keyboard reset doesn't work */

/* Other known bugs:
 *
 *      PS/2 raw input on Sun/Oracle VirtualBox on 32-bit builds running under DOS4G/W:
 *       Symptoms: For unknown reasons, raw input will only show the first byte of the PS/2 packet.
 *           The main loop will show all three (as "discarding" bytes). This probably has something
 *           to do with DOS4GW.EXE's shitty handling of the second interrupt controller, the
 *           same problem that plagues our Sound Blaster test program. The interrupt indicators
 *           at the top of the screen will show intermittent IRQ activity for IRQ 12 when PS/2 raw
 *           input is active.
 *
 *       Solution: Replace the DOS4G/W extender with DOS32a, or use another emulator that doesn't
 *                 have this strange conflict with DOS4G/W
 */
/* FIXME: This code does something to the BIOS's shift status where pusing '8' to toggle the LEDs
 *        only works once, the user has to type SHIFT+8 to toggle again. Why is that? */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <i86.h>
#include <dos.h>

#include <hw/8042/8042.h>

unsigned char chain_irq1 = 1;
void (__interrupt __far *prev_irq1)() = NULL;
static void __interrupt __far irq1() {
#if TARGET_MSDOS == 32
	(*((unsigned char*)(0xB8000)))++;	/* keyboard shift flags 1 */
	 *((unsigned char*)(0xB8001)) = 0x1E;
#else
	(*((unsigned char far*)MK_FP(0xB800,0)))++;	/* keyboard shift flags 1 */
	 *((unsigned char far*)MK_FP(0xB800,1)) = 0x1E;
#endif

	if (chain_irq1) {
		prev_irq1();
	}
	else {
		outp(0x20,0x20);
	}
}

unsigned char chain_irq12 = 1;
void (__interrupt __far *prev_irq12)() = NULL;
static void __interrupt __far irq12() {
#if TARGET_MSDOS == 32
	(*((unsigned char*)(0xB8002)))++;	/* keyboard shift flags 1 */
	 *((unsigned char*)(0xB8003)) = 0x1E;
#else
	(*((unsigned char far*)MK_FP(0xB800,2)))++;	/* keyboard shift flags 1 */
	 *((unsigned char far*)MK_FP(0xB800,3)) = 0x1E;
#endif

	if (chain_irq12) {
		prev_irq12();
	}
	else {
		outp(0xA0,0x20);
		outp(0x20,0x20);
	}
}

/* the problem with our raw input intervention is that the user might have had modifier
 * keys down (shift, alt, etc.) and if we don't clear those states, the user will find
 * themselves accidentally typing in all caps or worse */
void force_clear_bios_shift_states() {
#if TARGET_MSDOS == 32
	*((unsigned char*)(0x400+0x17)) = 0x00;	/* keyboard shift flags 1 */
	*((unsigned char*)(0x400+0x18)) = 0x00;	/* keyboard shift flags 2 */
#else
	*((unsigned char far*)MK_FP(0x40,0x17)) = 0x00;	/* keyboard shift flags 1 */
	*((unsigned char far*)MK_FP(0x40,0x18)) = 0x00;	/* keyboard shift flags 2 */
#endif
}

/* FIXME: This code has problems with sometimes leaving the keyboard unresponsive at random.
 *        Otherwise, a lot of fun. Perhaps a future upgrade would be to have this inner loop
 *        interpret scan codes directly, along with a subroutine to take in bytes and return
 *        integer values representing either ASCII or extended info */
void ps2_mouse_main() {
	int c;

	_cli();
	if (!k8042_probe_aux() && !k8042_probe_aux()) {
		printf("PS/2 AUX port does not appear to be present\n");
		_sti();
		return;
	}

	/* mouse interface test */
	k8042_disable_keyboard();
	k8042_disable_aux();
	k8042_drain_buffer();
	if (k8042_write_command(0xA9)) {
		c = k8042_read_output_wait();
		printf("Mouse interface test result: ");
		if (c >= 0) printf("0x%02x\n",c);
		else printf("??\n");
	}
	else {
		c = 0;
		printf("Mouse interface test failed, cannot write command\n");
	}
	k8042_enable_aux();
	k8042_enable_keyboard();
	_sti();

	if (c != 0x00)
		return;

	/* Okay, reset mouse */
	printf("Resetting PS/2 mouse (if possible)... "); fflush(stdout);
	_cli();
	chain_irq1 = chain_irq12 = 0;
	k8042_disable_keyboard();
	k8042_drain_buffer();
	if (k8042_write_aux(0xFF)) { /* "returns BAT value (0xAA) and device ID" my ass. It 0xFA acks like everybody else */
		c = k8042_read_output_wait();
		if (!k8042_output_was_aux()) printf("Non-AUX response 0x%02x\n",c);
		else if (c != 0xFA) printf("Not ACK 0x%02x\n",c);
		else printf("OK\n");
	}
	else {
		printf("..Cannot write AUX\n");
	}

	printf("Setting defaults... "); fflush(stdout);
	k8042_drain_buffer();
	if (k8042_write_aux(0xF6)) {
		c = k8042_read_output_wait();
		/* an ancient PS/2 mouse I have response with 0xFE */
		if (c == 0xFE) {
			c = k8042_read_output_wait();
			if (c < 0) c = k8042_read_output_wait();
			if (c < 0) c = k8042_read_output_wait();
		}
		if (!k8042_output_was_aux()) printf("Non-AUX response 0x%02x\n",c);
		else if (c != 0xFA) printf("Not ACK 0x%02x\n",c);
		else printf("OK\n");
	}
	else {
		printf("..Cannot write AUX\n");
	}

	printf("Setting rate to 60... "); fflush(stdout);
	k8042_drain_buffer();
	if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(60) && k8042_read_output_wait() == 0xFA) {
		printf("OK\n");
	}
	else {
		printf("..Cannot write AUX\n");
	}

	printf("Reading device ID... "); fflush(stdout);
	k8042_drain_buffer();
	if (k8042_write_aux(0xF2) && k8042_read_output_wait() == 0xFA) {
		/* FIXME: extra long delay needed for my Toshiba laptop */
		c = k8042_read_output_wait();
		if (c < 0) c = k8042_read_output_wait();
		if (c < 0) c = k8042_read_output_wait();
		printf("OK: %02x\n",c);
	}
	else {
		printf("..Cannot write AUX\n");
	}

	printf("Turning on streaming mode... "); fflush(stdout);
	k8042_drain_buffer();
	if (k8042_write_aux(0xEA) && k8042_read_output_wait() == 0xFA) {
		printf("OK\n");
	}
	else {
		printf("..Cannot write AUX\n");
	}

	printf("Enabling reporting mode... "); fflush(stdout);
	k8042_drain_buffer();
	if (k8042_write_aux(0xF4) && k8042_read_output_wait() == 0xFA) {
		printf("OK\n");
	}
	else {
		printf("..Cannot write AUX\n");
	}

	_sti();

	if (!(k8042_write_data(0xF4) && k8042_read_output_wait() == 0xFA))
		printf("Cannot enable keyboard\n");

	do {
		k8042_enable_keyboard();
		printf("PS/2 mouse:   1. raw input    2. Reinit AUX    3. Status    4. Init Scrollwheel\n");
		printf("              5. Init scroll + 4th/5th btns    6. DeviceID  7. Synaptics ID\n");
		printf("              8. Synaptics touchpad modes      9. Synaptics capabilities\n");
		printf("              A. Synaptics model ID            B. Synaptics serial\n");
		printf("              C. Synaptics resolutions         D. Synaptics Abs. mode\n");
		printf("              E. Synaptics relative mode\n");

		/* read the keyboard directly. we must do this to ensure that any AUX bytes coming in
		 * are discarded and removed from the H/W buffers. if we don't, then, depending on the
		 * 8042 controller we're talking to, keyboard I/O could easily be locked out because
		 * of the single path that both take through the 8042 controller. */
		while (1) {
			c = k8042_read_output_wait();
			if (c < 0) continue;

			if (k8042_output_was_aux())
				fprintf(stderr,"Discarding AUX input %02X\n",c);
			else if (c & 0x80)
				continue; /* ignore break codes */
			else {
				switch (c) {
					case 1:	c = 27; break;
					case 2: c = '1'; break;
					case 3: c = '2'; break;
					case 4: c = '3'; break;
					case 5: c = '4'; break;
					case 6: c = '5'; break;
					case 7: c = '6'; break;
					case 8: c = '7'; break;
					case 9: c = '8'; break;
					case 10: c = '9'; break;
					case 0x1E: c = 'A'; break;
					case 0x30: c = 'B'; break;
					case 0x2E: c = 'C'; break;
					case 0x20: c = 'D'; break;
					case 0x12: c = 'E'; break;
					case 0x39: c = ' '; break;
					default: c = -1; break;
				}

				if (c >= 0) break;
			}
		}

		k8042_disable_keyboard();
		if (c == 27) break;
		else if (c == 'E') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			/* E8 00 E8 00 E8 00 E8 00 E9 */
			if (k8042_write_aux_synaptics_mode(0x40)) {
				printf("OK\n");
			}
		}
		else if (c == 'D') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			/* E8 00 E8 00 E8 00 E8 00 E9 */
			if (k8042_write_aux_synaptics_mode(0xC1)) {
				printf("OK\n");
			}
		}
		else if (c == 'C') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			/* E8 00 E8 00 E8 00 E8 00 E9 */
			if (k8042_write_aux_synaptics_E8(0x08)) {
				int b1,b2,b3;
				b1 = k8042_read_output_wait();
				b2 = k8042_read_output_wait();
				b3 = k8042_read_output_wait();
				printf("Status: 0x%02X 0x%02x 0x%02x\n",b1,b2,b3);
			}
		}
		else if (c == 'B') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			/* E8 00 E8 00 E8 00 E8 00 E9 */
			if (k8042_write_aux_synaptics_E8(0x06)) {
				int b1,b2,b3;
				b1 = k8042_read_output_wait();
				b2 = k8042_read_output_wait();
				b3 = k8042_read_output_wait();
				printf("pre: 0x%02X 0x%02x 0x%02x\n",b1,b2,b3);
			}
			if (k8042_write_aux_synaptics_E8(0x07)) {
				int b1,b2,b3;
				b1 = k8042_read_output_wait();
				b2 = k8042_read_output_wait();
				b3 = k8042_read_output_wait();
				printf("suf: 0x%02X 0x%02x 0x%02x\n",b1,b2,b3);
			}
		}
		else if (c == 'A') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			/* E8 00 E8 00 E8 00 E8 00 E9 */
			if (k8042_write_aux_synaptics_E8(0x03)) {
				int b1,b2,b3;
				b1 = k8042_read_output_wait();
				b2 = k8042_read_output_wait();
				b3 = k8042_read_output_wait();
				printf("Status: 0x%02X 0x%02x 0x%02x\n",b1,b2,b3);
				printf("Yes, this is a Synaptics PS/2 touchpad\n");
			}
			else {
				printf("..Cannot write AUX\n");
			}
		}
		else if (c == '9') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			/* E8 00 E8 00 E8 00 E8 00 E9 */
			if (k8042_write_aux_synaptics_E8(0x02)) {
				int b1,b2,b3;
				b1 = k8042_read_output_wait();
				b2 = k8042_read_output_wait();
				b3 = k8042_read_output_wait();
				printf("Status: 0x%02X 0x%02x 0x%02x\n",b1,b2,b3);
				printf("Yes, this is a Synaptics PS/2 touchpad\n");
			}
			else {
				printf("..Cannot write AUX\n");
			}
		}
		else if (c == '8') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			/* E8 00 E8 00 E8 00 E8 00 E9 */
			if (k8042_write_aux_synaptics_E8(0x01)) {
				int b1,b2,b3;
				b1 = k8042_read_output_wait();
				b2 = k8042_read_output_wait();
				b3 = k8042_read_output_wait();
				printf("Status: 0x%02X 0x%02x 0x%02x\n",b1,b2,b3);
				if (b1 == 0x3B && b2 == 0x47) printf("Yes, this is a Synaptics PS/2 touchpad\n");
			}
			else {
				printf("..Cannot write AUX\n");
			}
		}
		else if (c == '7') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			/* E8 00 E8 00 E8 00 E8 00 E9 */
			if (k8042_write_aux_synaptics_E8(0x00)) {
				int b1,b2,b3;
				b1 = k8042_read_output_wait();
				b2 = k8042_read_output_wait();
				b3 = k8042_read_output_wait();
				printf("Status: 0x%02X 0x%02x 0x%02x\n",b1,b2,b3);
				if (b2 == 0x47) printf("Yes, this is a Synaptics PS/2 touchpad\n");
			}
			else {
				printf("..Cannot write AUX\n");
			}
		}
		else if (c == '6') {
			printf("Reading device ID... "); fflush(stdout);
			k8042_drain_buffer();
			if (k8042_write_aux(0xF2) && k8042_read_output_wait() == 0xFA) {
				c = k8042_read_output_wait();
				printf("OK: %02x\n",c);
			}
			else {
				printf("..Cannot write AUX\n");
			}
		}
		else if (c == '5') {
			k8042_drain_buffer();
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(200) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 200\n");
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(100) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 100\n");
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(80) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 80\n");
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(200) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 200\n");
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(200) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 200\n");
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(80) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 80\n");
		}
		else if (c == '4') {
			k8042_drain_buffer();
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(200) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 200\n");
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(100) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 100\n");
			if (k8042_write_aux(0xF3) && k8042_read_output_wait() == 0xFA && k8042_write_aux(80) && k8042_read_output_wait() == 0xFA)
				printf("  OK-set 80\n");
		}
		else if (c == '3') {
			k8042_drain_buffer();
			if (k8042_write_aux(0xE9) && k8042_read_output_wait() == 0xFA) {
				int b1,b2,b3;
				b1 = k8042_read_output_wait();
				b2 = k8042_read_output_wait();
				b3 = k8042_read_output_wait();
				printf("Status: 0x%02X 0x%02x 0x%02x\n",b1,b2,b3);
			}
		}
		else if (c == '2') {
			k8042_disable_aux();
			k8042_enable_aux();
			k8042_drain_buffer();

			printf("Turning on streaming mode... "); fflush(stdout);
			k8042_drain_buffer();
			if (k8042_write_aux(0xEA) && k8042_read_output_wait() == 0xFA) {
				printf("OK\n");
			}
			else {
				printf("..Cannot write AUX\n");
			}

			printf("Enabling reporting mode... "); fflush(stdout);
			k8042_drain_buffer();
			if (k8042_write_aux(0xF4) && k8042_read_output_wait() == 0xFA) {
				printf("OK\n");
			}
			else {
				printf("..Cannot write AUX\n");
			}
		}
		else if (c == '1') {
			_cli();
			k8042_enable_keyboard();
			printf("PS/2 mouse raw input: (hit ESC to exit)\n");
			do {
				/* NTS: to avoid any BIOS conflicts we read the keyboard ourself */
				c = k8042_read_output_wait();
				if (c < 0) continue;

				/* was that keyboard input or mouse input? */
				if (k8042_output_was_aux()) {
					printf("%02x ",c); /* mouse input */
					fflush(stdout);
				}
				else { /* keyboard input. we're looking for "escape" */
					if (c == 0x81) break; /* the "break" code */
					else {
						printf("<KEY %02x> ",c);
						fflush(stdout);
					}
				}
			} while(1);
			force_clear_bios_shift_states();
			printf("\n");
			_sti();
		}
	} while(1);

	/* most test routines in this program aren't equipped to distinguish AUX and KEYBOARD input,
	 * so we need to shutdown the AUX port and shutdown the mouse */
	printf("Disabling reporting mode... "); fflush(stdout);
	k8042_drain_buffer();
	if (k8042_write_aux(0xF5) && k8042_read_output_wait() == 0xFA) {
		printf("OK\n");
	}
	else {
		printf("..Cannot write AUX\n");
	}

	k8042_drain_buffer();
	if (k8042_write_aux(0xFF)) { /* "returns BAT value (0xAA) and device ID" my ass. It 0xFA acks like everybody else */
		c = k8042_read_output_wait();
		if (!k8042_output_was_aux()) printf("Non-AUX response 0x%02x\n",c);
		else if (c != 0xFA) printf("Not ACK 0x%02x\n",c);
		else printf("OK\n");
	}
	else {
		printf("..Cannot write AUX\n");
	}

	k8042_disable_aux();
	k8042_drain_buffer(); /* <- disabling AUX may cause readback of an ACK.
				    adding this fixes the bug where reiniting AUX then exiting
				    the menu leaves the keyboard unresponsive in VirtualBox and
				    on most actual PC hardware */
	k8042_enable_keyboard();
	chain_irq1 = chain_irq12 = 1;
}

int main() {
	unsigned char leds = 0x7;
	int c,i;

	printf("8042 keyboard controller test code\n");
	if (!k8042_probe())
		printf("Warning: unable to probe 8042 controller. Is it there?\n");

	prev_irq1 = _dos_getvect(K8042_IRQ+0x08);
	_dos_setvect(K8042_IRQ+0x8,irq1);

	prev_irq12 = _dos_getvect(K8042_MOUSE_IRQ-8+0x70);
	_dos_setvect(K8042_MOUSE_IRQ-8+0x70,irq12);

	do {
		/* NTS: Unfortunately, DOSBox 0.74's 8042 emulation ignores a lot of the commands we play with here, but
		 *      this code works on actual hardware. */
		/* NTS: It is VERY important that these commands disable the keyboard (0xAD) *THEN* drain the buffer
		 *      before issuing the command and reading back the response. If you do not, any keypress that
		 *      happens between draining and disabling the keyboard will get in the way and be mistaken for
		 *      the command's response. Very unlikely, but if you mash the keyboard and cause a lot of input
		 *      here that *can* happen. */
		printf("\n");
		printf("  1. Raw input                   2. Command byte             3. Self-test\n");
		printf("  4. Keyboard intf. test         5. Input port               6. Output port\n");
		printf("  7. Test inputs                 8. Cycle LEDs               9. Diag. echo\n");
		printf("  A. Keyboard ID                 B. Reset                    C. Write out\n");
		printf("  M: PS/2 aux. menu              S. Read scan code set       !. Scancode1\n");
		printf("  t: s.c.1 and disable xlat      @. Scancode2                #. Scancode3\n");
		c = getch();
		if (c == 'M') {
			ps2_mouse_main();
		}
		else if (c == '!' || c == '@' || c == '#') {
			int set = 1;
			int ok = 0;
			_cli();
			if (c == '!') set = 1;
			else if (c == '@') set = 2;
			else if (c == '#') set = 3;
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			/* set scan code #1 */
			if (k8042_write_data(0xF0) && k8042_read_output_wait() == 0xFA && k8042_write_data(set) && k8042_read_output_wait() == 0xFA) {
				/* it is unspecified whether the keyboard ACKs again after setting the scan code. VirtualBox seems to do it anyway */
				if (k8042_read_output_wait() != 0xFA) printf("...no extra ack?\n");
				/* Now wait... did the keyboard actually switch scan codes or not?
				 * Many laptops I own for example seem to ignore the set scancode command and when we disable translation, we get Scan Code Set 2 */
				c = 0xFF;
				if (k8042_write_data(0xF0) && k8042_read_output_wait() == 0xFA) {
					if (k8042_write_data(0x00)) {
						c = k8042_read_output_wait();
						if (c == 0xFA) {
							c = k8042_read_output_wait();
							if (c < 0) printf("...Ack, but then no response?\n");
						}
						else {
							if (k8042_read_output_wait() != 0xFA) printf("...no extra ack?\n");
						}
					}
				}

				if (	(set == 1 && (c == 0x43 || c == 0x01)) ||
					(set == 2 && (c == 0x41 || c == 0x02)) ||
					(set == 3 && (c == 0x3F || c == 0x03))) {
					/* now write command byte to disable translation */
					printf("I set scan code 1, now disabling translation\n");
					if (k8042_write_command_byte(0x05)) { /* XLAT=0 keyb/mouse enable SYS=1 INT2=0 INT=1 */
						printf("And translation should be disabled now\n");
						ok = 1;
					}
				}
				else {
					/* nope, it ignore us */
					printf("Nope, your keyboard did not switch to scan code set 1.\n");
				}
			}
			else {
				printf("Failed to send 0x00\n");
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();

			if (ok) {
				/* echo them */
				chain_irq1 = 0;
				printf("Printing raw input from keyboard. Hit ESC to quit.\n");
				do {
					_cli();
					c = k8042_read_output_wait();
					if (c >= 0) {
						printf("0x%02x ",c); fflush(stdout);
					}
					_sti();
				} while ((set == 1 && c != 0x01) ||
					 (set == 2 && c != 0x76) ||
					 (set == 3 && c != 0x08));
				/* eat ESC key-up */
				do {
					_cli();
					c = k8042_read_output_wait();
					if (c >= 0) {
						printf("0x%02x ",c); fflush(stdout);
					}
					_sti();
				} while ((set == 1 && c != 0x81 && c != 0x01) ||
					 (set == 2 && c != 0x76) ||
					 (set == 3 && c != 0x08));
				chain_irq1 = 1;

				/* fun's over */
				_cli();
				force_clear_bios_shift_states();
				chain_irq1 = 0;
				if (!k8042_write_command(0xAD))
					printf("Cannot disable keyboard\n");
				k8042_drain_buffer();
				/* set scan code #2 (most modern systems have this) */
				if (k8042_write_data(0xF0) && k8042_read_output_wait() == 0xFA && k8042_write_data(0x02) && k8042_read_output_wait() == 0xFA) {
					/* it is unspecified whether the keyboard ACKs again after setting the scan code. VirtualBox seems to do it anyway */
					if (k8042_read_output_wait() != 0xFA) printf("...no extra ack?\n");
					/* Now wait... did the keyboard actually switch scan codes or not?
					 * Many laptops I own for example seem to ignore the set scancode command and when we disable translation, we get Scan Code Set 2 */
					c = 0xFF;
					if (k8042_write_data(0xF0) && k8042_read_output_wait() == 0xFA) {
						if (k8042_write_data(0x00)) {
							c = k8042_read_output_wait();
							if (c == 0xFA) {
								c = k8042_read_output_wait();
								if (c < 0) printf("...Ack, but then no response?\n");
							}
							else {
								if (k8042_read_output_wait() != 0xFA) printf("...no extra ack?\n");
							}
						}
					}

					if (c == 0x41 || c == 0x02) {
						/* now write command byte to disable translation */
						printf("I set scan code 2\n");
					}
					if (k8042_write_command_byte(0x45)) { /* XLAT=1 keyb/mouse enable SYS=1 INT2=0 INT=1 */
						printf("And translation should be enabled now\n");
					}
				}
				else {
					printf("Failed to send 0x00\n");
				}

				if (!k8042_write_command(0xAE))
					printf("Cannot disable keyboard\n");

				chain_irq1 = 1;
				_sti();
			}
		}
		else if (c == 't') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			/* set scan code #1 */
			if (k8042_write_data(0xF0) && k8042_read_output_wait() == 0xFA && k8042_write_data(0x01) && k8042_read_output_wait() == 0xFA) {
				/* it is unspecified whether the keyboard ACKs again after setting the scan code. VirtualBox seems to do it anyway */
				if (k8042_read_output_wait() != 0xFA) printf("...no extra ack?\n");
				/* Now wait... did the keyboard actually switch scan codes or not?
				 * Many laptops I own for example seem to ignore the set scancode command and when we disable translation, we get Scan Code Set 2 */
				c = 0xFF;
				if (k8042_write_data(0xF0) && k8042_read_output_wait() == 0xFA) {
					if (k8042_write_data(0x00)) {
						c = k8042_read_output_wait();
					}
				}

				if (c == 0x43 || c == 0x01) {
					/* now write command byte to disable translation */
					printf("I set scan code 1, now disabling translation\n");
					if (k8042_write_command_byte(0x05)) { /* XLAT=0 keyb/mouse enable SYS=1 INT2=0 INT=1 */
						printf("And translation should be disabled now\n");
					}
				}
				else {
					/* nope, it ignore us */
					printf("Nope, your keyboard did not switch to scan code set 1.\n");
				}
			}
			else {
				printf("Failed to send 0x00\n");
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
			printf("Alright, go ahead and type. The input should be the same as normal\n");
			goto scancode_watch;
		}
		else if (c == 'S') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_data(0xF0) && k8042_read_output_wait() == 0xFA) {
				c = 0xFF;
				if (k8042_write_data(0x00)) {
					printf("Wrote 0xF0, ");
					i = 0;
					do { /* NTS: Some keyboard and keyboard controller combos I have take quite a long time (250-500ms) to reset! */
						c = k8042_read_output_wait();
						/* NTS: Some keyboard controllers return ACK then the response,
						 *      others the response then ACK */
					} while ((c < 0 || c == 0xFA) && (++i) < 2);
					if (c >= 0) printf("Read back: %02x ",c);
					else printf("?? ");
					printf("\n");
					if (c == 0x43) printf("Scan code set #1\n");
					else if (c == 0x41) printf("Scan code set #2\n");
					else if (c == 0x3F) printf("Scan code set #3\n");
					else if (c == 0xFA) printf("Immediate ACK, probably not supported by your keyboard\n");
					if (c != 0xFA) {
						c = k8042_read_output_wait();
						if (c != 0xFA) printf("WARNING: No ACK after return value!\n");
					}
					/* NTS: DOSBox 0.74's emulation is so minimalist it stupidly ACKs even this command (returns 0xFA) */
				}
				else {
					printf("Failed to send 0x00 after 0xF0\n");
				}
			}
			else {
				printf("Failed to send 0x00\n");
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == 'C') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_command(0xD2) && k8042_write_data(0xAA)) {
				printf("Wrote 0xAA, ");
				i = 0;
				do { /* NTS: Some keyboard and keyboard controller combos I have take quite a long time (250-500ms) to reset! */
					c = k8042_read_output_wait();
				} while (c < 0 && (++i) < 2);
				if (c >= 0) printf("Read back: %02x ",c);
				else printf("?? ");
				printf("\n");
				/* NTS: DOSBox 0.74's emulation is so minimalist it stupidly ACKs even this command (returns 0xFA) */
			}
			else {
				printf("Failed to send 0xF2\n");
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == 'B') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_data(0xFF) && k8042_read_output_wait() == 0xFA) {
				i = 0;
				do { /* NTS: Some keyboard and keyboard controller combos I have take quite a long time (250-500ms) to reset! */
					c = k8042_read_output_wait();
				} while (c < 0 && (++i) < 32);
				if (c >= 0) printf("Reset BAT: %02x ",c);
				else printf("?? ");
				printf("\n");

				/* power-on might be "disabled" state */
				if (k8042_write_data(0xF4) && k8042_read_output_wait() == 0xFA) {
					printf("Reenable OK\n");
				}
				else {
					printf("Failed to reenable keyboard\n");
				}
			}
			else {
				printf("Failed to send 0xF2\n");
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == 'A') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_data(0xF2) && k8042_read_output_wait() == 0xFA) {
				c = k8042_read_output_wait();
				if (c >= 0) printf("%02x ",c);
				else printf("?? ");

				c = k8042_read_output_wait();
				if (c >= 0) printf("%02x ",c);
				else printf("?? ");

				printf("\n");
			}
			else {
				printf("Failed to send 0xF2\n");
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == '9') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_data(0xEE)) {
				c = k8042_read_output_wait();
				if (c == 0xFA) { /* NTS: Hah! DOSBox 0.74's emulation is too lazy to implement this command! */
					printf("Your keyboard returned 0xFA (ACK) for command 0xEE, it shouldn't\n");
					c = k8042_read_output_wait(); /* NTS: I happen to know some old systems I have lying around would actually return 0xFA then 0xEE */
				}
				if (c == 0xEE)
					printf("OK\n");
				else
					printf("Keyboard did not return 0xEE\n");
			}
			else {
				printf("Failed to send 0xEE\n");
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == '8') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_data(0xED) && (c=k8042_read_output_wait()) == 0xFA) {
				if (k8042_write_data(leds) && (c=k8042_read_output_wait()) == 0xFA) {
				}
				else {
					printf("Failed to send 0xED data (c = 0x%02X)\n",c);
				}
			}
			else {
				printf("Failed to send 0xED (c = 0x%02X)\n",c);
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			/* hopefully this is effective at resolving those "keyboard becomes unresponsive" issues
			 * I see on test machines when you hold down '8' */
			k8042_drain_buffer();
			k8042_drain_buffer();

			printf("%u%u%u\n",(leds >> 2) & 1,(leds >> 1) & 1,leds & 1);
			leds = (leds+1)&7;

			chain_irq1 = 1;
			_sti();
		}
		else if (c == '1') {
scancode_watch:
			chain_irq1 = 0;
			printf("Printing raw input from keyboard. Hit ESC to quit.\n");
			do {
				_cli();
				c = k8042_read_output_wait();
				if (c >= 0) {
					printf("0x%02x ",c); fflush(stdout);
				}
				_sti();
			} while (c != 0x01);
			/* eat ESC key-up */
			do {
				_cli();
				c = k8042_read_output_wait();
				if (c >= 0) {
					printf("0x%02x ",c); fflush(stdout);
				}
				_sti();
			} while (c != 0x81);
			chain_irq1 = 1;
			printf("\n");
		}
		else if (c == '2') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_command(0x20)) {
				c = k8042_read_output_wait();
				if (c >= 0) {
					printf("Command byte: 0x%02x\n",c);
				}
				else {
					/* some emulators [DOSBox] do not emulate this command and do not respond */
					printf("Unable to read command byte. 0x64=0x%02x\n",inp(K8042_STATUS));
				}
			}
			else {
				printf("Unable to write command. 0x64=0x%02x\n",inp(K8042_STATUS));
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == '3') {
			_cli();
			chain_irq1 = 0;

			if (!(k8042_write_data(0xF5) && (c=k8042_read_output_wait()) == 0xFA))
				printf("Cannot disable keyboard (read 0x%02X)\n",c);

			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_command(0xAA)) {
				c = k8042_read_output_wait();
				if (c >= 0) {
					printf("Self-test result: 0x%02x\n",c);
				}
				else {
					/* some emulators [DOSBox] do not emulate this command and do not respond */
					printf("Unable to read response. 0x64=0x%02x\n",inp(K8042_STATUS));
				}
			}
			else {
				printf("Unable to write command. 0x64=0x%02x\n",inp(K8042_STATUS));
			}

			/* some motherboards seem to reset the command byte on self-test */
			if (!k8042_write_command_byte(0x45))
				printf("Cannot write command byte\n");

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			c = -1;
			if (!(k8042_write_data(0xF4) && (c=k8042_read_output_wait()) == 0xFA))
				printf("Cannot enable keyboard (read 0x%02X)\n",c);

			chain_irq1 = 1;
			_sti();
		}
		else if (c == '4') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_command(0xAB)) {
				c = k8042_read_output_wait();
				if (c >= 0) {
					printf("Self-test result: 0x%02x\n",c);
				}
				else {
					/* some emulators [DOSBox] do not emulate this command and do not respond */
					printf("Unable to read response. 0x64=0x%02x\n",inp(K8042_STATUS));
				}
			}
			else {
				printf("Unable to write command. 0x64=0x%02x\n",inp(K8042_STATUS));
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			if (!(k8042_write_data(0xF4) && k8042_read_output_wait() == 0xFA))
				printf("Cannot enable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == '5') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_command(0xC0)) {
				c = k8042_read_output_wait();
				if (c >= 0) {
					printf("Input port: 0x%02x\n",c);
				}
				else {
					/* some emulators [DOSBox] do not emulate this command and do not respond */
					printf("Unable to read response. 0x64=0x%02x\n",inp(K8042_STATUS));
				}
			}
			else {
				printf("Unable to write command. 0x64=0x%02x\n",inp(K8042_STATUS));
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == '6') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_command(0xD0)) {
				c = k8042_read_output_wait();
				if (c >= 0) {
					/* Holy shit---DOSBox actually emulates this! */
					printf("Output port: 0x%02x\n",c);
				}
				else {
					printf("Unable to read response. 0x64=0x%02x\n",inp(K8042_STATUS));
				}
			}
			else {
				printf("Unable to write command. 0x64=0x%02x\n",inp(K8042_STATUS));
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == '7') {
			_cli();
			chain_irq1 = 0;
			if (!k8042_write_command(0xAD))
				printf("Cannot disable keyboard\n");
			k8042_drain_buffer();
			if (k8042_write_command(0xE0)) {
				c = k8042_read_output_wait();
				if (c >= 0) {
					printf("Test port: 0x%02x\n",c);
				}
				else {
					printf("Unable to read response. 0x64=0x%02x\n",inp(K8042_STATUS));
				}
			}
			else {
				printf("Unable to write command. 0x64=0x%02x\n",inp(K8042_STATUS));
			}

			if (!k8042_write_command(0xAE))
				printf("Cannot disable keyboard\n");

			chain_irq1 = 1;
			_sti();
		}
		else if (c == 27) {
			break;
		}
	} while (1);

	_dos_setvect(K8042_MOUSE_IRQ-8+0x70,prev_irq12);
	_dos_setvect(K8042_IRQ+0x8,prev_irq1);
	return 0;
}

