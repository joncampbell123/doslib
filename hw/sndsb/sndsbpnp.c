
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

	/* if it's an ESS product... */
	if (ISAPNP_ID_FMATCH(id,'E','S','S')) {
		if (ISAPNP_ID_LMATCH(id,0x0100)) { /* ESS0100 */
			/* On an old Pentium 120MHz laptop this does not show
			 * up in PnP enumeration but is instead reported as
			 * a device node by the PnP BIOS.
			 *
			 *      Port 0x220 length 0x10
			 *      Port 0x388 length 0x04
			 *      IRQ 9
			 *      DMA channel 1 */
			*whatis = "ES688 Plug And Play AudioDrive";
			r = 1;
		}
	}
	/* ESS/Compaq product */
	else if (ISAPNP_ID_FMATCH(id,'C','P','Q')) {
		if (ISAPNP_ID_LMATCH(id,0xB040)) { /* CPQB040 ES1887 (Compaq) */
			/* Seen on a late 90's Compaq */
			*whatis = "ES1887 Compaq";
			r = 1;
		}
	}
	/* Yamaha product */
	else if (ISAPNP_ID_FMATCH(id,'Y','M','H')) {
		if (ISAPNP_ID_LMATCH(id,0x0021)) { /* YMH0021 OPL3-SAx (TODO: Which one? SA2 or SA3) */
			/* Seen on a late 90's Toshiba laptop (1997-ish) */
			*whatis = "Yamaha OPL3-SAx";
			r = 1;
		}
	}
	/* if it's a creative product... */
	else if (ISAPNP_ID_FMATCH(id,'C','T','L')) {
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

void pnp_opl3sax_ioport_chk(struct sndsb_ctx *cx,unsigned int iop,uint16_t base) {
	switch (iop) {
		case 0:	cx->baseio = base; break;
		case 1: cx->wssio = base; break;
		case 2: cx->oplio = base; break;
		case 3: cx->mpuio = base; break;
		case 4: cx->opl3sax_controlio = base; break;
	};
}

int isa_pnp_bios_sound_blaster_get_resources(uint32_t id,unsigned char node,struct isa_pnp_device_node far *devn,unsigned int devn_size,struct sndsb_ctx *cx) {
	unsigned char far *rsc, far *rf;
	struct isapnp_tag tag;
	unsigned int i,iop=0;

	cx->baseio = 0;
	cx->gameio = 0;
	cx->aweio = 0;
	cx->oplio = 0;
	cx->mpuio = 0;
	cx->dma16 = -1;
	cx->dma8 = -1;
	cx->irq = -1;

	if (devn_size <= sizeof(*devn)) return 0;
	rsc = (unsigned char far*)devn + sizeof(*devn);
	rf = (unsigned char far*)devn + devn_size;

	/* NTS: Yamaha OPL3-SAx chipset:
	 *         - There are two DMA channels, the first one (we care about) is for Sound Blaster emulation
	 *         - The card also offers a Windows Sound System interface at 0x530, which we don't care about because we focus on Sound Blaster.
	 *         - I have no plans for this code to use the WSS interface because this library's focus is and should remain on the Sound Blaster
	 *           interface. I will use the WSS interface however for minor things like reading Sound Blaster emulation IRQ and DMA assignment.
	 *           I will write a separate library for Windows Sound System type audio playback. If your code wants to support both, it can link
	 *           to both libraries and pick the best one for your needs. */
	if (	(ISAPNP_ID_FMATCH(id,'E','S','S') && ISAPNP_ID_LMATCH(id,0x0100)) || /* ESS0100 ES688 Plug And Play AudioDrive */
		(ISAPNP_ID_FMATCH(id,'C','P','Q') && ISAPNP_ID_LMATCH(id,0xB040)) || /* CPQB040 ESS1887 Compaq */
		(ISAPNP_ID_FMATCH(id,'Y','M','H') && ISAPNP_ID_LMATCH(id,0x0021))    /* YMH0021 OPL3-SAx (TODO: Which one? SA2 or SA3) */) {
		do {
			if (!isapnp_read_tag(&rsc,rf,&tag))
				break;
			if (tag.tag == ISAPNP_TAG_END)
				break;

			switch (tag.tag) {
				case ISAPNP_TAG_IO_PORT: {
					struct isapnp_tag_io_port far *x = (struct isapnp_tag_io_port far*)tag.data;
					if (ISAPNP_ID_FMATCH(id,'Y','M','H') && ISAPNP_ID_LMATCH(id,0x0021)) { /* OPL3-SAx */
						pnp_opl3sax_ioport_chk(cx,iop,x->min_range);
					}
					else {
						if (x->min_range == x->max_range) {
							if (x->min_range >= 0x210 && x->min_range <= 0x260 && (x->min_range&0xF) == 0 && x->length == 0x10)
								cx->baseio = x->min_range;
							else if (x->min_range >= 0x388 && x->min_range <= 0x38C && (x->min_range&3) == 0 && x->length == 4)
								cx->oplio = x->min_range;
							else if (x->min_range >= 0x300 && x->min_range <= 0x330 && (x->min_range&0xF) == 0 && x->length >= 2 && x->length <= 4)
								cx->mpuio = x->min_range;
						}
					}
					iop++;
				} break;
				case ISAPNP_TAG_FIXED_IO_PORT: {
					struct isapnp_tag_fixed_io_port far *x = (struct isapnp_tag_fixed_io_port far*)tag.data;
					if (ISAPNP_ID_FMATCH(id,'Y','M','H') && ISAPNP_ID_LMATCH(id,0x0021)) { /* OPL3-SAx */
						pnp_opl3sax_ioport_chk(cx,iop,x->base);
					}
					else {
						if (x->base >= 0x210 && x->base <= 0x280 && (x->base&0xF) == 0 && x->length == 0x10)
							cx->baseio = x->base;
						else if (x->base >= 0x388 && x->base <= 0x38C && (x->base&3) == 0 && x->length == 4)
							cx->oplio = x->base;
						else if (x->base >= 0x300 && x->base <= 0x330 && (x->base&0xF) == 0 && x->length >= 2 && x->length <= 4)
							cx->mpuio = x->base;
					}
					iop++;
					} break;
				case ISAPNP_TAG_IRQ_FORMAT: {
					struct isapnp_tag_irq_format far *x = (struct isapnp_tag_irq_format far*)tag.data;
					for (i=0;i < 16;i++) {
						if (x->irq_mask & (1U << (unsigned int)i)) { /* NTS: PnP devices usually support odd IRQs like IRQ 9 */
							if (cx->irq < 0) cx->irq = i;
						}
					}
					} break;
				case ISAPNP_TAG_DMA_FORMAT: {
					struct isapnp_tag_dma_format far *x = (struct isapnp_tag_dma_format far*)tag.data;
					if (ISAPNP_ID_FMATCH(id,'Y','M','H') && ISAPNP_ID_LMATCH(id,0x0021)) {
						/* the OPL3-SAx lists two DMA channels, the second one marked as bytecount and
						 * related to Sound Blaster emulation (Toshiba BIOS) */
						for (i=0;i < 8;i++) {
							if (x->dma_mask & (1U << (unsigned int)i)) {
								if (cx->dma8 < 0 && i < 4 && x->byte_count) cx->dma8 = i;
							}
						}
					}
					else { /* ESS */
						/* the PnP BIOS on the Compaq unit I have for the ESS 1868 lists two
						 * DMA channels (second one being DMA channel 5) but only the first
						 * one is used for 8 and 16-bit (so then the second one is the 2nd
						 * channel for full duplex?) */
						for (i=0;i < 8;i++) {
							if (x->dma_mask & (1U << (unsigned int)i)) {
								if (cx->dma8 < 0 && i < 4) cx->dma8 = i;
							}
						}
					}
					} break;
			}
		} while (1);

		if (ISAPNP_ID_FMATCH(id,'Y','M','H') && ISAPNP_ID_LMATCH(id,0x0021)) {
			/* The OPL3-SAx does not have a 16-bit DMA channel */
		}
		else {
			if (cx->dma8 >= 0 && cx->dma16 < 0)
				cx->dma16 = cx->dma8;
		}
	}
	else if (ISAPNP_ID_FMATCH(id,'C','T','L')) {
		/* Do nothing: Creative cards are usually probed from the ISA bus and not reported by the BIOS */
	}

	if (cx->baseio == 0) return ISAPNPSB_NO_RESOURCES;
	return 1;
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

	if (ISAPNP_ID_FMATCH(id,'E','S','S')) {
		if (ISAPNP_ID_LMATCH(id,0x0100)) { /* ESS0100 ES688 Plug And Play AudioDrive */
			/* TODO: I don't have any ISA cards of this type, only one integrated into a laptop */
		}
	}
	else if (ISAPNP_ID_FMATCH(id,'C','P','Q')) {
		if (ISAPNP_ID_LMATCH(id,0xB040)) { /* CPQB040 ES1887 (Compaq) */
			/* TODO: I don't have any ISA cards of this type, only one integrated into a laptop */
		}
	}
	else if (ISAPNP_ID_FMATCH(id,'Y','M','H')) {
		if (ISAPNP_ID_LMATCH(id,0x0021)) { /* YMH0021 OPL3-SAx (TODO: Which one? SA2 or SA3) */
			/* TODO: I don't have any ISA cards of this type, only one integrated into a laptop */
		}
	}
	else if (ISAPNP_ID_FMATCH(id,'C','T','L')) {
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

/* try device from ISA PnP BIOS device node (reported by BIOS not probed directly by us) */
int sndsb_try_isa_pnp_bios(uint32_t id,uint8_t node,struct isa_pnp_device_node far *devn,unsigned int devn_size) {
	struct sndsb_ctx *cx;
	int ok = 0;

	cx = sndsb_alloc_card();
	if (cx == NULL) return ISAPNPSB_TOO_MANY_CARDS;

	/* now extract ISA resources and init the card */
	ok = isa_pnp_bios_sound_blaster_get_resources(id,node,devn,devn_size,cx);
	if (ok < 1) {
		sndsb_free_card(cx);
		return ok;
	}

	/* try to init. note the init_card() function needs to know this is a PnP device. */
	cx->pnp_id = id;
	cx->pnp_bios_node = node;
	if (!sndsb_init_card(cx)) {
		sndsb_free_card(cx);
		return ISAPNPSB_FAILED;
	}

	isa_pnp_is_sound_blaster_compatible_id(cx->pnp_id,&cx->pnp_name);
	return 1;
}

/* try device from ISA PnP device by CSN (card select number) */
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

