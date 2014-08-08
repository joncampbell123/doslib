/* IDE power management commands test */

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
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

void do_drive_standby_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
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

void do_drive_sleep_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	int c;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
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

	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */
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

