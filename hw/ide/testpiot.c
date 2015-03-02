
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
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

#ifdef PIO_AUTODETECT

void ide_pio_autodetect(struct ide_controller *ide,unsigned char which) {
	unsigned char ok32=0,ok32vlb=0;
	unsigned char ident=0,identpacket=0;
	unsigned char *info_16 = cdrom_sector;			/* 512 bytes */
	unsigned char *info_32 = cdrom_sector + 512;		/* 1024 bytes */
	struct vga_msg_box vgabox;
	int c;

	pio_width_warning = 0;

	vga_msg_box_create(&vgabox,
		"I will test various data transfers to try and auto-detect whether\n"
		"or not your IDE controller can do 32-bit PIO.\n"
		"\n"
		"Hit ENTER to proceed, ESC to cancel"
		,0,0);
	do {
		c = getch();
		if (c == 0) c = getch() << 8;
	} while (!(c == 13 || c == 27));
	vga_msg_box_destroy(&vgabox);
	if (c == 27) return;

	/* OK start */
	vga_write_color(0x0E);
	vga_clear();
	vga_moveto(0,0);
	vga_write("PIO test in progress...\n\n");

	/* ====================================== 16-bit PIO ================================ */
	/* the secondary purpose of this test is to autodetect whether to carry out 32-bit PIO
	 * using Identify or Identify Packet commands */
	vga_write("16-bit PIO test: ");

	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;
	c = do_ide_controller_drive_check_select(ide,which);
	if (c < 0) return;
	do_ide_controller_atapi_device_check_post_host_reset(ide);
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c < 0) return;
	idelib_controller_ack_irq(ide);
	idelib_controller_reset_irq_counter(ide);

	/* first: try identify device */
	vga_write("[0xEC] ");
	idelib_controller_write_command(ide,0xEC/*Identify device*/);
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (!(ide->last_status&1)) {
		ident = 1; /* it worked! */
		vga_write("OK ");
		do_ide_controller_user_wait_drive_drq(ide);
		idelib_read_pio_general(info_16,512,ide,16/*PIO 16*/);
	}

	/* then try identify packet device */
	if (!ident) {
		vga_write("[0xA1] ");
		idelib_controller_write_command(ide,0xA1/*Identify packet device*/);
		if (ide->flags.io_irq_enable) {
			do_ide_controller_user_wait_irq(ide,1);
			idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
		}
		do_ide_controller_user_wait_busy_controller(ide);
		do_ide_controller_user_wait_drive_ready(ide);
		if (!(ide->last_status&1)) {
			identpacket = 1; /* it worked! */
			vga_write("OK ");
			do_ide_controller_user_wait_drive_drq(ide);
			idelib_read_pio_general(info_16,512,ide,16/*PIO 16*/);
		}
	}
	vga_write("\n");

	if (!ident && !identpacket) {
		vga_write("Sorry, neither command worked. I don't have a reliable way to test.\nHit ENTER to continue.");
		wait_for_enter_or_escape();
		return;
	}

	/* ====================================== 32-bit PIO VLB ================================ */
	vga_write("32-bit PIO VLB test: ");

	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;
	c = do_ide_controller_drive_check_select(ide,which);
	if (c < 0) return;
	do_ide_controller_atapi_device_check_post_host_reset(ide);
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c < 0) return;
	idelib_controller_ack_irq(ide);
	idelib_controller_reset_irq_counter(ide);

	/* do it */
	idelib_controller_write_command(ide,identpacket ? 0xA1/*Identify packet device*/ : 0xEC/*Identify device*/);
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (!(ide->last_status&1)) {
		vga_write("OK ");
		do_ide_controller_user_wait_drive_drq(ide);
		idelib_read_pio_general(info_32,1024,ide,33/*PIO 32 VLB*/);
		idelib_discard_pio_general(512,ide,16/*PIO 16*/); /* <- in case 32-bit PIO did nothing, discard using 16-bit PIO */

		/* compare the data. is it the same? if so, 32-bit PIO is supported */
		if (!memcmp(info_16,info_32,512)) {
			vga_write("Data matches 16-bit PIO read, 32-bit VLB PIO supported");
			ok32vlb = 1;
		}
		/* if we see the 16-bit PIO interleaved with some other WORD junk across 1024 bytes then
		 * we can assume it's likely the 16-bit data in the lower WORD and the contents of
		 * IDE registers 0x1F2-0x1F3 in the upper WORD */
		else if (!ide_memcmp_every_other_word(info_16,info_32,256)) {
			vga_write("Like 16-bit PIO interleaved with junk, not supported");
		}
		/* another possibility seen on a Toshiba Satellite Pro, is that 32-bit PIO is flat out
		 * ignored and we read back 0xFFFFFFFF, and the device acts as no PIO ever occurred */
		else if (!ide_memcmp_all_FF(info_32,1024)) {
			vga_write("No data whatsoever (0xFFFFFFFF), not supported");
		}
		else {
			vga_write("???");
		}
	}
	vga_write("\n");

	/* ====================================== 32-bit PIO ================================ */
	vga_write("32-bit PIO test: ");

	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;
	c = do_ide_controller_drive_check_select(ide,which);
	if (c < 0) return;
	do_ide_controller_atapi_device_check_post_host_reset(ide);
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c < 0) return;
	idelib_controller_ack_irq(ide);
	idelib_controller_reset_irq_counter(ide);

	/* do it */
	idelib_controller_write_command(ide,identpacket ? 0xA1/*Identify packet device*/ : 0xEC/*Identify device*/);
	if (ide->flags.io_irq_enable) {
		do_ide_controller_user_wait_irq(ide,1);
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}
	do_ide_controller_user_wait_busy_controller(ide);
	do_ide_controller_user_wait_drive_ready(ide);
	if (!(ide->last_status&1)) {
		vga_write("OK ");
		do_ide_controller_user_wait_drive_drq(ide);
		idelib_read_pio_general(info_32,1024,ide,32/*PIO 32*/);
		idelib_discard_pio_general(512,ide,16/*PIO 16*/); /* <- in case 32-bit PIO did nothing, discard using 16-bit PIO */

		/* compare the data. is it the same? if so, 32-bit PIO is supported */
		if (!memcmp(info_16,info_32,512)) {
			vga_write("Data matches 16-bit PIO read, 32-bit PIO supported");
			ok32 = 1;
		}
		/* if we see the 16-bit PIO interleaved with some other WORD junk across 1024 bytes then
		 * we can assume it's likely the 16-bit data in the lower WORD and the contents of
		 * IDE registers 0x1F2-0x1F3 in the upper WORD */
		else if (!ide_memcmp_every_other_word(info_16,info_32,256)) {
			vga_write("Like 16-bit PIO interleaved with junk, not supported");
		}
		/* another possibility seen on a Toshiba Satellite Pro, is that 32-bit PIO is flat out
		 * ignored and we read back 0xFFFFFFFF, and the device acts as no PIO ever occurred */
		else if (!ide_memcmp_all_FF(info_32,1024)) {
			vga_write("No data whatsoever (0xFFFFFFFF), not supported");
		}
		else {
			vga_write("???");
		}
	}
	vga_write("\n");

	/* DEBUG */
	vga_write_color(0x0F);
	vga_write("\nHit enter to accept new PIO mode, ESC to cancel\n");
	c = wait_for_enter_or_escape();
	if (c == 27) return;

	if (ok32) ide->pio_width = 32;
	else if (ok32vlb) ide->pio_width = 33;
	else ide->pio_width = 16;
}

#endif /* PIO_AUTODETECT */

