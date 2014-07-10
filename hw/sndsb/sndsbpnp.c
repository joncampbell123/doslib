/* TODO: Drag out the old Pentium 120MHz laptop you have in storage. It appears
 *       to have an ESS PnP Audiodrive chipset which you should add recognition
 *       for here. */

/* Additional support functions for Sound Blaster 16 ISA Plug & Play based cards.
 * This is compiled into a separate library (sndsbpnp.lib) so that a programmer
 * can include it only if they want to handle the cards directly. In most cases,
 * such as a game, the DOS user is expected to use an external utility such as
 * the Creative PnP utilities to setup and assign resources to the Sound Blaster
 * in a way that works with traditional DOS applications and/or the sndsb library.
 * The ISAPNP library can be a bit top heavy and can add significant bloat to
 * your program especially if you don't need it.
 *
 * This code requires at link time: 
 *     sndsb     Sound Blaster library
 *     isapnp    ISA Plug & Play support library */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>

int isa_pnp_is_sound_blaster_compatible_id(uint32_t id,char const **whatis) {
	int r=0;

	/* if it's a creative product... */
	if (ISAPNP_ID_FMATCH(id,'C','T','L')) {
		if (ISAPNP_ID_LMATCH(id,0x0070)) { /* CTL0070 */
			/* For ref (configuration regs):
			 *   IO[0]  = Base I/O port
			 *   IO[1]  = MPU I/O port
			 *   IO[2]  = OPL3 I/O port
			 *   IRQ[0] = Interrupt request line
			 *   DMA[0] = 8-bit DMA channel
			 *   DMA[1] = 16-bit DMA channel
			 */
			*whatis = "Creative ViBRA16C PnP";
			r = 1;
		}
		else if (ISAPNP_ID_LMATCH(id,0x00C3)) { /* CTL00C3 */
			/* For ref (configuration regs):
			 *   IO[0]  = Base I/O port
			 *   IO[1]  = MPU I/O port
			 *   IO[2]  = OPL3 I/O port
			 *   IRQ[0] = Interrupt request line
			 *   DMA[0] = 8-bit DMA channel
			 *   DMA[1] = 16-bit DMA channel
			 */
			*whatis = "Creative AWE64 PnP";
			r = 1;
		}
		else if (ISAPNP_ID_LMATCH(id,0x00B2)) { /* CTL00B2 */
			/* For ref (configuration regs):
			 *   IO[0]  = Base I/O port
			 *   IO[1]  = MPU I/O port
			 *   IO[2]  = OPL3 I/O port
			 *   IRQ[0] = Interrupt request line
			 *   DMA[0] = 8-bit DMA channel
			 *   DMA[1] = 16-bit DMA channel
			 */
			*whatis = "Creative AWE64 Gold PnP";
			r = 1;
		}
		else if (ISAPNP_ID_LMATCH(id,0x00F0)) { /* CTL00F0 */
			/* For ref (configuration regs):
			 *   IO[0]  = Base I/O port
			 *   IO[1]  = MPU I/O port
			 *   IO[2]  = OPL3 I/O port
			 *   IRQ[0] = Interrupt request line
			 *   DMA[0] = 8-bit DMA channel
			 *   DMA[1] = 16-bit DMA channel
			 */
			*whatis = "Creative ViBRA16X/XV PnP";
			r = 1;
		}
	}

	/* Microsoft Virtual PC SB16 PnP emulation? */
	if (ISAPNP_ID('T','B','A',0x0,0x3,0xB,0x0) == id) { /* TBA03B0 */
		*whatis = "Virtual PC Sound Blaster 16 PnP";
		r = 1;
	}

	return r;
}

int isa_pnp_iobase_typical_sb(uint16_t io) {
	return ((io&0xF) == 0) && (io >= 0x210 && io <= 0x280);
}

int isa_pnp_iobase_typical_mpu(uint16_t io) {
	return ((io&0xF) == 0) && (io == 0x300 || io == 0x330);
}

/* NTS: The caller is expected to pnp_wake_scn() then siphon off the device id */
int isa_pnp_sound_blaster_get_resources(uint32_t id,unsigned char csn,struct sndsb_ctx *cx) {
	cx->baseio = 0;
	cx->gameio = 0;
	cx->aweio = 0;
	cx->oplio = 0;
	cx->mpuio = 0;
	cx->dma16 = -1;
	cx->dma8 = -1;
	cx->irq = -1;

	if (ISAPNP_ID_FMATCH(id,'C','T','L')) {
		/* Creative SB PnP cards are fairly consistent on the resource layout. If you have one that this
		 * code fails to match, feel free to add it here */
		if (	ISAPNP_ID_LMATCH(id,0x0070) || /* CTL0070 Creative ViBRA16C PnP */
			ISAPNP_ID_LMATCH(id,0x00B2) || /* CTL00B2 Creative AWE64 Gold PnP */
			ISAPNP_ID_LMATCH(id,0x00C3) || /* CTL00C3 Creative AWE64 PnP */
			ISAPNP_ID_LMATCH(id,0x00F0)) { /* CTL00F0 Creative ViBRA16X/XV PnP */
			/* For ref (configuration regs):
			 *   IO[0]  = Base I/O port
			 *   IO[1]  = MPU I/O port
			 *   IO[2]  = OPL3 I/O port
			 *   IRQ[0] = Interrupt request line
			 *   DMA[0] = 8-bit DMA channel
			 *   DMA[1] = 16-bit DMA channel
			 */
			isa_pnp_write_address(0x07);	/* log device select */
			isa_pnp_write_data(0x00);	/* main device */

			cx->baseio = isa_pnp_read_io_resource(0);
			if (cx->baseio == 0 || cx->baseio == 0xFFFF) cx->baseio = 0;
			cx->mpuio = isa_pnp_read_io_resource(1);
			if (cx->mpuio == 0 || cx->mpuio == 0xFFFF) cx->mpuio = 0;
			cx->oplio = isa_pnp_read_io_resource(2);
			if (cx->oplio == 0 || cx->oplio == 0xFFFF) cx->oplio = 0;
			cx->dma8 = isa_pnp_read_dma(0);
			if ((cx->dma8&7) == 4) cx->dma8 = -1;
			cx->dma16 = isa_pnp_read_dma(1);
			if ((cx->dma16&7) == 4) cx->dma16 = -1;
			cx->irq = isa_pnp_read_irq(0);
			if ((cx->irq&0xF) == 0) cx->irq = -1;

			/* logical device #1: gameport */
			isa_pnp_write_address(0x07);	/* log device select */
			isa_pnp_write_data(0x01);	/* main device */

			cx->gameio = isa_pnp_read_io_resource(0);
			if (cx->gameio == cx->baseio || cx->gameio == 0 || cx->gameio == 0xFFFF) cx->gameio = 0;

			/* SB AWE: logical device #2: wavetable */
			if (ISAPNP_ID_LMATCH(id,0x00C3) || ISAPNP_ID_LMATCH(id,0x00B2)) {
				isa_pnp_write_address(0x07);	/* log device select */
				isa_pnp_write_data(0x02);	/* main device */

				cx->aweio = isa_pnp_read_io_resource(0);
				if (cx->aweio == cx->baseio || cx->aweio == 0 || cx->aweio == 0xFFFF) cx->aweio = 0;
			}
		}
	}

	if (cx->baseio == 0) return ISAPNPSB_NO_RESOURCES;
	return 1;
}

int sndsb_try_isa_pnp(uint32_t id,uint8_t csn) {
	struct sndsb_ctx *cx;
	int ok = 0;

	cx = sndsb_alloc_card();
	if (cx == NULL) return ISAPNPSB_TOO_MANY_CARDS;

	/* now extract ISA resources and init the card */
	ok = isa_pnp_sound_blaster_get_resources(id,csn,cx);
	if (ok < 1) {
		sndsb_free_card(cx);
		return ok;
	}

	/* try to init. note the init_card() function needs to know this is a PnP device. */
	cx->pnp_id = id;
	cx->pnp_csn = csn;
	if (!sndsb_init_card(cx)) {
		sndsb_free_card(cx);
		return ISAPNPSB_FAILED;
	}

	isa_pnp_is_sound_blaster_compatible_id(cx->pnp_id,&cx->pnp_name);
	return 1;
}

