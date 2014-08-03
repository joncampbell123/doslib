
#include <hw/cpu/cpu.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_IDE_CONTROLLER 8

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
	union {							/* 0x1F7. command(W) and status(R) */
		uint8_t			command;
		uint8_t			status;
	};
};

struct ide_controller {
	uint16_t			base_io;		/* 0x1F0, 0x170, etc */
	uint16_t			alt_io;			/* 0x3F6, 0x3F7, etc */
	volatile uint16_t		irq_fired;		/* IRQ counter */
	int8_t				irq;
	struct ide_controller_flags	flags;
	struct ide_taskfile		taskfile;
	uint8_t				last_status;
};

extern const struct ide_controller	ide_isa_standard[4];
extern struct ide_controller		ide_controller[MAX_IDE_CONTROLLER];
extern int8_t				idelib_init;

void idelib_controller_update_status(struct ide_controller *ide);
const struct ide_controller *idelib_get_standard_isa_port(int i);

int idelib_controller_is_busy(struct ide_controller *ide);
int idelib_controller_is_error(struct ide_controller *ide);
int idelib_controller_is_drq_ready(struct ide_controller *ide);
int idelib_controller_is_drive_ready(struct ide_controller *ide);

void idelib_controller_drive_select(struct ide_controller *ide,unsigned char which,unsigned char head);
void idelib_enable_interrupt(struct ide_controller *ide,unsigned char en);
int idelib_controller_allocated(struct ide_controller *ide);
struct ide_controller *idelib_probe(struct ide_controller *ide);
struct ide_controller *idelib_get_controller(int i);
struct ide_controller *idelib_new_controller();
struct ide_controller *idelib_by_base_io(uint16_t io);
struct ide_controller *idelib_by_alt_io(uint16_t io);
struct ide_controller *idelib_by_irq(int8_t irq);
void ide_vlb_sync32_pio(struct ide_controller *ide);

void free_idelib();
int init_idelib();

