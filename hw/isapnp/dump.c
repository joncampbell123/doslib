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

static unsigned char tmp[4096];

int main(int argc,char **argv) {
	FILE *fp=NULL;

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

	printf("ISA PnP BIOS found, dumping info now.\n");
	{
		fp = fopen("PNPENTRY.TXT","wb");
		if (!fp) { fprintf(stderr,"Error writing dumpfile\n"); return 1; }

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
	}

	free_isa_pnp_bios();
	return 0;
}

