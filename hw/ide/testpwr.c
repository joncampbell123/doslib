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

#ifdef POWER_MENU

/* =================================== test functions ================================= */

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

			/* NTS: some laptops, especially Toshiba, have a hard drive connected to the primary
			 *      channel. for some reason, issuing ANY command after sleep causes the IDE
			 *      controller to signal BUSY and never come out of it. The only way out of
			 *      the situation is to do a host reset (reset the IDE controller). */
			if (do_ide_controller_user_wait_busy_timeout_controller(ide,1000/*ms*/)) { /* if we waited too long for IDE ready */
				vga_msg_box_create(&vgabox,"Host reset in progress",0,0);

				idelib_device_control_set_reset(ide,1);
				t8254_wait(t8254_us2ticks(1000000));
				idelib_device_control_set_reset(ide,0);

				vga_msg_box_destroy(&vgabox);
			}

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

void do_drive_device_reset_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	struct ide_taskfile *tsk;
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
	idelib_controller_reset_irq_counter(ide);

	/* make sure the registers are cleared */
	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
	tsk->error = 0x99;
	tsk->sector_count = 0xAA;
	tsk->lba0_3 = 0xBB;
	tsk->lba1_4 = 0xCC;
	tsk->lba2_5 = 0xDD;
	idelib_controller_apply_taskfile(ide,0x3E/*base_io+1-5*/,IDELIB_TASKFILE_LBA48_UPDATE|IDELIB_TASKFILE_LBA48);
	idelib_controller_write_command(ide,0x08); /* <- device reset */
	do_ide_controller_user_wait_busy_controller(ide);

	/* it MIGHT have fired an IRQ... */
	idelib_controller_ack_irq(ide);

	/* NTS: Device reset doesn't necessary seem to signal an IRQ, at least not
	 *      immediately. On some implementations the IRQ will have fired by now,
	 *      on others, it will have fired by now for hard drives but for CD-ROM
	 *      drives will not fire until the registers are read back, and others
	 *      don't fire at all. So don't count on the IRQ, just poll and busy
	 *      wait for the drive to signal readiness. */

	if (idelib_controller_update_taskfile(ide,0xFF,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/) == 0) {
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

	wait_for_enter_or_escape();
	vga_msg_box_destroy(&vgabox);
	do_ide_controller_atapi_device_check_post_host_reset(ide); /* NTS: This will change the taskfile further! */
	do_ide_controller_user_wait_drive_ready(ide);
}

/* =================================== menu for tests ================================= */

static const char *drive_main_power_states_strings[] = {
	"Show IDE register taskfile",		/* 0 */
	"Device reset",
	"Standby",
	"Sleep",
	"Idle",
	"Power mode"				/* 5 */
};

void do_drive_power_states_test(struct ide_controller *ide,unsigned char which) {
	VGA_ALPHA_PTR vga;
	struct menuboxbounds mbox;
	char backredraw=1;
	unsigned int x,y;
	char redraw=1;
	int select=-1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_main_power_states_strings);

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
			vga_write(" << Power states");
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
				case 1: /* device reset */
					do_drive_device_reset_test(ide,which);
					break;
				case 2: /* standby */
					do_drive_standby_test(ide,which);
					break;
				case 3: /* sleep */
					do_drive_sleep_test(ide,which);
					break;
				case 4: /* idle */
					do_drive_idle_test(ide,which);
					break;
				case 5: /* check power mode */
					do_drive_check_power_mode(ide,which);
					break;
			};
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = mbox.item_max;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > mbox.item_max)
				select = -1;

			redraw = 1;
		}
	}
}

#endif /* POWER_MENU */

