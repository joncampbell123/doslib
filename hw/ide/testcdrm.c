
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
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

static void do_cdrom_drive_read_test(struct ide_controller *ide,unsigned char which,unsigned char continuous) {
	uint16_t drq_log[((unsigned long)sizeof(cdrom_sector))/2048UL];
	unsigned long sector = 16; /* read the ISO 9660 table of contents */
	unsigned long tlen = 2048; /* one sector */
	unsigned long tlen_sect = 1;
	unsigned char user_esc = 0;
	struct vga_msg_box vgabox;
	unsigned int drq_log_ent;
	unsigned int cleared=0;
	uint8_t buf[12] = {0};
	unsigned int x,y,i;
	int c;

	sector = prompt_cdrom_sector_number();
	if (sector == ~0UL)
		return;
	tlen_sect = prompt_cdrom_sector_count();
	if (tlen_sect == 0UL || tlen_sect == ~0UL)
		return;
	if (tlen_sect > ((unsigned long)sizeof(cdrom_sector) / 2048UL))
		tlen_sect = ((unsigned long)sizeof(cdrom_sector) / 2048UL);
	if (tlen_sect > ((65536UL/2048UL)-1UL)) /* don't try ATAPI requests 64KB or larger, the size field is 16-bit wide */
		tlen_sect = ((65536UL/2048UL)-1UL);
	tlen = tlen_sect * 2048UL;

again:	/* jump point: send execution back here for another sector */
	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;
	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
	if (idelib_controller_atapi_prepare_packet_command(ide,/*xfer=to host no DMA*/0x04,/*byte count=*/tlen) < 0) /* fill out taskfile with command */
		return;
	if (idelib_controller_apply_taskfile(ide,0xBE/*base_io+1-5&7*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/) < 0) /* also writes command */
		return;

	/* NTS: Despite OSDev ATAPI advice, IRQ doesn't seem to fire at this stage, we must poll wait */
	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_update_atapi_state(ide);
	if (!(ide->last_status&1)) { /* if no error, read result from count register */
		do_warn_if_atapi_not_in_command_state(ide); /* sector count register should signal we're in the command stage */

		do_construct_atapi_scsi_mmc_read(buf,sector,tlen_sect,cdrom_read_mode);
		idelib_controller_reset_irq_counter(ide); /* IRQ will fire after command completion */
		idelib_controller_atapi_write_command(ide,buf,12); /* write 12-byte ATAPI command data */
		if (ide->flags.io_irq_enable) { /* NOW we wait for the IRQ */
			if (do_ide_controller_user_wait_irq(ide,1) < 0)
				return;
			idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
		}

		if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
			return;

		if (!idelib_controller_is_error(ide)) { /* OK. success. now read the data */
			unsigned int ret_len = 0,drq_len,ey;

			/* NTS: I hate to break it to newbie IDE programmers, but reading back the sector isn't
			 *      quite the simple "read N bytes" from the drive. In reality, you wait for the drive
			 *      to signal DRQ, and then read back the length of data it has available for you to
			 *      read by, then you read that amount, and if more data is due, then you wait for
			 *      another IRQ and DRQ signal.
			 *
			 *      In most cases, the DRQ returned by the drive is the same length you passed in,
			 *      but NOT ALWAYS. Many cheap laptop drives for example will only return "2048"
			 *      because they don't have a lot of buffer, and many DVD-ROM drives like to vary
			 *      the DRQ size per transfer for whatever reason, whether "dynamically" according
			 *      to CD-ROM spin speed or based on whatever data it's managed to read and buffer
			 *      so far.
			 *
			 *      On the positive side, it means that on an error, the transfer can abort early if
			 *      it needs to. */
			drq_log_ent = 0;
			memset(cdrom_sector,0,tlen);
			while (ret_len < tlen) {
				if (idelib_controller_is_error(ide)) {
					vga_msg_box_create(&vgabox,"Error",0,0);
					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
					break;
				}

				idelib_controller_update_atapi_state(ide); /* having completed the command, read ATAPI state again */
				idelib_controller_update_atapi_drq(ide); /* also need to read back the DRQ (data) length the drive has chosen */
				if (idelib_controller_atapi_complete_state(ide)) { /* if suddenly in complete state, exit out */
					do_warn_if_atapi_not_in_data_input_state(ide); /* sector count register should signal we're in the completed stage (command/data=0 input/output=1) */
					break;
				}

				if (do_ide_controller_user_wait_drive_drq(ide) < 0) {
					user_esc = 1;
					break;
				}
				idelib_controller_update_atapi_state(ide); /* having completed the command, read ATAPI state again */
				idelib_controller_update_atapi_drq(ide); /* also need to read back the DRQ (data) length the drive has chosen */
				do_warn_if_atapi_not_in_data_input_state(ide); /* sector count register should signal we're in the completed stage (command/data=0 input/output=1) */

				assert(drq_log_ent < (sizeof(drq_log)/sizeof(drq_log[0])));
				drq_len = idelib_controller_read_atapi_drq(ide);
				drq_log[drq_log_ent++] = drq_len;
				if (drq_len < 2048UL || (drq_len % 2048UL) != 0UL || (drq_len+ret_len) > tlen) {
					/* we're asking for one sector (2048) bytes, the drive should return that, if not, something's wrong.
					 * even cheap POS drives in old laptops will at least always return 2048! */
					sprintf(tmp,"Warning: ATAPI device returned unexpected DRQ=%u (%u+%u = %u)",
						drq_len,ret_len,tlen);
					vga_msg_box_create(&vgabox,tmp,0,0);
					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
					break;
				}

				/* OK. read it in */
				idelib_read_pio_general(cdrom_sector+ret_len,drq_len,ide,IDELIB_PIO_WIDTH_DEFAULT);
				if (ide->flags.io_irq_enable) { /* NOW we wait for another IRQ (completion) */
					if (do_ide_controller_user_wait_irq(ide,1) < 0) {
						user_esc = 1;
						break;
					}
					idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
				}

				if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0) {
					user_esc = 1;
					break;
				}

				ret_len += drq_len;
			}
			idelib_controller_update_atapi_state(ide); /* having completed the command, read ATAPI state again */
			do_warn_if_atapi_not_in_complete_state(ide); /* sector count register should signal we're in the completed stage (command/data=1 input/output=1) */

			/* ---- draw contents on the screen ---- */
			vga_write_color(0x0E);
			if (!cleared) {
				vga_clear();
				cleared = 1;
			}

			vga_moveto(0,0);
			vga_write("Contents of CD-ROM sector ");
			sprintf(tmp,"%lu-%lu",sector,sector+tlen_sect-1UL); vga_write(tmp);
			sprintf(tmp,"(%lu) bytes",(unsigned long)tlen); vga_write(tmp);

			ey = 3+16+3;
			vga_moveto(0,3+16+1);
			sprintf(tmp,"%u/%lu in %u DRQ transfers: ",ret_len,tlen,drq_log_ent);
			vga_write(tmp);
			for (x=0;x < drq_log_ent;x++) {
				int len = sprintf(tmp,"%u ",drq_log[x]);
				if ((vga_pos_x+len) > vga_width) vga_write("\n ");
				vga_write(tmp);
			}
			while (vga_pos_y <= ey) vga_write(" ");

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

			for (i=0;i < (tlen/256UL);i++) { /* 16x16x8 = 2^(4+4+3) = 2^11 = 2048 */
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

				if (continuous && !user_esc) {
					if (kbhit()) {
						c = getch();
						if (c == 0) c = getch() << 8;
					}
					break;
				}
				else {
					if ((c=wait_for_enter_or_escape()) == 27)
						break; /* allow user to exit early by hitting ESC */
				}
			}

			if (c != 27 && !user_esc) {
				/* if the user hit ENTER, then read another sector and display that too */
				sector += tlen_sect;
				goto again;
			}
		}
		else {
			common_ide_success_or_error_vga_msg_box(ide,&vgabox);
			wait_for_enter_or_escape();
			vga_msg_box_destroy(&vgabox);

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

static const char *drive_cdrom_reading_menustrings[] = {
	"Show IDE register taskfile",		/* 0 */
	"Read CD-ROM data sectors",
	"Read CD-ROM data continuously"
};

void do_drive_cdrom_reading(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_cdrom_reading_menustrings);

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
				case 1: /*Read sectors*/
				case 2: /*Read sectors continuously*/
					do_cdrom_drive_read_test(ide,which,/*continuous=*/(select==2));
					redraw = backredraw = 1;
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

