
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
#include "test.h"

void common_ide_success_or_error_vga_msg_box(struct ide_controller *ide,struct vga_msg_box *vgabox) {
	if (!(ide->last_status&1)) {
		vga_msg_box_create(vgabox,"Success",0,0);
	}
	else {
		sprintf(tmp,"Device rejected with error %02X",ide->last_status);
		vga_msg_box_create(vgabox,tmp,0,0);
	}
}
	
void common_failed_to_read_taskfile_vga_msg_box(struct vga_msg_box *vgabox) {
	vga_msg_box_create(vgabox,"Failed to read taskfile",0,0);
}

void do_warn_if_atapi_not_in_command_state(struct ide_controller *ide) {
	struct vga_msg_box vgabox;

	if (!idelib_controller_atapi_command_state(ide)) {
		sprintf(tmp,"WARNING: ATAPI device not in command state as expected (state=%u)",idelib_controller_read_atapi_state(ide));
		vga_msg_box_create(&vgabox,tmp,0,0);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

void do_warn_if_atapi_not_in_data_input_state(struct ide_controller *ide) {
	struct vga_msg_box vgabox;

	if (!idelib_controller_atapi_data_input_state(ide)) {
		sprintf(tmp,"WARNING: ATAPI device not in data input state as expected (state=%u)",idelib_controller_read_atapi_state(ide));
		vga_msg_box_create(&vgabox,tmp,0,0);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
	}
}

void do_warn_if_atapi_not_in_complete_state(struct ide_controller *ide) {
	struct vga_msg_box vgabox;

	if (!idelib_controller_atapi_complete_state(ide)) {
		sprintf(tmp,"WARNING: ATAPI device not in complete state as expected (state=%u)",idelib_controller_read_atapi_state(ide));
		vga_msg_box_create(&vgabox,tmp,0,0);
		wait_for_enter_or_escape();
		vga_msg_box_destroy(&vgabox);
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

void do_common_show_ide_taskfile(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	int c;

	if (idelib_controller_update_taskfile(ide,0xFF,0) == 0) {
		struct ide_taskfile *tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);

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

int confirm_pio32_warning(struct ide_controller *ide) {
	if (ide->pio_width < 32 && pio_width_warning) {
		struct vga_msg_box vgabox;
		char proceed = 1;
		int c;

		if (pio_width_warning) {
			vga_msg_box_create(&vgabox,
				"WARNING: Data I/O will not function correctly if your IDE controller\n"
				"does not support 32-bit PIO. IDE data transfers are traditionally\n"
				"carried out as 16-bit I/O.\n"
				"\n"
				"In most cases, if the IDE controller is connected as a PCI device and\n"
				"the controller was made 1998 or later, it's likely safe to enable.\n"
				"Some hardware in the 1994-1997 timeframe may have problems.\n"
				"\n"
				"Though not confirmed, 32-bit PIO may also work if your IDE controller\n"
				"is connected to the VESA local bus port of your 486 motherboard. In\n"
				"that case it is recommended to try 32-bit VLB sync PIO first.\n"
				"\n"
				"Hit ENTER to proceed, ESC to cancel"
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

		return proceed;
	}

	return 1;
}

