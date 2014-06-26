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

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <isapnp/isapnp.h>

static unsigned char far devnode_raw[4096];

int main(int argc,char **argv) {
	char tmp[129];
	int c,i;

	printf("ISA Plug & Play test program\n");

	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
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

	printf("Your BIOS provides a ISA PnP structure\n");
	printf("   version=0x%02X length=0x%02X  control=0x%04X  event_notify_flag @ 0x%08lX\n",
		isa_pnp_info.version,		isa_pnp_info.length,
		isa_pnp_info.control,		(unsigned long)isa_pnp_info.event_notify_flag_addr);
	printf("   realmode entry: %04X:%04X pm entry: %08lX + %04X oem device id=0x%08lX\n",
		isa_pnp_info.rm_ent_segment,		isa_pnp_info.rm_ent_offset,
		isa_pnp_info.pm_ent_segment_base,	isa_pnp_info.pm_ent_offset,
		isa_pnp_info.oem_device_id);
	printf("   data segment: realmode: %04X  pm base: %08lX\n",
		isa_pnp_info.rm_ent_data_segment,
		(unsigned long)isa_pnp_info.pm_ent_data_segment_base);
	printf("WARNING: Part of the ISA PnP callback mechanism involves doing direct calls via\n");
	printf("         the entry pointers above. If your BIOS is badly written, this program\n");
	printf("         will crash when asked to do so.\n");
#if TARGET_MSDOS == 32
	printf("This program runs in protected mode, and will use the protected mode interface.\n");
	printf("If you would like to use the real-mode interface, hit 'R' (Shift+R) now.\n");
	printf("Or hit 'W' (Shift+W) to enable 'Windows 95' mode.\n");
#else
	printf("This program runs in real mode, and will use the real mode interface.\n");
#endif
	while ((c=getch()) != 13) {
#if TARGET_MSDOS == 32
		if (c == 'R') {
			if (isa_pnp_pm_use) {
				isa_pnp_pm_use = 0;
				isa_pnp_pm_win95_mode = 0;
				printf("\n\n**** I will use DPMI real-mode calling functions to call into PnP BIOS\n");
			}
		}
		else if (c == 'W') {
			if (isa_pnp_pm_win95_mode == 0) {
				isa_pnp_pm_win95_mode = 1;
				isa_pnp_pm_use = 1;
				printf("\n\n**** I will use DPMI prot-mode calling functions to call into PnP BIOS, Win95 style\n");
			}
		}

#endif
	}

	int10_setmode(3);
	vga_bios_set_80x50_text();

	while (1) {
		vga_write_color(0x07);
		vga_clear();
		vga_moveto(0,0);
		vga_write("1. Get Number of System Device Nodes     2. Get statically alloc resource info\n");
		vga_write("3. ISA PnP configuration                 4. Get ESCD information\n");
		vga_write("5. Send message                          6. Init key and readback\n");
		vga_write("7. Readback                        SHIFT-7. Readback with detail\n");
		vga_write_sync();

		c = getch();
again:		if (c == 27) {
			break;
		}
		else if (c == '&') { /* SHIFT-7 = & */
			if (isapnp_read_data == 0 || isapnp_probe_next_csn == 0) {
				vga_write("I haven't yet asked your BIOS for the PnP read port.\n");
				vga_write("Hit ESC and then use command '3' to init the I/O port from the PnP BIOS\n");
				vga_write("Or type 'D' to reprogram and use the read port at I/O port 0x20F.\n");
				vga_write("Be aware that if you do, and then run the program later with BIOS settings,\n");
				vga_write("that the BIOS settings may not work (because we changed the RD_DATA port)\n");
				c = getch();
				if (c == '3') goto again;
				else if (c == 'D' || c == 'd') {
					isapnp_read_data = 0x20F;
					isapnp_probe_next_csn = 1;
					for (i=0;i < 256;i++) { /* I can't assume that I can write once and set all the cards */
						isa_pnp_init_key();
						isa_pnp_wake_csn(i);
						isa_pnp_set_read_data_port(0x20F);

						/* return back to "wait for key" state */
						isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
					}
				}
			}
			else {
				struct isapnp_tag tag;
				unsigned char far *rsc;
				unsigned char data[256],cc;
				unsigned int ok=0,j,csn;

				vga_write("Listing all CSNs\n");
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
						sprintf(tmp,"index %u:\n",csn);
						vga_write(tmp);

						isa_pnp_write_address(0x06); /* CSN */
						isa_pnp_write_data(csn);

						isa_pnp_write_address(0x07); /* logical dev index */
						isa_pnp_write_data(0); /* main dev */

						isa_pnp_write_address(0x06); /* CSN */
						if (isa_pnp_read_data() != csn)
							vga_write(" - Warning, cannot readback CSN[1]\n");

						/* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
						/* if we don't, then we only read back the resource data */
						isa_pnp_init_key();
						isa_pnp_wake_csn(csn);

						for (j=0;j < 9;j++) data[j] = isa_pnp_read_config();
						sprintf(tmp,"assign %u: %02x %02x %02x %02x %02x %02x %02x %02x %02x ",csn,
							data[0],data[1],data[2],data[3],
							data[4],data[5],data[6],data[7],
							data[8]);
						vga_write(tmp);

						{
							isa_pnp_product_id_to_str(tmp,*((uint32_t*)data));
							vga_write(tmp);
							vga_write("\n");
						}

						do { /* parse until done */
							cc = isa_pnp_read_config();
							data[0] = cc;
							if (cc & 0x80) {
								unsigned int len;
								/* large tag; TODO */
								data[1] = isa_pnp_read_config();
								data[2] = isa_pnp_read_config();
								len = ((unsigned int)data[2] << 8) | (unsigned int)data[1];
								if ((len+3) > sizeof(data)) break;
								for (j=0;(unsigned char)j < len;j++) data[j+3] = isa_pnp_read_config();
							}
							else {
								unsigned char len = cc & 7;
								for (j=0;(unsigned char)j < len;j++) data[j+1] = isa_pnp_read_config();
							}

							rsc = data;
							if (!isapnp_read_tag(&rsc,data+sizeof(data),&tag))
								break;
							if (tag.tag == ISAPNP_TAG_END)
								break;

							sprintf(tmp,"%14s: ",isapnp_tag_str(tag.tag));
							if (tag.tag == ISAPNP_TAG_START_DEPENDENT_FUNCTION ||
								tag.tag == ISAPNP_TAG_END_DEPENDENT_FUNCTION)
								vga_write("---------------------");
							vga_write(tmp);

							switch (tag.tag) {
/*---------------------------------------------------------------------------------*/
case ISAPNP_TAG_PNP_VERSION: {
	struct isapnp_tag_pnp_version far *x = (struct isapnp_tag_pnp_version far*)tag.data;
	sprintf(tmp,"PnP v%u.%u Vendor=0x%02X",x->pnp>>4,x->pnp&0xF,x->vendor);
	vga_write(tmp);
} break;
case ISAPNP_TAG_COMPATIBLE_DEVICE_ID: {
	struct isapnp_tag_compatible_device_id far *x = (struct isapnp_tag_compatible_device_id far*)tag.data;
	isa_pnp_product_id_to_str(tmp,x->id);
	vga_write(tmp);
} break;
case ISAPNP_TAG_IRQ_FORMAT: {
	struct isapnp_tag_irq_format far *x = (struct isapnp_tag_irq_format far*)tag.data;
	sprintf(tmp,"HTE=%u LTE=%u HTL=%u LTL=%u\n",x->hte,x->lte,x->htl,x->ltl);
	vga_write(tmp);
	vga_write("                IRQ: ");
	for (i=0;i < 16;i++) {
		if (x->irq_mask & (1U << (unsigned int)i)) {
			sprintf(tmp,"%u ",i);
			vga_write(tmp);
		}
	}
} break;
case ISAPNP_TAG_DMA_FORMAT: {
	struct isapnp_tag_dma_format far *x = (struct isapnp_tag_dma_format far*)tag.data;
	if (x->bus_master) vga_write("BusMaster ");
	if (x->byte_count) vga_write("ByteCount ");
	if (x->word_count) vga_write("WordCount ");

	sprintf(tmp,"Speed=%s xferPref=%s\n",isapnp_dma_speed_str[x->dma_speed],isapnp_dma_xfer_preference_str[x->xfer_preference]);
	vga_write(tmp);

	vga_write("                DMA channels: ");
	for (i=0;i < 8;i++) {
		if (x->dma_mask & (1U << (unsigned int)i)) {
			sprintf(tmp,"%u ",i);
			vga_write(tmp);
		}
	}
} break;
case ISAPNP_TAG_START_DEPENDENT_FUNCTION: {
	if (tag.len > 0) {
		struct isapnp_tag_start_dependent_function far *x =
			(struct isapnp_tag_start_dependent_function far*)tag.data;
		sprintf(tmp," Priority=%s",isapnp_sdf_priority_str(x->priority));
		vga_write(tmp);
	}
} break;
case ISAPNP_TAG_END_DEPENDENT_FUNCTION:
  break;
case ISAPNP_TAG_IO_PORT: {
	struct isapnp_tag_io_port far *x = (struct isapnp_tag_io_port far*)tag.data;
	sprintf(tmp,"Decode %ubit, range 0x%04X-0x%04X, align 0x%02X, length: 0x%02X",x->decode_16bit?16:10,
		x->min_range,x->max_range,x->alignment,x->length);
	vga_write(tmp);
} break;
case ISAPNP_TAG_FIXED_IO_PORT: {
	struct isapnp_tag_fixed_io_port far *x = (struct isapnp_tag_fixed_io_port far*)tag.data;
	sprintf(tmp,"At %04X, length %02X",x->base,x->length);
	vga_write(tmp);
} break;
case ISAPNP_TAG_LOGICAL_DEVICE_ID: {
	struct isapnp_tag_logical_device_id far *x = (struct isapnp_tag_logical_device_id far*)tag.data;
	isa_pnp_product_id_to_str(tmp,x->logical_device_id);
	vga_write(tmp);
} break;
case ISAPNP_TAG_FIXED_MEMORY_LOCATION_32: {
	struct isapnp_tag_fixed_memory_location_32 far *x =
		(struct isapnp_tag_fixed_memory_location_32 far*)tag.data;
	sprintf(tmp,"%s %s %s %s %s %s\n",
		x->writeable ? "Writeable" : "Read only",
		x->cacheable ? "Cacheable" : "No-cache",
		x->support_hi_addr ? "Hi-addr" : "",
		isapnp_fml32_miosize_str[x->memory_io_size],
		x->shadowable ? "Shadowable" : "No-shadow",
		x->expansion_rom ? "Expansion ROM" : "RAM");
	vga_write(tmp);
	sprintf(tmp,"                Base 0x%08lX len 0x%08lX",
		(unsigned long)x->base,		(unsigned long)x->length);
	vga_write(tmp);
} break;
case ISAPNP_TAG_ANSI_ID_STRING: {
	i = tag.len;
	if ((i+1) > sizeof(tmp)) i = sizeof(tmp) - 1;
	if (i != 0) _fmemcpy(tmp,tag.data,i);
	tmp[i] = 0;
	vga_write(tmp);
} break;
default:
	vga_write("FIXME: not implemented");
break;
/*---------------------------------------------------------------------------------*/
							};

							vga_write("\n");
						} while (1);

						while (getch() != 13);
					}

					/* return back to "wait for key" state */
					isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */

					csn++;
				} while (csn < 255);
				while (getch() != 13);
			}
		}
		else if (c == '7') {
			if (isapnp_read_data == 0 || isapnp_probe_next_csn == 0) {
				vga_write("I haven't yet asked your BIOS for the PnP read port.\n");
				vga_write("Hit ESC and then use command '3' to init the I/O port from the PnP BIOS\n");
				vga_write("Or type 'D' to reprogram and use the read port at I/O port 0x20F.\n");
				vga_write("Be aware that if you do, and then run the program later with BIOS settings,\n");
				vga_write("that the BIOS settings may not work (because we changed the RD_DATA port)\n");
				c = getch();
				if (c == '3') goto again;
				else if (c == 'D' || c == 'd') {
					isapnp_read_data = 0x20F;
					isapnp_probe_next_csn = 1;
					for (i=0;i < 256;i++) { /* I can't assume that I can write once and set all the cards */
						isa_pnp_init_key();
						isa_pnp_wake_csn(i);
						isa_pnp_set_read_data_port(0x20F);

						/* return back to "wait for key" state */
						isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
					}
				}
			}
			else {
				unsigned char data[9];
				unsigned int i,ok=0,j;

				vga_write("Listing all CSNs\n");
				i = 1;
				do {
					isa_pnp_init_key();
					isa_pnp_wake_csn(i);

					ok = 0;
					if (!ok) {
						isa_pnp_write_address(0x06); /* CSN */
						if (isa_pnp_read_data() == i) ok = 1;
					}
			
					if (ok) {
						sprintf(tmp,"index %u:\n",i);
						vga_write(tmp);

						isa_pnp_write_address(0x06); /* CSN */
						isa_pnp_write_data(i);

						isa_pnp_write_address(0x07); /* logical dev index */
						isa_pnp_write_data(0); /* main dev */

						isa_pnp_write_address(0x06); /* CSN */
						if (isa_pnp_read_data() != i)
							vga_write(" - Warning, cannot readback CSN[1]\n");

						/* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
						/* if we don't, then we only read back the resource data */
						isa_pnp_init_key();
						isa_pnp_wake_csn(i);

						for (j=0;j < 9;j++) data[j] = isa_pnp_read_config();
						sprintf(tmp,"assign %u: %02x %02x %02x %02x %02x %02x %02x %02x %02x ",i,
							data[0],data[1],data[2],data[3],
							data[4],data[5],data[6],data[7],
							data[8]);
						vga_write(tmp);

						{
							isa_pnp_product_id_to_str(tmp,*((uint32_t*)data));
							vga_write(tmp);
							vga_write("\n");
						}

						/* dump configuration bytes to prove we can read it */
						/* NTS: the configuration data covers ALL logical devices, you will get the same
						 *      data no matter what logical dev index you use */
						vga_write(" - Config: ");
						for (j=0;j < 22;j++) {
							data[0] = isa_pnp_read_config();
							sprintf(tmp,"%02x ",data[0]);
							vga_write(tmp);
						}
						vga_write("\n");

						while (getch() != 13);
					}

					/* return back to "wait for key" state */
					isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */

					i++;
				} while (i < 255);
				while (getch() != 13);
			}
		}
		else if (c == '6') {
			if (isapnp_read_data == 0 || isapnp_probe_next_csn == 0) {
				vga_write("I haven't yet asked your BIOS for the PnP read port.\n");
				vga_write("Hit ESC and then use command '3' to init the I/O port from the PnP BIOS\n");
				vga_write("Or type 'D' to reprogram and use the read port at I/O port 0x20F.\n");
				vga_write("Be aware that if you do, and then run the program later with BIOS settings,\n");
				vga_write("that the BIOS settings may not work (because we changed the RD_DATA port)\n");
				c = getch();
				if (c == '3') goto again;
				else if (c == 'D' || c == 'd') {
					isapnp_read_data = 0x20F;
					isapnp_probe_next_csn = 1;

					for (i=0;i < 256;i++) {
						isa_pnp_init_key();
						isa_pnp_wake_csn(i);
						isa_pnp_write_address(0x00);	/* RD_DATA port */
						isa_pnp_write_data(0x20F >> 2);
	
						/* return back to "wait for key" state */
						isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
					}
				}
			}
			else {
				unsigned char data[9],cc;
				unsigned int i,ok=0,j;

				vga_write("ISA PnP key (reset CSNs)\n");
				isa_pnp_init_key();
				for (i=1;i < 256;i++) { /* FIXME: "a write to bit 2 causes all CARDS to reset their CSNs to zero" (or is that all log devs)? */
					isa_pnp_wake_csn(i);
					isa_pnp_write_address(0x02);
					isa_pnp_write_data(0x04); /* reset CSN */
				}
				for (i=1;i < 256;i++) { /* FIXME: "a write to bit 1 causes all CARDS to enter wait for key" (or is that all log devs)? */
					isa_pnp_wake_csn(i);
					isa_pnp_write_address(0x02);
					isa_pnp_write_data(0x02); /* wait for key state */
				}

				/* FIXME: This works in Microsoft Virtual PC, but re-selecting the dev later doesn't work? */
				vga_write("Reprogramming all CSNs\n");
				i = 1;
				do {
					isa_pnp_init_key();
					isa_pnp_wake_csn(0); /* any card with CSN=0 */
					ok = isa_pnp_init_key_readback(data);
					if (ok) {
						sprintf(tmp,"assign %u: %02x %02x %02x %02x %02x %02x %02x %02x %02x ",i,
							data[0],data[1],data[2],data[3],
							data[4],data[5],data[6],data[7],
							data[8]);
						vga_write(tmp);

						{
							isa_pnp_product_id_to_str(tmp,*((uint32_t*)data));
							vga_write(tmp);
							vga_write("\n");
						}

						isa_pnp_write_address(0x06); /* CSN */
						isa_pnp_write_data(i);

						isa_pnp_write_address(0x07); /* logical dev index */
						isa_pnp_write_data(0); /* main dev */

						isa_pnp_write_address(0x06); /* CSN */
						if (isa_pnp_read_data() != i)
							vga_write(" - Warning, cannot readback CSN[1]\n");

						isa_pnp_init_key();
						isa_pnp_wake_csn(i);

						isa_pnp_write_address(0x06); /* CSN */
						if (isa_pnp_read_data() != i)
							vga_write(" - Warning, cannot readback CSN[1]\n");

						/* read an discard the vendor, serial number data */
						for (j=0;j < 9;j++) {
							cc = isa_pnp_read_config();
							if (cc != data[j]) {
								vga_write(" - WARNING: Despite key and CSN device does not re-send vendor_id and serial and checksum\n");
								break;
							}
						}

						/* dump configuration bytes to prove we can read it */
						vga_write(" - Config: ");
						for (j=0;j < 22;j++) {
							data[0] = isa_pnp_read_config();
							sprintf(tmp,"%02x ",data[0]);
							vga_write(tmp);
						}
						vga_write("\n");

						i++;
						while (getch() != 13);
					}
					else if (i == 1) { /* if this happened on the first attempt, then the RD_DATA port is conflicting */
						vga_write("WARNING: Failure to read PnP card & configuration on first pass. The RD_DATA\n");
						vga_write("         port may be conflicting with another device on the ISA bus!\n");

						sprintf(tmp,"got: %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
							data[0],data[1],data[2],data[3],
							data[4],data[5],data[6],data[7],
							data[8]);
						vga_write(tmp);

					}

					/* return back to "wait for key" state */
					isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
				} while (i < 255 && ok);
				while (getch() != 13);
			}
		}
		else if (c == '5') {
			unsigned int ret_ax=0;

			vga_write_color(0x07);
			vga_clear();
			vga_moveto(0,0);
			vga_write("1. Power off                   2. PnP OS Active              3. PnP OS Inactive\n");
			c = getch();

			if (c == '1') {
				ret_ax = isa_pnp_bios_send_message(0x0041);	/* POWER_OFF */
			}
			else if (c == '2') {
				ret_ax = isa_pnp_bios_send_message(0x0042);	/* PNP_OS_ACTIVE */
			}
			else if (c == '3') {
				ret_ax = isa_pnp_bios_send_message(0x0043);	/* PNP_OS_INACTIVE */
			}

			vga_write("Result: ");
			sprintf(tmp,"AX=%04x\n",ret_ax);
			vga_write(tmp);
			while (getch() != 13);
		}
		else if (c == '4') {
			unsigned int min_escd_write=0,escd_size=0;
			unsigned long nv_base=0;
			unsigned int ret_ax;

			vga_write("Calling...");
			vga_write_sync();

			ret_ax = isa_pnp_bios_get_escd_info(&min_escd_write,&escd_size,&nv_base);

			vga_write_color(0x07);
			vga_clear();
			vga_moveto(0,0);

			vga_write("BIOS call results:\n");
			sprintf(tmp,"    return value (AX): 0x%04X\n",ret_ax);
			vga_write(tmp);
			vga_write("\n");

			if (ret_ax == 0) {
				sprintf(tmp,"    Min ESCD write: %u\n",min_escd_write);
				vga_write(tmp);

				sprintf(tmp,"    ESCD size: %u\n",escd_size);
				vga_write(tmp);

				sprintf(tmp,"    NV Base: 0x%lX\n",nv_base);
				vga_write(tmp);
			}

			/* TODO: Find an emulator or actual PC where this function is implemented */

			vga_write_sync();
			while (getch() != 13);
		}
		else if (c == '3') {
			struct isapnp_pnp_isa_cfg far *nfo;
			unsigned int ret_ax;
			int c;

			vga_write("Calling...");
			vga_write_sync();

			_fmemset(devnode_raw,0,sizeof(*nfo));
			ret_ax = isa_pnp_bios_get_pnp_isa_cfg(devnode_raw);
			nfo = (struct isapnp_pnp_isa_cfg far*)devnode_raw;

			vga_write_color(0x07);
			vga_clear();
			vga_moveto(0,0);

			vga_write("BIOS call results:\n");
			sprintf(tmp,"    return value (AX): 0x%04X\n",ret_ax);
			vga_write(tmp);
			vga_write("\n");

			if (ret_ax == 0) {
				sprintf(tmp,"    Struct revision: %u\n",nfo->revision);
				vga_write(tmp);
				if (nfo->revision == 1) {
					if (isapnp_probe_next_csn < nfo->total_csn)
						isapnp_probe_next_csn = nfo->total_csn;

					sprintf(tmp,"    Total CSNs: %u\n",nfo->total_csn);
					vga_write(tmp);

					isapnp_read_data = nfo->isa_pnp_port;
					sprintf(tmp,"    ISA PnP port: 0x%X\n",nfo->isa_pnp_port);
					vga_write(tmp);

				}
			}

			vga_write("Press 'D' to set the RD_DATA port to BIOS value (if you changed it earlier)\n");
			vga_write("Press '8' to set the RD_DATA port to 0x20B\n");
			vga_write_sync();
			while ((c=getch()) != 13) {
				if (c == '8') {
					if (isapnp_read_data != 0) {
						vga_write("Programming BIOS value into RD_DATA\n");
						for (i=0;i < 256;i++) { /* I can't assume that I can write once and set all the cards */
							isa_pnp_init_key();
							isa_pnp_wake_csn(i);
							isa_pnp_set_read_data_port(isapnp_read_data = 0x20B);

							/* return back to "wait for key" state */
							isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
						}
					}
				}
				else if (c == 'D') {
					if (isapnp_read_data != 0) {
						vga_write("Programming BIOS value into RD_DATA\n");
						for (i=0;i < 256;i++) { /* I can't assume that I can write once and set all the cards */
							isa_pnp_init_key();
							isa_pnp_wake_csn(i);
							isa_pnp_set_read_data_port(isapnp_read_data);

							/* return back to "wait for key" state */
							isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
						}
					}
				}
			}
		}
		else if (c == '2') {
			unsigned int ret_ax;

			vga_write("Calling...");
			vga_write_sync();

			ret_ax = isa_pnp_bios_get_static_alloc_resinfo(devnode_raw);

			vga_write_color(0x07);
			vga_clear();
			vga_moveto(0,0);

			vga_write("BIOS call results:\n");
			sprintf(tmp,"    return value (AX): 0x%04X\n",ret_ax);
			vga_write(tmp);
			vga_write("\n");

			/* TODO: Find an emulator or actual PC where this function is implemented */

			vga_write_sync();
			while (getch() != 13);
		}
		else if (c == '1') {
			struct isa_pnp_device_node far *devn;
			unsigned char numnodes=0xFF;
			unsigned int ret_ax,nodesize=0xFFFF;
			struct isapnp_tag tag;
			unsigned char node;

			vga_write("Calling...");
			vga_write_sync();

			ret_ax = isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize);

			vga_write_color(0x07);
			vga_clear();
			vga_moveto(0,0);

			vga_write("BIOS call results:\n");
			sprintf(tmp,"    return value (AX): 0x%04X\n",ret_ax);
			vga_write(tmp);
			sprintf(tmp,"             NumNodes: 0x%02X\n",numnodes);
			vga_write(tmp);
			sprintf(tmp,"             NodeSize: 0x%04X\n",nodesize);
			vga_write(tmp);
			vga_write("\n");

			if (ret_ax != 0 || numnodes == 0xFF || nodesize > sizeof(devnode_raw)) {
				vga_write("Nothing to see here. Hit ENTER to continue\n");
				vga_write_sync();
				while (getch() != 13);
			}
			else {
				vga_write("Ok... whew! Your BIOS is obviously not crap, so let's move on...\n\n");
				vga_write("Would you like me to enumerate each node? [yn] ");
				vga_write_sync();

				if (getch() != 'y')
					continue;

				/* NTS: How nodes are enumerated in the PnP BIOS: set node = 0, pass address of node
				 *      to BIOS. BIOS, if it returns node information, will also overwrite node with
				 *      the node number of the next node, or with 0xFF if this is the last one.
				 *      On the last one, stop enumerating. */
				for (node=0;node != 0xFF;) {
					const char *dev_class = NULL,*dev_type = NULL,*dev_stype = NULL,*dev_itype = NULL;
					char dev_product[256] = {0};
					unsigned char far *rsc;
					char pnpid[8];
					int liter;

					vga_write_color(0x07);
					vga_clear();
					vga_moveto(0,0);

					sprintf(tmp,"Node %02X ",node);
					vga_write(tmp);
					vga_write_sync();

					/* apparently, start with 0. call updates node to
					 * next node number, or 0xFF to signify end */
					ret_ax = isa_pnp_bios_get_sysdev_node(&node,devnode_raw,
						ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW);

					if (ret_ax != 0)
						break;

					devn = (struct isa_pnp_device_node far*)devnode_raw;
					sprintf(tmp," -> %02X: AX=%04X\n",node,ret_ax);
					vga_write(tmp);

					isa_pnp_product_id_to_str(pnpid,devn->product_id);
					sprintf(tmp,"Size=%u handle=%02X product_id=%08lX (%s) DeviceType=%02X,%02X,%02X\n",
						devn->size,devn->handle,
						(unsigned long)devn->product_id,pnpid,
						(unsigned char)devn->type_code[0],
						(unsigned char)devn->type_code[1],
						(unsigned char)devn->type_code[2]);
					vga_write(tmp);

					dev_type = isa_pnp_device_type(devn->type_code,&dev_stype,&dev_itype);
					dev_class = isa_pnp_device_category(devn->product_id);
					isa_pnp_device_description(dev_product,devn->product_id);

					if (dev_class != NULL) {
						vga_write("Device category: ");
						vga_write(dev_class);
						vga_write("\n");
					}

					if (dev_product[0] != 0) {
						vga_write("Device descript: ");
						vga_write(dev_product);
						vga_write("\n");
					}

					if (dev_type != NULL) {
						vga_write("Device:          ");
						vga_write(dev_type);
						vga_write("\n");
					}
					if (dev_stype != NULL) {
						vga_write("                 ");
						vga_write(dev_stype);
						vga_write("\n");
					}
					if (dev_itype != NULL) {
						vga_write("                 ");
						vga_write(dev_itype);
						vga_write("\n");
					}

					sprintf(tmp,"Attributes (%04X): \n",devn->attributes); vga_write(tmp);
					if (devn->attributes & ISAPNP_DEV_ATTR_CANT_DISABLE)
						vga_write("   - Device cannot be disabled\n");
					if (devn->attributes & ISAPNP_DEV_ATTR_CANT_CONFIGURE)
						vga_write("   - Device is not configurable\n");
					if (devn->attributes & ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_OUTPUT)
						vga_write("   - Device is capable of being primary output device\n");
					if (devn->attributes & ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_INPUT)
						vga_write("   - Device is capable of being primary input device\n");
					if (devn->attributes & ISAPNP_DEV_ATTR_CAN_BE_PRIMARY_IPL)
						vga_write("   - Device is capable of being primary IPL device\n");
					if (devn->attributes & ISAPNP_DEV_ATTR_DOCKING_STATION_DEVICE)
						vga_write("   - Device is a docking station device\n");
					if (devn->attributes & ISAPNP_DEV_ATTR_REMOVEABLE_STATION_DEVICE)
						vga_write("   - Device is a removeable station device\n");

					switch (ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE(devn->attributes)) {
						case ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_ONLY_NEXT_BOOT:
							vga_write("   - Device can only be configured for next boot\n"); break;
						case ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_AT_RUNTIME:
							vga_write("   - Device can be configured at runtime\n"); break;
						case ISAPNP_DEV_ATTR_WHEN_CONFIGURABLE_ONLY_RUNTIME:
							vga_write("   - Device can only be configured at runtime\n"); break;
					};

					rsc = devnode_raw + sizeof(*devn);
					for (i=sizeof(*devn);i < devn->size;i++) {
						if (vga_pos_x >= (vga_width-2)) vga_write("\n");
						sprintf(tmp,"%02X ",devnode_raw[i]);
						vga_write(tmp);
					}
					vga_write("\n");
					vga_write_sync();
					while ((c=getch()) != 13) {
						if (c == 27) break;
					}
					if (c == 27)
						break;

					/* the three configuration blocks are in this one buffer, one after the other */
					for (liter=0;liter <= 2;liter++) {
						if (!isapnp_read_tag(&rsc,devnode_raw + devn->size,&tag))
							break;
						if (tag.tag == ISAPNP_TAG_END)
							continue;

						vga_write_color(0x07);
						vga_clear();
						vga_moveto(0,0);

						vga_write(isapnp_config_block_str[liter]);
						vga_write(" resource configuration block\n");
						vga_write("\n");

						do {
							if (tag.tag == ISAPNP_TAG_END) /* end tag */
								break;

							sprintf(tmp,"%14s: ",isapnp_tag_str(tag.tag));
							if (tag.tag == ISAPNP_TAG_START_DEPENDENT_FUNCTION ||
								tag.tag == ISAPNP_TAG_END_DEPENDENT_FUNCTION)
								vga_write("---------------------");
							vga_write(tmp);

							switch (tag.tag) {
/*---------------------------------------------------------------------------------*/
case ISAPNP_TAG_PNP_VERSION: {
	struct isapnp_tag_pnp_version far *x = (struct isapnp_tag_pnp_version far*)tag.data;
	sprintf(tmp,"PnP v%u.%u Vendor=0x%02X",x->pnp>>4,x->pnp&0xF,x->vendor);
	vga_write(tmp);
} break;
case ISAPNP_TAG_COMPATIBLE_DEVICE_ID: {
	struct isapnp_tag_compatible_device_id far *x = (struct isapnp_tag_compatible_device_id far*)tag.data;
	isa_pnp_product_id_to_str(tmp,x->id);
	vga_write(tmp);
} break;
case ISAPNP_TAG_IRQ_FORMAT: {
	struct isapnp_tag_irq_format far *x = (struct isapnp_tag_irq_format far*)tag.data;
	sprintf(tmp,"HTE=%u LTE=%u HTL=%u LTL=%u\n",x->hte,x->lte,x->htl,x->ltl);
	vga_write(tmp);
	vga_write("                IRQ: ");
	for (i=0;i < 16;i++) {
		if (x->irq_mask & (1U << (unsigned int)i)) {
			sprintf(tmp,"%u ",i);
			vga_write(tmp);
		}
	}
} break;
case ISAPNP_TAG_DMA_FORMAT: {
	struct isapnp_tag_dma_format far *x = (struct isapnp_tag_dma_format far*)tag.data;
	if (x->bus_master) vga_write("BusMaster ");
	if (x->byte_count) vga_write("ByteCount ");
	if (x->word_count) vga_write("WordCount ");

	sprintf(tmp,"Speed=%s xferPref=%s\n",isapnp_dma_speed_str[x->dma_speed],isapnp_dma_xfer_preference_str[x->xfer_preference]);
	vga_write(tmp);

	vga_write("                DMA channels: ");
	for (i=0;i < 8;i++) {
		if (x->dma_mask & (1U << (unsigned int)i)) {
			sprintf(tmp,"%u ",i);
			vga_write(tmp);
		}
	}
} break;
case ISAPNP_TAG_START_DEPENDENT_FUNCTION: {
	if (tag.len > 0) {
		struct isapnp_tag_start_dependent_function far *x =
			(struct isapnp_tag_start_dependent_function far*)tag.data;
		sprintf(tmp," Priority=%s",isapnp_sdf_priority_str(x->priority));
		vga_write(tmp);
	}
} break;
case ISAPNP_TAG_END_DEPENDENT_FUNCTION:
  break;
case ISAPNP_TAG_IO_PORT: {
	struct isapnp_tag_io_port far *x = (struct isapnp_tag_io_port far*)tag.data;
	sprintf(tmp,"Decode %ubit, range 0x%04X-0x%04X, align 0x%02X, length: 0x%02X",x->decode_16bit?16:10,
		x->min_range,x->max_range,x->alignment,x->length);
	vga_write(tmp);
} break;
case ISAPNP_TAG_FIXED_IO_PORT: {
	struct isapnp_tag_fixed_io_port far *x = (struct isapnp_tag_fixed_io_port far*)tag.data;
	sprintf(tmp,"At %04X, length %02X",x->base,x->length);
	vga_write(tmp);
} break;
case ISAPNP_TAG_LOGICAL_DEVICE_ID: {
	struct isapnp_tag_logical_device_id far *x = (struct isapnp_tag_logical_device_id far*)tag.data;
	isa_pnp_product_id_to_str(tmp,x->logical_device_id);
	vga_write(tmp);
} break;
case ISAPNP_TAG_FIXED_MEMORY_LOCATION_32: {
	struct isapnp_tag_fixed_memory_location_32 far *x =
		(struct isapnp_tag_fixed_memory_location_32 far*)tag.data;
	sprintf(tmp,"%s %s %s %s %s %s\n",
		x->writeable ? "Writeable" : "Read only",
		x->cacheable ? "Cacheable" : "No-cache",
		x->support_hi_addr ? "Hi-addr" : "",
		isapnp_fml32_miosize_str[x->memory_io_size],
		x->shadowable ? "Shadowable" : "No-shadow",
		x->expansion_rom ? "Expansion ROM" : "RAM");
	vga_write(tmp);
	sprintf(tmp,"                Base 0x%08lX len 0x%08lX",
		(unsigned long)x->base,		(unsigned long)x->length);
	vga_write(tmp);
} break;
case ISAPNP_TAG_ANSI_ID_STRING: {
	i = tag.len;
	if ((i+1) > sizeof(tmp)) i = sizeof(tmp) - 1;
	if (i != 0) _fmemcpy(tmp,tag.data,i);
	tmp[i] = 0;
	vga_write(tmp);
} break;
default:
	vga_write("FIXME: not implemented");
break;
/*---------------------------------------------------------------------------------*/
							};

							vga_write("\n");
						} while (isapnp_read_tag(&rsc,devnode_raw + devn->size,&tag));

						vga_write_sync();
						while ((c=getch()) != 13) {
							if (c == 27) {
								liter = 2;
								break;
							}
						}

						if (c == 27)
							break;
					}
				}

				vga_write_color(0x07);
				vga_clear();
				vga_moveto(0,0);
				vga_write("That's it for nodes.");
				vga_write_sync();
				while ((c=getch()) != 13) {
					if (c == 27) break;
				}
			}
		}
	}

	int10_setmode(3);
	free_isa_pnp_bios();

	return 0;
}

