
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

unsigned char sanitizechar(unsigned char c) {
	if (c < 32) return '.';
	return c;
}

int wait_for_enter_or_escape() {
	int c;

	do {
		c = getch();
		if (c == 0) c = getch() << 8;
	} while (!(c == 13 || c == 27));

	return c;
}

/* construct ATAPI/SCSI-MMC READ command according to user's choice, either READ(10) or READ(12) */
void do_construct_atapi_scsi_mmc_read(unsigned char *buf/*must be 12 bytes*/,uint32_t sector,uint32_t tlen_sect,unsigned char read_mode) {
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

/* check for another possible case where 16-bit PIO word is in lower half of 32-bit read, junk in upper */
int ide_memcmp_every_other_word(unsigned char *pio16,unsigned char *pio32,unsigned int words) {
	while (words > 0) {
		uint16_t a = *((uint16_t*)pio16);
		uint16_t b = (uint16_t)(*((uint32_t*)pio32) & 0xFFFF);
		if (a != b) return (int)(a-b);
		pio16 += 2;
		pio32 += 4;
		words--;
	}

	return 0;
}

/* return 0 if all bytes are 0xFF */
int ide_memcmp_all_FF(unsigned char *d,unsigned int bytes) {
	while (bytes > 0) {
		if (*d != 0xFF) return 1;
		d++;
		bytes--;
	}

	return 0;
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

