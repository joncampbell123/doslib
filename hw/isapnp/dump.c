/* notes:
 *
 *     Ancient Pentium Pro 200MHZ system:
 *        - Real mode calling works perfectly.
 *        - Protected mode interface crashed until we increased the stack size during
 *          the call to 0x200 bytes (up from 0x100)
 *        - PnP structures reveal some mystery devices at I/O ports 0x78-0x7F and 0x80...
 *          what are they?
 *        - Another mystery device at 0x290-0x297, what is it?
 *        - BIOS settings allow you to specify a Plug & Play OS, and then, if Serial &
 *          Parallel ports are set to "Auto", the BIOS says it doesn't configure them and
 *          leaves it up to the OS to do it. Sure enough, according to the PnP info,
 *          the serial ports show up as being disabled (IO base 0 len 0) waiting to be
 *          set to one of the "possible resources" listed! Neat!
 */

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
#include <hw/8254/8254.h>		/* 8254 timer */
#include <isapnp/isapnp.h>

static unsigned char tmp[128];
static unsigned char devnode_raw[8192];
static char dev_product[256];

static int dump_pnp_entry() {
	FILE *fp;

	printf("ISA PnP BIOS found, dumping info now.\n");

	fp = fopen("PNPENTRY.TXT","w");
	if (!fp) { fprintf(stderr,"Error writing dumpfile\n"); return -1; }

	fprintf(fp,"BIOS Provides PnP structure at F000:%04X\n",(unsigned int)isa_pnp_bios_offset);
	fprintf(fp,"\n");

	memcpy(tmp,isa_pnp_info.signature,4); tmp[4] = 0;
	fprintf(fp,"  Signature:                   %s\n",tmp);

	fprintf(fp,"  Version:                     0x%02x (BCD as version %x.%x)\n",isa_pnp_info.version,isa_pnp_info.version>>4,isa_pnp_info.version&0xF);
	fprintf(fp,"  Length:                      0x%02x bytes\n",isa_pnp_info.length);
	fprintf(fp,"  Control:                     0x%02x\n",isa_pnp_info.control);

	fprintf(fp,"                                 - ");
	switch (isa_pnp_info.control&3) {
		case 0:		fprintf(fp,"Event notification is not supported"); break;
		case 1:		fprintf(fp,"Event notification is handled through polling"); break;
		case 2:		fprintf(fp,"Event notification is asynchronous (at interrupt time)"); break;
		default:	fprintf(fp,"Event notification is ???"); break;
	};
	fprintf(fp,"\n");

	fprintf(fp,"  Event notify flag at:        0x%08lx (physical mem address)\n",(unsigned long)isa_pnp_info.event_notify_flag_addr);
	fprintf(fp,"  Real-mode entry at:          %04X:%04X\n",isa_pnp_info.rm_ent_segment,isa_pnp_info.rm_ent_offset);
	fprintf(fp,"  Protected-mode entry at:     Segment base 0x%08lX offset 0x%04X\n",
			(unsigned long)isa_pnp_info.pm_ent_segment_base,(unsigned int)isa_pnp_info.pm_ent_offset);

	fprintf(fp,"  OEM device ID:               0x%08lX",
			(unsigned long)isa_pnp_info.oem_device_id);
	if (isa_pnp_info.oem_device_id != 0UL) {
		isa_pnp_product_id_to_str(tmp,(uint32_t)isa_pnp_info.oem_device_id);
		fprintf(fp," EISA product ident '%s'",tmp);
	}
	fprintf(fp,"\n");

	fprintf(fp,"  Real-mode data segment:      0x%04X\n",
			(unsigned int)isa_pnp_info.rm_ent_data_segment);
	fprintf(fp,"  Protected mode data segment: Segment base 0x%08lx\n",
			(unsigned long)isa_pnp_info.pm_ent_data_segment_base);

	fclose(fp);
	return 0;
}

static void fprintf_devnode_pnp_decode(FILE *fp_raw,unsigned char far *rsc,unsigned char far *rsc_fence,unsigned int liter_limit) {
	struct isapnp_tag tag;
	unsigned char liter;
	unsigned int i;

	for (liter=0;liter <= liter_limit;liter++) {
		if (!isapnp_read_tag(&rsc,rsc_fence,&tag))
			break;

		fprintf(fp_raw,"* ISA PnP %s resource configuration block\n",isapnp_config_block_str[liter]);

		do {
			fprintf(fp_raw,"  %14s: ",isapnp_tag_str(tag.tag));
			if (tag.tag == ISAPNP_TAG_START_DEPENDENT_FUNCTION || tag.tag == ISAPNP_TAG_END_DEPENDENT_FUNCTION)
				fprintf(fp_raw,"---------------------");

			if (tag.tag == ISAPNP_TAG_END) { /* end tag */
				fprintf(fp_raw,"\n");
				break;
			}

			switch (tag.tag) {
				case ISAPNP_TAG_PNP_VERSION: {
					struct isapnp_tag_pnp_version far *x = (struct isapnp_tag_pnp_version far*)tag.data;
					fprintf(fp_raw,"PnP v%u.%u Vendor=0x%02X\n",x->pnp>>4,x->pnp&0xF,x->vendor);
					} break;
				case ISAPNP_TAG_COMPATIBLE_DEVICE_ID: {
					struct isapnp_tag_compatible_device_id far *x = (struct isapnp_tag_compatible_device_id far*)tag.data;
					isa_pnp_product_id_to_str(tmp,x->id);
					fprintf(fp_raw,"0x%08lx '%s'\n",(unsigned long)x->id,tmp);
					} break;
				case ISAPNP_TAG_IRQ_FORMAT: {
					struct isapnp_tag_irq_format far *x = (struct isapnp_tag_irq_format far*)tag.data;
					fprintf(fp_raw,"HTE=%u LTE=%u HTL=%u LTL=%u\n",x->hte,x->lte,x->htl,x->ltl);
					fprintf(fp_raw,"             IRQ lines: ");
					for (i=0;i < 16;i++) {
						if (x->irq_mask & (1U << (unsigned int)i))
							fprintf(fp_raw,"%u ",i);
					}
					fprintf(fp_raw,"\n");
					} break;
				case ISAPNP_TAG_DMA_FORMAT: {
					struct isapnp_tag_dma_format far *x = (struct isapnp_tag_dma_format far*)tag.data;
					if (x->bus_master) fprintf(fp_raw,"BusMaster ");
					if (x->byte_count) fprintf(fp_raw,"ByteCount ");
					if (x->word_count) fprintf(fp_raw,"WordCount ");

					fprintf(fp_raw,"Speed=%s xferPref=%s\n",isapnp_dma_speed_str[x->dma_speed],isapnp_dma_xfer_preference_str[x->xfer_preference]);

					fprintf(fp_raw,"             DMA channels: ");
					for (i=0;i < 8;i++) {
						if (x->dma_mask & (1U << (unsigned int)i))
							fprintf(fp_raw,"%u ",i);
					}
					fprintf(fp_raw,"\n");
					} break;
				case ISAPNP_TAG_START_DEPENDENT_FUNCTION: {
					if (tag.len > 0) {
						struct isapnp_tag_start_dependent_function far *x = (struct isapnp_tag_start_dependent_function far*)tag.data;
						fprintf(fp_raw,"Priority=%s",isapnp_sdf_priority_str(x->priority));
					}
					fprintf(fp_raw,"\n");
					} break;
				case ISAPNP_TAG_END_DEPENDENT_FUNCTION:
					break;
				case ISAPNP_TAG_IO_PORT: {
					struct isapnp_tag_io_port far *x = (struct isapnp_tag_io_port far*)tag.data;
					fprintf(fp_raw,"Decode %ubit, range 0x%04X-0x%04X, align 0x%02X, length: 0x%02X",
						x->decode_16bit?16:10,x->min_range,x->max_range,x->alignment,x->length);
					fprintf(fp_raw,"\n");
					} break;
				case ISAPNP_TAG_FIXED_IO_PORT: {
					struct isapnp_tag_fixed_io_port far *x = (struct isapnp_tag_fixed_io_port far*)tag.data;
					fprintf(fp_raw,"At %04X, length %02X\n",x->base,x->length);
					} break;
				case ISAPNP_TAG_LOGICAL_DEVICE_ID: {
					struct isapnp_tag_logical_device_id far *x = (struct isapnp_tag_logical_device_id far*)tag.data;
					isa_pnp_product_id_to_str(tmp,x->logical_device_id);
					fprintf(fp_raw,"0x%08lx '%s'\n",(unsigned long)x->logical_device_id,tmp);
					} break;
				case ISAPNP_TAG_FIXED_MEMORY_LOCATION_32: {
					struct isapnp_tag_fixed_memory_location_32 far *x = (struct isapnp_tag_fixed_memory_location_32 far*)tag.data;
					fprintf(fp_raw,"%s %s %s %s %s %s\n",
						x->writeable ? "Writeable" : "Read only",
						x->cacheable ? "Cacheable" : "No-cache",
						x->support_hi_addr ? "Hi-addr" : "",
						isapnp_fml32_miosize_str[x->memory_io_size],
						x->shadowable ? "Shadowable" : "No-shadow",
						x->expansion_rom ? "Expansion ROM" : "RAM");
					fprintf(fp_raw,"             Base 0x%08lX len 0x%08lX\n",
						(unsigned long)x->base,
						(unsigned long)x->length);
					} break;
				case ISAPNP_TAG_ANSI_ID_STRING: {
					i = tag.len;
					if ((i+1) > sizeof(tmp)) i = sizeof(tmp) - 1;
					if (i != 0) _fmemcpy(tmp,tag.data,i);
					tmp[i] = 0;
					fprintf(fp_raw,"\"%s\"\n",tmp);
					} break;
				default:
					fprintf(fp_raw,"(not implemented)\n");
					break;
			};
		} while (isapnp_read_tag(&rsc,rsc_fence,&tag));
	}
}

static int dump_isa_pnp_bios_devnodes() {
	const char *dev_class = NULL,*dev_type = NULL,*dev_stype = NULL,*dev_itype = NULL;
	struct isa_pnp_device_node far *devn;
	unsigned int ret_ax,nodesize=0xFFFF;
	unsigned char numnodes=0xFF;
	unsigned char current_node;
	unsigned char node;
	FILE *fp,*fp_raw;

	printf("Asking for sys device nodes..."); fflush(stdout);
	ret_ax = isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize);
	if (ret_ax != 0 || numnodes == 0xFF || nodesize == 0 || nodesize > sizeof(devnode_raw)) {
		printf(" failed. AX=0x%04X\n",ret_ax);
		return 0;
	}
	printf(" ok. numnodes=%u largest_node_size=%u\n",
		numnodes,nodesize);

	fp = fopen("PNPBIOSN.TXT","w");
	if (!fp) { fprintf(stderr,"Error writing dumpfile\n"); return -1; }
	fprintf(fp,"BIOS reports %u device nodes, largest node is %u bytes\n\n",
		numnodes,nodesize);

	for (node=0;node != 0xFF;) {
		current_node = node;
		ret_ax = isa_pnp_bios_get_sysdev_node(&node,devnode_raw,ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW);
		if (ret_ax != 0) break;

		devn = (struct isa_pnp_device_node far*)devnode_raw;
		if (devn->size < 11 || devn->size > sizeof(devnode_raw)) {
			printf("skipping: node=0x%02x invalid size %u\n",
				current_node,devn->size);
			continue;
		}

		printf("dumping: node=0x%02x size=%u handle=0x%02x\n",
			current_node,devn->size,devn->handle);

		sprintf(tmp,"PNPBDV%02X.BIN",(unsigned char)current_node);
		fp_raw = fopen((char*)tmp,"wb");
		if (fp_raw == NULL) {
			fprintf(stderr,"Unable to write %s\n",tmp);
			fclose(fp);
			return 1;
		}
		fwrite(devnode_raw,devn->size,1,fp_raw);
		fclose(fp_raw);

		/* then decoded output */
		sprintf(tmp,"PNPBDV%02X.TXT",(unsigned char)current_node);
		fp_raw = fopen((char*)tmp,"w");
		if (fp_raw == NULL) {
			fprintf(stderr,"Unable to write %s\n",tmp);
			fclose(fp);
			return 1;
		}

		isa_pnp_product_id_to_str(tmp,devn->product_id);
		fprintf(fp,"PnP BIOS device: node=0x%02x size=%u handle=%u product_id=0x%08lx (%s) DeviceType=0x%02X,0x%02X,0x%02X\n",
			current_node,
			devn->size,
			devn->handle,
			(unsigned long)devn->product_id,tmp,
			(unsigned char)devn->type_code[0],
			(unsigned char)devn->type_code[1],
			(unsigned char)devn->type_code[2]);
		fprintf(fp_raw,	"PnP BIOS device:\n"
				"  Node:                  0x%02x\n"
				"  Size:                  %u bytes\n"
				"  Handle:                %u\n"
				"  Product ID:            0x%08lx '%s'\n"
				"  Device type:           0x%02X,0x%02X,0x%02X\n",
			current_node,
			devn->size,
			devn->handle,
			(unsigned long)devn->product_id,tmp,
			(unsigned char)devn->type_code[0],
			(unsigned char)devn->type_code[1],
			(unsigned char)devn->type_code[2]);

		dev_product[0] = 0;
		dev_type = isa_pnp_device_type(devn->type_code,&dev_stype,&dev_itype);
		dev_class = isa_pnp_device_category(devn->product_id);
		isa_pnp_device_description(dev_product,devn->product_id);

		if (dev_class != NULL)
			fprintf(fp_raw,	"  Device category:       %s\n",dev_class);
		if (dev_product[0] != 0)
			fprintf(fp_raw,	"  Device description:    %s\n",dev_product);
		if (dev_type != NULL)
			fprintf(fp_raw,	"  Device:                %s\n",dev_type);
		if (dev_stype != NULL)
			fprintf(fp_raw,	"  Device subtype:        %s\n",dev_stype);
		if (dev_itype != NULL)
			fprintf(fp_raw,	"  Device interface type: %s\n",dev_itype);

		fprintf(fp_raw,	"  Attributes:            0x%04x\n",devn->attributes);
		if (devn->attributes & ISAPNP_DEV_ATTR_CANT_DISABLE)
			fprintf(fp_raw,	"                           - Device cannot be disabled\n");
		if (devn->attributes & ISAPNP_DEV_ATTR_CANT_CONFIGURE)
			fprintf(fp_raw,	"                           - Device is not configurable\n");
		if (devn->attributes & ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_OUTPUT)
			fprintf(fp_raw, "                           - Device is capable of being primary output device\n");
		if (devn->attributes & ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_INPUT)
			fprintf(fp_raw,	"                           - Device is capable of being primary input device\n");
		if (devn->attributes & ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_IPL)
			fprintf(fp_raw,	"                           - Device is capable of being primary IPL device\n");
		if (devn->attributes & ISAPNP_DEV_ATTR_DOCKING_STATION_DEVICE)
			fprintf(fp_raw,	"                           - Device is a docking station device\n");
		if (devn->attributes & ISAPNP_DEV_ATTR_REMOVEABLE_STATION_DEVICE)
			fprintf(fp_raw,	"                           - Device is a removeable station device\n");

		switch (ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE(devn->attributes)) {
			case ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_ONLY_NEXT_BOOT:
				fprintf(fp_raw,	"                           - Device can only be configured for next boot\n"); break;
			case ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_AT_RUNTIME:
				fprintf(fp_raw,	"                           - Device can be configured at runtime\n"); break;
			case ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_ONLY_RUNTIME:
				fprintf(fp_raw,	"                           - Device can only be configured at runtime\n"); break;
		};

		/* the three configuration blocks are in this one buffer, one after the other */
		fprintf_devnode_pnp_decode(fp_raw,devnode_raw + sizeof(*devn),devnode_raw + devn->size,2);
		fclose(fp_raw);
	}

	fclose(fp);
	return 0;
}

static int dump_isa_pnp_device_enum() {
	struct isapnp_pnp_isa_cfg far *nfo;
	unsigned int ok=0,i,j,csn;
	unsigned int ret_ax;
	unsigned int ff=0;
	unsigned char cc;
	FILE *fp,*fp_raw;
	char vendorid[9];

	printf("Asking for ISA PnP device enumeration..."); fflush(stdout);

	_fmemset(devnode_raw,0,sizeof(*nfo));
	ret_ax = isa_pnp_bios_get_pnp_isa_cfg(devnode_raw);
	nfo = (struct isapnp_pnp_isa_cfg far*)devnode_raw;

	if (ret_ax != 0) {
		printf(" failed. nothing returned\n");
		return 0;
	}

	printf(" ok. rev=%u csn=%u pnp_port=0x%X\n",
		nfo->revision,
		nfo->total_csn,
		nfo->isa_pnp_port);

	fp = fopen("PNPISAEN.TXT","w");
	if (!fp) { fprintf(stderr,"Error writing dumpfile\n"); return -1; }

	fprintf(fp,"ISA PnP device enumeration.\n");
	fprintf(fp,"BIOS struct revision:      %u\n",nfo->revision);
	fprintf(fp,"BIOS Total CSNs:           %u\n",nfo->total_csn);
	fprintf(fp,"BIOS ISA PnP I/O port:     0x%X\n",nfo->isa_pnp_port);

	if (nfo->revision == 1 && nfo->total_csn != 0 && nfo->isa_pnp_port != 0) {
		isapnp_read_data = nfo->isa_pnp_port;
		for (i=0;i < 256;i++) {
			isa_pnp_init_key();
			isa_pnp_wake_csn(i);
			isa_pnp_set_read_data_port(isapnp_read_data);
			isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
		}

		csn = 1;
		do {
			isa_pnp_init_key();
			isa_pnp_wake_csn(csn);

			ok = 0;
			if (!ok) {
				isa_pnp_write_address(0x06); /* CSN */
				if (isa_pnp_read_data() == csn) ok = 1;
			}

			if (ok) {
				printf("Reading ISA PnP CSN %u\n",csn);

				isa_pnp_write_address(0x06); /* CSN */
				isa_pnp_write_data(csn);

				isa_pnp_write_address(0x07); /* logical dev index */
				isa_pnp_write_data(0); /* main dev */

				isa_pnp_write_address(0x06); /* CSN */
				if (isa_pnp_read_data() != csn)
					fprintf(stderr," - Warning, cannot readback CSN[1]\n");

				sprintf(tmp,"PNPISA%02X.BIN",csn);
				fp_raw = fopen(tmp,"wb");
				if (fp_raw == NULL) {
					fprintf(stderr,"Cannot open %s for write\n",tmp);
					fclose(fp);
					return 1;
				}

				/* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
				/* if we don't, then we only read back the resource data */
				isa_pnp_init_key();
				isa_pnp_wake_csn(csn);

				for (j=0;j < 9;j++) vendorid[j] = isa_pnp_read_config();
				fprintf(fp,"CSN 0x%02X: Serial/Vendor ID: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					csn,
					vendorid[0],vendorid[1],vendorid[2],vendorid[3],
					vendorid[4],vendorid[5],vendorid[6],vendorid[7],
					vendorid[8]);

				{
					isa_pnp_product_id_to_str(tmp,*((uint32_t*)vendorid));
					fprintf(fp,"    Product ID 0x%08lx '%s'\n",*((uint32_t*)vendorid),tmp);
				}

				fwrite(vendorid,9,1,fp_raw);

				/* try to read the data, as raw as possible */
				i=0;
				ff=0;
				while (i < sizeof(devnode_raw)) {
					cc = isa_pnp_read_config();
					if (cc == 0xFF) {
						ff++;
						if (ff >= 256) break; /* a run of 256 FF's means end of data */
					}
					else {
						ff=0;
					}

					devnode_raw[i++] = cc;
				}

				if (i > 0) {
					assert(i <= sizeof(devnode_raw));
					fwrite(devnode_raw,i,1,fp_raw);
				}

				fclose(fp_raw);

				sprintf(tmp,"PNPISA%02X.TXT",csn);
				fp_raw = fopen(tmp,"w");
				if (fp_raw == NULL) {
					fprintf(stderr,"Cannot open %s for write\n",tmp);
					fclose(fp);
					return 1;
				}

				fprintf(fp_raw,"CSN 0x%02X: Serial/Vendor ID: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					csn,
					vendorid[0],vendorid[1],vendorid[2],vendorid[3],
					vendorid[4],vendorid[5],vendorid[6],vendorid[7],
					vendorid[8]);

				{
					isa_pnp_product_id_to_str(tmp,*((uint32_t*)vendorid));
					fprintf(fp_raw,"    Product ID 0x%08lx '%s'\n\n",*((uint32_t*)vendorid),tmp);
				}

				fprintf_devnode_pnp_decode(fp_raw,devnode_raw,devnode_raw + i,0);
				fclose(fp_raw);
			}

			/* return back to "wait for key" state */
			isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
			csn++;
		} while (csn < 255);
	}

	fclose(fp);
	return 0;
}

int main(int argc,char **argv) {
	printf("ISA Plug & Play dump program\n");

	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
	}
	if (!find_isa_pnp_bios()) {
		printf("ISA PnP BIOS not found\n");
		return 1;
	}

	if (dump_pnp_entry()) return 1;
	if (dump_isa_pnp_bios_devnodes()) return 1;
	if (dump_isa_pnp_device_enum()) return 1;

	free_isa_pnp_bios();
	return 0;
}

