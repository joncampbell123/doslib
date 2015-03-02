
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
#include <hw/pci/pci.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/ide/idelib.h>

#include "testutil.h"
#include "testmbox.h"
#include "testcmui.h"
#include "testbusy.h"
#include "testpiot.h"
#include "testrvfy.h"
#include "testrdwr.h"
#include "testidnt.h"
#include "testcdej.h"
#include "testpiom.h"
#include "testtadj.h"
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

#ifdef TWEAK_MENU

static const char *drive_tweaks_and_adjustments_strings[] = {
	""					/* 0 */ /* "Allow 32-bit PIO for ATAPI command" */
};

void do_drive_tweaks_and_adjustments(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_tweaks_and_adjustments_strings);

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* select the drive we want */
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);

	/* in case the IDE controller is busy for that time */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* read back: did the drive select take effect? if not, it might not be there. another common sign is the head/drive select reads back 0xFF */
	c = do_ide_controller_drive_check_select(ide,which);
	if (c < 0) return;

	/* it might be a CD-ROM drive, which in some cases might not raise the Drive Ready bit */
	do_ide_controller_atapi_device_check_post_host_reset(ide);

	/* wait for the drive to indicate readiness */
	/* NTS: If the drive never becomes ready even despite our reset hacks, there's a strong
	 *      possibility that the device doesn't exist. This can happen for example if there
	 *      is a master attached but no slave. */
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c < 0) return;

	/* for completeness, clear pending IRQ */
	idelib_controller_ack_irq(ide);

	while (1) {
		if (backredraw) {
			vga = vga_alpha_ram;
			backredraw = 0;
			redraw = 1;

			for (y=0;y < vga_height;y++) {
				for (x=0;x < vga_width;x++) {
					*vga++ = 0x1E00 + 177;
				}
			}

			vga_moveto(0,0);

			vga_write_color(0x1F);
			vga_write("        IDE controller ");
			sprintf(tmp,"@%X",ide->base_io);
			vga_write(tmp);
			if (ide->alt_io != 0) {
				sprintf(tmp," alt %X",ide->alt_io);
				vga_write(tmp);
			}
			if (ide->irq >= 0) {
				sprintf(tmp," IRQ %d",ide->irq);
				vga_write(tmp);
			}
			vga_write(which ? " Slave" : " Master");
			vga_write(" << Tweaks and adjustments");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');

			vga_write_color(0xC);
			vga_write("WARNING: This code talks directly to your hard disk controller.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
			vga_write_color(0xC);
			vga_write("         If you value the data on your hard drive do not run this program.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			redraw = 0;

			drive_tweaks_and_adjustments_strings[0] =
				ide->pio32_atapi_command ?
				 "Allow 32-bit PIO for ATAPI command (ENABLED)" :
				 "Allow 32-bit PIO for ATAPI command (DISABLED)";

			vga_moveto(mbox.ofsx,mbox.ofsy - 2);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE drive main menu");
			while (vga_pos_x < (mbox.width+mbox.ofsx) && vga_pos_x != 0) vga_writec(' ');

			menuboxbound_redraw(&mbox,select);
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1)
				break;

			switch (select) {
				case 0: /* 16-bit */
					redraw = 1;
					ide->pio32_atapi_command ^= 1;
					break;
			};
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = mbox.item_max;

			redraw = 1;
		}
		else if (c == 0x4B00) { /* left */
			redraw = 1;
		}
		else if (c == 0x4D00) { /* right */
			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > mbox.item_max)
				select = -1;

			redraw = 1;
		}
	}
}

#endif /* TWEAK_MENU */

