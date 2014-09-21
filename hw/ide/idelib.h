
#ifndef __DOSLIB_HW_IDE_IDELIB_H
#define __DOSLIB_HW_IDE_IDELIB_H

#include <hw/cpu/cpu.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_IDE_CONTROLLER 16

struct ide_controller_flags {
	uint8_t		io_irq_enable:1;	/* if set, disk I/O should use IRQ to signal completion */
	uint8_t		_reserved_:1;
};

struct ide_taskfile {
	union {							/* 0x1F1. error/feature */
		uint8_t			error;
		uint8_t			features;		
	};
	uint16_t			sector_count;		/* 0x1F2. double wide for LBA48 */
	union {
		uint16_t		lba0_3;			/* 0x1F3. if LBA48, contains [15:8 = byte 3] and [7:0 = byte 0] of LBA,
								          or contains only byte 0 of LBA, or sector number of CHS */
		uint16_t		chs_sector;
	};
	union {
		uint16_t		lba1_4;			/* 0x1F4. if LBA48, contains [15:8 = byte 4] and [7:0 = byte 1] of LBA,
								          or contains only byte 1 of LBA, or [7:0] of cylinder in CHS */
		uint16_t		chs_cylinder_low;
	};
	union {
		uint16_t		lba2_5;			/* 0x1F5. if LBA48, contains [15:8 = byte 5] and [7:0 = byte 2] of LBA,
								          or contains only byte 2 of LBA, or [15:8] of cylinder in CHS */
		uint16_t		chs_cylinder_high;
	};
	uint8_t				head_select;		/* 0x1F6. mode [bits 7:5] select [bit 4] head [bits 3:0] */
	uint8_t				command;		/* 0x1F7. command(W) and status(R) */
	uint8_t				status;			/* also port 0x3F6 status */
	uint8_t				assume_lba48:1;		/* assume LBA48 response (i.e. 0x1F2-0x1F5 are 16-bit wide) */
	uint8_t				_reserved_:7;
};

struct ide_controller {
	uint16_t			base_io;		/* 0x1F0, 0x170, etc */
	uint16_t			alt_io;			/* 0x3F6, 0x3F7, etc */
	volatile uint16_t		irq_fired;		/* IRQ counter */
	int8_t				irq;
	struct ide_controller_flags	flags;
	struct ide_taskfile		taskfile[2];		/* one per drive */
	uint8_t				head_select;
	uint8_t				device_control;		/* 0x3F6. control bits last written */
	uint8_t				drive_address;		/* 0x3F7. drive select and head */
	uint8_t				last_status;
	uint8_t				pio_width;		/* PIO width (16=16-bit 32=32-bit 33=32-bit VLB key) */
	uint8_t				selected_drive:1;	/* which drive is selected */
	uint8_t				pio32_atapi_command:1;	/* if set, allow 32-bit PIO when sending ATAPI command */
	uint8_t				_reserved_:6;
};

enum {
	IDELIB_PIO_WIDTH_DEFAULT=0,
	IDELIB_PIO_WIDTH_16=16,
	IDELIB_PIO_WIDTH_32=32,
	IDELIB_PIO_WIDTH_32_VLB=33
};

extern const struct ide_controller	ide_isa_standard[2];
extern struct ide_controller		ide_controller[MAX_IDE_CONTROLLER];
extern int8_t				idelib_init;

void idelib_controller_update_status(struct ide_controller *ide);
const struct ide_controller *idelib_get_standard_isa_port(int i);

int idelib_controller_is_busy(struct ide_controller *ide);
int idelib_controller_is_error(struct ide_controller *ide);
int idelib_controller_is_drq_ready(struct ide_controller *ide);
int idelib_controller_is_drive_ready(struct ide_controller *ide);

enum {
	IDELIB_DRIVE_SELECT_MASTER=0,
	IDELIB_DRIVE_SELECT_SLAVE=1
};

enum {
	IDELIB_DRIVE_SELECT_MODE_LBA48=0x02,		/* LBA48 (2 << 5) = 0x40 */
	IDELIB_DRIVE_SELECT_MODE_CHS=0x05,		/* LBA (5 << 5) = 0xA0 */
	IDELIB_DRIVE_SELECT_MODE_LBA=0x07		/* CHS (7 << 5) = 0xE0 */
};

enum {
	IDELIB_TASKFILE_LBA48=0x01,
	IDELIB_TASKFILE_LBA48_UPDATE=0x02,
	IDELIB_TASKFILE_SELECTED_UPDATE=0x04		/* allow update function to change "selected drive" pointer */
};

void idelib_controller_drive_select(struct ide_controller *ide,unsigned char which/*1=slave 0=master*/,unsigned char head/*CHS mode head value*/,unsigned char mode/*upper 3 bits*/);
int idelib_controller_apply_taskfile(struct ide_controller *ide,unsigned char portmask,unsigned char flags);
int idelib_controller_update_taskfile(struct ide_controller *ide,unsigned char portmask,unsigned char flags);
struct ide_taskfile *idelib_controller_get_taskfile(struct ide_controller *ide,int which);
void idelib_read_pio16(unsigned char *buf,unsigned int len,struct ide_controller *ide);
void idelib_read_pio32(unsigned char *buf,unsigned int len,struct ide_controller *ide);
void idelib_read_pio_general(unsigned char *buf,unsigned int lw,struct ide_controller *ide,unsigned char pio_width);
void idelib_write_pio16(unsigned char *buf,unsigned int len,struct ide_controller *ide);
void idelib_write_pio32(unsigned char *buf,unsigned int len,struct ide_controller *ide);
void idelib_write_pio_general(unsigned char *buf,unsigned int lw,struct ide_controller *ide,unsigned char pio_width);
void idelib_discard_pio16(unsigned int len,struct ide_controller *ide);
void idelib_discard_pio32(unsigned int len,struct ide_controller *ide);
void idelib_discard_pio_general(unsigned int lw,struct ide_controller *ide,unsigned char pio_width);

void idelib_otr_enable_interrupt(struct ide_controller *ide,unsigned char en);
void idelib_enable_interrupt(struct ide_controller *ide,unsigned char en);
int idelib_controller_allocated(struct ide_controller *ide);
struct ide_controller *idelib_probe(struct ide_controller *ide);
struct ide_controller *idelib_get_controller(int i);
struct ide_controller *idelib_new_controller();
struct ide_controller *idelib_by_base_io(uint16_t io);
struct ide_controller *idelib_by_alt_io(uint16_t io);
struct ide_controller *idelib_by_irq(int8_t irq);
void ide_vlb_sync32_pio(struct ide_controller *ide);
int idelib_controller_atapi_prepare_packet_command(struct ide_controller *ide,unsigned char features,unsigned int bytecount);
void idelib_controller_atapi_write_command(struct ide_controller *ide,unsigned char *buf,unsigned int len/*Only "12" is supported!*/);
int idelib_controller_update_atapi_state(struct ide_controller *ide);
int idelib_controller_read_atapi_state(struct ide_controller *ide);
int idelib_controller_update_atapi_drq(struct ide_controller *ide);
unsigned int idelib_controller_read_atapi_drq(struct ide_controller *ide);

static inline void idelib_controller_ack_irq(struct ide_controller *ide) {
	/* reading port 0x3F6 (normal method of status update) does not clear IRQ.
	 * reading port 0x1F7 reads status AND clears IRQ */
	ide->last_status = inp(ide->base_io+7);
	if (!(ide->last_status&0x80)) ide->taskfile[ide->selected_drive].status = ide->last_status;
}

static inline void idelib_controller_write_command(struct ide_controller *ide,unsigned char cmd) { /* WARNING: does not check controller or drive readiness */
	outp(ide->base_io+7,cmd);
}

static inline void idelib_controller_reset_irq_counter(struct ide_controller *ide) {
	ide->irq_fired = 0;
}

static inline unsigned char idelib_read_device_control(struct ide_controller *ide) {
	return ide->device_control;
}

static inline void idelib_write_device_control(struct ide_controller *ide,unsigned char byte) {
	ide->device_control = byte;
	outp(ide->alt_io,byte);
}

static inline void idelib_device_control_set_reset(struct ide_controller *ide,unsigned char reset) {
	idelib_write_device_control(ide,(ide->device_control & (~0x06)) | (reset ? 0x04/*SRST*/ : (ide->flags.io_irq_enable?0x00:0x02/*nIEN*/)));
}

static inline int idelib_controller_atapi_data_input_state(struct ide_controller *ide) {
	return (idelib_controller_read_atapi_state(ide) == 2); /* [bit 1] input/output == 1 [bit 0] command/data == 0 */
}

static inline int idelib_controller_atapi_command_state(struct ide_controller *ide) {
	return (idelib_controller_read_atapi_state(ide) == 1); /* [bit 1] input/output == 0 [bit 0] command/data == 1 */
}

static inline int idelib_controller_atapi_complete_state(struct ide_controller *ide) {
	return (idelib_controller_read_atapi_state(ide) == 3); /* [bit 1] input/output == 1 [bit 0] command/data == 1 */
}

void free_idelib();
int init_idelib();

#endif /* __DOSLIB_HW_IDE_IDELIB_H */

