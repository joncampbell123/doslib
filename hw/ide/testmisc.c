
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
#include "testcdrm.h"
#include "testmumo.h"
#include "testmisc.h"
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

#ifdef MISC_TEST

/*-----------------------------------------------------------------*/

static const char *drive_misc_tests_menustrings[] = {
	"Show IDE register taskfile",		/* 0 */
	"Flush cache",
	"Flush cache ext (LBA48)",
	"Execute device diagnostic",
	"CFA Disable Media Card Pass through",
	"CFA Enable Media Card Pass through"	/* 5*/
};

void do_drive_misc_tests(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	struct vga_msg_box vgabox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_misc_tests_menustrings);

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
			vga_write("        IDE controller read/write tests ");
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

			vga_moveto(mbox.ofsx,mbox.ofsy - 2);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE controller main menu");
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
				case 0: /* show IDE register taskfile */
					do_common_show_ide_taskfile(ide,which);
					redraw = backredraw = 1;
					break;
				case 1: /* Flush cache */
					do_ide_flush(ide,which,/*lba48*/0);
					if (!idelib_controller_is_error(ide))
						vga_msg_box_create(&vgabox,"Success",0,0);
					else
						common_ide_success_or_error_vga_msg_box(ide,&vgabox);

					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
					redraw = 1;
					break;
				case 2: /* Flush cache ext */
					do_ide_flush(ide,which,/*lba48*/1);
					if (!idelib_controller_is_error(ide))
						vga_msg_box_create(&vgabox,"Success",0,0);
					else
						common_ide_success_or_error_vga_msg_box(ide,&vgabox);

					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
					redraw = 1;
					break;
				case 3: /* Execute device diagnostic */
					do_ide_device_diagnostic(ide,which);
					if (!idelib_controller_is_error(ide))
						vga_msg_box_create(&vgabox,"Success",0,0);
					else
						common_ide_success_or_error_vga_msg_box(ide,&vgabox);

					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
					redraw = 1;
					break;
				case 4: /* Disable media card pass through */
					do_ide_media_card_pass_through(ide,which,0);
					if (!idelib_controller_is_error(ide))
						vga_msg_box_create(&vgabox,"Success",0,0);
					else
						common_ide_success_or_error_vga_msg_box(ide,&vgabox);

					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
					redraw = 1;
					break;
				case 5: /* Enable media card pass through */
					do_ide_media_card_pass_through(ide,which,1);
					if (!idelib_controller_is_error(ide))
						vga_msg_box_create(&vgabox,"Success",0,0);
					else
						common_ide_success_or_error_vga_msg_box(ide,&vgabox);

					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
					redraw = 1;
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

#endif /* MISC_TEST */

