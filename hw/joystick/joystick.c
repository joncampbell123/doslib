 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/vga/vga.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/joystick/joystick.h>

void init_joystick(struct joystick_t *joy) {
	memset(joy,0,sizeof(*joy));
}

void read_joystick_buttons(struct joystick_t *joy) {
	joystick_update_button_state(joy,inp(joy->port));
}

void read_joystick_positions(struct joystick_t *joy,uint8_t which/*bitmask*/) {
	uint16_t pc=0,tc=0,dc;
	uint8_t check,msk,b;
	uint32_t sofar=0;
	uint8_t btn=0xFF;
	uint8_t a;

	check = which & 0xF;
	if (check == 0) return;

	/* trigger read. axis bits will change to 1 */
	outp(joy->port,0xFF);
	tc=read_8254(0);

	/* wait for bits to change from 1 to 0 for each axis */
	do {
		a = inp(joy->port);
		btn &= a; /* help the host program respond to buttons pushed during this wait loop */

		/* for any bits that change to 1, note the time */
		b = (~a) & check;
		if (b != 0) {
			for (dc=0,msk=1;dc < 4;dc++,msk<<=1U) {
				if ((b&msk) != 0)
					joy->reading[dc] = (uint16_t)sofar;
			}

			check &= ~b;
			if (check == 0) break;
		}

		/* else, track time */
		pc=tc;
		tc=read_8254(0); // NTS: remember the timer counts DOWN to zero, not up!
		if (tc > pc)
			dc = (pc + (uint16_t)t8254_counter[0] - tc);
		else
			dc = (pc - tc);

		sofar += dc;
		if (sofar > 0xFFFFUL) {
			sofar = 0xFFFFUL;
			for (dc=0,msk=1;dc < 4;dc++,msk<<=1U) {
				if ((check&msk) != 0)
					joy->reading[dc] = (uint16_t)sofar;
			}
			break;
		}
	} while (1);

	joystick_update_button_state(joy,btn);
}

int probe_joystick(struct joystick_t *joy,uint16_t port) {
	uint8_t a,b;

	if (joy->port == port)
		return 1;

	a = inp(port);
	/* trigger some bits, see if they clear */
	outp(port,0xFF);
	b = inp(port);

	if (a == 0xFF && b == 0xFF) /* all 0xFF? fail */
		return 0;

	joystick_update_button_state(joy,b);
	joy->port = port;
	return 1;
}

