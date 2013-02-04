/* test.c
 *
 * 8259 programmable interrupt controller test program.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */

/* NTS: As of 2011/02/27 the 8254 routines no longer do cli/sti for us, we are expected
 *      to do them ourself. This is for performance reasons as well as for sanity reasons
 *      should we ever need to use the subroutines from within an interrupt handler */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8259/8259.h>

void polltest() {
	unsigned char ann[128],anni,cc;
	int pat;

	printf("Polling test. Interrupts that occur will be announced. Hit ESC to exit.\n");
	while (1) {
		_cli();
		anni = 0;
		pat = 3200;
		do {
			cc = p8259_poll(0);
			if (cc & 0x80) {
				ann[anni++] = cc & 7;
				if (anni >= sizeof(anni)) break;
			}
			else {
				break;
			}

			cc = p8259_poll(8);
			if (cc & 0x80) {
				ann[anni++] = (cc & 7) + 8;
				if (anni >= sizeof(anni)) break;
			}
			else {
				break;
			}
		} while (--pat != 0);
		_sti();

		/* we ate interrupts, so we need to reissue them */
		for (cc=0;cc < anni;cc++) {
			union REGS regs;
			switch (ann[cc]) {
				case 0:		just_int86(0x08,&regs,&regs); break;
				case 1:		just_int86(0x09,&regs,&regs); break;
				case 2:		just_int86(0x0A,&regs,&regs); break;
				case 3:		just_int86(0x0B,&regs,&regs); break;
				case 4:		just_int86(0x0C,&regs,&regs); break;
				case 5:		just_int86(0x0D,&regs,&regs); break;
				case 6:		just_int86(0x0E,&regs,&regs); break;
				case 7:		just_int86(0x0F,&regs,&regs); break;

				case 8:		just_int86(0x70,&regs,&regs); break;
				case 9:		just_int86(0x71,&regs,&regs); break;
				case 10:	just_int86(0x72,&regs,&regs); break;
				case 11:	just_int86(0x73,&regs,&regs); break;
				case 12:	just_int86(0x74,&regs,&regs); break;
				case 13:	just_int86(0x75,&regs,&regs); break;
				case 14:	just_int86(0x76,&regs,&regs); break;
				case 15:	just_int86(0x77,&regs,&regs); break;
			};
		}

		/* print them out */
		if (anni != 0) {
			for (cc=0;cc < anni;cc++) printf("%u ",ann[cc]);
			printf("\n");
		}

		if (kbhit()) {
			if (getch() == 27)
				break;
		}
	}
}

void irrisr() {
	unsigned short irr,isr;

	printf("Reading IRR/ISR hit ESC to quit\n");
	while (1) {
		if (kbhit()) {
			if (getch() == 27)
				break;
		}

		_cli();
		do {
			irr  = p8259_read_IRR(0);
			irr |= p8259_read_IRR(8) << 8;
			isr  = p8259_read_ISR(0);
			isr |= p8259_read_ISR(8) << 8;
		} while ((irr|isr) == 0);
		_sti();

		if ((irr|isr) != 0) printf("IRR=%04x ISR=%04x\n",irr,isr);
	}
}

int main() {
	int c;

	printf("8259 test program\n");
	if (!probe_8259()) {
		printf("There does not appear to be a PIC on your system\n");
		return 1;
	}
	printf("Slave PIC: %s\n",p8259_slave_present ? "yes" : "no");

	while (1) {
		printf("  1: 8259 poll test\n");
		printf("  2: 8259 IRR/ISR\n");
		printf("ESC: quit\n");

		c = getch();
		if (c == '1') {
			polltest();
		}
		else if (c == '2') {
			irrisr();
		}
		else if (c == 27) {
			break;
		}
	}

	return 0;
}

