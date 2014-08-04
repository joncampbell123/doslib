/* THINGS TO DO:
 *
 *      - Modularize copy-pasta'd code such as:
 *         * The "if you value your data" warning
 *         * ATAPI packet commands
 *         * EVERYTHING---This code basically works on test hardware, now it's time to clean it up
 *           modularize and refactor.
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

static int read_mode = 12;
static int pio_width = 16;	/* 16: standard I/O   32: 32-bit I/O   33: 32-bit I/O with VLB "sync sequence" */
static int pio_width_warning = 1;
static unsigned char big_scary_write_test_warning = 1;

static char tmp[1024];
static uint16_t ide_info[256];
static unsigned char cdrom_sector[512U*63U];	/* ~32KB, enough for CD-ROM sector or 63 512-byte sectors */

static unsigned char sanitizechar(unsigned char c) {
	if (c < 32) return '.';
	return c;
}

/* construct ATAPI/SCSI-MMC READ command according to user's choice, either READ(10) or READ(12) */
static void do_construct_atapi_scsi_mmc_read(unsigned char *buf/*must be 12 bytes*/,uint32_t sector,uint32_t tlen_sect) {
	memset(buf,0,12);
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
}

static void common_ide_success_or_error_vga_msg_box(struct ide_controller *ide,struct vga_msg_box *vgabox) {
	if (!(ide->last_status&1)) {
		vga_msg_box_create(vgabox,"Success",0,0);
	}
	else {
		sprintf(tmp,"Device rejected with error %02X",ide->last_status);
		vga_msg_box_create(vgabox,tmp,0,0);
	}
}
	
static void common_failed_to_read_taskfile_vga_msg_box(struct vga_msg_box *vgabox) {
	vga_msg_box_create(vgabox,"Failed to read taskfile",0,0);
}

static int wait_for_enter_or_escape() {
	int c;

	do {
		c = getch();
		if (c == 0) c = getch() << 8;
	} while (!(c == 13 || c == 27));

	return c;
}

static void do_warn_if_atapi_not_in_command_state(struct ide_controller *ide) {
	struct vga_msg_box vgabox;

	if (!idelib_controller_atapi_command_state(ide)) {
		sprintf(tmp,"WARNING: ATAPI device not in command state as expected (state=%u)",idelib_controller_read_atapi_state(ide));
		vga_msg_box_create(&vgabox,tmp,0,0);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

static void do_warn_if_atapi_not_in_data_input_state(struct ide_controller *ide) {
	struct vga_msg_box vgabox;

	if (!idelib_controller_atapi_data_input_state(ide)) {
		sprintf(tmp,"WARNING: ATAPI device not in data input state as expected (state=%u)",idelib_controller_read_atapi_state(ide));
		vga_msg_box_create(&vgabox,tmp,0,0);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

static void do_warn_if_atapi_not_in_complete_state(struct ide_controller *ide) {
	struct vga_msg_box vgabox;

	if (!idelib_controller_atapi_complete_state(ide)) {
		sprintf(tmp,"WARNING: ATAPI device not in complete state as expected (state=%u)",idelib_controller_read_atapi_state(ide));
		vga_msg_box_create(&vgabox,tmp,0,0);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

/*-----------------------------------------------------------------*/

/* returns: -1 if user said to cancel
 *          0 if not busy
 *          1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_busy_controller(struct ide_controller *ide) {
	struct vga_msg_box vgabox;
	int ret = 0,c = 0;
	
	if (ide == NULL)
		return -1;

	/* use the alt status register if possible, else the base I/O.
	 * the alt status register is said not to clear pending interrupts */
	idelib_controller_update_status(ide);
	if (idelib_controller_is_busy(ide)) {
		unsigned long show_countdown = 0x1000UL;
		/* if the drive&controller is busy then show the dialog and wait for non-busy
		 * or until the user forces us to proceed */

		do {
			idelib_controller_update_status(ide);
			if (!idelib_controller_is_busy(ide)) break;

			/* if the drive&controller is busy then show the dialog and wait for non-busy
			 * or until the user forces us to proceed */
			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL)
					vga_msg_box_create(&vgabox,"IDE controller busy, waiting...\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);
			}


			if (kbhit()) {
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ret = -1;
					break;
				}
				else if (c == ' ') {
					ret = 1;
					break;
				}
			}
		} while (1);

		if (ret == 0 && c != 0) ungetch(c);
		if (show_countdown == 0UL)
			vga_msg_box_destroy(&vgabox);
	}

	return ret;
}

/* returns: -2 if an error happened
 *          -1 if user said to cancel
 *           0 if not busy
 *           1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_drive_drq(struct ide_controller *ide) {
	struct vga_msg_box vgabox;
	int ret = 0,c = 0;

	if (ide == NULL)
		return -1;

	/* NTS: We ignore the Drive Ready Bit, because logically, that bit reflects
	 *      when the drive is ready for another command. Obviously if we're waiting
	 *      for DATA related to a command, then the command is not complete!
	 *      I'm also assuming there are dumbshit IDE controller and hard drive
	 *      implementations dumb enough to return such confusing state. */
	/* use the alt status register if possible, else the base I/O.
	 * the alt status register is said not to clear pending interrupts */
	idelib_controller_update_status(ide);
	if (!idelib_controller_is_drq_ready(ide)) {
		unsigned long show_countdown = 0x1000UL;

		do {
			idelib_controller_update_status(ide);
			if (idelib_controller_is_drq_ready(ide))
				break;
			else if (idelib_controller_is_error(ide)) {
				ret = -2;
				break;
			}

			/* if the drive&controller is busy then show the dialog and wait for non-busy
			 * or until the user forces us to proceed */
			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL)
					vga_msg_box_create(&vgabox,"Waiting for Data Request from IDE device\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);
			}

			if (kbhit()) {
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ret = -1;
					break;
				}
				else if (c == ' ') {
					ret = 1;
					break;
				}
			}
		} while (1);

		if (ret == 0 && c != 0) ungetch(c);
		if (show_countdown == 0UL)
			vga_msg_box_destroy(&vgabox);
	}

	return ret;
}

/* returns: -1 if user said to cancel
 *          0 if not busy
 *          1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_irq(struct ide_controller *ide,uint16_t count) {
	struct vga_msg_box vgabox;
	int ret = 0,c = 0;

	if (ide == NULL)
		return -1;

	if (ide->irq_fired < count) {
		unsigned long show_countdown = 0x1000UL;

		do {
			if (ide->irq_fired >= count)
				break;

			/* if the drive&controller is busy then show the dialog and wait for non-busy
			 * or until the user forces us to proceed */
			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL)
					vga_msg_box_create(&vgabox,"Waiting for IDE IRQ\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);
			}

			if (kbhit()) {
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ret = -1;
					break;
				}
				else if (c == ' ') {
					ret = 1;
					break;
				}
			}
		} while (1);

		if (ret == 0 && c != 0) ungetch(c);
		if (show_countdown == 0UL)
			vga_msg_box_destroy(&vgabox);
	}

	return ret;
}

/* returns: -1 if user said to cancel
 *          0 if not busy
 *          1 if still busy, but user said to proceed */
int do_ide_controller_user_wait_drive_ready(struct ide_controller *ide) {
	struct vga_msg_box vgabox;
	int ret = 0,c;

	if (ide == NULL)
		return -1;

	/* use the alt status register if possible, else the base I/O.
	 * the alt status register is said not to clear pending interrupts */
	idelib_controller_update_status(ide);
	if (!idelib_controller_is_drive_ready(ide)) {
		unsigned long show_countdown = 0x1000UL;

		/* if the drive&controller is busy then show the dialog and wait for non-busy
		 * or until the user forces us to proceed */

		do {
			idelib_controller_update_status(ide);
			if (idelib_controller_is_drive_ready(ide))
				break;
			else if (idelib_controller_is_error(ide))
				break; /* this is possible too?? or is VirtualBox fucking with us when the CD-ROM drive is on Primary slave? */

			if (show_countdown > 0UL) {
				if (--show_countdown == 0UL)
					vga_msg_box_create(&vgabox,"IDE device not ready, waiting...\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);
			}

			if (kbhit()) {
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ret = -1;
					break;
				}
				else if (c == ' ') {
					ret = 1;
					break;
				}
			}
		} while (1);

		if (ret == 0 && c != 0) ungetch(c);
		if (show_countdown == 0UL)
			vga_msg_box_destroy(&vgabox);
	}

	return ret;
}

/* A warning to people who may write their own IDE code: If a drive
 * has been put to sleep, or put through a host reset, the "drive ready"
 * bit will NEVER come up. If you are naive, your code will loop forever
 * waiting for drive ready. To get drive ready to come up you have to
 * send it a NO-OP command, which it will reply with an error, but it
 * will then signal readiness.
 *
 * Now, in this case the Status register (0x1F7/0x3F6) will always
 * read back 0x00, but 0x00 is also what would be read back if no
 * such device existed. How do we tell the two scenarios apart?
 * Well it turns out that if the device is there, then anything you
 * read/write from ports 0x1F2-0x1F5 will read back the same value.
 * If the IDE device does not exist, then ports 0x1F2-0x1F5 will
 * always return 0x00 (or 0xFF?) no matter what we write.
 *
 * Once we know the device is there, we can then "trick" the device
 * into reporting readiness by issuing ATA NO-OP, which the device
 * will respond to with an error, but more importantly, it will set
 * the Device Ready bit. */
/* NOTE: The caller must have already gone through the process of writing the
 *       head & drive select and waiting for controller readiness. if the
 *       caller doesn't do that, this routine will probably end up running through
 *       the routine on the wrong IDE device */
int do_ide_controller_atapi_device_check_post_host_reset(struct ide_controller *ide) {
	idelib_controller_update_status(ide);
	if (idelib_controller_is_busy(ide)) return -1;			/* YOU'RE SUPPOSED TO WAIT FOR CONTROLLER NOT-BUSY! */
	if (idelib_controller_is_drive_ready(ide)) return 0;		/* Drive indicates readiness, nothing to do */

	if ((ide->last_status&0xC1) == 0) { /* <- typical post-reset state. VirtualBox/QEMU never raises Drive Ready bit */
		struct ide_taskfile *tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);

		/* OK: If I write things to 0x1F2-0x1F5 can I read them back? */
		tsk->sector_count = 0x55;
		tsk->lba0_3 = 0xAA;
		tsk->lba1_4 = 0x3F;

		idelib_controller_apply_taskfile(ide,0x1C/*regs 2-4*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear*/);
		tsk->sector_count = tsk->lba0_3 = tsk->lba1_4 = 0; /* DEBUG: just to be sure */
		idelib_controller_update_taskfile(ide,0x1C/*regs 2-4*/,0);

		if (tsk->sector_count != 0x55 || tsk->lba0_3 != 0xAA || tsk->lba1_4 != 0x3F)
			return -1;	/* Nope. IDE device is not there (also possibly the device is fucked up) */

		idelib_controller_update_status(ide);
	}
	if ((ide->last_status&0xC1) == 0) { /* <- if the first test did not trigger drive ready, then whack the command port with ATA NO-OP */
		idelib_controller_write_command(ide,0x00);
		t8254_wait(t8254_us2ticks(100000)); /* <- give it 100ms to respond */
		idelib_controller_update_status(ide);
	}
	if ((ide->last_status&0xC1) == 0) { /* <- if the NO-OP test didn't work, then forget it, I'm out of ideas */
		return -1;
	}

	idelib_controller_ack_irq(ide);
	return 0;
}

void do_ide_controller_drive_write_unc_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	char redraw=1;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	int c;

	vga_msg_box_create(&vgabox,"WARNING!!! WARNING!!! WARNING!!!\n"
		"\n"
		"This write test uses the WRITE UNCORRECTABLE command on\n"
		"your hard drive to write pseudo-uncorrectable sectors.\n"
		"Until the pseudo-uncorrectable sectors are written, reads\n"
		"from the sectors will show up as errors.\n"
		"\n"
		"Hit ESC to cancel, or ENTER if this is what you want"
		,0,0);
	do {
		c = getch();
		if (c == 0) c = getch() << 8;
	} while (!(c == 13 || c == 27));
	vga_msg_box_destroy(&vgabox);
	if (c == 27) return;

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* select the drive we want */
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);

	/* in case the IDE controller is busy for that time */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* it might be a CD-ROM drive, which in some cases might not raise the Drive Ready bit */
	do_ide_controller_atapi_device_check_post_host_reset(ide);

	/* wait for the drive to indicate readiness */
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c != 0) return;

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
			vga_write(" Uncorrectable write");
			vga_write(" test");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');

			vga_write_color(0xC);
			vga_write("WARNING: This code talks directly to your hard disk controller.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
			vga_write_color(0xC);
			vga_write("         If you value the data on your hard drive do not run this program.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			const int cols = 2;

			redraw = 0;

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE drive menu");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			y = 7;
			vga_moveto(8,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("Write Unc 1x ");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("Write Unc 4x ");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("Write Unc 63x ");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("Write Unc 255x ");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 4) ? 0x70 : 0x0F);
			vga_write("Write Unc 1024x ");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 5) ? 0x70 : 0x0F);
			vga_write("Write Unc 16384x ");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 6) ? 0x70 : 0x0F);
			vga_write("Write Unc 65536x ");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');
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
			else if (select >= 0 && select <= 6) {
				unsigned long long sector = 0;
				unsigned long tlen_sect = 1;
				unsigned char stop = 0;

				switch (select) {
					case 0:		tlen_sect = 1UL; break;
					case 1:		tlen_sect = 4UL; break;
					case 2:		tlen_sect = 63UL; break;
					case 3:		tlen_sect = 255UL; break;
					case 4:		tlen_sect = 1024UL; break;
					case 5:		tlen_sect = 16384UL; break;
					case 6:		tlen_sect = 65536UL; break;
				}

				while (!stop) {
					if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
						do_ide_controller_user_wait_drive_ready(ide) == 0) {
						ide->irq_fired = 0;

						sprintf(tmp,"WriteUnc sector %016llu (%016llX) %u sects",sector,sector,(unsigned int)tlen_sect);
						vga_moveto(0,0);
						vga_write_color(0x0E);
						vga_write(tmp);
						vga_write("           ");

						/* select drive */
						idelib_controller_drive_select(ide,which,/*head=*/0,IDELIB_DRIVE_SELECT_MODE_CHS);
						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;

						/* the command takes LBA and count LBA48-style */
						outp(ide->base_io+1,0x5A); /* <- pseudo-uncorrectable without logging */
						outp(ide->base_io+2,tlen_sect >> 8); /* number of sectors hi byte */
						outp(ide->base_io+3,sector >> 24ULL);
						outp(ide->base_io+4,sector >> 32ULL);
						outp(ide->base_io+5,sector >> 40ULL);

						outp(ide->base_io+1,0x5A); /* <- pseudo-uncorrectable without logging */
						outp(ide->base_io+2,tlen_sect); /* number of sectors lo byte */
						outp(ide->base_io+3,sector);
						outp(ide->base_io+4,sector >> 8ULL);
						outp(ide->base_io+5,sector >> 16ULL);

						outp(ide->base_io+7,0x45); /* <- write uncorrectable */
						if (ide->flags.io_irq_enable)
							do_ide_controller_user_wait_irq(ide,1);

						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
						x = inp(ide->base_io+7); /* what's the status? */
						if (!(x&1)) { /* if no error, read result from count register */
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
				}

				redraw = backredraw = 1;
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 6;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 6)
				select = -1;

			redraw = 1;
		}
	}
}

void do_ide_controller_drive_readverify_test(struct ide_controller *ide,unsigned char which) {
#if TARGET_MSDOS == 16 && defined(__COMPACT__)
#else
	struct vga_msg_box vgabox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y,i;
	char redraw=1;
	int select=-1;
	int c;

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* select the drive we want */
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);

	/* in case the IDE controller is busy for that time */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* it might be a CD-ROM drive, which in some cases might not raise the Drive Ready bit */
	do_ide_controller_atapi_device_check_post_host_reset(ide);

	/* wait for the drive to indicate readiness */
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c != 0) return;

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
			vga_write(" Read verify");
			vga_write(" test");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');

			vga_write_color(0xC);
			vga_write("WARNING: This code talks directly to your hard disk controller.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
			vga_write_color(0xC);
			vga_write("         If you value the data on your hard drive do not run this program.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			const int cols = 2;

			redraw = 0;

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE drive menu");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			y = 7;
			vga_moveto(8,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("LBA PIO ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO ");
			vga_write("READ (42h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO MS ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 4) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO TRK ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 5) ? 0x70 : 0x0F);
			vga_write("LBA PIO MS ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 6) ? 0x70 : 0x0F);
			vga_write("LBA PIO 63x ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 7) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO MS ");
			vga_write("READ (42h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 8) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO 63x ");
			vga_write("READ (42h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');
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
			else if (select == 0 || select == 1 || select == 2 || select == 3 || select == 4 || select == 5 || select == 6 || select == 7 || select == 8) {
				unsigned long long sector = 0;
				unsigned long track;
				unsigned int sectn,head;
				unsigned long tlen = 512; /* one sector */
				unsigned int stop = 0;
				unsigned long tlen_sect = 1;
				unsigned char cmd;
				unsigned int sect_per_block = 1; /* READ/WRITE MULTIPLE */
				unsigned int tracks=0,heads=0,sects=0;

				if (select == 0 || select == 3 || select == 4) {
					/* ask for C/H/S geometry or MULTIPLE block size */
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
								for (i=0;i < 256;i++)
									ide_info[i] = inpw(ide->base_io+0); /* read 16-bit word from data port, PIO */

								tracks = ide_info[1];
								heads = ide_info[3] & 0xFF;
								sects = ide_info[6] & 0xFF;

								sprintf(tmp,"C/H/S %u/%u/%u MULTBLK=%u",tracks,heads,sects,sect_per_block);
								vga_msg_box_create(&vgabox,tmp,0,0);
								do {
									c = getch();
									if (c == 0) c = getch() << 8;
								} while (!(c == 13 || c == 27));
								vga_msg_box_destroy(&vgabox);

								if (c == 27) {
									tracks = heads = sects = 0;
								}
							}
						}

						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
					}
				}

				if (sect_per_block == 0)
					sect_per_block = 1;

				if (select == 2 || select == 7 || select == 8) /* READ LBA48 */
					cmd = 0x42; /* FIXME */
				else
					cmd = 0x40;

				while (((tracks != 0 && heads != 0 && sects != 0) || select == 1 || select == 2 || select == 5 || select == 6 || select == 7 || select == 8) && !stop) {
					if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
						do_ide_controller_user_wait_drive_ready(ide) == 0) {
						if (select == 0 || select == 3 || select == 4) { /* CHS */
							sectn = 1 + ((unsigned long)sector % (unsigned long)sects);
							head = ((unsigned long)sector / (unsigned long)sects) % (unsigned long)heads;
							track = (unsigned long)sector / (unsigned long)sects / (unsigned long)heads;

							if (select == 3) { /* multisector read with CHS but don't cross track boundaries */
								tlen_sect = 7;
								if ((sectn+tlen_sect) > sects) tlen_sect = (sects - sectn) + 1;
								tlen = 512 * tlen_sect;
							}
							else if (select == 4) { /* read to end of track */
								tlen_sect = (sects - sectn) + 1;
								if (tlen_sect > sects) tlen_sect = sects;
								tlen = 512 * tlen_sect;
							}
						}
						else if (select == 5 || select == 7) {
							tlen_sect = 7;
							tlen = 512 * tlen_sect;
						}
						else if (select == 6 || select == 8) {
							tlen_sect = 63;
							tlen = 512 * tlen_sect;
						}

						assert(tlen <= sizeof(cdrom_sector));
						sprintf(tmp,"%s sector %016llu (%016llX) %u sects","Reading",sector,sector,(unsigned int)tlen_sect);
						vga_moveto(0,0);
						vga_write_color(0x0E);
						vga_write(tmp);
						if (select == 0 || select == 3 || select == 4) {
							sprintf(tmp," CHS %lu/%lu/%lu",
								(unsigned long)track,
								(unsigned long)head,
								(unsigned long)sectn);
							vga_write(tmp);
						}
						vga_write("           ");

						memset(cdrom_sector,0,tlen);

						if (kbhit()) {
							c = getch();
							if (c == 0) c = getch() << 8;
							if (c == 27) {
								stop = 1;
								break;
							}
						}

						y = 0;
						if (select == 0 || select == 3 || select == 4) { /* CHS */
							idelib_controller_drive_select(ide,which,head,IDELIB_DRIVE_SELECT_MODE_CHS);
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);
							outp(ide->base_io+2,tlen_sect); /* number of sectors (NOTE: 0 means 256 sectors) */
							outp(ide->base_io+3,sectn); /* sector # */
							outp(ide->base_io+4,track); /* track */
							outp(ide->base_io+5,track >> 8UL);
						}
						else if (select == 2 || select == 7 || select == 8 || select == 11 || select == 12) { /* LBA48 */
							vga_write("LBA48");

							outp(ide->base_io+6,0x40 | (which << 4));
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);

							outp(ide->base_io+2,tlen_sect >> 8); /* number of sectors hi byte */
							outp(ide->base_io+3,sector >> 24ULL);
							outp(ide->base_io+4,sector >> 32ULL);
							outp(ide->base_io+5,sector >> 40ULL);

							outp(ide->base_io+2,tlen_sect); /* number of sectors lo byte */
							outp(ide->base_io+3,sector);
							outp(ide->base_io+4,sector >> 8ULL);
							outp(ide->base_io+5,sector >> 16ULL);
						}
						else { /* LBA */
							outp(ide->base_io+6,0xE0 | (which << 4) | ((sector >> 24UL) & 0xFUL));
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);
							outp(ide->base_io+2,tlen_sect); /* number of sectors (NOTE: 0 means 256 sectors) */
							outp(ide->base_io+3,sector); /* sector # */
							outp(ide->base_io+4,sector >> 8ULL); /* track */
							outp(ide->base_io+5,sector >> 16ULL);
						}
						ide->irq_fired = 0;
						outp(ide->base_io+7,cmd); /* command */

						/* unlike the actual read command, it does not raise DRQ, and does not fire an interrupt
						 * until the whole sector verify is complete. */
						if (ide->flags.io_irq_enable)
							do_ide_controller_user_wait_irq(ide,1);

						ide->irq_fired = 0; /* <- having waited, reset counter */
						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
						x = inp(ide->base_io+7); /* what's the status? */

						/* we want to know (for debugging) if the IRQ occurs multiple times */
						if (ide->irq_fired > 1) {
							sprintf(tmp,"Device fired too many IRQs (%u)",ide->irq_fired);
							vga_msg_box_create(&vgabox,tmp,0,0);
							do {
								c = getch();
								if (c == 0) c = getch() << 8;
							} while (!(c == 13 || c == 27));
							vga_msg_box_destroy(&vgabox);
						}

						if (x&1) {
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

					if (!stop) {
						if (select == 2 && (sector & 0xFFFFFFULL) == 0ULL) {
							if (sector < 0x1000000ULL)
								sector += 0x1000000ULL;
							else if (sector < 0x10000000ULL)
								sector += 0x10000000ULL - 0x1000000ULL;
							else if (sector < 0x100000000ULL)
								sector += 0x100000000ULL - 0x10000000ULL;
							else
								sector = (sector + 1ULL) - 0x100000000ULL;
						}
						else {
							sector += tlen_sect;
						}
					}
					redraw = backredraw = 1;
				}
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 8;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 8)
				select = -1;

			redraw = 1;
		}
	}
#endif
}

void do_ide_controller_drive_rw_test(struct ide_controller *ide,unsigned char which,unsigned char writetest) {
	struct vga_msg_box vgabox;
	char redraw=1;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y,i;
	int select=-1;
	int c;

	if (big_scary_write_test_warning && writetest) {
		vga_msg_box_create(&vgabox,"WARNING!!! WARNING!!! WARNING!!!\n"
			"\n"
			"The write test OVERWRITES data on the selected hard drive!\n"
			"I hope the drive doesn't have anything important!\n"
			"To cancel the test, hit ESC now!"
			,0,0);
		do {
			c = getch();
			if (c == 0) c = getch() << 8;
		} while (!(c == 13 || c == 27));
		vga_msg_box_destroy(&vgabox);
		if (c == 27) return;

		vga_msg_box_create(&vgabox,"WARNING!!! WARNING!!! WARNING!!!\n"
			"\n"
			"The hard drive contents WILL BE OVERWRITTEN BY THIS TEST PROGRAM!\n"
			"If you value your data, even on unrelated drives, hit ESC now!"
			,0,0);
		do {
			c = getch();
			if (c == 0) c = getch() << 8;
		} while (!(c == 13 || c == 27));
		vga_msg_box_destroy(&vgabox);
		if (c == 27) return;

		vga_msg_box_create(&vgabox,"FINAL WARNING!!!\n"
			"\n"
			"This will overwrite the drive's partition table, file system, and data!\n"
			"Last change to hit ESC and save the data, if you value it!"
			,0,0);
		do {
			c = getch();
			if (c == 0) c = getch() << 8;
		} while (!(c == 13 || c == 27));
		vga_msg_box_destroy(&vgabox);
		if (c == 27) return;

		big_scary_write_test_warning = 0;
	}

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* select the drive we want */
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);

	/* in case the IDE controller is busy for that time */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* it might be a CD-ROM drive, which in some cases might not raise the Drive Ready bit */
	do_ide_controller_atapi_device_check_post_host_reset(ide);

	/* wait for the drive to indicate readiness */
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c != 0) return;

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
			vga_write(writetest ? " Write" : " Read");
			vga_write(" test");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');

			vga_write_color(0xC);
			vga_write("WARNING: This code talks directly to your hard disk controller.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
			vga_write_color(0xC);
			vga_write("         If you value the data on your hard drive do not run this program.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			const int cols = 2;

			redraw = 0;

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE drive menu");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			y = 7;
			vga_moveto(8,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("LBA PIO ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO ");
			vga_write(writetest ? "WRITE (34h)" : "READ (24h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO MS ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 4) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO TRK ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 5) ? 0x70 : 0x0F);
			vga_write("LBA PIO MS ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 6) ? 0x70 : 0x0F);
			vga_write("LBA PIO 63x ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 7) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO MS ");
			vga_write(writetest ? "WRITE (34h)" : "READ (24h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 8) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO 63x ");
			vga_write(writetest ? "WRITE (34h)" : "READ (24h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 9) ? 0x70 : 0x0F);
			vga_write("LBAMULT PIO MS ");
			vga_write(writetest ? "WRITE (C5h)" : "READ (C4h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 10) ? 0x70 : 0x0F);
			vga_write("LBAMULT PIO 63x ");
			vga_write(writetest ? "WRITE (C5h)" : "READ (C4h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 11) ? 0x70 : 0x0F);
			vga_write("LBA48MULT PIO MS ");
			vga_write(writetest ? "WRITE (39h)" : "READ (29h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 12) ? 0x70 : 0x0F);
			vga_write("LBA48MULT PIO 63 ");
			vga_write(writetest ? "WRITE (39h)" : "READ (29h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');
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
			else if (select == 0 || select == 1 || select == 2 || select == 3 || select == 4 || select == 5 || select == 6 || select == 7 || select == 8 || select == 9 || select == 10 || select == 11 || select == 12) {
				unsigned long long sector = 0;
				unsigned long track;
				unsigned int sectn,head;
				unsigned long tlen = 512; /* one sector */
				unsigned int stop = 0;
				unsigned long tlen_sect = 1;
				unsigned char cmd;
				unsigned int sect_per_block = 1; /* READ/WRITE MULTIPLE */
				unsigned int tracks=0,heads=0,sects=0;

				if (select == 0 || select == 3 || select == 4 || select == 9 || select == 10 || select == 11 || select == 12) {
					/* ask for C/H/S geometry or MULTIPLE block size */
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
								for (i=0;i < 256;i++)
									ide_info[i] = inpw(ide->base_io+0); /* read 16-bit word from data port, PIO */

								if (select == 9 || select == 10 || select == 11 || select == 12)
									sect_per_block = ide_info[59] & 0xFF;

								tracks = ide_info[1];
								heads = ide_info[3] & 0xFF;
								sects = ide_info[6] & 0xFF;

								sprintf(tmp,"C/H/S %u/%u/%u MULTBLK=%u",tracks,heads,sects,sect_per_block);
								vga_msg_box_create(&vgabox,tmp,0,0);
								do {
									c = getch();
									if (c == 0) c = getch() << 8;
								} while (!(c == 13 || c == 27));
								vga_msg_box_destroy(&vgabox);

								if (c == 27) {
									tracks = heads = sects = 0;
								}
							}
						}

						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
					}
				}

				if (sect_per_block == 0)
					sect_per_block = 1;

				/* NTS: The draft ATA-8 spec copy I have says WRITE MULTIPLE is 0xC3. That's WRONG! */
				if (select == 11 || select == 12) /* READ/WRITE MULTIPLE EXT (LBA48) */
					cmd = writetest ? 0x39 : 0x29;
				else if (select == 9 || select == 10) /* READ/WRITE MULTIPLE */
					cmd = writetest ? 0xC5 : 0xC4;
				else if (select == 2 || select == 7 || select == 8) /* READ/WRITE LBA48 */
					cmd = writetest ? 0x34 : 0x24;
				else
					cmd = writetest ? 0x30 : 0x20;

				while (((tracks != 0 && heads != 0 && sects != 0) || select == 1 || select == 2 || select == 5 || select == 6 || select == 7 || select == 8 || select == 9 || select == 10 || select == 11 || select == 12) && !stop) {
					if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
						do_ide_controller_user_wait_drive_ready(ide) == 0) {
						if (select == 0 || select == 3 || select == 4) { /* CHS */
							sectn = 1 + ((unsigned long)sector % (unsigned long)sects);
							head = ((unsigned long)sector / (unsigned long)sects) % (unsigned long)heads;
							track = (unsigned long)sector / (unsigned long)sects / (unsigned long)heads;

							if (select == 3) { /* multisector read with CHS but don't cross track boundaries */
								tlen_sect = 7;
								if ((sectn+tlen_sect) > sects) tlen_sect = (sects - sectn) + 1;
								tlen = 512 * tlen_sect;
							}
							else if (select == 4) { /* read to end of track */
								tlen_sect = (sects - sectn) + 1;
								if (tlen_sect > sects) tlen_sect = sects;
								tlen = 512 * tlen_sect;
							}
						}
						else if (select == 5 || select == 7 || select == 9 || select == 11) {
							tlen_sect = 7;
							tlen = 512 * tlen_sect;
						}
						else if (select == 6 || select == 8 || select == 10 || select == 12) {
							tlen_sect = 63;
							tlen = 512 * tlen_sect;
						}

						assert(tlen <= sizeof(cdrom_sector));
						sprintf(tmp,"%s sector %016llu (%016llX) %u sects",writetest?"Writing":"Reading",sector,sector,(unsigned int)tlen_sect);
						vga_moveto(0,0);
						vga_write_color(0x0E);
						vga_write(tmp);
						if (select == 0 || select == 3 || select == 4) {
							sprintf(tmp," CHS %lu/%lu/%lu",
								(unsigned long)track,
								(unsigned long)head,
								(unsigned long)sectn);
							vga_write(tmp);
						}
						vga_write("           ");

						if (writetest) {
							unsigned int i,l,j;

							for (i=0;i < tlen_sect;i++) {
								memset(cdrom_sector+(i*512),0xAA,512);
								l = (unsigned int)sprintf(cdrom_sector+(i*512)+2,
									"IDE TEST PROGRAM WAS HERE sector %llu (%u/%u)",
									sector+(unsigned long long)i,
									i+1,tlen_sect);
								for (j=l-1;j != ((unsigned int)(-1));j--) {
									cdrom_sector[(j*2)+(i*512)+2+0] = cdrom_sector[j+(i*512)+2];
									cdrom_sector[(j*2)+(i*512)+2+1] = 0x07;
								}
							}
						}
						else {
							memset(cdrom_sector,0,tlen);
						}

						if (kbhit()) {
							c = getch();
							if (c == 0) c = getch() << 8;
							if (c == 27) {
								stop = 1;
								break;
							}
						}

						y = 0;
						if (select == 0 || select == 3 || select == 4) { /* CHS */
							idelib_controller_drive_select(ide,which,head,IDELIB_DRIVE_SELECT_MODE_CHS);
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);
							outp(ide->base_io+2,tlen_sect); /* number of sectors (NOTE: 0 means 256 sectors) */
							outp(ide->base_io+3,sectn); /* sector # */
							outp(ide->base_io+4,track); /* track */
							outp(ide->base_io+5,track >> 8UL);
						}
						else if (select == 2 || select == 7 || select == 8 || select == 11 || select == 12) { /* LBA48 */
							vga_write("LBA48");

							outp(ide->base_io+6,0x40 | (which << 4));
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);

							outp(ide->base_io+2,tlen_sect >> 8); /* number of sectors hi byte */
							outp(ide->base_io+3,sector >> 24ULL);
							outp(ide->base_io+4,sector >> 32ULL);
							outp(ide->base_io+5,sector >> 40ULL);

							outp(ide->base_io+2,tlen_sect); /* number of sectors lo byte */
							outp(ide->base_io+3,sector);
							outp(ide->base_io+4,sector >> 8ULL);
							outp(ide->base_io+5,sector >> 16ULL);
						}
						else { /* LBA */
							outp(ide->base_io+6,0xE0 | (which << 4) | ((sector >> 24UL) & 0xFUL));
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);
							outp(ide->base_io+2,tlen_sect); /* number of sectors (NOTE: 0 means 256 sectors) */
							outp(ide->base_io+3,sector); /* sector # */
							outp(ide->base_io+4,sector >> 8ULL); /* track */
							outp(ide->base_io+5,sector >> 16ULL);
						}
						ide->irq_fired = 0;
						outp(ide->base_io+7,cmd); /* command */

						if (sect_per_block == 1) {
							/* Traditional PIO: One DRQ+IRQ per sector. */
							for (sectn=0;sectn < tlen_sect;sectn++) {
								/* READ: IRQ will fire when data is ready.
								 * WRITE: Do not wait for IRQ yet */
								if (!writetest && ide->flags.io_irq_enable)
									do_ide_controller_user_wait_irq(ide,1);

								ide->irq_fired = 0; /* <- having waited, reset counter */
								if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
								if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
								x = inp(ide->base_io+7); /* what's the status? */
								if (!(x&1)) { /* if no error, read result from count register */
									if (writetest) {
										if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
											if (pio_width >= 32) {
												if (pio_width == 33) ide_vlb_sync32_pio(ide);

												for (i=0;i < (512/4UL);i++)
													outpd(ide->base_io+0,((uint32_t*)cdrom_sector)[i+(sectn*128)]);
											}
											else {
												for (i=0;i < (512/2UL);i++)
													outpw(ide->base_io+0,((uint16_t*)cdrom_sector)[i+(sectn*256)]);
											}

											/* IRQ will fire when writing complete */
											if (ide->flags.io_irq_enable)
												do_ide_controller_user_wait_irq(ide,1);
										}

										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
									}
									else { /* read */
										if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
											if (pio_width >= 32) {
												if (pio_width == 33) ide_vlb_sync32_pio(ide);

												for (i=0;i < (512/4UL);i++)
													((uint32_t*)cdrom_sector)[i+(sectn*128)] = inpd(ide->base_io+0);
											}
											else {
												for (i=0;i < (512/2UL);i++)
													((uint16_t*)cdrom_sector)[i+(sectn*256)] = inpw(ide->base_io+0);
											}

											y = 1;
										}

										/* wait for controller */
										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
									}
								}

								/* we want to know (for debugging) if the IRQ occurs multiple times */
								if (ide->irq_fired > 1) {
									sprintf(tmp,"Device fired too many IRQs (%u)",ide->irq_fired);
									vga_msg_box_create(&vgabox,tmp,0,0);
									do {
										c = getch();
										if (c == 0) c = getch() << 8;
									} while (!(c == 13 || c == 27));
									vga_msg_box_destroy(&vgabox);
								}

								if (x&1) break;
							}
						}
						else {
							/* READ/WRITE MULTIPLE: One DRQ+IRQ every 'sect_per_block' sectors */
							for (sectn=0;sectn < tlen_sect;sectn += sect_per_block) {
								unsigned int rem = tlen_sect - sectn;
								if (rem > sect_per_block) rem = sect_per_block;

								/* READ: IRQ will fire when data is ready.
								 * WRITE: Do not wait for IRQ yet */
								if (!writetest && ide->flags.io_irq_enable)
									do_ide_controller_user_wait_irq(ide,1);

								ide->irq_fired = 0; /* <- having waited, reset counter */
								if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
								if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
								x = inp(ide->base_io+7); /* what's the status? */
								if (!(x&1)) { /* if no error, read result from count register */
									if (writetest) {
										if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
											/* write sector */
											for (i=0;i < (256UL*rem);i++)
												outpw(ide->base_io+0,((uint16_t*)cdrom_sector)[i+(sectn*256)]);

											/* IRQ will fire when writing complete */
											if (ide->flags.io_irq_enable)
												do_ide_controller_user_wait_irq(ide,1);
										}

										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
									}
									else { /* read */
										if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
											/* suck in the data and display it */
											for (i=0;i < (256UL*rem);i++)
												((uint16_t*)cdrom_sector)[i+(sectn*256)] = inpw(ide->base_io+0);

											y = 1;
										}

										/* wait for controller */
										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
									}
								}

								/* we want to know (for debugging) if the IRQ occurs multiple times */
								if (ide->irq_fired > 1) {
									sprintf(tmp,"Device fired too many IRQs (%u)",ide->irq_fired);
									vga_msg_box_create(&vgabox,tmp,0,0);
									do {
										c = getch();
										if (c == 0) c = getch() << 8;
									} while (!(c == 13 || c == 27));
									vga_msg_box_destroy(&vgabox);
								}

								if (x&1) break;
							}
						}

						if (x&1) {
							if (select == 2) {
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

						if (!stop && y) {
							for (i=0;i < (tlen>>1U);i++)
								vga_alpha_ram[i+(vga_width*2)] = ((uint16_t*)cdrom_sector)[i];

							if (select == 2) {
								if (sector == 0ULL) {
									for (i=0;i < (tlen>>1U);i++)
										vga_alpha_ram[i+(vga_width*(vga_height - 9))] = ((uint16_t*)cdrom_sector)[i];
								}
								else if (sector == 0x1000000ULL) {
									for (i=0;i < (tlen>>1U);i++)
										vga_alpha_ram[i+(vga_width*(vga_height - 5))] = ((uint16_t*)cdrom_sector)[i];
								}

								if (sector == 0ULL || sector == 0x1000000ULL) {
									vga_msg_box_create(&vgabox,"Pausing contents to let you view",0,0);
									do {
										c = getch();
										if (c == 0) c = getch() << 8;
									} while (!(c == 13 || c == 27));
									vga_msg_box_destroy(&vgabox);
								}
							}
						}
					}
					else {
						stop = 1;
					}

					if (!stop) {
						if (select == 2 && (sector & 0xFFFFFFULL) == 0ULL) {
							if (sector < 0x1000000ULL)
								sector += 0x1000000ULL;
							else if (sector < 0x10000000ULL)
								sector += 0x10000000ULL - 0x1000000ULL;
							else if (sector < 0x100000000ULL)
								sector += 0x100000000ULL - 0x10000000ULL;
							else
								sector = (sector + 1ULL) - 0x100000000ULL;
						}
						else {
							sector += tlen_sect;
						}
					}
					redraw = backredraw = 1;
				}
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 12;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 12)
				select = -1;

			redraw = 1;
		}
	}
}

/* NOTE: In typical standards-compliant manner, the "media lock/unlock" and "get media status" commands
         don't seem to apply to actual CD-ROM drives attached to the IDE bus. Don't be surprised if on
         any hardware you test this on you get errors. */
void do_ide_controller_drive_media_status_notify(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	char redraw=1;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	int c;

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* select the drive we want */
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);

	/* in case the IDE controller is busy for that time */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* it might be a CD-ROM drive, which in some cases might not raise the Drive Ready bit */
	do_ide_controller_atapi_device_check_post_host_reset(ide);

	/* wait for the drive to indicate readiness */
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c != 0) return;

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
			const int cols = 1;
			const int ofsx = 4;
			const int width = vga_width - 8;

			redraw = 0;

			y = 5;
			vga_moveto(ofsx,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE drive main menu");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			y = 7;
			vga_moveto(ofsx,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("Disable Media Status Notification");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("Enable Media Status Notification");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("Get Media Status Notification");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');
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
			else if (select == 0) {
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) == 0) {
					ide->irq_fired = 0;
					outp(ide->base_io+2,0x00);
					outp(ide->base_io+3,0x00);
					outp(ide->base_io+4,0x00);
					outp(ide->base_io+5,0x00);
					outp(ide->base_io+1,0x31); /* <- disable */
					outp(ide->base_io+7,0xEF); /* <- set features */
					if (ide->flags.io_irq_enable)
						do_ide_controller_user_wait_irq(ide,1);

					if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
					if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
					x = inp(ide->base_io+7); /* what's the status? */
					if (!(x&1)) {
						vga_msg_box_create(&vgabox,"Success",0,0);
					}
					else {
						sprintf(tmp,"Device rejected with error %02X",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					do {
						c = getch();
						if (c == 0) c = getch() << 8;
					} while (!(c == 13 || c == 27));
					vga_msg_box_destroy(&vgabox);
				}
			}
			else if (select == 1) {
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) == 0) {
					ide->irq_fired = 0;
					outp(ide->base_io+2,0x00);
					outp(ide->base_io+3,0x00);
					outp(ide->base_io+4,0x00);
					outp(ide->base_io+5,0x00);
					outp(ide->base_io+1,0x95); /* <- enable */
					outp(ide->base_io+7,0xEF); /* <- set features */
					if (ide->flags.io_irq_enable)
						do_ide_controller_user_wait_irq(ide,1);

					if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
					if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
					x = inp(ide->base_io+7); /* what's the status? */
					if (!(x&1)) {
						unsigned char version,b;

						version = inp(ide->base_io+4);	/* LBA 15:8 */
						b = inp(ide->base_io+5);	/* LBA 23:16 */

						sprintf(tmp,"Success:\nVersion = %u\nPEJ=%u Lock=%u PENA=%u",
							version,
							(b >> 2) & 1,
							(b >> 1) & 1,
							 b       & 1);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					else {
						sprintf(tmp,"Device rejected with error %02X",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					do {
						c = getch();
						if (c == 0) c = getch() << 8;
					} while (!(c == 13 || c == 27));
					vga_msg_box_destroy(&vgabox);
				}
			}
			else if (select == 2) {
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) == 0) {
					ide->irq_fired = 0;
					outp(ide->base_io+1,0x00);
					outp(ide->base_io+2,0x00);
					outp(ide->base_io+3,0x00);
					outp(ide->base_io+4,0x00);
					outp(ide->base_io+5,0x00);
					outp(ide->base_io+7,0xDA); /* <- get media status */
					if (ide->flags.io_irq_enable)
						do_ide_controller_user_wait_irq(ide,1);

					if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
					if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
					x = inp(ide->base_io+7); /* what's the status? */
					if (!(x&1)) {
						unsigned char version,b;

						version = inp(ide->base_io+4);	/* LBA 15:8 */
						b = inp(ide->base_io+5);	/* LBA 23:16 */

						sprintf(tmp,"Success:\nVersion = %u\nPEJ=%u Lock=%u PENA=%u",
							version,
							(b >> 2) & 1,
							(b >> 1) & 1,
							 b       & 1);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					else {
						sprintf(tmp,"Device rejected with error %02X",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					do {
						c = getch();
						if (c == 0) c = getch() << 8;
					} while (!(c == 13 || c == 27));
					vga_msg_box_destroy(&vgabox);
				}
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 2;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 2)
				select = -1;

			redraw = 1;
		}
	}
}

int do_ide_controller_drive_check_select(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	int c,ret = 0;

	if (idelib_controller_update_taskfile(ide,0xC0/* status and drive/select */,IDELIB_TASKFILE_LBA48_UPDATE) < 0)
		return -1;

	if (which != ide->selected_drive || ide->head_select == 0xFF) {
		vga_msg_box_create(&vgabox,"IDE controller drive select unsuccessful\n\nHit ESC to cancel, spacebar to proceed anyway",0,0);

		do {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27) {
				ret = -1;
				break;
			}
			else if (c == ' ') {
				ret = 1;
				break;
			}
		} while (1);

		vga_msg_box_destroy(&vgabox);
		return ret;
	}

	return ret;
}

void do_drive_standby_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_reset_irq_counter(ide);
	idelib_controller_write_command(ide,0xE0); /* <- standby immediate */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	common_ide_success_or_error_vga_msg_box(ide,&vgabox);
	wait_for_enter_or_escape();
	vga_msg_box_destroy(&vgabox);
}

void do_drive_idle_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_reset_irq_counter(ide);
	idelib_controller_write_command(ide,0xE1); /* <- idle immediate */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	common_ide_success_or_error_vga_msg_box(ide,&vgabox);
	wait_for_enter_or_escape();
	vga_msg_box_destroy(&vgabox);
}
		
void do_drive_device_reset_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_reset_irq_counter(ide);
	idelib_controller_write_command(ide,0x08); /* <- device reset */
	do_ide_controller_user_wait_busy_controller(ide);

	/* NTS: Device reset doesn't necessary seem to signal an IRQ, at least not
	 *      immediately. On some implementations the IRQ will have fired by now,
	 *      on others, it will have fired by now for hard drives but for CD-ROM
	 *      drives will not fire until the registers are read back, and others
	 *      don't fire at all. So don't count on the IRQ, just poll and busy
	 *      wait for the drive to signal readiness. */

	if (idelib_controller_update_taskfile(ide,0xFF,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/) == 0) {
		struct ide_taskfile *tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);

		sprintf(tmp,"Device response: (0x1F1-0x1F7) %02X %02X %02X %02X %02X %02X %02X",
			tsk->error,	tsk->sector_count,
			tsk->lba0_3,	tsk->lba1_4,
			tsk->lba2_5,	tsk->head_select,
			tsk->status);
		vga_msg_box_create(&vgabox,tmp,0,0);
	}
	else {
		common_failed_to_read_taskfile_vga_msg_box(&vgabox);
	}

	/* it MIGHT have fired an IRQ... */
	idelib_controller_ack_irq(ide);

	wait_for_enter_or_escape();
	vga_msg_box_destroy(&vgabox);
	do_ide_controller_atapi_device_check_post_host_reset(ide);
	do_ide_controller_user_wait_drive_ready(ide);
}

void do_drive_sleep_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	int c;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_reset_irq_counter(ide);
	idelib_controller_write_command(ide,0xE6); /* <- sleep */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	/* do NOT wait for drive ready---the drive is never ready when it's asleep! */
	if (!(ide->last_status&1)) {
		vga_msg_box_create(&vgabox,"Success.\n\nHit ENTER to re-awaken the device",0,0);
		c = wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);

		if (c != 27) { /* if the user did NOT hit ESCAPE, then re-awaken the device */
			/* clear IRQ state, wait for IDE controller to signal ready.
			 * once in sleep state, the device probably won't respond normally until device reset.
			 * do NOT wait for drive ready, the drive will never be ready in this state because
			 * (duh) it's asleep. */

			/* now re-awaken the device with DEVICE RESET */
			idelib_controller_ack_irq(ide);
			idelib_controller_reset_irq_counter(ide);
			idelib_controller_write_command(ide,0x08); /* <- device reset */
			do_ide_controller_user_wait_busy_controller(ide);
			idelib_controller_update_taskfile(ide,0xFF,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/); /* updating the taskfile seems to help with getting CD-ROM drives online */

			/* it MIGHT have fired an IRQ... */
			idelib_controller_ack_irq(ide);

			if (!idelib_controller_is_error(ide))
				vga_msg_box_create(&vgabox,"Success",0,0);
			else
				common_ide_success_or_error_vga_msg_box(ide,&vgabox);

			wait_for_enter_or_escape();
			vga_msg_box_destroy(&vgabox);

			do_ide_controller_atapi_device_check_post_host_reset(ide);
			do_ide_controller_user_wait_drive_ready(ide);
		}
		else {
			do_ide_controller_atapi_device_check_post_host_reset(ide);
			do_ide_controller_user_wait_busy_controller(ide);
		}
	}
	else {
		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

void do_drive_check_power_mode(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	struct ide_taskfile *tsk;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_reset_irq_counter(ide);

	/* in case command doesn't do anything, fill sector count value with 0x0A */
	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
	tsk->sector_count = 0x0A; /* our special "codeword" to tell if the command updated it or not */
	idelib_controller_apply_taskfile(ide,0x04/*base_io+2*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/);

	idelib_controller_write_command(ide,0xE5); /* <- check power mode */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);

	if (!(ide->last_status&1)) { /* if no error, read result from count register */
		const char *what = "";
		unsigned char x;

		/* read back from sector count field */
		idelib_controller_update_taskfile(ide,0x04/*base_io+2*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/);

		x = tsk->sector_count;
		if (x == 0) what = "Standby mode";
		else if (x == 0x0A) what = "?? (failure to update reg probably)";
		else if (x == 0x40) what = "NV Cache Power mode spun/spin down";
		else if (x == 0x41) what = "NV Cache Power mode spun/spin up";
		else if (x == 0x80) what = "Idle mode";
		else if (x == 0xFF) what = "Active or idle";

		sprintf(tmp,"Result: %02X %s",x,what);
		vga_msg_box_create(&vgabox,tmp,0,0);
	}
	else {
		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
	}

	wait_for_enter_or_escape();
	vga_msg_box_destroy(&vgabox);
}

void do_drive_identify_device_test(struct ide_controller *ide,unsigned char which,unsigned char command) {
	struct vga_msg_box vgabox;
	unsigned int x,y,i;
	uint16_t info[256];

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_reset_irq_counter(ide);
	idelib_controller_write_command(ide,command); /* <- (A1h) identify packet device (ECh) identify device */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (!(ide->last_status&1)) {
		/* read it */
		do_ide_controller_user_wait_drive_drq(ide);
		idelib_read_pio_general((unsigned char*)info,512,ide,pio_width);

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
	else {
		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

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

		do_construct_atapi_scsi_mmc_read(buf,sector,tlen_sect);
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

		if (!idelib_controller_is_error(ide)) { /* OK. success. now read the data */
			memset(cdrom_sector,0,tlen);
			do_ide_controller_user_wait_drive_drq(ide);
			do_warn_if_atapi_not_in_data_input_state(ide); /* sector count register should signal we're in the completed stage (command/data=0 input/output=1) */
			idelib_read_pio_general(cdrom_sector,tlen,ide,pio_width);
			if (ide->flags.io_irq_enable) { /* NOW we wait for another IRQ (completion) */
				do_ide_controller_user_wait_irq(ide,1);
				idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
			}
			idelib_controller_update_atapi_state(ide); /* having completed the command, read ATAPI state again */
			do_warn_if_atapi_not_in_complete_state(ide); /* sector count register should signal we're in the completed stage (command/data=1 input/output=1) */

			/* ---- draw contents on the screen ---- */
			vga_write_color(0x0E); vga_clear();

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
			do_warn_if_atapi_not_in_complete_state(ide); /* sector count register should signal we're in the completed stage (command/data=1 input/output=1) */
		}
	}
	else {
		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

void do_ide_controller_drive(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	char redraw=1;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y,i;
	int select=-1;
	int c;

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
			const int cols = 2;
			const int ofsx = 4;
			const int width = vga_width - 8;

			redraw = 0;

			y = 5;
			vga_moveto(ofsx,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE controller main menu");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			y = 7;
			vga_moveto(ofsx,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("Standby");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("Sleep");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("Idle");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("Device reset");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 4) ? 0x70 : 0x0F);
			vga_write("Power Mode");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 5) ? 0x70 : 0x0F);
			vga_write("Identify");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 6) ? 0x70 : 0x0F);
			vga_write("Ident Packet");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 7) ? 0x70 : 0x0F);
			vga_write("Eject CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 8) ? 0x70 : 0x0F);
			vga_write("Load CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 9) ? 0x70 : 0x0F);
			vga_write("Start CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 10) ? 0x70 : 0x0F);
			vga_write("Stop CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 11) ? 0x70 : 0x0F);
			vga_write("Read CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 12) ? 0x70 : 0x0F);
			vga_write("Read[2] CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 13) ? 0x70 : 0x0F);
			vga_write("Read HDD tests >>");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 14) ? 0x70 : 0x0F);
			vga_write("Write HDD tests >>");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 15) ? 0x70 : 0x0F);
			vga_write("Read2x CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 16) ? 0x70 : 0x0F);
			vga_write("Read15x CD-ROM");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx,y++);
			vga_write_color((select == 17) ? 0x70 : 0x0F);
			vga_write("CD-ROM READ");
			if (read_mode == 12) vga_write("(12)");
			else vga_write("(10)");
			vga_write(" (chg)");
			while (vga_pos_x < ((width/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			y = 7;
			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 18) ? 0x70 : 0x0F);
			vga_write("Set Multiple Mode test");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 19) ? 0x70 : 0x0F);
			vga_write("Set Multiple Mode >>");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 20) ? 0x70 : 0x0F);
			vga_write("Media Status Notify >>");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 21) ? 0x70 : 0x0F);
			vga_write("Media Eject");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 22) ? 0x70 : 0x0F);
			vga_write("Media Lock");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 23) ? 0x70 : 0x0F);
			vga_write("Media Unlock");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 24) ? 0x70 : 0x0F);
			vga_write("Write Uncorrectable");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 25) ? 0x70 : 0x0F);
			vga_write("CD-ROM INQUIRY");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 26) ? 0x70 : 0x0F);
			vga_write("CD-ROM READ TOC");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 27) ? 0x70 : 0x0F);
			vga_write("PIO WIDTH: ");
			if (pio_width == 33) vga_write("32-bit sync");
			else if (pio_width == 32) vga_write("32-bit");
			else vga_write("16-bit");
			vga_write(" (chg)");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 28) ? 0x70 : 0x0F);
			vga_write("NOP test");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 29) ? 0x70 : 0x0F);
			vga_write("Read verify tests >>");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(ofsx+(width/cols),y++);
			vga_write_color((select == 30) ? 0x70 : 0x0F);
			vga_write("Show IDE register taskfile");
			while (vga_pos_x < (((width*2)/cols)+ofsx) && vga_pos_x != 0) vga_writec(' ');
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1)
				break;
			else if (select == 0) /* standby */
				do_drive_standby_test(ide,which);
			else if (select == 1) /* sleep */
				do_drive_sleep_test(ide,which);
			else if (select == 2) /* idle */
				do_drive_idle_test(ide,which);
			else if (select == 3) /* device reset */
				do_drive_device_reset_test(ide,which);
			else if (select == 4) /* check power mode */
				do_drive_check_power_mode(ide,which);
			else if (select == 5 || select == 6) { /* identify device OR identify packet device */
				do_drive_identify_device_test(ide,which,select == 6 ? 0xA1 : 0xEC);
				redraw = backredraw = 1;
			}
			else if (select == 7 || select == 8 || select == 9 || select == 10) { /* ATAPI eject/load CD-ROM */
				static const unsigned char cmd[4] = {2/*eject*/,3/*load*/,1/*start*/,0/*stop*/};
				do_drive_atapi_eject_load(ide,which,cmd[select-7]);
			}
			else if (select == 11) { /* ATAPI read CD-ROM */
				do_drive_read_one_sector_test(ide,which);
				redraw = backredraw = 1;
			}

			else if (select == 28) { /* NOP */
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) >= 0) {
					struct ide_taskfile *tsk;

					idelib_controller_reset_irq_counter(ide);

					/* in case command doesn't do anything, fill sector count value with 0x0A */
					tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
					tsk->features = 0x00; /* will read back into error reg */
					tsk->sector_count = 0x12;
					tsk->lba0_3 = 0x34;
					tsk->lba1_4 = 0x56;
					tsk->lba2_5 = 0x78;
					tsk->command = 0x00; /* <- NOP */
					idelib_controller_apply_taskfile(ide,0xBE/*base_io+1-5&7*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/);
					if (ide->flags.io_irq_enable) {
						do_ide_controller_user_wait_irq(ide,1);
						idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
					}
					do_ide_controller_user_wait_busy_controller(ide);
					do_ide_controller_user_wait_drive_ready(ide);
					if (!(ide->last_status&1)) {
						vga_msg_box_create(&vgabox,"Success?? (It's not supposed to!)",0,0);
					}
					else if ((ide->last_status&0xC9) != 0x41) {
						sprintf(tmp,"Device rejected with error %02X (not the way it's supposed to)",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					else {
						idelib_controller_update_taskfile(ide,0xBE,0);
						if ((tsk->error&0x04) != 0x04) {
							sprintf(tmp,"Device rejected with non-abort (not the way it's supposed to)",x);
							vga_msg_box_create(&vgabox,tmp,0,0);
						}
						else {
							unsigned char c=0,cc=0;

							c=tsk->sector_count; cc += (c == 0x12)?1:0;
							c=tsk->lba0_3; cc += (c == 0x34)?1:0;
							c=tsk->lba1_4; cc += (c == 0x56)?1:0;
							c=tsk->lba2_5; cc += (c == 0x78)?1:0;

							if (cc == 4)
								vga_msg_box_create(&vgabox,"Success",0,0);
							else
								vga_msg_box_create(&vgabox,"Failed. Registers were modified.",0,0);
						}
					}

					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
				}
			}

			else if (select == 30) { /* show IDE register taskfile */
				if (idelib_controller_update_taskfile(ide,0xFF,0) == 0) {
					struct ide_taskfile *tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);

					redraw = backredraw = 1;
					vga_write_color(0x0F);
					vga_clear();

					vga_moveto(0,0);
					vga_write("IDE taskfile:\n\n");

					vga_write("IDE controller:\n");
					sprintf(tmp," selected_drive=%u last_status=0x%02x drive_address=0x%02x\n device_control=0x%02x head_select=0x%02x\n\n",
						ide->selected_drive,	ide->last_status,
						ide->drive_address,	ide->device_control,
						ide->head_select);
					vga_write(tmp);

					vga_write("IDE device:\n");
					sprintf(tmp," assume_lba48=%u\n",tsk->assume_lba48);
					vga_write(tmp);
					sprintf(tmp," error=0x%02x\n",tsk->error); /* aliased to features */
					vga_write(tmp);
					sprintf(tmp," sector_count=0x%04x\n",tsk->sector_count);
					vga_write(tmp);
					sprintf(tmp," lba0_3/chs_sector=0x%04x\n",tsk->lba0_3);
					vga_write(tmp);
					sprintf(tmp," lba1_4/chs_cyl_low=0x%04x\n",tsk->lba1_4);
					vga_write(tmp);
					sprintf(tmp," lba2_5/chs_cyl_high=0x%04x\n",tsk->lba2_5);
					vga_write(tmp);
					sprintf(tmp," head_select=0x%02x\n",tsk->head_select);
					vga_write(tmp);
					sprintf(tmp," command=0x%02x\n",tsk->command);
					vga_write(tmp);
					sprintf(tmp," status=0x%02x\n",tsk->status);
					vga_write(tmp);

					do {
						c = getch();
						if (c == 0) c = getch() << 8;
					} while (!(c == 13 || c == 27));
				}
				else {
					common_failed_to_read_taskfile_vga_msg_box(&vgabox);
					wait_for_enter_or_escape();
					vga_msg_box_destroy(&vgabox);
				}
			}
			else if (select == 21) { /* media eject */
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) == 0) {
					ide->irq_fired = 0;
					outp(ide->base_io+7,0xED); /* <- media eject */
					if (ide->flags.io_irq_enable)
						do_ide_controller_user_wait_irq(ide,1);

					do_ide_controller_user_wait_busy_controller(ide);
					do_ide_controller_user_wait_drive_ready(ide);
					x = inp(ide->base_io+7); /* what's the status? */
					if (!(x&1)) {
						vga_msg_box_create(&vgabox,"Success",0,0);
					}
					else {
						sprintf(tmp,"Device rejected with error %02X",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					do {
						c = getch();
						if (c == 0) c = getch() << 8;
					} while (!(c == 13 || c == 27));
					vga_msg_box_destroy(&vgabox);
				}
			}
			else if (select == 22) { /* media lock */
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) == 0) {
					ide->irq_fired = 0;
					outp(ide->base_io+7,0xDE); /* <- media lock */
					if (ide->flags.io_irq_enable)
						do_ide_controller_user_wait_irq(ide,1);

					do_ide_controller_user_wait_busy_controller(ide);
					do_ide_controller_user_wait_drive_ready(ide);
					x = inp(ide->base_io+7); /* what's the status? */
					if (!(x&1)) {
						vga_msg_box_create(&vgabox,"Success",0,0);
					}
					else {
						sprintf(tmp,"Device rejected with error %02X",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					do {
						c = getch();
						if (c == 0) c = getch() << 8;
					} while (!(c == 13 || c == 27));
					vga_msg_box_destroy(&vgabox);
				}
			}
			else if (select == 23) { /* media unlock */
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) == 0) {
					ide->irq_fired = 0;
					outp(ide->base_io+7,0xDF); /* <- media unlock */
					if (ide->flags.io_irq_enable)
						do_ide_controller_user_wait_irq(ide,1);

					do_ide_controller_user_wait_busy_controller(ide);
					do_ide_controller_user_wait_drive_ready(ide);
					x = inp(ide->base_io+7); /* what's the status? */
					if (!(x&1)) {
						vga_msg_box_create(&vgabox,"Success",0,0);
					}
					else {
						sprintf(tmp,"Device rejected with error %02X",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
					}
					do {
						c = getch();
						if (c == 0) c = getch() << 8;
					} while (!(c == 13 || c == 27));
					vga_msg_box_destroy(&vgabox);
				}
			}

			else if (select == 13 || select == 14) { /* read/write tests */
				do_ide_controller_drive_rw_test(ide,which,select==14?1:0);
				redraw = backredraw = 1;
			}
			else if (select == 29) { /* read verify */
				do_ide_controller_drive_readverify_test(ide,which);
				redraw = backredraw = 1;
			}
			else if (select == 24) {
				do_ide_controller_drive_write_unc_test(ide,which);
				redraw = backredraw = 1;
			}
			else if (select == 17) {
				if (read_mode == 12)
					read_mode = 10;
				else
					read_mode = 12;
				redraw = 1;
			}
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
											if (pio_width >= 32) {
												if (pio_width == 33) ide_vlb_sync32_pio(ide);

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
			else if (select == 20) {
				do_ide_controller_drive_media_status_notify(ide,which);
				redraw = backredraw = 1;
			}
			else if (select == 27) {
				char proceed = 1;

				if (pio_width_warning) {
					vga_msg_box_create(&vgabox,"WARNING: Data I/O will not function correctly if your\n"
						"IDE controller and/or ATA device does not support 32-bit PIO when enabled\n"
						"\n"
						"Hit ENTER to proceed switching on 32-bit PIO, ESC to cancel"
						,0,0);
					do {
						c = getch();
						if (c == 0) c = getch() << 8;
					} while (!(c == 13 || c == 27));
					vga_msg_box_destroy(&vgabox);
					if (c == 27) {
						proceed = 0;
					}
					else {
						pio_width_warning = 0;
					}
				}

				if (proceed) {
					if (pio_width == 32) pio_width = 33;
					else if (pio_width == 16) pio_width = 32;
					else pio_width = 16;
					redraw = 1;
				}
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
			else if (select == 25) { /* ATAPI INQUIRY */
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) == 0) {
					unsigned long tlen = 2048,rlen;

					y = 0;
					outp(ide->base_io+1,0x04); /* no DMA, xfer to host */
					outp(ide->base_io+2,0x00);
					outp(ide->base_io+3,0);
					outp(ide->base_io+4,tlen);
					outp(ide->base_io+5,tlen >> 8UL);
					outp(ide->base_io+7,0xA0); /* <- ATAPI PACKET */
					/* NTS: Do NOT wait for IRQ, the CD-ROM won't fire one here */
					do_ide_controller_user_wait_busy_controller(ide);
					do_ide_controller_user_wait_drive_ready(ide);
					x = inp(ide->base_io+7); /* what's the status? */
					if (!(x&1)) { /* if no error, read result from count register */
						unsigned char y1,y2,y3;
						uint8_t buf[12] = {0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00};

						y1 = inp(ide->base_io+7);

						buf[0] = 0x12;	/* INQUIRY */
						buf[3] = tlen >> 8; /* allocation length */
						buf[4] = tlen;

						ide->irq_fired = 0;
						for (i=0;i < 6;i++)
							outpw(ide->base_io+0,((uint16_t*)buf)[i]);

						if (ide->flags.io_irq_enable)
							do_ide_controller_user_wait_irq(ide,1);

						do_ide_controller_user_wait_busy_controller(ide);
						do_ide_controller_user_wait_drive_ready(ide);
						x = inp(ide->base_io+7); /* what's the status? */
						if (!(x&1)) {
							y2 = inp(ide->base_io+7);

							rlen = 0;	
							ide->irq_fired = 0;
							memset(cdrom_sector,0,tlen);
							if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
								rlen = inp(ide->base_io+4);
								rlen |= inp(ide->base_io+5) << 8UL;

								/* suck in the data and display it */
								for (i=0;i < ((rlen+1)>>1U);i++)
									((uint16_t*)cdrom_sector)[i] = inpw(ide->base_io+0);
							}

							/* NTS: So... nobody ever told me this but ATAPI PACKET signals another IRQ after transfer? */
							if (ide->flags.io_irq_enable)
								do_ide_controller_user_wait_irq(ide,1);

							/* wait for controller */
							do_ide_controller_user_wait_busy_controller(ide);
							do_ide_controller_user_wait_drive_ready(ide);
							y = 1;
							x = inp(ide->base_io+7); /* what's the status? */
							if (!(x&1)) {
								y3 = inp(ide->base_io+7);

								sprintf(tmp,"Success: rlen=%lu  %02X/%02X/%02X %lu",(unsigned long)rlen,y1,y2,y3,ide->irq_fired);
								vga_msg_box_create(&vgabox,tmp,0,0);
							}
							else {
								sprintf(tmp,"Device rejected with error %02X after read",x);
								vga_msg_box_create(&vgabox,tmp,0,0);
							}
						}
						else {
							sprintf(tmp,"Device rejected with error %02X",x);
							vga_msg_box_create(&vgabox,tmp,0,0);
						}
						do {
							c = getch();
							if (c == 0) c = getch() << 8;
						} while (!(c == 13 || c == 27));
						vga_msg_box_destroy(&vgabox);

						if (y) {
							vga_write_color(0xF);
							vga_clear();
							vga_moveto(0,0);

							for (y=0;y < 8;y++) {
								for (x=0;x < 16;x++) {
									sprintf(tmp,"%02X ",cdrom_sector[(y*16)+x]);
									vga_write(tmp);
								}

								for (x=0;x < 16;x++) {
									i = cdrom_sector[(y*16)+x];
									if (i < 32 || i > 127) i = '.';
									vga_writec(i);
								}

								if ((y%vga_height) == (vga_height-1))
									while (getch() != 13);

								vga_write("\n");
							}

							while (getch() != 13);
							redraw = backredraw = 1;
						}
					}
					else {
						sprintf(tmp,"Device rejected with error %02X",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
						do {
							c = getch();
							if (c == 0) c = getch() << 8;
						} while (!(c == 13 || c == 27));
						vga_msg_box_destroy(&vgabox);
					}
				}
			}
			else if (select == 26) { /* ATAPI READ TOC/PMA/ATIP/etc */
				if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
					do_ide_controller_user_wait_drive_ready(ide) == 0) {
					unsigned long tlen = 2048,rlen;

					y = 0;
					outp(ide->base_io+1,0x04); /* no DMA, xfer to host */
					outp(ide->base_io+2,0x00);
					outp(ide->base_io+3,0);
					outp(ide->base_io+4,tlen);
					outp(ide->base_io+5,tlen >> 8UL);
					outp(ide->base_io+7,0xA0); /* <- ATAPI PACKET */
					/* NTS: Do NOT wait for IRQ, the CD-ROM won't fire one here */
					do_ide_controller_user_wait_busy_controller(ide);
					do_ide_controller_user_wait_drive_ready(ide);
					x = inp(ide->base_io+7); /* what's the status? */
					if (!(x&1)) { /* if no error, read result from count register */
						unsigned char y1,y2,y3;
						uint8_t buf[12] = {0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00,
							0x00,0x00,0x00,0x00};

						y1 = inp(ide->base_io+7);

						buf[0] = 0x43;	/* READ TOC */
						buf[1] = 0x02;
						buf[7] = tlen >> 8; /* allocation length */
						buf[8] = tlen;
//						buf[9] = 0x40;	/* CONTROL */

						ide->irq_fired = 0;
						for (i=0;i < 6;i++)
							outpw(ide->base_io+0,((uint16_t*)buf)[i]);

						if (ide->flags.io_irq_enable)
							do_ide_controller_user_wait_irq(ide,1);

						do_ide_controller_user_wait_busy_controller(ide);
						do_ide_controller_user_wait_drive_ready(ide);
						x = inp(ide->base_io+7); /* what's the status? */
						if (!(x&1)) {
							y2 = inp(ide->base_io+7);

							rlen = 0;	
							ide->irq_fired = 0;
							memset(cdrom_sector,0,tlen);
							if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
								rlen = inp(ide->base_io+4);
								rlen |= inp(ide->base_io+5) << 8UL;

								/* suck in the data and display it */
								for (i=0;i < ((rlen+1)>>1U);i++)
									((uint16_t*)cdrom_sector)[i] = inpw(ide->base_io+0);
							}

							/* NTS: So... nobody ever told me this but ATAPI PACKET signals another IRQ after transfer? */
							if (ide->flags.io_irq_enable)
								do_ide_controller_user_wait_irq(ide,1);

							/* wait for controller */
							do_ide_controller_user_wait_busy_controller(ide);
							do_ide_controller_user_wait_drive_ready(ide);
							y = 1;
							x = inp(ide->base_io+7); /* what's the status? */
							if (!(x&1)) {
								y3 = inp(ide->base_io+7);

								sprintf(tmp,"Success: rlen=%lu  %02X/%02X/%02X %lu",(unsigned long)rlen,y1,y2,y3,ide->irq_fired);
								vga_msg_box_create(&vgabox,tmp,0,0);
							}
							else {
								sprintf(tmp,"Device rejected with error %02X after read",x);
								vga_msg_box_create(&vgabox,tmp,0,0);
							}
						}
						else {
							sprintf(tmp,"Device rejected with error %02X",x);
							vga_msg_box_create(&vgabox,tmp,0,0);
						}
						do {
							c = getch();
							if (c == 0) c = getch() << 8;
						} while (!(c == 13 || c == 27));
						vga_msg_box_destroy(&vgabox);

						if (y) {
							vga_write_color(0xF);
							vga_clear();
							vga_moveto(0,0);

							for (y=0;y < 8;y++) {
								for (x=0;x < 16;x++) {
									sprintf(tmp,"%02X ",cdrom_sector[(y*16)+x]);
									vga_write(tmp);
								}

								for (x=0;x < 16;x++) {
									i = cdrom_sector[(y*16)+x];
									if (i < 32 || i > 127) i = '.';
									vga_writec(i);
								}

								if ((y%vga_height) == (vga_height-1))
									while (getch() != 13);

								vga_write("\n");
							}

							while (getch() != 13);
							redraw = backredraw = 1;
						}
					}
					else {
						sprintf(tmp,"Device rejected with error %02X",x);
						vga_msg_box_create(&vgabox,tmp,0,0);
						do {
							c = getch();
							if (c == 0) c = getch() << 8;
						} while (!(c == 13 || c == 27));
						vga_msg_box_destroy(&vgabox);
					}
				}
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 30;

			redraw = 1;
		}
		else if (c == 0x4B00) {
			if (select >= 18) select -= 18;
			redraw = 1;
		}
		else if (c == 0x4D00) {
			if (select < 18) select += 18;
			if (select > 30) select -= 18;
			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 30)
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

