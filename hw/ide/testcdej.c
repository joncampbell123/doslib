
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
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

void do_drive_atapi_eject_load(struct ide_controller *ide,unsigned char which,unsigned char atapi_eject_how) {
	struct vga_msg_box vgabox;
	uint8_t buf[12] = {0x1B/*EJECT*/,0x00,0x00,0x00, /* ATAPI EJECT (START/STOP UNIT) command */
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00};
	/* NTS: buf[4] is filled in with how to start/stop the unit:
	 *   0x00 STOP (spin down)
	 *   0x01 START (spin up)
	 *   0x02 EJECT (eject CD, usually eject CD-ROM tray)
	 *   0x03 LOAD (load CD, close CD tray if drive is capable) */

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;
	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
	if (idelib_controller_atapi_prepare_packet_command(ide,/*xfer=to host no DMA*/0x04,/*byte count=*/0) < 0) /* fill out taskfile with command */
		return;
	if (idelib_controller_apply_taskfile(ide,0xBE/*base_io+1-5&7*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/) < 0) /* also writes command */
		return;

	/* NTS: Despite OSDev ATAPI advice, IRQ doesn't seem to fire at this stage, we must poll wait */
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	idelib_controller_update_atapi_state(ide);
	if (!(ide->last_status&1)) { /* if no error, read result from count register */
		do_warn_if_atapi_not_in_command_state(ide); /* sector count register should signal we're in the command stage */

		buf[4] = atapi_eject_how; /* fill in byte 4 which tells ATAPI how to start/stop the unit */
		idelib_controller_reset_irq_counter(ide); /* IRQ will fire after command completion */
		idelib_controller_atapi_write_command(ide,buf,12); /* write 12-byte ATAPI command data */
		if (ide->flags.io_irq_enable) { /* NOW we wait for the IRQ */
			do_ide_controller_user_wait_irq(ide,1);
			idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
		}
		do_ide_controller_user_wait_busy_controller(ide);
		do_ide_controller_user_wait_drive_ready(ide);
		idelib_controller_update_atapi_state(ide); /* having completed the command, read ATAPI state again */
		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);

		do_warn_if_atapi_not_in_complete_state(ide); /* sector count register should signal we're in the completed stage (command/data=1 input/output=1) */
	}
	else {
		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

static const char *drive_cdrom_startstop_strings[] = {
	"Show IDE register taskfile",		/* 0 */
	"Eject",
	"Load",
	"Start",
	"Stop"
};

void do_drive_cdrom_startstop_test(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_cdrom_startstop_strings);

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
			vga_write(" << CD-ROM eject/load");
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
				case 0: /* show IDE register taskfile */
					do_common_show_ide_taskfile(ide,which);
					redraw = backredraw = 1;
					break;
				case 1: /*Eject*/
				case 2: /*Load*/
				case 3: /*Start*/
				case 4: /*Stop*/ {
					static const unsigned char cmd[4] = {2/*eject*/,3/*load*/,1/*start*/,0/*stop*/};
					do_drive_atapi_eject_load(ide,which,cmd[select-1]);
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

