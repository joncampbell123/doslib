
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

struct ide_controller		ide_controller[MAX_IDE_CONTROLLER];
int8_t				idelib_init = -1;

const struct ide_controller ide_isa_standard[4] = {
	/*base alt fired irq flags*/
	{0x1F0,0x3F6,0,14,{0}},
	{0x170,0x376,0,15,{0}},
	{0x1E8,0x3EE,0,11,{0}},	/* <- fixme: is this right? */
	{0x168,0x36E,0,10,{0}}	/* <- fixme; is this right? */
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

int idelib_controller_is_busy(struct ide_controller *ide) {
	unsigned char c;

	if (ide == NULL) return 0;
	c = inp(ide->alt_io != 0 ? /*0x3F6-ish status*/ide->alt_io : /*status register*/(ide->base_io+7));
	return (c&0x80);
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

	newide->irq_fired = 0;
	newide->irq = irq;
	newide->flags.io_irq_enable = (newide->irq >= 0) ? 1 : 0;	/* unless otherwise known, use the IRQ */
	newide->base_io = ide->base_io;
	newide->alt_io = alt_io;
	return newide;
}

void ide_vlb_sync32_pio(struct ide_controller *ide) {
	inp(ide->base_io+2);
	inp(ide->base_io+2);
	inp(ide->base_io+2);
}

