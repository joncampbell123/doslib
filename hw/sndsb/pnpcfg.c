
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
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>

/* NTS: caller is expected to WAKE[CSN] the card for us */
void ms_virtualpc_prompt() {
	int dma,irq=0;

	printf("Currently configured:\n");

	isa_pnp_write_address(0x07);	/* log device select */
	isa_pnp_write_data(0x00);	/* main device */

	/* NTS: I/O port is reported "fixed" in both PnP BIOS and configuration data */
	dma = isa_pnp_read_dma(0);	/* FIXME: Does Virtual PC support changing this value? */
	irq = isa_pnp_read_irq(0);	/* FIXME: Does Virtual PC support changing this value? */

	printf("     DMA (main)            %d\n",dma);
	printf("     IRQ                   %d\n",irq);

	while (getch() != 13);
}

enum {
    SB_VIBRA=0,
    SB_AWE
};

/* NTS: caller is expected to WAKE[CSN] the card for us */
/* TODO: On AWE32/AWE64 cards the wavetable section is logical device #2 */
void ct_sb16pnp_prompt(int typ) {
	int io[3]={0,0,0},dma[2]={0,0},irq=0,val,game,wavetable;

	printf("Answer the questions. You may enter -1 to skip card, -2 to unassign\n");
	printf("or -3 to not change.\n");
	printf("Currently configured:\n");

	isa_pnp_write_address(0x07);	/* log device select */
	isa_pnp_write_data(0x00);	/* sound blaster (0) */

	io[0] = isa_pnp_read_io_resource(0);
	io[1] = isa_pnp_read_io_resource(1);
	io[2] = isa_pnp_read_io_resource(2);
	dma[0] = isa_pnp_read_dma(0);
	dma[1] = isa_pnp_read_dma(1);
	irq = isa_pnp_read_irq(0);

	isa_pnp_write_address(0x07);	/* log device select */
	isa_pnp_write_data(0x01);	/* gameport (1) */

	game = isa_pnp_read_io_resource(0);

	if (typ == SB_AWE) {
		isa_pnp_write_address(0x07);	/* log device select */
		isa_pnp_write_data(0x02);	/* wavetable (AWE) (2) */

		wavetable = isa_pnp_read_io_resource(0);
	}

	isa_pnp_write_address(0x07);	/* log device select */
	isa_pnp_write_data(0x00);	/* main device */

	printf("     I/O (SoundBlaster)    0x%03x\n",io[0]);
	printf("     I/O (MPU)             0x%03x\n",io[1]);
	printf("     I/O (OPL3)            0x%03x\n",io[2]);
	printf("     I/O (GAME)            0x%03x\n",game);
	if (typ == SB_AWE) printf("     I/O (AWE)             0x%03x\n",wavetable);
	printf("     DMA (8-bit)           %d\n",dma[0]);
	printf("     DMA (16-bit)          %d\n",dma[1]);
	printf("     IRQ                   %d\n",irq);

	/* 0x220,0x240,0x260,0x280 */
	printf(" - SoundBlaster Port? (2x0 x=2,4,6,8)        :"); fflush(stdout); val=2; scanf("%d",&val);
	if (val >= 2 && (val&1) == 0) io[0] = 0x200 + (val * 0x10);
	else if (val == -1) return;
	else if (val == -2) io[0] = 0;

	printf(" - MPU Port? (3x0 x=0,3)                     :"); fflush(stdout); val=0; scanf("%d",&val);
	if (val == 0 || val == 3) io[1] = 0x300 + (val * 0x10);
	else if (val == -1) return;
	else if (val == -2) io[1] = 0;

	printf(" - OPL3 Port? (0=388 or 1=392)               :"); fflush(stdout); val=0; scanf("%d",&val);
	if (val >= 0 && val <= 15) io[2] = 0x388 + (val * 0x4);
	else if (val == -1) return;
	else if (val == -2) io[2] = 0;

	printf(" - GAME Port? (2x0 0-7)                      :"); fflush(stdout); val=0; scanf("%d",&val);
	if (val >= 0 && val <= 7) game = 0x200 + val;
	else if (val == -1) return;
	else if (val == -2) game = 0;

	if (typ == SB_AWE) {
		/* despite Creative's PNP configuration data, programming
		   experience says we can literally place it anywhere in
		   the 6xx range! */
		printf(" - AWE Port? (6x0 x=0,2,4,6,8)                 :"); fflush(stdout); val=0; scanf("%d",&val);
		if (val >= 0 && val <= 9) wavetable = 0x600 + (val * 0x10);
		else if (val == -1) return;
		else if (val == -2) wavetable = 0;
	}

	printf(" - IRQ? (5,7,9,10)                           :"); fflush(stdout); val=0; scanf("%d",&val);
	if (val >= 2 && val <= 15) irq = val;
	else if (val == -1) return;
	else if (val == -2) irq = -1;

	printf(" - 8-bit DMA? (0,1,3)                        :"); fflush(stdout); val=0; scanf("%d",&val);
	if (val >= 0 && val <= 3) dma[0] = val;
	else if (val == -1) return;
	else if (val == -2) dma[0] = -1;

	printf(" - 16-bit DMA? (0,1,3,5,6,7)                 :"); fflush(stdout); val=0; scanf("%d",&val);
	if (val >= 0 && val <= 7) dma[1] = val;
	else if (val == -1) return;
	else if (val == -2) dma[1] = -1;

	/* disable the device IO (0) */
	isa_pnp_write_address(0x07);	/* log device select */
	isa_pnp_write_data(0x00);	/* main device */

	isa_pnp_write_data_register(0x30,0x00);	/* activate: bit 0 */
	isa_pnp_write_data_register(0x31,0x00); /* IO range check: bit 0 */

	isa_pnp_write_io_resource(0,io[0]);
	isa_pnp_write_io_resource(1,io[1]);
	isa_pnp_write_io_resource(2,io[2]);
	isa_pnp_write_dma(0,dma[0]);
	isa_pnp_write_dma(1,dma[1]);
	isa_pnp_write_irq(0,irq);
	isa_pnp_write_irq_mode(0,2);	/* edge level high */

	/* enable the device IO */
	isa_pnp_write_data_register(0x30,0x01);	/* activate: bit 0 */
	isa_pnp_write_data_register(0x31,0x00); /* IO range check: bit 0 */

	/* disable the device IO (1) */
	isa_pnp_write_address(0x07);	/* log device select */
	isa_pnp_write_data(0x01);	/* main device */

	isa_pnp_write_data_register(0x30,0x00);	/* activate: bit 0 */
	isa_pnp_write_data_register(0x31,0x00); /* IO range check: bit 0 */

	isa_pnp_write_io_resource(0,game);

	/* enable the device IO */
	isa_pnp_write_data_register(0x30,0x01);	/* activate: bit 0 */
	isa_pnp_write_data_register(0x31,0x00); /* IO range check: bit 0 */

	if (typ == SB_AWE) {
		/* disable the device IO (2) */
		isa_pnp_write_address(0x07);	/* log device select */
		isa_pnp_write_data(0x02);	/* main device */

		isa_pnp_write_data_register(0x30,0x00);	/* activate: bit 0 */
		isa_pnp_write_data_register(0x31,0x00); /* IO range check: bit 0 */

		isa_pnp_write_io_resource(0,wavetable);

		/* enable the device IO */
		isa_pnp_write_data_register(0x30,0x01);	/* activate: bit 0 */
		isa_pnp_write_data_register(0x31,0x00); /* IO range check: bit 0 */
	}

	/* return back to "wait for key" state */
	isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */

	printf("Okay, I reconfigured the card\n");
	while (getch() != 13);
}

int main() {
	printf("Sound Blaster Plug & Play configuration utility\n");

	if (!probe_8237()) {
		printf("Cannot init 8237 DMA\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!init_sndsb()) {
		printf("Cannot init library\n");
		return 1;
	}
	if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
	}
	if (find_isa_pnp_bios()) {
		char tmp[192];
		unsigned int j;
		const char *whatis = NULL;
		unsigned char csn,data[192];

		memset(data,0,sizeof(data));
		if (isa_pnp_bios_get_pnp_isa_cfg(data) == 0) {
			struct isapnp_pnp_isa_cfg *nfo = (struct isapnp_pnp_isa_cfg*)data;
			isapnp_probe_next_csn = nfo->total_csn;
			isapnp_read_data = nfo->isa_pnp_port;
		}
		else {
			printf("  ISA PnP BIOS failed to return configuration info\n");
		}

		if (isapnp_read_data != 0) {
			printf("Scanning ISA PnP devices...\n");
			for (csn=1;csn < 255;csn++) {
				isa_pnp_init_key();
				isa_pnp_wake_csn(csn);

				isa_pnp_write_address(0x06); /* CSN */
				if (isa_pnp_read_data() == csn) {
					/* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
					/* if we don't, then we only read back the resource data */
					isa_pnp_init_key();
					isa_pnp_wake_csn(csn);

					for (j=0;j < 9;j++) data[j] = isa_pnp_read_config();

					if (isa_pnp_is_sound_blaster_compatible_id(*((uint32_t*)data),&whatis)) {
						isa_pnp_product_id_to_str(tmp,*((uint32_t*)data));

						printf("  [%u]: %02x %02x %02x %02x %02x %02x %02x %02x %02x ",csn,
							data[0],data[1],data[2],data[3],
							data[4],data[5],data[6],data[7],
							data[8]);

						printf("%s %s\n",tmp,whatis);

						if (!memcmp(tmp,"CTL",3)) {
							if (!memcmp(tmp+3,"0070",4) || !memcmp(tmp+3,"00F0",4)) {
								isa_pnp_init_key();
								isa_pnp_wake_csn(csn);
								ct_sb16pnp_prompt(SB_VIBRA);
							}
							/* NTS: The 00B2 version is the "Gold" version */
							else if (!memcmp(tmp+3,"00C3",4) || !memcmp(tmp+3,"00B2",4)) {
								isa_pnp_init_key();
								isa_pnp_wake_csn(csn);
								ct_sb16pnp_prompt(SB_AWE);
							}
						}
						else if (!memcmp(tmp,"TBA03B0",7)) {
							isa_pnp_init_key();
							isa_pnp_wake_csn(csn);
							ms_virtualpc_prompt();
						}
					}
				}
			}
		}
	}
	else {
		printf("Warning, ISA PnP BIOS not found\n");
	}

	return 0;
}

