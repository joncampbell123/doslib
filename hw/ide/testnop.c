/* IDE NOP test */

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

#ifdef NOP_TEST
void do_ide_controller_drive_nop_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	struct ide_taskfile *tsk;

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;
	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */

	/* fill the registers with a pattern and then issue the NOP command.
	 * the NOP command should come back with unmodified registers */
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
		sprintf(tmp,"Device rejected with status %02X (not the way it's supposed to)",ide->last_status);
		vga_msg_box_create(&vgabox,tmp,0,0);
	}
	else {
		idelib_controller_update_taskfile(ide,0xBE,0);
		if ((tsk->error&0x04) != 0x04) {
			vga_msg_box_create(&vgabox,"Device rejected with non-abort (not the way it's supposed to)",0,0);
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
#endif /* NOP_TEST */

