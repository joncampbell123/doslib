/* THINGS TO DO:
 *
 *      - Modularize copy-pasta'd code such as:
 *         * The "if you value your data" warning
 *         * ATAPI packet commands
 *         * EVERYTHING---This code basically works on test hardware, now it's time to clean it up
 *           modularize and refactor.
 *      - Add menu item where the user can ask this code to test whether or not the IDE controller
 *        supports 32-bit PIO properly.
 *      - Start using this code for reference and implementation of IDE emulation within DOSBox-X
 *      - Add menu items to allow the user to play with SET FEATURES command
 *      - Add submenu where the user can play with the S.M.A.R.T. ATA commands
 *      - **TEST THIS CODE ON AS MANY MACHINES AS POSSIBLE** The IDE interface is one of those
 *        hardware standards that is conceptually simple yet so manu manufacturers managed to fuck
 *        up their implementation in some way or another.
 *      - Cleanup this code, move library into idelib.c+idelib.h
 *      - Fix corner cases where we're IDE busy waiting and user commands to break free (ESC or spacebar
 *        do not break out of read/write loop, forcing the user to CTRL+ALT+DEL or press reset.
 *
 * Also don't forget:
 *   - Test programs for specific IDE chipsets (like Intel PIIX3) to demonstrate IDE DMA READ/WRITE commands
 *
 * Interesting notes:
 *   - Toshiba Satellite Pro 465CDX/2.1
 *      - Putting the hard drive to sleep seems to put the IDE controller to sleep too. IDE controller
 *        "busy" bit is stuck on when hard drive asleep. Only way out seems to be doing a "host reset"
 *        on that IDE controller. Note that NORMAL behavior is that the IDE controller remains not-busy
 *        but the device is not ready.
 *
 *      - The CD-ROM drive (or secondary IDE?) appears to ignore 32-bit I/O to port 0x170 (base_io+0).
 *        If you attempt to read a sector or identify command results using 32-bit PIO, you get only
 *        0xFFFFFFFF. Reading data only works when PIO is done as 16-bit. NORMAL behavior suggests
 *        either 32-bit PIO gets 32 bits at a time (PCI based IDE) or 32-bit PIO gets 16 bits of IDE
 *        data and 16 bits of the adjacent 16-bit I/O register due to 386/486-era ISA subdivision of
 *        32-bit I/O into two 16-bit I/O reads. Supposedly, on pre-PCI controllers, 32-bit PIO could
 *        be made to work if you tell the card in advance using the "VLB keying sequence", but I have
 *        yet to find such a card.
 *
 * Known hardware this code has trouble with (so far):
 * 
 *   - Ancient (1999-2002-ish) DVD-ROM drives. They generally work with this code but there seem to be a lot
 *     of edge cases. One DVD-ROM drive I own will not raise "drive ready" after receipt of a command it doesn't
 *     recognize. */

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

unsigned char			cdrom_read_mode = 12;
unsigned char			pio_width_warning = 1;
unsigned char			big_scary_write_test_warning = 1;

char				tmp[1024];
uint16_t			ide_info[256];
unsigned char			cdrom_sector[512U*63U];	/* ~32KB, enough for CD-ROM sector or 63 512-byte sectors */

/*-----------------------------------------------------------------*/

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

void do_drive_read_one_sector_test(struct ide_controller *ide,unsigned char which) {
	unsigned long sector = 16; /* read the ISO 9660 table of contents */
	unsigned long tlen = 2048; /* one sector */
	unsigned long tlen_sect = 1;
	struct vga_msg_box vgabox;
	uint8_t buf[12] = {0};
	unsigned int x,y,i;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;
	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
	if (idelib_controller_atapi_prepare_packet_command(ide,/*xfer=to host no DMA*/0x04,/*byte count=*/tlen) < 0) /* fill out taskfile with command */
		return;
	if (idelib_controller_apply_taskfile(ide,0xBE/*base_io+1-5&7*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/) < 0) /* also writes command */
		return;

	/* NTS: Despite OSDev ATAPI advice, IRQ doesn't seem to fire at this stage, we must poll wait */
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	idelib_controller_update_atapi_state(ide);
	if (!(ide->last_status&1)) { /* if no error, read result from count register */
		do_warn_if_atapi_not_in_command_state(ide); /* sector count register should signal we're in the command stage */

		do_construct_atapi_scsi_mmc_read(buf,sector,tlen_sect,cdrom_read_mode);
		idelib_controller_reset_irq_counter(ide); /* IRQ will fire after command completion */
		idelib_controller_atapi_write_command(ide,buf,12); /* write 12-byte ATAPI command data */
		if (ide->flags.io_irq_enable) { /* NOW we wait for the IRQ */
			do_ide_controller_user_wait_irq(ide,1);
			idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
		}
		do_ide_controller_user_wait_busy_controller(ide);
		do_ide_controller_user_wait_drive_ready(ide);
		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);

		if (!idelib_controller_is_error(ide)) { /* OK. success. now read the data */
			memset(cdrom_sector,0,tlen);
			do_ide_controller_user_wait_drive_drq(ide);
			idelib_controller_update_atapi_state(ide); /* having completed the command, read ATAPI state again */
			idelib_controller_update_atapi_drq(ide); /* also need to read back the DRQ (data) length the drive has chosen */
			do_warn_if_atapi_not_in_data_input_state(ide); /* sector count register should signal we're in the completed stage (command/data=0 input/output=1) */
			if (idelib_controller_read_atapi_drq(ide) != tlen) {
				/* we're asking for one sector (2048) bytes, the drive should return that, if not, something's wrong.
				 * even cheap POS drives in old laptops will at least always return 2048! */
				sprintf(tmp,"Warning: ATAPI device returned DRQ=%u (expected %u)",
					idelib_controller_read_atapi_drq(ide),tlen);
				vga_msg_box_create(&vgabox,tmp,0,0);
				wait_for_enter_or_escape();
				vga_msg_box_destroy(&vgabox);
			}
			idelib_read_pio_general(cdrom_sector,tlen,ide,IDELIB_PIO_WIDTH_DEFAULT);
			if (ide->flags.io_irq_enable) { /* NOW we wait for another IRQ (completion) */
				do_ide_controller_user_wait_irq(ide,1);
				idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
			}
			do_ide_controller_user_wait_busy_controller(ide);
			do_ide_controller_user_wait_drive_ready(ide);
			idelib_controller_update_atapi_state(ide); /* having completed the command, read ATAPI state again */
			do_warn_if_atapi_not_in_complete_state(ide); /* sector count register should signal we're in the completed stage (command/data=1 input/output=1) */

			/* ---- draw contents on the screen ---- */
			vga_write_color(0x0E); vga_clear();

			vga_moveto(0,0);
			vga_write("Contents of CD-ROM sector 16 (2048) bytes");

			vga_moveto(0,2);
			vga_write_color(0x08);
			vga_write("BYTE ");

			vga_moveto(5,2);
			for (x=0;x < 16;x++) {
				sprintf(tmp,"+%X ",x);
				vga_write(tmp);
			}

			vga_moveto(5+(16*3)+1,2);
			for (x=0;x < 16;x++) {
				sprintf(tmp,"%X",x);
				vga_write(tmp);
			}

			for (i=0;i < 8;i++) { /* 16x16x8 = 2^(4+4+3) = 2^11 = 2048 */
				for (y=0;y < 16;y++) {
					vga_moveto(0,y+3);
					vga_write_color(0x08);
					sprintf(tmp,"%04X ",(i*256)+(y*16));
					vga_write(tmp);
				}

				for (y=0;y < 16;y++) {
					vga_moveto(5,y+3);
					vga_write_color(0x0F);
					for (x=0;x < 16;x++) {
						sprintf(tmp,"%02X ",cdrom_sector[(i*256)+(y*16)+x]);
						vga_write(tmp);
					}

					vga_moveto(5+(16*3)+1,y+3);
					vga_write_color(0x0E);
					for (x=0;x < 16;x++) {
						vga_writec(sanitizechar(cdrom_sector[(i*256)+(y*16)+x]));
					}
				}

				if (wait_for_enter_or_escape() == 27)
					break; /* allow user to exit early by hitting ESC */
			}
		}
		else {
			idelib_controller_update_atapi_state(ide); /* having completed the command, read ATAPI state again */
			do_warn_if_atapi_not_in_complete_state(ide); /* sector count register should signal we're in the completed stage (command/data=1 input/output=1) */
		}
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

static const char *drive_main_pio_mode_strings[] = {
	"16-bit PIO (standard)",		/* 0 */
	"32-bit PIO",
	"32-bit PIO (VLB sync)",
	"Autodetect"
};

void do_drive_pio_mode(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_main_pio_mode_strings);

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

	/* match selection to PIO mode */
	if (ide->pio_width == 16)
		select = 0;
	else if (ide->pio_width == 32)
		select = 1;
	else if (ide->pio_width == 33)
		select = 2;

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
			vga_write(" << PIO mode");
			if (ide->pio_width == 33)
				vga_write(" [32-bit VLB]");
			else if (ide->pio_width == 32)
				vga_write(" [32-bit]");
			else
				vga_write(" [16-bit]");
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
			unsigned char doexit = 0;

			if (select == -1)
				break;

			switch (select) {
				case 0: /* 16-bit */
					doexit = 1;
					ide->pio_width = 16;
					break;
				case 1: /* 32-bit */
					if (confirm_pio32_warning(ide)) {
						ide->pio_width = 32;
						doexit = 1;
					}
					break;
				case 2: /* 32-bit VLB */
					if (confirm_pio32_warning(ide)) {
						ide->pio_width = 33;
						doexit = 1;
					}
					break;
				case 3: /* Autodetect */
					redraw = backredraw = 1;
					ide_pio_autodetect(ide,which);
					if (ide->pio_width == 16)
						select = 0;
					else if (ide->pio_width == 32)
						select = 1;
					else if (ide->pio_width == 33)
						select = 2;
					break;
			};

			if (doexit)
				break;
			else
				redraw = 1;
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

static const char *drive_main_menustrings[] = {
	"Show IDE register taskfile",		/* 0 */
	"Identify (ATA)",
	"Identify packet (ATAPI)",
	"Power states >>",
	"CD-ROM eject/load >>",
	"PIO mode >>",				/* 5 */
	"Read HDD tests >>",
	"Read verify HDD tests >>",
	"Write HDD tests >>",
	"No-op",
	"Tweaks and adjustments >>"		/* 10 */
};

void do_ide_controller_drive(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_main_menustrings);

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

			/* update a string or two: PIO mode */
			if (ide->pio_width == 33)
				drive_main_menustrings[5] = "PIO mode (currently: 32-bit VLB) >>";
			else if (ide->pio_width == 32)
				drive_main_menustrings[5] = "PIO mode (currently: 32-bit) >>";
			else
				drive_main_menustrings[5] = "PIO mode (currently: 16-bit) >>";

			vga_moveto(mbox.ofsx,mbox.ofsy - 2);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE controller main menu");
			while (vga_pos_x < (mbox.width+mbox.ofsx) && vga_pos_x != 0) vga_writec(' ');

			menuboxbound_redraw(&mbox,select);

#if 0
			else if (select == 7) { /* ATAPI read CD-ROM */
				do_drive_read_one_sector_test(ide,which);
				redraw = backredraw = 1;
			}
#endif
#if 0
			vga_moveto(ofsx,y++);
			vga_write_color((select == 8) ? 0x70 : 0x0F);
			vga_write("Read[2] CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 11) ? 0x70 : 0x0F);
			vga_write("Read2x CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 12) ? 0x70 : 0x0F);
			vga_write("Read15x CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 13) ? 0x70 : 0x0F);
			vga_write("CD-ROM READ");
			if (read_mode == 12) vga_write("(12)");
			else vga_write("(10)");
			vga_write(" (chg)");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			y = 7;
			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 14) ? 0x70 : 0x0F);
			vga_write("Set Multiple Mode test");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 15) ? 0x70 : 0x0F);
			vga_write("Set Multiple Mode >>");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 16) ? 0x70 : 0x0F);
			vga_write("Media Status Notify >>");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 17) ? 0x70 : 0x0F);
			vga_write("Media Eject");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 18) ? 0x70 : 0x0F);
			vga_write("Media Lock");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 19) ? 0x70 : 0x0F);
			vga_write("Media Unlock");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 21) ? 0x70 : 0x0F);
			vga_write("CD-ROM INQUIRY");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 22) ? 0x70 : 0x0F);
			vga_write("CD-ROM READ TOC");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 24) ? 0x70 : 0x0F);
			vga_write("NOP test");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');
#endif
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
				case 1: /*Identify*/
				case 2: /*Identify packet*/
					do_drive_identify_device_test(ide,which,select == 2 ? 0xA1/*Identify packet*/ : 0xEC/*Identify*/);
					redraw = backredraw = 1;
					break;
				case 3: /* power states */
					do_drive_power_states_test(ide,which);
					redraw = backredraw = 1;
					break;
				case 4: /* CD-ROM start/stop/eject/load */
					do_drive_cdrom_startstop_test(ide,which);
					redraw = backredraw = 1;
					break;
				case 5: /* PIO mode */
					do_drive_pio_mode(ide,which);
					redraw = backredraw = 1;
					break;
				case 6: /* Read HDD test */
					do_ide_controller_drive_rw_test(ide,which,0);
					redraw = backredraw = 1;
					break;
				case 7: /* Read verify */
					do_ide_controller_drive_readverify_test(ide,which);
					redraw = backredraw = 1;
					break;
				case 8: /* Write HDD test */
					do_ide_controller_drive_rw_test(ide,which,1);
					redraw = backredraw = 1;
					break;
				case 9: /* NOP */
					do_ide_controller_drive_nop_test(ide,which);
					break;
				case 10: /* Tweaks and adjustments */
					do_drive_tweaks_and_adjustments(ide,which);
					redraw = backredraw = 1;
					break;
			};
#if 0 /* PORTME */
			else if (select == 12 || select == 15 || select == 16) { /* ATAPI read CD-ROM */
				unsigned long drqlen = 0,drqrem = 0;
				unsigned long tlen = 2048; /* one sector */
				unsigned long origlen = 0;
				unsigned long sector = 0;
				unsigned int stop = 0;
				unsigned long tlen_sect = 1;

				while (!stop) {
					if (select == 15) {
						tlen = 2048 * 2; /* 4KB */
						tlen_sect = 2;
					}
					else if (select == 16) {
						tlen = 2048 * 15; /* 30KB */
						tlen_sect = 15;
					}
					else {
						tlen = 2048;
						tlen_sect = 1;
					}

					/* debugging hack: all reference sites like OSDev describe how to do PACKET read
					   commands to a CD-ROM drive in an ideal situation: transfer length is exactly
					   the number of sectors you asked for. But real-world testing tells me there are
					   problems with that.

					   Let's list the 3 common 'types' of drive I've seen so far in my testing this software:

					- Most CD-ROM drives will carry out your multisector transfer as one whole block. So
					  when you read back the count register at the beginning, you will usually get back
					  the total transfer length you put in (assuming you put in exactly (2048 * sectors)
					  and your read operation also requests that many sectors). This is made possible by
					  the drive having enough buffer to read in the data, then send it to the host in one
					  go. Most drives except cheap pieces of shit fall into the category, and, if you choose
					  to write your own IDE implementation, are the best drives to start with.

					- Some CD-ROM drives will carry out multisector transfers just fine, BUT will require
					  that we break the IDE data transfer into 2048-byte blocks (meaning: a cheap CD-ROM
					  controller that buffers only one sector at a time, typically found in laptops).
					  If you were to follow OSDev's wiki sample code on these drives, you would incorrectly
					  assume the returned data was only 2048 bytes long, and when your code went on to
					  issue another command the drive would (rightly) abort it and signal an error.

					  Seen on: Fujitsu laptop (2005-ish), internal DVD-ROM drive always wants 2048-byte DRQ block transfers
						   Toshiba laptop (1997-ish), internal CD-ROM drive always wants 2048-byte DRQ block transfers

					- Some CD-ROM drives try to be "smart" about IDE bus utilization, and will dynamically
					  change the DRQ block transfer length DURING the overall IDE data transfer (as seen
					  on some finicky 2002-2004-ish DVD-ROM drives in my test lab). On these drives, you
					  MUST check the "interrupt reason" register (base+2) to make sure the drive is expecting
					  data transfer, and THEN read the count from LBA bits 23:8, and then transfer exactly
					  that much data. The drive can and will change the DRQ blocksize for each iteration, so
					  be ready for it.

					  Seen on: Test desktop PC with Pioneer DVD-ROM (model 115) attached to IDE controller:
					              The drive varies the DRQ block size in 2048-byte increments between 2048 and your requested
					              block size. The firmware appears to do this based on the current motor speed and on the
					              speed of the last data transfer. On my test desktop machine, this led to fluctuating DRQ
					              block transfers that started at 2048 (as the drive spins up) and then stayed in the
						      12288 to 18432 range. Prior to checking status and block size per DRQ transfer, this
						      code did not work properly with the drive.

					So if you plan on writing your own IDE driver, don't be naive in your code. Keep the
					comments written here in mind, because that is the kind of stuff your code will have
					to deal with */
					if ((tlen+2048) < sizeof(cdrom_sector))
						tlen += 36;

					assert(tlen <= sizeof(cdrom_sector));

					if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
						do_ide_controller_user_wait_drive_ready(ide) == 0) {
						sprintf(tmp,"Reading sector %lu (%u sectors rmode=%u last_drqlen=%lu)           ",sector,tlen_sect,read_mode,drqlen);
						vga_moveto(0,0);
						vga_write_color(0x0E);
						vga_write(tmp);

						if (kbhit()) {
							c = getch();
							if (c == 0) c = getch() << 8;
							if (c == 27) {
								stop = 1;
								break;
							}
						}

						y = 0;
						outp(ide->base_io+1,0x04); /* no DMA, xfer to host */
						outp(ide->base_io+2,0x00);
						outp(ide->base_io+3,0);
						outp(ide->base_io+4,tlen);
						outp(ide->base_io+5,tlen >> 8UL);
						outp(ide->base_io+7,0xA0); /* <- ATAPI PACKET */
						/* NTS: IRQ will NOT fire here */
						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
						x = inp(ide->base_io+7); /* what's the status? */

						if (!(x&1)) { /* if no error, read result from count register */
							y = inp(ide->base_io+2);
							if ((y&3) == 1) { /* input/output == 0, command/data == 1 */
								uint8_t buf[12] = {0x00,0x00,0x00,0x00,
									0x00,0x00,0x00,0x00,
									0x00,0x00,0x00,0x00};

								if (read_mode == 12) {
									/* command: READ(12) */
									buf[0] = 0xA8;

									/* fill in the Logical Block Address */
									buf[2] = sector >> 24;
									buf[3] = sector >> 16;
									buf[4] = sector >> 8;
									buf[5] = sector;

									buf[6] = tlen_sect >> 24UL;
									buf[7] = tlen_sect >> 16UL;
									buf[8] = tlen_sect >> 8UL;
									buf[9] = tlen_sect;
								}
								else {
									/* command: READ(10) */
									buf[0] = 0x28;

									/* fill in the Logical Block Address */
									buf[2] = sector >> 24;
									buf[3] = sector >> 16;
									buf[4] = sector >> 8;
									buf[5] = sector;

									buf[7] = tlen_sect >> 8;
									buf[8] = tlen_sect;
								}

								ide->irq_fired = 0;
								for (i=0;i < 6;i++)
									outpw(ide->base_io+0,((uint16_t*)buf)[i]);

								if (ide->flags.io_irq_enable)
									do_ide_controller_user_wait_irq(ide,1);

								if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
								if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
								x = inp(ide->base_io+7); /* what's the status? */
								if (!(x&1)) {
									y = inp(ide->base_io+2);
									if (!(y&1)) { /* command/data == 0 */
										unsigned long xo,xl,total=0;

										/* the ATAPI device will modify the mid/high LBA registers
										 * to tell you how much to transfer per DRQ.

										   NTS: Some websites like OSDev state or imply this is the
											actual "transfer length". They're wrong. It seems to
											be closer to the ATA spec's wording that it is the
											"maximum byte count to be transferred in any DRQ
											assertion for PIO transfers". In other words, you
											read the max byte count you asked for in blocks of
											this size.

											This is important to know for several reasons, one
											of which is that some (especially older) drives will
											often return a byte count of 2048 here to effectively
											state that they're cheap controllers and do not have
											much buffering. If you were to blindly take that as
											the total count, you would read too little data, and
											then confuse the drive sending another command when
											the drive is waiting for you to finish reading, who
											will then return an error and abort the command.

										   NTS: Drives do not return partial blocks. The byte count
											returned is all or nothing.

										   NTS: Some DVD-ROM drives in my collection appear to return
											fluctuating values, often not a multiple of what I asked
											for. I get the impression that from these drives I am
											to re-read the counter value per block, right?

											Buuuuuut.... a Toshiba laptop I own seems to signal error
											on the last DRQ block if this loop reads the counter,
											so... I dunno */
										xo = 0;
										drqlen = 0;
										drqrem = tlen;
										origlen = tlen;
										while (drqrem > 0UL) {
											if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
											if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;

											/* OK, is the device still expecting data transfer? */
											y = inp(ide->base_io+2);
											if ((y&3) != 2) break;

											/* wait for DRQ or for error */
											c = do_ide_controller_user_wait_drive_drq(ide);
											if (c == -2 || c == -1) {
												vga_msg_box_create(&vgabox,"DRQ error",0,0);
												stop = 1;
												break;
											}

											drqlen = ((unsigned long)inp(ide->base_io+4)) +
												(((unsigned long)inp(ide->base_io+5)) << 8UL);
											if (drqlen > sizeof(cdrom_sector))
												drqlen = sizeof(cdrom_sector);
											if (drqrem < drqlen)
												break;

											vga_moveto(0,1);
											sprintf(tmp,"DRQ=%lu REM=%lu          ",drqlen,drqrem);
											vga_write(tmp);

											xl = drqlen;
											memset(cdrom_sector+xo,0,xl);
											/* NTS: wait_drive_drq() by design will return -2 if the drive
												sets the error bit, whether or not DRQ is asserted */
												/* suck in the data and display it */
											if (ide->pio_width >= 32) {
												if (ide->pio_width == 33) ide_vlb_sync32_pio(ide);

												/* suck in the data and display it */
												for (i=0;i < ((xl+3)>>2U);i++)
													((uint32_t*)cdrom_sector)[(xo>>2UL)+i] = inpd(ide->base_io+0);
											}
											else {
												for (i=0;i < ((xl+1U)>>1U);i++)
													((uint16_t*)cdrom_sector)[(xo>>1UL)+i] = inpw(ide->base_io+0);
											}

											xo += xl;
											total += xl;
											drqrem -= xl;
										}
										tlen = total;
										tlen_sect = total >> 11UL;

										/* wait for controller */
										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										y = 1;
										x = inp(ide->base_io+7); /* what's the status? */
										if (!(x&1)) {
										}
										else {
											stop = 1;
											sprintf(tmp,"Device rejected with error %02X after read (got %lu of %lu)",x,total,tlen,origlen);
											vga_msg_box_create(&vgabox,tmp,0,0);
										}
									}
									else {
										stop = 1;
										sprintf(tmp,"Protocol error, PACKET IR=%02x [2]",y);
										vga_msg_box_create(&vgabox,tmp,0,0);
									}
								}
								else {
									stop = 1;
									sprintf(tmp,"Device rejected with error %02X [2]",x);
									vga_msg_box_create(&vgabox,tmp,0,0);
								}
								if (stop) {
									do {
										c = getch();
										if (c == 0) c = getch() << 8;
									} while (!(c == 13 || c == 27));
									vga_msg_box_destroy(&vgabox);
									if (c == 13) stop = 0;
								}
								else if (y) {
									for (i=0;i < (tlen>>1U);i++)
										vga_alpha_ram[i+(vga_width*2)] = ((uint16_t*)cdrom_sector)[i];
								}
							}
							else {
								stop = 1;
								sprintf(tmp,"Protocol err, PACKET IR=%02X",y);
								vga_msg_box_create(&vgabox,tmp,0,0);
								do {
									c = getch();
									if (c == 0) c = getch() << 8;
								} while (!(c == 13 || c == 27));
								vga_msg_box_destroy(&vgabox);
							}
						}
						else {
							stop = 1;
							sprintf(tmp,"Device rejected with error %02X",x);
							vga_msg_box_create(&vgabox,tmp,0,0);
							do {
								c = getch();
								if (c == 0) c = getch() << 8;
							} while (!(c == 13 || c == 27));
							vga_msg_box_destroy(&vgabox);
						}
					}
					else {
						stop = 1;
					}

					if (!stop) sector += tlen_sect;
					redraw = backredraw = 1;
				}
			}
			else if (select == 18) {
				vga_write_color(0x0E);
				vga_clear();
				vga_moveto(0,0);
				vga_write("Your drive supports block sizes:\n");
				vga_write_color(0x0F);

				for (i=1;i <= 256;i++) {
					if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
						do_ide_controller_user_wait_drive_ready(ide) == 0) {
						ide->irq_fired = 0;
						outp(ide->base_io+1,0x00);
						outp(ide->base_io+2,i&0xFF); /* Set Multiple block size */
						outp(ide->base_io+3,0);
						outp(ide->base_io+4,0);
						outp(ide->base_io+5,0);
						outp(ide->base_io+7,0xC6); /* <- SET MULTIPLE */

						if (ide->flags.io_irq_enable)
							do_ide_controller_user_wait_irq(ide,1);

						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
						x = inp(ide->base_io+7); /* what's the status? */
						if (!(x&1)) {
							sprintf(tmp,"%u ",i&0xFF);
							vga_write(tmp);

							/* confirm by using IDENTIFY */
							if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
								do_ide_controller_user_wait_drive_ready(ide) == 0) {
								ide->irq_fired = 0;
								outp(ide->base_io+7,0xEC); /* <- identify device */
								if (ide->flags.io_irq_enable)
									do_ide_controller_user_wait_irq(ide,1);

								if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
								if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
								x = inp(ide->base_io+7); /* what's the status? */
								if (!(x&1)) { /* if no error, read result from count register */
									if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
										/* wait for Data Request */
										uint16_t info[256];
										unsigned int j;

										for (j=0;j < 256;j++)
											info[j] = inpw(ide->base_io+0); /* read 16-bit word from data port, PIO */

										/* the 59th word (bits 7:0) reflect the last Set Multiple command */
										/* the 47th word (bits 7:0) reflect the larget value supported */
										if ((info[59]&0xFF) != (i&0xFF)) {
											sprintf(tmp," [NOT REFLECTED w59=%04x]",info[59]);
											vga_write(tmp);
										}
										else if ((info[47]&0xFF) < (i&0xFF)) {
											sprintf(tmp," [BEYOND MAX w47=%04x]",info[47]);
										}
									}
									else {
										break;
									}
								}
							}
						}
						else if (x == 256 || x == 1) {
							sprintf(tmp,"[should support %u!] ",i&0xFF);
							vga_write(tmp);
						}
					}
					else {
						break;
					}
				}

				vga_write("\nWrite complete");

				do {
					c = getch();
					if (c == 0) c = getch() << 8;
				} while (!(c == 13 || c == 27));

				redraw = backredraw = 1;
			}
			else if (select == 19) {
				unsigned int val=0,re=1,x=0,ok=0;

				vga_write_color(0x0E);
				vga_clear();

				do {
					if (re) {
						vga_write_color(0x0E);
						vga_clear();
						vga_moveto(0,0);
						vga_write("Enter a block size (0..255): ");
						vga_write_color(0x0F);
						val = 0;
						re = 0;
					}

					c = getch();
					if (c == 0) c = getch() << 8;

					if (c == 13) {
						if (val < 256) {
							if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
								do_ide_controller_user_wait_drive_ready(ide) == 0) {
								ide->irq_fired = 0;
								outp(ide->base_io+1,0x00);
								outp(ide->base_io+2,val&0xFF); /* Set Multiple block size */
								outp(ide->base_io+3,0);
								outp(ide->base_io+4,0);
								outp(ide->base_io+5,0);
								outp(ide->base_io+7,0xC6); /* <- SET MULTIPLE */

								if (ide->flags.io_irq_enable)
									do_ide_controller_user_wait_irq(ide,1);

								if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
								if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
								x = inp(ide->base_io+7); /* what's the status? */
								if (!(x&1)) {
									/* confirm by using IDENTIFY */
									if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
										do_ide_controller_user_wait_drive_ready(ide) == 0) {
										ide->irq_fired = 0;
										outp(ide->base_io+7,0xEC); /* <- identify device */
										if (ide->flags.io_irq_enable)
											do_ide_controller_user_wait_irq(ide,1);

										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
										if (!(x&1)) { /* if no error, read result from count register */
											if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
												/* wait for Data Request */
												uint16_t info[256];
												unsigned int j;

												for (j=0;j < 256;j++)
													info[j] = inpw(ide->base_io+0);

												/* the 59th word (bits 7:0) reflect the last Set Multiple command */
												/* the 47th word (bits 7:0) reflect the larget value supported */
												if ((info[59]&0xFF) == (val&0xFF)) {
													/* SUCCESS */
													ok = 1;
													vga_msg_box_create(&vgabox,"Success!",0,0);
													do {
														c = getch();
														if (c == 0) c = getch() << 8;
													} while (!(c == 13 || c == 27));
													vga_msg_box_destroy(&vgabox);
													break;
												}
											}
										}
									}
								}
							}

							if (!ok) {
								/* FAILED */
								vga_msg_box_create(&vgabox,"Failed to set block size",0,0);
								do {
									c = getch();
									if (c == 0) c = getch() << 8;
								} while (!(c == 13 || c == 27));
								vga_msg_box_destroy(&vgabox);

								val = x = 0;
								re = 1;
							}
						}
						else {
							val = x = 0;
							re = 1;
						}
					}
					else if (c == 27) {
						break;
					}
					else if (isdigit(c)) {
						if (x < 3) {
							vga_moveto(40+x,0);
							vga_writec(c);
							val = (val * 10U) + ((unsigned int)(c - '0'));
							x++;
						}
					}
					else if (c == 8) {
						if (x > 0) {
							val /= 10U;
							x--;
							vga_moveto(40+x,0);
							vga_writec(' ');
						}
					}
				} while (1);

				redraw = backredraw = 1;
			}
#endif
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

static void (interrupt *my_ide_old_irq)() = NULL;
static struct ide_controller *my_ide_irq_ide = NULL;
static unsigned long ide_irq_counter = 0;
static int my_ide_irq_number = -1;

static void interrupt my_ide_irq() {
	int i;

	/* we CANNOT use sprintf() here. sprintf() doesn't work to well from within an interrupt handler,
	 * and can cause crashes in 16-bit realmode builds. */
	i = vga_width*(vga_height-1);
	vga_alpha_ram[i++] = 0x1F00 | 'I';
	vga_alpha_ram[i++] = 0x1F00 | 'R';
	vga_alpha_ram[i++] = 0x1F00 | 'Q';
	vga_alpha_ram[i++] = 0x1F00 | ':';
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter / 100000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /  10000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /   1000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /    100UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /     10UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /       1L) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ' ';

	ide_irq_counter++;
	if (my_ide_irq_ide != NULL) {
		my_ide_irq_ide->irq_fired++;

		/* ack PIC */
		if (my_ide_irq_ide->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
}

void do_ide_controller_hook_irq(struct ide_controller *ide) {
	if (my_ide_irq_number >= 0 || my_ide_old_irq != NULL || ide->irq < 0)
		return;

	/* let the IRQ know what IDE controller */
	my_ide_irq_ide = ide;

	/* enable on IDE controller */
	p8259_mask(ide->irq);
	idelib_enable_interrupt(ide,1);

	/* hook IRQ */
	my_ide_old_irq = _dos_getvect(irq2int(ide->irq));
	_dos_setvect(irq2int(ide->irq),my_ide_irq);
	my_ide_irq_number = ide->irq;

	/* enable at PIC */
	p8259_unmask(ide->irq);
}

void do_ide_controller_unhook_irq(struct ide_controller *ide) {
	if (my_ide_irq_number < 0 || my_ide_old_irq == NULL || ide->irq < 0)
		return;

	/* disable on IDE controller, then mask at PIC */
	idelib_enable_interrupt(ide,0);
	p8259_mask(ide->irq);

	/* restore the original vector */
	_dos_setvect(irq2int(ide->irq),my_ide_old_irq);
	my_ide_irq_number = -1;
	my_ide_old_irq = NULL;
}

void do_ide_controller_enable_irq(struct ide_controller *ide,unsigned char en) {
	if (!en || ide->irq < 0 || ide->irq != my_ide_irq_number)
		do_ide_controller_unhook_irq(ide);
	if (en && ide->irq >= 0)
		do_ide_controller_hook_irq(ide);
}

void do_ide_controller(struct ide_controller *ide) {
	struct vga_msg_box vgabox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	char redraw=1;
	int select=-1;
	int c;

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c < 0) return;

	/* if the IDE struct says to use interrupts, then do it */
	do_ide_controller_enable_irq(ide,ide->flags.io_irq_enable);

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

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Main menu");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');
			y++;

			vga_moveto(8,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("Host Reset");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("Tinker with Master device");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("Tinker with Slave device");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("Currently using ");
			vga_write(ide->flags.io_irq_enable ? "IRQ" : "polling");
			vga_write(", switch to ");
			vga_write((!ide->flags.io_irq_enable) ? "IRQ" : "polling");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1) {
				break;
			}
			else if (select == 0) { /* host reset */
				if (ide->alt_io != 0) {
					/* just in case the IDE controller demands it, wait for not-busy before banging
					 * the host reset bit. if the IDE controller is hung, the user can command us
					 * to continue regardless */
					c = do_ide_controller_user_wait_busy_controller(ide);
					if (c >= 0) { /* if not busy, or busy and user commands us to continue */
						vga_msg_box_create(&vgabox,"Host reset in progress",0,0);

						idelib_device_control_set_reset(ide,1);
						t8254_wait(t8254_us2ticks(1000000));
						idelib_device_control_set_reset(ide,0);

						vga_msg_box_destroy(&vgabox);

						/* now wait for not busy */
						c = do_ide_controller_user_wait_busy_controller(ide);
					}
				}
			}
			else if (select == 1) {
				do_ide_controller_drive(ide,0/*master*/);
				redraw = backredraw = 1;
			}
			else if (select == 2) {
				do_ide_controller_drive(ide,1/*slave*/);
				redraw = backredraw = 1;
			}
			else if (select == 3) {
				if (ide->irq >= 0)
					ide->flags.io_irq_enable = !ide->flags.io_irq_enable;
				else
					ide->flags.io_irq_enable = 0;

				do_ide_controller_enable_irq(ide,ide->flags.io_irq_enable);
				redraw = backredraw = 1;
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 3;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 3)
				select = -1;

			redraw = 1;
		}
	}

	do_ide_controller_enable_irq(ide,0);
	idelib_enable_interrupt(ide,1); /* NTS: Most BIOSes know to unmask the IRQ at the PIC, but there might be some
					        idiot BIOSes who don't clear the nIEN bit in the device control when
						executing INT 13h, so it's probably best to do it for them. */
}

void do_main_menu() {
	char redraw=1;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	struct ide_controller *ide;
	unsigned int x,y,i;
	int select=-1;
	int c;

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
			vga_write("        IDE controller test program");
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

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Exit program");
			y++;

			for (i=0;i < MAX_IDE_CONTROLLER;i++) {
				ide = idelib_get_controller(i);
				if (ide != NULL) {
					vga_moveto(8,y++);
					vga_write_color((select == (int)i) ? 0x70 : 0x0F);

					sprintf(tmp,"Controller @ %04X",ide->base_io);
					vga_write(tmp);

					if (ide->alt_io != 0) {
						sprintf(tmp," alt %04X",ide->alt_io);
						vga_write(tmp);
					}

					if (ide->irq >= 0) {
						sprintf(tmp," IRQ %2d",ide->irq);
						vga_write(tmp);
					}
				}
			}
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1) {
				break;
			}
			else if (select >= 0 && select < MAX_IDE_CONTROLLER) {
				ide = idelib_get_controller(select);
				if (ide != NULL) do_ide_controller(ide);
				backredraw = redraw = 1;
			}
		}
		else if (c == 0x4800) {
			if (select <= -1)
				select = MAX_IDE_CONTROLLER - 1;
			else
				select--;

			while (select >= 0 && idelib_get_controller(select) == NULL)
				select--;

			redraw = 1;
		}
		else if (c == 0x5000) {
			select++;
			while (select >= 0 && select < MAX_IDE_CONTROLLER && idelib_get_controller(select) == NULL)
				select++;
			if (select >= (MAX_IDE_CONTROLLER-1))
				select = -1;

			redraw = 1;
		}
	}
}

int main(int argc,char **argv) {
	struct ide_controller *idectrl;
	struct ide_controller *newide;
	int i;

	printf("IDE ATA/ATAPI test program\n");

	/* we take a GUI-based approach (kind of) */
	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
	/* the IDE code has some timing requirements and we'll use the 8254 to do it */
	/* I bet that by the time motherboard manufacturers stop implementing the 8254 the IDE port will be long gone too */
	if (!probe_8254()) {
		printf("8254 chip not detected\n");
		return 1;
	}
	/* interrupt controller */
	if (!probe_8259()) {
		printf("8259 chip not detected\n");
		return 1;
	}
	if (!init_idelib()) {
		printf("Cannot init IDE lib\n");
		return 1;
	}
	if (pci_probe(-1/*default preference*/) != PCI_CFG_NONE) {
		printf("PCI bus detected.\n");
	}

	printf("Probing standard IDE ports...\n");
	for (i=0;(idectrl = (struct ide_controller*)idelib_get_standard_isa_port(i)) != NULL;i++) {
		printf("   %3X/%3X IRQ %d: ",idectrl->base_io,idectrl->alt_io,idectrl->irq); fflush(stdout);

		if ((newide = idelib_probe(idectrl)) != NULL) {
			printf("FOUND: alt=%X irq=%d\n",newide->alt_io,newide->irq);
		}
		else {
			printf("\x0D                             \x0D"); fflush(stdout);
		}
	}

	if (int10_getmode() != 3) {
		int10_setmode(3);
		update_state_from_vga();
	}

	do_main_menu();
	free_idelib();
	return 0;
}

