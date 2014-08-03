
#include <hw/cpu/cpu.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_IDE_CONTROLLER 8

struct ide_controller_flags {
	uint8_t		io_irq_enable:1;	/* if set, disk I/O should use IRQ to signal completion */
	uint8_t		_reserved_:1;
};

struct ide_controller {
	uint16_t			base_io;		/* 0x1F0, 0x170, etc */
	uint16_t			alt_io;			/* 0x3F6, 0x3F7, etc */
	volatile uint16_t		irq_fired;		/* IRQ counter */
	int8_t				irq;
	struct ide_controller_flags	flags;
	uint8_t				last_status;
};

extern const struct ide_controller	ide_isa_standard[4];
extern struct ide_controller		ide_controller[MAX_IDE_CONTROLLER];
extern int8_t				idelib_init;

void idelib_controller_update_status(struct ide_controller *ide);
const struct ide_controller *idelib_get_standard_isa_port(int i);
int idelib_controller_is_busy(struct ide_controller *ide);
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

