
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
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

#ifdef MULTIPLE_MODE_MENU

static const char *drive_multiple_mode_menustrings[] = {
	"Show IDE register taskfile",		/* 0 */
	"Set sector count",
	"Probe valid values"
};

void do_drive_multiple_mode(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	uint16_t info[256];
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int r;
	int c;

	/* OK do it */
	r = do_ide_identify((unsigned char*)info,sizeof(info),ide,which,0xEC/*ATA IDENTIFY DEVICE*/);
	if (r > 0) {
		struct vga_msg_box vgabox;

		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
	else if (r < 0) {
		return;
	}

	/* WORD 47: [15:8] RESERVED
	 *          [7:0]  Maximum number of sectors that shall be transferred per interrupt on READ/WRITE multiple */
	/* WORD 59: [15:9] RESERVED
	 *          [8]    Multiple sector setting is valid
	 *          [7:0]  Current setting for number of sectors to transfer per interrupt for multiple read/write */
	if (!(info[47]&0xFF)) {
		struct vga_msg_box vgabox;

		vga_msg_box_create(&vgabox,"WARNING: IDE device does not appear to support SET MULTIPLE.\nENTER to continue, ESC to cancel.",0,0);
		r = wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
		if (r == 27/*ESC*/) return;
	}

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_multiple_mode_menustrings);

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
			vga_write("        IDE multiple mode: ");
			sprintf(tmp,"sec=%u / max=%u ",info[59]&0xFF,info[47]&0xFF);
			vga_write(tmp);
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
				case 1: /*set sector count*/ {
					r = prompt_sector_count();
					if (r >= 0 && r < 256) {
						do_ide_set_multiple_mode(ide,which,r);
						do_ide_identify((unsigned char*)info,sizeof(info),ide,which,0xEC/*ATA IDENTIFY DEVICE*/);
						redraw = backredraw = 1;
					}
					} break;
				case 2: /*probe valid values*/ {
					unsigned char orig_sz = info[59]&0xFF;

					redraw = backredraw = 1;
					vga_moveto(0,0);
					vga_write_color(0x0F);
					vga_clear();
					vga_write("Probing valid sector counts:\n\n");
					vga_write_color(0x0E);

					for (r=0;r < 256;r++) {
						/* set sector count, then use IDENTIFY DEVICE to read back the value */
						if (do_ide_set_multiple_mode(ide,which,r) < 0)
							break;
						if (do_ide_identify((unsigned char*)info,sizeof(info),ide,which,0xEC/*ATA IDENTIFY DEVICE*/) < 0)
							break;

						if ((info[59]&0xFF) == r) {
							sprintf(tmp,"%u ",r);
							vga_write(tmp);
						}
					}
					vga_write("\n\n");

					/* and then restore the sector count */
					do_ide_set_multiple_mode(ide,which,orig_sz);
					do_ide_identify((unsigned char*)info,sizeof(info),ide,which,0xEC/*ATA IDENTIFY DEVICE*/);

					vga_write_color(0x0F);
					vga_write("Hit ENTER to continue.\n");
					wait_for_enter_or_escape();
					} break;
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
#endif /* MULTIPLE_MODE_MENU */

