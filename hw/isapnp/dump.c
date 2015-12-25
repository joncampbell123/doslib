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
static unsigned char devnode_raw[4096];

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

static int dump_isa_pnp_bios_devnodes() {
	const char *dev_class = NULL,*dev_type = NULL,*dev_stype = NULL,*dev_itype = NULL;
	struct isa_pnp_device_node far *devn;
	unsigned int ret_ax,nodesize=0xFFFF;
	unsigned char numnodes=0xFF;
	char dev_product[256] = {0};
	unsigned char current_node;
	struct isapnp_tag tag;
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

		fclose(fp_raw);
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

	free_isa_pnp_bios();
	return 0;
}

