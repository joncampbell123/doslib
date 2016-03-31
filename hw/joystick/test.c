 
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

struct joystick_t {
	uint16_t		port;		// there is only ONE joystick port!
	uint8_t			buttons;	// button states (last read), bitfield for buttons 1-4, read from bits 5-8 of the I/O port
	uint16_t		reading[4];	// joystick readings
};

enum joystick_button_t {
	JOYSTICK_BUTTON_1=0x01,
	JOYSTICK_BUTTON_2=0x02,
	JOYSTICK_BUTTON_3=0x04,
	JOYSTICK_BUTTON_4=0x08
};

enum joystick_reading_t {
	JOYSTICK_READING_A_X_AXIS=0,		// Joystick A, X axis
	JOYSTICK_READING_A_Y_AXIS=1,		// Joystick A, Y axis
	JOYSTICK_READING_B_X_AXIS=2,		// Joystick B, X axis
	JOYSTICK_READING_B_Y_AXIS=3		// Joystick B, Y axis
};

#define JOYSTICK_BUTTON_BITFIELD(x) (1U << (x))

void init_joystick(struct joystick_t *joy) {
	memset(joy,0,sizeof(*joy));
}

static inline void joystick_update_button_state(struct joystick_t *joy,const uint8_t b) {
	joy->buttons = ((~b) >> 4U) & 0xFU;
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

	printf("port=%03x a=%02x b=%02x\n",port,a,b);

	if (a == 0xFF && b == 0xFF) /* all 0xFF? fail */
		return 0;

	joystick_update_button_state(joy,b);
	joy->port = port;
	return 1;
}

int main(int argc,char **argv) {
	uint16_t ppos[4] = {0xFFFFU,0xFFFFU,0xFFFFU,0xFFFFU};
	uint8_t pbuttons = 0xFF;
	uint8_t fullredraw = 1;
	struct joystick_t joy;
	uint8_t redraw = 0;
	uint8_t die = 0;
	char tmp[128];
	int c;

	printf("Joystick test program\n");
	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	
	/* most sound cards put the joystick on port 0x201 */
	init_joystick(&joy);
	if (!probe_joystick(&joy,0x201)) {
		unsigned int i;

		/* okay, try 0x200 through 0x207 */
		for (i=0x200;i <= 0x207;i++) {
			if (probe_joystick(&joy,i))
				break;
		}

		if (i > 0x207) {
			printf("No joystick found\n");
			return 1;
		}
	}

	printf("Found joystick on I/O port %03xh. Buttons=0x%x\n",joy.port,joy.buttons);
	printf("Hit ENTER to continue.\n");
	while (getch() != 13);

	int10_setmode(3);
	update_state_from_vga();
	
	while (!die) {
		if (fullredraw) {
			vga_write_color(0x07);
			vga_clear();
			fullredraw = 0;
			redraw = 1;
		}
		if (redraw) {
			redraw = 0;
			vga_write_color(0x07);
			sprintf(tmp,"Joystick on port %03xh Buttons={%u,%u,%u,%u}",
				joy.port,
				(joy.buttons & JOYSTICK_BUTTON_1) ? 1 : 0,
				(joy.buttons & JOYSTICK_BUTTON_2) ? 1 : 0,
				(joy.buttons & JOYSTICK_BUTTON_3) ? 1 : 0,
				(joy.buttons & JOYSTICK_BUTTON_4) ? 1 : 0);

			vga_moveto(0,0);
			vga_write(tmp);

			sprintf(tmp,"Joystick position A=(%u,%u) B=(%u,%u)       ",
				joy.reading[0],	joy.reading[1],
				joy.reading[2],	joy.reading[3]);

			vga_moveto(0,1);
			vga_write(tmp);
		}

		read_joystick_buttons(&joy);
		read_joystick_positions(&joy,0xF/*all of them*/);
		if (pbuttons != joy.buttons) {
			pbuttons = joy.buttons;
			redraw = 1;
		}
		for (c=0;c < 4;c++) {
			if (ppos[c] != joy.reading[c]) {
				ppos[c] = joy.reading[c];
				redraw = 1;
			}
		}

		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27) break;
		}
	}

	vga_moveto(0,vga_state.vga_height-1);
	vga_sync_bios_cursor();
	return 0;
}

