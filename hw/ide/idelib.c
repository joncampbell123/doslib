
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/ide/idelib.h>
#include <hw/pci/pci.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>

struct ide_controller		ide_controller[MAX_IDE_CONTROLLER];
int8_t				idelib_init = -1;

const struct ide_controller ide_isa_standard[2] = {
	/*base alt fired irq flags*/
	{0x1F0,0x3F6,0,14,{0}},
	{0x170,0x376,0,15,{0}}
};

const struct ide_controller *idelib_get_standard_isa_port(int i) {
	if (i < 0 || i >= 4) return NULL;
	return &ide_isa_standard[i];
}

int init_idelib() {
	if (idelib_init < 0) {
		memset(ide_controller,0,sizeof(ide_controller));
		idelib_init = 0;

		cpu_probe();
		probe_dos();
		detect_windows();

		/* do NOT under any circumstances talk directly to IDE from under Windows! */
		if (windows_mode != WINDOWS_NONE) return (idelib_init=0);

		/* init OK */
		idelib_init = 1;
	}

	return (int)idelib_init;
}

void free_idelib() {
}

void idelib_controller_update_status(struct ide_controller *ide) {
	if (ide == NULL) return;
	ide->last_status = inp(ide->alt_io != 0 ? /*0x3F6-ish status*/ide->alt_io : /*status register*/(ide->base_io+7));
	if (ide->alt_io != 0) ide->drive_address = inp(ide->alt_io+1);

	/* if the IDE controller is NOT busy, then also note the status according to the selected drive's taskfile */
	if (!(ide->last_status&0x80)) ide->taskfile[ide->selected_drive].status = ide->last_status;
}

int idelib_controller_is_busy(struct ide_controller *ide) {
	if (ide == NULL) return 0;
	return !!(ide->last_status&0x80);
}

int idelib_controller_is_error(struct ide_controller *ide) {
	if (ide == NULL) return 0;
	return !!(ide->last_status&0x01) && !(ide->last_status&0x80)/*and not busy*/;
}

int idelib_controller_is_drq_ready(struct ide_controller *ide) {
	if (ide == NULL) return 0;
	return !!(ide->last_status&0x08) && !(ide->last_status&0x80)/*and not busy*/;
}

int idelib_controller_is_drive_ready(struct ide_controller *ide) {
	if (ide == NULL) return 0;
	return !!(ide->last_status&0x40) && !(ide->last_status&0x80)/*and not busy*/;
}

int idelib_controller_allocated(struct ide_controller *ide) {
	if (ide == NULL) return 0;
	return (ide->base_io != 0);
}

struct ide_controller *idelib_get_controller(int i) {
	if (i < 0 || i >= MAX_IDE_CONTROLLER) return NULL;
	if (!idelib_controller_allocated(&ide_controller[i])) return NULL;
	return &ide_controller[i];
}

/* NTS: The controller is "allocated" if the base I/O port is nonzero.
 *      Therefore, if the caller never fills in base_io, it remains
 *      unallocated, and next call the same struct is returned. */
struct ide_controller *idelib_new_controller() {
	int i;

	for (i=0;i < MAX_IDE_CONTROLLER;i++) {
		if (!idelib_controller_allocated(&ide_controller[i]))
			return &ide_controller[i];
	}

	return NULL;
}

struct ide_controller *idelib_by_base_io(uint16_t io) {
	int i;

	if (io == 0)
		return NULL;

	for (i=0;i < MAX_IDE_CONTROLLER;i++) {
		if (ide_controller[i].base_io == io)
			return &ide_controller[i];
	}

	return NULL;
}

struct ide_controller *idelib_by_alt_io(uint16_t io) {
	int i;

	if (io == 0)
		return NULL;

	for (i=0;i < MAX_IDE_CONTROLLER;i++) {
		if (ide_controller[i].alt_io == io)
			return &ide_controller[i];
	}

	return NULL;
}

struct ide_controller *idelib_by_irq(int8_t irq) {
	int i;

	if (irq < 0)
		return NULL;

	for (i=0;i < MAX_IDE_CONTROLLER;i++) {
		if (ide_controller[i].irq == irq)
			return &ide_controller[i];
	}

	return NULL;
}

/* To explain: an IDE controller struct is passed in by the caller
 * only to describe what resources to use. A completely new IDE
 * controller struct is allocated and initialized based on what
 * we were given. The caller can then dispose of the original
 * struct. This gives the caller freedom to declare a struct on
 * the stack and use it without consequences. */
struct ide_controller *idelib_probe(struct ide_controller *ide) {
	struct ide_controller *newide = NULL;
	uint16_t alt_io = 0;
	int8_t irq = -1;

	/* allocate a new empty slot. bail if all are full */
	newide = idelib_new_controller();
	if (newide == NULL)
		return NULL;

	/* we can work with the controller if alt_io is zero or irq < 0, but we
	 * require the base_io to be valid */
	if (ide->base_io == 0)
		return NULL;

	/* IDE I/O is always 8-port aligned */
	if ((ide->base_io & 7) != 0)
		return NULL;

	/* alt I/O if present is always 8-port aligned and at the 6th port */
	if (ide->alt_io != 0 && (ide->alt_io & 1) != 0)
		return NULL;

	/* don't probe if base io already taken */
	if (idelib_by_base_io(ide->base_io) != NULL)
		return NULL;

	irq = ide->irq;

	/* if the alt io conflicts, then don't use it */
	alt_io = ide->alt_io;
	if (alt_io != 0 && idelib_by_alt_io(alt_io) != NULL)
		alt_io = 0;

	/* the alt I/O port is supposed to be readable, and it usually exists as a
	 * mirror of what you read from the status port (+7). and there's no
	 * conceivable reason I know of that the controller would happen to be busy
	 * at this point in execution.
	 *
	 * NTS: we could reset the hard drives here as part of the test, but that might
	 *      not be wise in case the BIOS is cheap and sets up the controller only
	 *      once the way it expects.u */
	if (alt_io != 0 && inp(alt_io) == 0xFF)
		alt_io = 0;

	/* TODO: come up with a more comprehensive IDE I/O port test routine.
	 *       but one that reliably detects without changing hard disk state. */
	if (inp(ide->base_io+7) == 0xFF)
		return NULL;

	newide->drive_address = 0;
	newide->pio32_atapi_command = 0; /* always assume ATAPI packet commands are to be written with 16-bit PIO */
	newide->pio_width = IDELIB_PIO_WIDTH_16; /* always default to 16-bit PIO */
	newide->irq_fired = 0;
	newide->irq = irq;
	newide->flags.io_irq_enable = (newide->irq >= 0) ? 1 : 0;	/* unless otherwise known, use the IRQ */
	newide->base_io = ide->base_io;
	newide->alt_io = alt_io;

	idelib_controller_update_taskfile(newide,0xFF/*all registers*/,IDELIB_TASKFILE_SELECTED_UPDATE);

	newide->device_control = 0x08+(newide->flags.io_irq_enable?0x00:0x02); /* can't read device control but we can guess */
	if (ide->alt_io != 0) outp(ide->alt_io,newide->device_control);

	/* construct IDE taskfile from register contents */
	memset(&newide->taskfile,0,sizeof(newide->taskfile));
	newide->taskfile[newide->selected_drive].head_select = newide->head_select;

	return newide;
}

void ide_vlb_sync32_pio(struct ide_controller *ide) {
	inp(ide->base_io+2);
	inp(ide->base_io+2);
	inp(ide->base_io+2);
}

void idelib_controller_drive_select(struct ide_controller *ide,unsigned char which/*1=slave 0=master*/,unsigned char head/*CHS mode head value*/,unsigned char mode/*upper 3 bits*/) {
	if (ide == NULL) return;
	ide->head_select = ((which&1)<<4) + (head&0xF) + ((mode&7)<<5);
	outp(ide->base_io+6/*0x1F6*/,ide->head_select);

	/* and let it apply to whatever drive it selects */
	ide->selected_drive = (ide->head_select >> 4) & 1;
	ide->taskfile[ide->selected_drive].head_select = ide->head_select;
}

void idelib_otr_enable_interrupt(struct ide_controller *ide,unsigned char en) { /* "off the record" interrupt control */
	if (en) {
		if (!(ide->device_control&0x02)) /* if nIEN=0 already do nothing */
			return;

		/* force clear IRQ */
		inp(ide->base_io+7);

		/* enable at IDE controller */
		if (ide->alt_io != 0) idelib_write_device_control(ide,0x08); /* nIEN=0 (enable) and not reset */
		else ide->device_control = 0x00; /* fake it */
	}
	else {
		if (ide->device_control&0x02) /* if nIEN=1 already do nothing */
			return;

		/* disable at IDE controller */
		if (ide->alt_io != 0) idelib_write_device_control(ide,0x08+0x02); /* nIEN=1 (disable) and not reset */
		else ide->device_control = 0x02; /* fake it */
	}
}

void idelib_enable_interrupt(struct ide_controller *ide,unsigned char en) {
	if (en) {
		if (!(ide->device_control&0x02)) /* if nIEN=0 already do nothing */
			return;

		ide->flags.io_irq_enable = 1;
		ide->irq_fired = 0;

		/* force clear IRQ */
		inp(ide->base_io+7);

		/* enable at IDE controller */
		if (ide->alt_io != 0) idelib_write_device_control(ide,0x08); /* nIEN=0 (enable) and not reset */
		else ide->device_control = 0x00; /* fake it */
	}
	else {
		if (ide->device_control&0x02) /* if nIEN=1 already do nothing */
			return;

		/* disable at IDE controller */
		if (ide->alt_io != 0) idelib_write_device_control(ide,0x08+0x02); /* nIEN=1 (disable) and not reset */
		else ide->device_control = 0x02; /* fake it */

		ide->flags.io_irq_enable = 0;
		ide->irq_fired = 0;
	}
}

int idelib_controller_apply_taskfile(struct ide_controller *ide,unsigned char portmask,unsigned char flags) {
	if (portmask & 0x80/*base+7*/) {
		/* if the IDE controller is busy we cannot apply the taskfile */
		ide->last_status = inp(ide->alt_io != 0 ? /*0x3F6-ish status*/ide->alt_io : /*status register*/(ide->base_io+7));
		if (ide->last_status&0x80) return -1; /* if the controller is busy, then the other registers have no meaning */
	}
	if (portmask & 0x40/*base+6*/) {
		/* we do not write the head/drive select/mode here but we do note what was written */
		/* if writing the taskfile would select a different drive or head, then error out */
		ide->head_select = inp(ide->base_io+6);
		if (ide->selected_drive != ((ide->head_select >> 4) & 1)) return -1;
		if ((ide->head_select&0x10) != (ide->taskfile[ide->selected_drive].head_select&0x10)) return -1;
		outp(ide->base_io+6,ide->taskfile[ide->selected_drive].head_select);
	}

	if (flags&IDELIB_TASKFILE_LBA48_UPDATE)
		ide->taskfile[ide->selected_drive].assume_lba48 = (flags&IDELIB_TASKFILE_LBA48)?1:0;

	/* and read back the upper 3 bits for the current mode, to detect LBA48 commands.
	 * NTS: Unfortunately some implementations, even though you wrote 0x4x, will return 0xEx regardless,
	 *      so it's not really that easy. In that case, the only way to know is if the flags are set
	 *      in the taskfile indicating that an LBA48 command was issued. */
	if (ide->taskfile[ide->selected_drive].assume_lba48/*we KNOW we issued an LBA48 command*/ ||
		(ide->head_select&0xE0) == 0x40/*LBA48 command was issued (NTS: but most IDE controllers replace with 0xE0, so...)*/) {
		/* write 16 bits to 0x1F2-0x1F5 */
		if (portmask & 0x04) {
			outp(ide->base_io+2,ide->taskfile[ide->selected_drive].sector_count>>8);
			outp(ide->base_io+2,ide->taskfile[ide->selected_drive].sector_count);
		}
		if (portmask & 0x08) {
			outp(ide->base_io+3,ide->taskfile[ide->selected_drive].lba0_3>>8);
			outp(ide->base_io+3,ide->taskfile[ide->selected_drive].lba0_3);
		}
		if (portmask & 0x10) {
			outp(ide->base_io+4,ide->taskfile[ide->selected_drive].lba1_4>>8);
			outp(ide->base_io+4,ide->taskfile[ide->selected_drive].lba1_4);
		}
		if (portmask & 0x20) {
			outp(ide->base_io+5,ide->taskfile[ide->selected_drive].lba2_5>>8);
			outp(ide->base_io+5,ide->taskfile[ide->selected_drive].lba2_5);
		}
	}
	else {
		if (portmask & 0x04)
			outp(ide->base_io+2,ide->taskfile[ide->selected_drive].sector_count);
		if (portmask & 0x08)
			outp(ide->base_io+3,ide->taskfile[ide->selected_drive].lba0_3);
		if (portmask & 0x10)
			outp(ide->base_io+4,ide->taskfile[ide->selected_drive].lba1_4);
		if (portmask & 0x20)	
			outp(ide->base_io+5,ide->taskfile[ide->selected_drive].lba2_5);
	}

	if (portmask & 0x02)
		outp(ide->base_io+1,ide->taskfile[ide->selected_drive].features);

	/* and finally, the command */
	if (portmask & 0x80/*base+7*/) {
		outp(ide->base_io+7,ide->taskfile[ide->selected_drive].command);
		ide->last_status = inp(ide->alt_io != 0 ? /*0x3F6-ish status*/ide->alt_io : /*status register*/(ide->base_io+7));
	}

	return 0;
}

int idelib_controller_update_taskfile(struct ide_controller *ide,unsigned char portmask,unsigned char flags) {
	if (portmask & 0x80/*base+7*/) {
		ide->last_status = inp(ide->alt_io != 0 ? /*0x3F6-ish status*/ide->alt_io : /*status register*/(ide->base_io+7));
		if (ide->last_status&0x80) return -1; /* if the controller is busy, then the other registers have no meaning */
	}

	if (portmask & 0x40/*base+6*/) {
		/* NTS: we do not pay attention to the drive select bit, because some IDE implementations
		 *      will read back the wrong value especially if only a slave is connected to the chain, no master */
		ide->head_select = inp(ide->base_io+6);
		if (ide->alt_io != 0) ide->drive_address = inp(ide->alt_io+1);
		ide->taskfile[ide->selected_drive].head_select = ide->head_select;
	}

	if (flags&IDELIB_TASKFILE_LBA48_UPDATE)
		ide->taskfile[ide->selected_drive].assume_lba48 = (flags&IDELIB_TASKFILE_LBA48)?1:0;

	/* and read back the upper 3 bits for the current mode, to detect LBA48 commands.
	 * NTS: Unfortunately some implementations, even though you wrote 0x4x, will return 0xEx regardless,
	 *      so it's not really that easy. In that case, the only way to know is if the flags are set
	 *      in the taskfile indicating that an LBA48 command was issued. the program using this function
	 *      will presumably let us know if it is by sending:
	 *      flags = IDELIB_TASKFILE_LBA48 | IDELIB_TASKFILE_LBA48_UPDATE */
	if (ide->taskfile[ide->selected_drive].assume_lba48/*we KNOW we issued an LBA48 command*/ ||
		(ide->head_select&0xE0) == 0x40/*LBA48 command was issued*/) {
		/* read back 16 bits from 0x1F2-0x1F5 */
		if (portmask & 0x04) {
			ide->taskfile[ide->selected_drive].sector_count  = (uint16_t)inp(ide->base_io+2) << 8;
			ide->taskfile[ide->selected_drive].sector_count |= (uint16_t)inp(ide->base_io+2);
		}
		if (portmask & 0x08) {
			ide->taskfile[ide->selected_drive].lba0_3  = (uint16_t)inp(ide->base_io+3) << 8;
			ide->taskfile[ide->selected_drive].lba0_3 |= (uint16_t)inp(ide->base_io+3);
		}
		if (portmask & 0x10) {
			ide->taskfile[ide->selected_drive].lba1_4  = (uint16_t)inp(ide->base_io+4) << 8;
			ide->taskfile[ide->selected_drive].lba1_4 |= (uint16_t)inp(ide->base_io+4);
		}
		if (portmask & 0x20) {
			ide->taskfile[ide->selected_drive].lba2_5  = (uint16_t)inp(ide->base_io+5) << 8;
			ide->taskfile[ide->selected_drive].lba2_5 |= (uint16_t)inp(ide->base_io+5);
		}
	}
	else {
		if (portmask & 0x04)
			ide->taskfile[ide->selected_drive].sector_count = (uint16_t)inp(ide->base_io+2);
		if (portmask & 0x08)
			ide->taskfile[ide->selected_drive].lba0_3 = (uint16_t)inp(ide->base_io+3);
		if (portmask & 0x10)
			ide->taskfile[ide->selected_drive].lba1_4 = (uint16_t)inp(ide->base_io+4);
		if (portmask & 0x20)
			ide->taskfile[ide->selected_drive].lba2_5 = (uint16_t)inp(ide->base_io+5);
	}

	if (portmask & 0x02)
		ide->taskfile[ide->selected_drive].error = (uint16_t)inp(ide->base_io+1);

	return 0;
}

struct ide_taskfile *idelib_controller_get_taskfile(struct ide_controller *ide,int which) {
	if (which < 0) which = ide->selected_drive;
	else if (which >= 2) return NULL;
	return &ide->taskfile[which];
}

void idelib_read_pio16(unsigned char *buf,unsigned int len,struct ide_controller *ide) {
	unsigned int lws = len >> 1;

	len &= 1;
	while (lws != 0) {
		*((uint16_t*)buf) = inpw(ide->base_io); /* from data port */
		buf += 2;
		lws--;
	}
	if (len != 0) *buf = inp(ide->base_io);
}

void idelib_write_pio16(unsigned char *buf,unsigned int len,struct ide_controller *ide) {
	unsigned int lws = len >> 1;

	len &= 1;
	while (lws != 0) {
		outpw(ide->base_io,*((uint16_t*)buf)); /* to data port */
		buf += 2;
		lws--;
	}
	if (len != 0) outp(ide->base_io,*buf);
}

void idelib_discard_pio16(unsigned int len,struct ide_controller *ide) {
	unsigned int lws = len >> 1;

	len &= 1;
	while (lws != 0) {
		inpw(ide->base_io); /* from data port */
		lws--;
	}
	if (len != 0) inp(ide->base_io);
}

void idelib_read_pio32(unsigned char *buf,unsigned int len,struct ide_controller *ide) {
	unsigned int lws = len >> 2;

	len &= 3;
	while (lws != 0) {
		*((uint32_t*)buf) = inpd(ide->base_io); /* from data port */
		buf += 4;
		lws--;
	}
	if (len & 2) {
		*((uint16_t*)buf) = inpw(ide->base_io);
		buf += 2;
	}
	if (len & 1) *buf = inp(ide->base_io);
}

void idelib_write_pio32(unsigned char *buf,unsigned int len,struct ide_controller *ide) {
	unsigned int lws = len >> 2;

	len &= 3;
	while (lws != 0) {
		outpd(ide->base_io,*((uint32_t*)buf)); /* to data port */
		buf += 4;
		lws--;
	}
	if (len & 2) {
		outpw(ide->base_io,*((uint16_t*)buf));
		buf += 2;
	}
	if (len & 1) outp(ide->base_io,*buf);
}

void idelib_discard_pio32(unsigned int len,struct ide_controller *ide) {
	unsigned int lws = len >> 2;

	len &= 3;
	while (lws != 0) {
		inpd(ide->base_io); /* from data port */
		lws--;
	}
	if (len & 2) inpw(ide->base_io);
	if (len & 1) inp(ide->base_io);
}

void idelib_discard_pio_general(unsigned int lw,struct ide_controller *ide,unsigned char pio_width) {
	if (pio_width == 0)
		pio_width = ide->pio_width;

	if (pio_width >= 32) {
		if (pio_width == 33) ide_vlb_sync32_pio(ide);
		idelib_discard_pio32(lw,ide);
	}
	else {
		idelib_discard_pio16(lw,ide);
	}
}

void idelib_read_pio_general(unsigned char *buf,unsigned int lw,struct ide_controller *ide,unsigned char pio_width) {
	if (pio_width == 0)
		pio_width = ide->pio_width;

	if (pio_width >= 32) {
		if (pio_width == 33) ide_vlb_sync32_pio(ide);
		idelib_read_pio32(buf,lw,ide);
	}
	else {
		idelib_read_pio16(buf,lw,ide);
	}
}

void idelib_write_pio_general(unsigned char *buf,unsigned int lw,struct ide_controller *ide,unsigned char pio_width) {
	if (pio_width == 0)
		pio_width = ide->pio_width;

	if (pio_width >= 32) {
		if (pio_width == 33) ide_vlb_sync32_pio(ide);
		idelib_write_pio32(buf,lw,ide);
	}
	else {
		idelib_write_pio16(buf,lw,ide);
	}
}

int idelib_controller_atapi_prepare_packet_command(struct ide_controller *ide,unsigned char features,unsigned int bytecount) {
	struct ide_taskfile *tsk;

	if (ide == NULL) return -1;
	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
	tsk->features = features;				/* 0x1F1 */
	tsk->sector_count = 0; /* unused? */			/* 0x1F2 */
	tsk->lba0_3 = 0; /* unused */				/* 0x1F3 */
	tsk->lba1_4 = bytecount & 0xFF;				/* 0x1F4 */
	tsk->lba2_5 = bytecount >> 8;				/* 0x1F5 */
	tsk->command = 0xA0;	/* ATAPI packet command */	/* 0x1F7 */
	return 0;
}

void idelib_controller_atapi_write_command(struct ide_controller *ide,unsigned char *buf,unsigned int len/*Only "12" is supported!*/) {
	unsigned int i;

	if (len > 12) len = 12; /* max 12 bytes (6 words) */

	if (ide->pio_width >= IDELIB_PIO_WIDTH_32 && ide->pio32_atapi_command) {
		if (ide->pio_width == IDELIB_PIO_WIDTH_32_VLB) ide_vlb_sync32_pio(ide);

		len = (len + 3) / 4; /* command bytes transmitted in DWORDs */
		for (i=0;i < 3 && i < len;i++) /* 3x4 = 12 */
			outpd(ide->base_io+0,((uint32_t*)buf)[i]);
		for (;i < 3;i++)
			outpd(ide->base_io+0,0/*pad*/);
	}
	else {
		len = (len + 1) / 2; /* command bytes transmitted in WORDs */
		for (i=0;i < 6 && i < len;i++) /* 6x2 = 12 */
			outpw(ide->base_io+0,((uint16_t*)buf)[i]);
		for (;i < 6;i++)
			outpw(ide->base_io+0,0/*pad*/);
	}
}

int idelib_controller_update_atapi_state(struct ide_controller *ide) {
	struct ide_taskfile *tsk;

	if (ide == NULL) return -1;
	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
	tsk->sector_count = inp(ide->base_io+2);
	return 0;
}

int idelib_controller_read_atapi_state(struct ide_controller *ide) {
	struct ide_taskfile *tsk;

	if (ide == NULL) return -1;
	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
	return (tsk->sector_count&3);
}

int idelib_controller_update_atapi_drq(struct ide_controller *ide) {
	return idelib_controller_update_taskfile(ide,0x30/*base_io+4-5*/,0);
}

unsigned int idelib_controller_read_atapi_drq(struct ide_controller *ide) {
	struct ide_taskfile *tsk;

	if (ide == NULL) return 0;
	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);
	return ((tsk->lba1_4&0xFF) + ((tsk->lba2_5&0xFF) << 8));
}

