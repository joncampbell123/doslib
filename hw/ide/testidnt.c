
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
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

int do_ide_identify(unsigned char *info/*512*/,unsigned int sz,struct ide_controller *ide,unsigned char which,unsigned char command) {
	if (sz < 512)
		return -1;
	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return -1;

	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
	idelib_controller_reset_irq_counter(ide);
	idelib_controller_write_command(ide,command); /* <- (A1h) identify packet device (ECh) identify device */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (ide->last_status&1) return 1;

	/* read it */
	do_ide_controller_user_wait_drive_drq(ide);
	idelib_read_pio_general((unsigned char*)info,512,ide,IDELIB_PIO_WIDTH_DEFAULT);
	return 0;
}

void do_drive_identify_device_test(struct ide_controller *ide,unsigned char which,unsigned char command) {
	unsigned int x,y,i;
	uint16_t info[256];
	int r;

	r = do_ide_identify((unsigned char*)info,sizeof(info),ide,which,command);
	if (r == 0) {
		/* ------------ PAGE 1 -------------*/
		vga_write_color(0x0E); vga_clear();

		vga_moveto(0,0); vga_write("Serial: ");
		for (x=0;x < 10;x++) {
			tmp[x*2 + 0] = info[10+x] >> 8;
			tmp[x*2 + 1] = info[10+x] & 0xFF;
		}
		tmp[10*2] = 0; vga_write(tmp);

		vga_write(" F/W rev: ");
		for (x=0;x < 4;x++) {
			tmp[x*2 + 0] = info[23+x] >> 8;
			tmp[x*2 + 1] = info[23+x] & 0xFF;
		}
		tmp[4*2] = 0; vga_write(tmp);

		vga_write(" Model: ");
		for (x=0;x < 20;x++) {
			tmp[x*2 + 0] = info[27+x] >> 8;
			tmp[x*2 + 1] = info[27+x] & 0xFF;
		}
		tmp[20*2] = 0; vga_write(tmp);

		vga_moveto(0,1);
		sprintf(tmp,"Logical C/H/S: %u/%u/%u  ",info[1],info[3],info[6]);
		vga_write(tmp);
		sprintf(tmp,"Current C/H/S: %u/%u/%u",  info[54],info[55],info[56]);
		vga_write(tmp);

		vga_moveto(0,2);
		sprintf(tmp,"Current Capacity: %lu  Total user addressable: %lu  ",
			(unsigned long)info[57] | ((unsigned long)info[58] << 16UL),
			(unsigned long)info[60] | ((unsigned long)info[61] << 16UL));
		vga_write(tmp);

		vga_moveto(0,3);
		vga_write_color(0x0D);
		vga_write("For more information consult ATA documentation. Hit ENTER to continue.");

		/* top row */
		vga_moveto(0,5);
		vga_write_color(0x08);
		vga_write("WORD ");

		vga_moveto(5,5);
		for (x=0;x < 8;x++) {
			sprintf(tmp,"+%u   ",x);
			vga_write(tmp);
		}
		vga_moveto(46,5);
		for (x=0;x < 8;x++) {
			sprintf(tmp,"%u ",x);
			vga_write(tmp);
		}
		vga_write("HI/LO byte order");

		/* data, page by page */
		for (i=0;i < 2;i++) {
			for (y=0;y < 16;y++) {
				vga_moveto(0,6+y);
				vga_write_color(0x08);
				for (x=0;x < 8;x++) {
					sprintf(tmp,"%04x ",(y*8)+(i*128)+x);
					vga_write(tmp);
				}

				vga_moveto(5,6+y);
				vga_write_color(0x0F);
				for (x=0;x < 8;x++) {
					sprintf(tmp,"%04x ",info[(y*8)+(i*128)+x]);
					vga_write(tmp);
				}

				vga_moveto(46,6+y);
				vga_write_color(0x0E);
				for (x=0;x < 8;x++) {
					vga_writec(sanitizechar(info[(y*8)+(i*128)+x] >> 8));
					vga_writec(sanitizechar(info[(y*8)+(i*128)+x] & 0xFF));
				}
			}

			if (wait_for_enter_or_escape() == 27)
				break; /* allow ESC to immediately break out */
		}
	}
	else if (r > 0) {
		struct vga_msg_box vgabox;

		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

