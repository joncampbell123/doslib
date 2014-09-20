
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

int do_ide_media_card_pass_through(struct ide_controller *ide,unsigned char which,unsigned char enable) {
	struct ide_taskfile *tsk;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return -1;

	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
	idelib_controller_reset_irq_counter(ide);
	tsk->features = (enable != 0 ? 1 : 0);
	idelib_controller_apply_taskfile(ide,0x02/*base_io+1*/,IDELIB_TASKFILE_LBA48_UPDATE|IDELIB_TASKFILE_LBA48);
	idelib_controller_write_command(ide,0xD1); /* <- (D1h) CHECK MEDIA CARD TYPE */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (ide->last_status&1) return 1;
	return 0;
}

int do_ide_set_multiple_mode(struct ide_controller *ide,unsigned char which,unsigned char sz) {
	struct ide_taskfile *tsk;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return -1;

	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
	idelib_controller_reset_irq_counter(ide);
	tsk->sector_count = sz;
	idelib_controller_apply_taskfile(ide,0x04/*base_io+2*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/);
	idelib_controller_write_command(ide,0xC6); /* <- (C6h) SET MULTIPLE MODE */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (ide->last_status&1) return 1;
	return 0;
}

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

int do_ide_flush(struct ide_controller *ide,unsigned char which,unsigned char lba48) {
	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return -1;

	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
	idelib_controller_reset_irq_counter(ide);
	idelib_controller_write_command(ide,lba48?0xEA:0xE7); /* <- (E7h) FLUSH CACHE (EA) FLUSH CACHE EXT */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (ide->last_status&1) return 1;
	return 0;
}
	
int do_ide_device_diagnostic(struct ide_controller *ide,unsigned char which) {
	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return -1;

	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
	idelib_controller_reset_irq_counter(ide);
	idelib_controller_write_command(ide,0x90); /* <- (90h) EXECUTE DEVICE DIAGNOSTIC */
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (ide->last_status&1) return 1;
	return 0;
}

void do_ident_save(const char *basename,struct ide_controller *ide,unsigned char which,uint16_t *nfo/*512 bytes/256 words*/,unsigned char command) {
	unsigned char range[512];
	unsigned char orgl;
	char fname[24];
	int fd,r;

	/* also record how the drive records SET MULTIPLE MODE */
	orgl = nfo[59]&0xFF;
	vga_write("Scanning SET MULTIPLE MODE supported values\n");
	for (r=0;r < 256;r++) {
		/* set sector count, then use IDENTIFY DEVICE to read back the value */
		if (do_ide_set_multiple_mode(ide,which,r) < 0)
			break;
		if (do_ide_identify((unsigned char*)nfo,512,ide,which,command) < 0)
			break;
		range[r] = nfo[59]&0xFF;

		/* then set it back to "1" */
		if (do_ide_set_multiple_mode(ide,which,1) < 0)
			break;
		if (do_ide_identify((unsigned char*)nfo,512,ide,which,command) < 0)
			break;
		range[r+256] = nfo[59]&0xFF;

		vga_write(".");
	}
	vga_write("\n");

	/* restore the original value */
	if (do_ide_set_multiple_mode(ide,which,orgl) < 0)
		return;
	if (do_ide_identify((unsigned char*)nfo,512,ide,which,command) < 0)
		return;

	/* we might be running off the IDE drive we're about to write to.
	 * so to avoid hangups in INT 13h and DOS we need to temporarily unhook the IRQ and
	 * reenable the IRQ for the BIOS */
	do_ide_controller_enable_irq(ide,0);
	idelib_enable_interrupt(ide,1); /* <- in case of stupid/lazy BIOS */

	/* if SMARTDRV is resident, now is a good time to write to disk */
	if (smartdrv_version != 0) {
		for (r=0;r < 4;r++) smartdrv_flush();
	}

	/* OK. start writing */
	sprintf(fname,"%s.ID",basename);
	fd = open(fname,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0666);
	if (fd >= 0) {
		write(fd,nfo,512);
		close(fd);
	}

	sprintf(fname,"%s.SMM",basename);
	fd = open(fname,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0666);
	if (fd >= 0) {
		write(fd,range,512);
		close(fd);
	}

	/* if SMARTDRV is resident, flush our snapshot to disk because we're going
	 * to take over the IDE controller again */
	if (smartdrv_version != 0) {
		for (r=0;r < 4;r++) smartdrv_flush();
	}

	/* OK. hook it again. */
	do_ide_controller_enable_irq(ide,ide->flags.io_irq_enable);
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
		sprintf(tmp,"Current Capacity: %lu  Total user addressable: %lu",
			(unsigned long)info[57] | ((unsigned long)info[58] << 16UL),
			(unsigned long)info[60] | ((unsigned long)info[61] << 16UL));
		vga_write(tmp);

		vga_moveto(0,3);
		sprintf(tmp,"LBA48 capacity: %llu",
			(unsigned long long)info[100] | ((unsigned long long)info[101] << 16ULL) | ((unsigned long long)info[102] << 32ULL) | ((unsigned long long)info[103] << 48ULL));
		vga_write(tmp);

		vga_moveto(0,4);
		vga_write_color(0x0D);
		vga_write("For more information read ATA documentation. ENTER to continue, 'S' to save.");

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

			/* wait for ENTER, ESC, or 'S' */
			do {
				r=getch();
				if (r == 's' || r == 'S') {
					vga_write_color(0x0E); vga_clear(); vga_moveto(0,0); 
					do_ident_save(command == 0xA1 ? "ATAPI" : "ATA",ide,which,info,command);
					vga_write("\nSaved. Hit ENTER to continue.\n");
					wait_for_enter_or_escape();
					break;
				}
				else if (r == 27 || r == 13) {
					break;
				}
			} while (1);

			/* ESC or 'S' to break out */
			if (r == 's' || r == 27)
				break;
		}
	}
	else if (r > 0) {
		struct vga_msg_box vgabox;

		common_ide_success_or_error_vga_msg_box(ide,&vgabox);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

