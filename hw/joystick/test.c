 
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
	/* NTS: Most ISA/PCI sound cards for some reason map the *same* joystick port across I/O ports 0x200-0x207 */
	/*      It's entirely possible to "discover" 8 joysticks this way when there is really only one attached. */
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

