
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8250/8250.h>
#include <hw/isapnp/isapnp.h>
#include <hw/parport/parport.h>

static unsigned char devnode_raw[4096];

int is_parport_or_compat_pnp_device(struct isa_pnp_device_node far *devn) {
	if (devn->type_code[0] == 7 && devn->type_code[1] == 1)
		return 1;

	return 0;
}

void pnp_parport_scan() {
	/* most of the time the serial ports are BIOS controlled and on the motherboard.
	 * they usually don't even show up in a PnP isolation scan. so we have to use
	 * the "get device nodes" functions of the PnP BIOS. */
	{
		struct isa_pnp_device_node far *devn;
		unsigned int ret_ax,nodesize=0xFFFF;
		unsigned char numnodes=0xFF;
		struct isapnp_tag tag;
		unsigned char node;

		printf("Enumerating PnP system device nodes...\n");

		ret_ax = isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize);
		if (ret_ax == 0 && numnodes != 0xFF && nodesize < sizeof(devnode_raw)) {
			/* NTS: How nodes are enumerated in the PnP BIOS: set node = 0, pass address of node
			 *      to BIOS. BIOS, if it returns node information, will also overwrite node with
			 *      the node number of the next node, or with 0xFF if this is the last one.
			 *      On the last one, stop enumerating. */
			for (node=0;node != 0xFF;) {
				unsigned char far *rsc;
				int eport = -1; /* ECP port */
				int eplen = -1;
				int port = -1;
				int plen = -1;
				int type = -1;
				int irq = -1;
				int dma = -1;

				/* apparently, start with 0. call updates node to
				 * next node number, or 0xFF to signify end */
				ret_ax = isa_pnp_bios_get_sysdev_node(&node,devnode_raw,
						ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW);

				if (ret_ax != 0)
					break;

				devn = (struct isa_pnp_device_node far*)devnode_raw;
				if (!is_parport_or_compat_pnp_device(devn))
					continue;

				/* there are three config blocks, one after the other.
				 *  [allocated]
				 *  [possible]
				 *  [??]
				 * since we're not a configuration utility, we only care about the first one */
				rsc = devnode_raw + sizeof(*devn);
				if (isapnp_read_tag(&rsc,devnode_raw + devn->size,&tag)) {
					do {
						if (tag.tag == ISAPNP_TAG_END) /* end tag */
							break;

						switch (tag.tag) {
/* NTS: This code will NOT match I/O ports if they are above 0x400 because some BIOSes
 * have ECP compliant ports and announce both the base I/O and the ECP (+0x400) I/O.
 * We only want the base I/O */
/*---------------------------------------------------------------------------------*/
case ISAPNP_TAG_IRQ_FORMAT: {
	struct isapnp_tag_irq_format far *x = (struct isapnp_tag_irq_format far*)tag.data;
	unsigned int i;
	for (i=0;i < 16 && irq < 0;i++) {
		if (x->irq_mask & (1U << (unsigned int)i))
			irq = i;
	}
} break;
case ISAPNP_TAG_IO_PORT: {
	struct isapnp_tag_io_port far *x = (struct isapnp_tag_io_port far*)tag.data;
	if (x->length >= 3) {
		if (port < 0) {
			port = x->min_range;
			plen = x->length;
		}
		else if (eport < 0) {
			eport = x->min_range;
			eplen = x->length;
		}
	}
} break;
case ISAPNP_TAG_FIXED_IO_PORT: {
	struct isapnp_tag_fixed_io_port far *x = (struct isapnp_tag_fixed_io_port far*)tag.data;
	if (x->length >= 3) {
		if (port < 0) {
			port = x->base;
			plen = x->length;
		}
		else if (eport < 0) {
			eport = x->base;
			eplen = x->length;
		}
	}
} break;
case ISAPNP_TAG_DMA_FORMAT: {
	struct isapnp_tag_dma_format far *x = (struct isapnp_tag_dma_format far*)tag.data;
	unsigned int i;
	for (i=0;i < 8;i++) {
		if (x->dma_mask & (1U << (unsigned int)i))
			dma = i;
	}
} break;
/*---------------------------------------------------------------------------------*/
						};
					} while (isapnp_read_tag(&rsc,devnode_raw + devn->size,&tag));
				}

				if (port < 0)
					continue;

				if (eport > 0) {
					if (eport < port) {
						int x;
						x = port; port = eport; eport = x;
						x = plen; plen = eplen; eplen = x;
					}
				}

				if (eport > 0) {
					if ((port+0x400) == eport)
						type = PARPORT_ECP;
				}
				else if (dma >= 0) {
					/* ECP ports use a DMA channel */
					type = PARPORT_ECP;
				}

				if (plen >= 8) { /* if the I/O range is 8 long, then it might be EPP */
					if (type == PARPORT_ECP)
						type = PARPORT_ECP_AND_EPP;
					else
						type = PARPORT_EPP;
				}

				if (add_pnp_parport(port,irq,dma,type))
					printf("Found PnP port @ 0x%03x IRQ %d DMA %d assume '%s'\n",port,irq,dma,type >= 0 ? parport_type_str[type] : "?");
			}
		}
	}
}

