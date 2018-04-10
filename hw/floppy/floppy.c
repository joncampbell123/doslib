
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* DMA controller */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/dos/doswin.h>
#include <hw/floppy/floppy.h>

int8_t                          floppy_controllers_enable_2nd = 0;

struct floppy_controller        floppy_controllers[MAX_FLOPPY_CONTROLLER];
int8_t                          floppy_controllers_init = -1;

/* standard I/O ports for floppy controllers */
struct floppy_controller		floppy_standard_isa[2] = {
	/*base, IRQ, DMA*/
	{0x3F0, 6,   2},
	{0x370, 6,   2}
};

struct floppy_controller *floppy_get_standard_isa_port(int x) {
	if (x < 0 || x >= (sizeof(floppy_standard_isa)/sizeof(floppy_standard_isa[0]))) return NULL;
    if (x != 0 && !floppy_controllers_enable_2nd) return NULL;
	return &floppy_standard_isa[x];
}

struct floppy_controller *floppy_get_controller(int x) {
	if (x < 0 || x >= MAX_FLOPPY_CONTROLLER) return NULL;
	if (floppy_controllers[x].base_io == 0) return NULL;
	return &floppy_controllers[x];
}

struct floppy_controller *alloc_floppy_controller() {
	unsigned int i=0;

	while (i < MAX_FLOPPY_CONTROLLER) {
		if (floppy_controllers[i].base_io == 0)
			return &floppy_controllers[i];
	}

	return NULL;
}

void floppy_controller_read_ps2_status(struct floppy_controller *i) {
	if (i->ps2_mode) {
		i->ps2_status[0] = inp(i->base_io+0);
		i->ps2_status[1] = inp(i->base_io+1);
	}
	else {
		i->ps2_status[0] = i->ps2_status[1] = 0xFF;
	}
}

void floppy_controller_set_data_transfer_rate(struct floppy_controller *i,unsigned char rsel) {
	if (rsel > 3) return;
	floppy_controller_write_CCR(i,(i->control_cfg & ~3) + rsel); /* change bits [1:0] */
}

void floppy_controller_drive_select(struct floppy_controller *i,unsigned char drv) {
	if (drv > 3) return;

	i->digital_out &= ~0x03;
	i->digital_out |= drv;
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

void floppy_controller_set_motor_state(struct floppy_controller *i,unsigned char drv,unsigned char set) {
	if (drv > 3) return;

	i->digital_out &= ~(0x10 << drv);
	i->digital_out |= (set?(0x10 << drv):0x00);
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

void floppy_controller_enable_dma(struct floppy_controller *i,unsigned char set) {
	if (i->dma < 0) set = 0;
	i->use_dma = !!set;

	/* 82077AA refers to this bit as "!DMAGATE", and only in AT mode.
	 * Setting it gates both the IRQ and DMA. It says it has no effect
	 * in PS/2 mode. Doh! */
	i->digital_out &= ~0x08;
	i->digital_out |= ((i->use_irq || i->use_dma)?0x08:0x00);
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

void floppy_controller_enable_irq(struct floppy_controller *i,unsigned char set) {
	if (i->irq < 0) set = 0;
	i->use_irq = !!set;

	/* 82077AA refers to this bit as "!DMAGATE", and only in AT mode.
	 * Setting it gates both the IRQ and DMA. It says it has no effect
	 * in PS/2 mode. Doh! */
	i->digital_out &= ~0x08;
	i->digital_out |= ((i->use_irq || i->use_dma)?0x08:0x00);
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

void floppy_controller_enable_irqdma_gate_otr(struct floppy_controller *i,unsigned char set) {
	unsigned char c;

	c = i->digital_out;
	c &= ~0x08;
	c |= (set?0x08:0x00);
	outp(i->base_io+2,c);			/* 0x3F2 Digital Output Register */
}

void floppy_controller_set_reset(struct floppy_controller *i,unsigned char set) {
	i->digital_out &= ~0x04;
	i->digital_out |= (set?0x00:0x04);	/* bit is INVERTED (0=reset 1=normal) */
	outp(i->base_io+2,i->digital_out);	/* 0x3F2 Digital Output Register */
}

struct floppy_controller *floppy_controller_probe(struct floppy_controller *i) {
	struct floppy_controller *ret = NULL;
	uint8_t t1;

	if (i == NULL) return NULL;
	if (i->base_io == 0) return NULL;

	/* is anything there? the best we can hope for is to probe the I/O port range and see if SOMETHING responds */
	t1 = inp(i->base_io+4); /* main status register (we can't assume the ability to read the digital output register, the data reg might be 0xFF) */
	if (t1 == 0xFF) return NULL;

	ret = alloc_floppy_controller();
	if (ret == NULL) return NULL;
	memset(ret,0,sizeof(*ret));

	ret->base_io = i->base_io;
	if (i->irq >= 2 && i->irq <= 15)
		ret->irq = i->irq;
	else
		ret->irq = -1;

	if (i->dma >= 0 && i->dma <= 7)
		ret->dma = i->dma;
	else
		ret->dma = -1;

	ret->use_dma = (ret->dma >= 0);
	ret->use_irq = (ret->irq >= 0);

	/* assume middle-of-the-road defaults */
	ret->current_srt = 8;		/* 4/8/14/16ms for 1M/500K/300K/250K */
	ret->current_hut = 8;		/* 64/128/213/256 for 1M/500K/300K/250K */
	ret->current_hlt = 0x40;	/* 64/128/213/256 for 1M/500K/300K/250K */

	/* assume controller has ND (Non-DMA) and EIS (implied seek) turned off.
	 * most BIOSes do that. */

	/* if something appears at 0x3F0-0x3F1, assume "PS/2 mode".
	 * there are finer intricate details where the meaning of some bits
	 * completely change or invert their meaning between PS/2 and Model 30,
	 * so we don't concern ourself with them, we only care that there's
	 * something there and we can let the program using this lib figure it out. */
	t1 = inp(ret->base_io+0);
	t1 &= inp(ret->base_io+1);
	if (t1 != 0xFF) ret->ps2_mode = 1;

	/* what about the AT & PS/2 style CCR & DIR */
	t1 = inp(ret->base_io+7);
	if (t1 != 0xFF) ret->at_mode = 1;

	/* and ... guess */
	floppy_controller_write_DOR(ret,0x04+(ret->use_irq?0x08:0x00));	/* most BIOSes: DMA/IRQ enable, !reset, motor off, drive A select */
	floppy_controller_read_status(ret);

	/* is the Digital Out port readable? */
	t1 = inp(ret->base_io+2);
	if (t1 == ret->digital_out) ret->digital_out_rw = 1;

	floppy_controller_read_DIR(ret);

	return ret;
}

int init_floppy_controller_lib() {
	if (floppy_controllers_init < 0) {
		memset(floppy_controllers,0,sizeof(floppy_controllers));
		floppy_controllers_init = 0;

		cpu_probe();
		probe_dos();
		detect_windows();

		/* do NOT under any circumstances talk directly to the floppy from under Windows! */
		if (windows_mode != WINDOWS_NONE) return (floppy_controllers_init=0);

		/* init OK */
		floppy_controllers_init = 1;
	}

	return floppy_controllers_init;
}

void free_floppy_controller_lib() {
}

