
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

