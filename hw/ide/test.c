/* THINGS TO DO:
 *
 *      - Modularize copy-pasta'd code such as:
 *         * The "if you value your data" warning
 *         * ATAPI packet commands
 *         * EVERYTHING---This code basically works on test hardware, now it's time to clean it up
 *           modularize and refactor.
 *      - Add menu item where the user can ask this code to test whether or not the IDE controller
 *        supports 32-bit PIO properly.
 *      - Start using this code for reference and implementation of IDE emulation within DOSBox-X
 *      - Add menu items to allow the user to play with SET FEATURES command
 *      - Add submenu where the user can play with the S.M.A.R.T. ATA commands
 *      - **TEST THIS CODE ON AS MANY MACHINES AS POSSIBLE** The IDE interface is one of those
 *        hardware standards that is conceptually simple yet so manu manufacturers managed to fuck
 *        up their implementation in some way or another.
 *      - Cleanup this code, move library into idelib.c+idelib.h
 *      - Fix corner cases where we're IDE busy waiting and user commands to break free (ESC or spacebar
 *        do not break out of read/write loop, forcing the user to CTRL+ALT+DEL or press reset.
 *
 * Also don't forget:
 *   - Test programs for specific IDE chipsets (like Intel PIIX3) to demonstrate IDE DMA READ/WRITE commands
 *
 * Interesting notes:
 *   - Toshiba Satellite Pro 465CDX/2.1
 *      - Putting the hard drive to sleep seems to put the IDE controller to sleep too. IDE controller
 *        "busy" bit is stuck on when hard drive asleep. Only way out seems to be doing a "host reset"
 *        on that IDE controller. Note that NORMAL behavior is that the IDE controller remains not-busy
 *        but the device is not ready.
 *
 *      - The CD-ROM drive (or secondary IDE?) appears to ignore 32-bit I/O to port 0x170 (base_io+0).
 *        If you attempt to read a sector or identify command results using 32-bit PIO, you get only
 *        0xFFFFFFFF. Reading data only works when PIO is done as 16-bit. NORMAL behavior suggests
 *        either 32-bit PIO gets 32 bits at a time (PCI based IDE) or 32-bit PIO gets 16 bits of IDE
 *        data and 16 bits of the adjacent 16-bit I/O register due to 386/486-era ISA subdivision of
 *        32-bit I/O into two 16-bit I/O reads. Supposedly, on pre-PCI controllers, 32-bit PIO could
 *        be made to work if you tell the card in advance using the "VLB keying sequence", but I have
 *        yet to find such a card.
 *
 * Known hardware this code has trouble with (so far):
 * 
 *   - Ancient (1999-2002-ish) DVD-ROM drives. They generally work with this code but there seem to be a lot
 *     of edge cases. One DVD-ROM drive I own will not raise "drive ready" after receipt of a command it doesn't
 *     recognize. */

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
#include <hw/pci/pci.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/ide/idelib.h>

#include "testutil.h"
#include "testmbox.h"
#include "testcmui.h"
#include "testbusy.h"
#include "testpiot.h"
#include "testrvfy.h"
#include "testrdwr.h"
#include "testidnt.h"
#include "testcdej.h"
#include "testpiom.h"
#include "testtadj.h"
#include "testcdrm.h"
#include "testmumo.h"
#include "testmisc.h"
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

#ifdef ISAPNP
#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>
#endif

unsigned char			opt_ignore_smartdrv = 0;
unsigned char			opt_no_irq = 0;
unsigned char			opt_no_pci = 0;
unsigned char			opt_no_isapnp = 0;
unsigned char			opt_no_isa_probe = 0;
unsigned char			opt_irq_chain = 1;

unsigned char			cdrom_read_mode = 12;
unsigned char			pio_width_warning = 1;
unsigned char			big_scary_write_test_warning = 1;

char				tmp[1024];
uint16_t			ide_info[256];
#ifdef ISAPNP
static unsigned char far	devnode_raw[4096];
#endif

#if TARGET_MSDOS == 32
unsigned char			cdrom_sector[512U*256U];/* ~128KB, enough for 64 CD-ROM sector or 256 512-byte sectors */
#else
# if defined(__LARGE__) || defined(__COMPACT__)
unsigned char			cdrom_sector[512U*16U];	/* ~8KB, enough for 4 CD-ROM sector or 16 512-byte sectors */
# else
unsigned char			cdrom_sector[512U*8U];	/* ~4KB, enough for 2 CD-ROM sector or 8 512-byte sectors */
# endif
#endif

/*-----------------------------------------------------------------*/

static const char *drive_main_menustrings[] = {
	"Show IDE register taskfile",		/* 0 */
	"Identify (ATA)",
	"Identify packet (ATAPI)",
	"Power states >>",
	"PIO mode >>",				/* 4 */ /* rewritten */
	"No-op",				/* 5 */
	"Tweaks and adjustments >>",
	"CD-ROM eject/load >>",
	"CD-ROM reading >>",
	"Multiple mode >>",
	"Read/Write tests >>",			/* 10 */
	"Miscellaneous >>"
};

void do_ide_controller_drive(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_main_menustrings);

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* select the drive we want */
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);

	/* in case the IDE controller is busy for that time */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* read back: did the drive select take effect? if not, it might not be there. another common sign is the head/drive select reads back 0xFF */
	c = do_ide_controller_drive_check_select(ide,which);
	if (c < 0) return;

	/* it might be a CD-ROM drive, which in some cases might not raise the Drive Ready bit */
	do_ide_controller_atapi_device_check_post_host_reset(ide);

	/* wait for the drive to indicate readiness */
	/* NTS: If the drive never becomes ready even despite our reset hacks, there's a strong
	 *      possibility that the device doesn't exist. This can happen for example if there
	 *      is a master attached but no slave. */
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c < 0) return;

	/* for completeness, clear pending IRQ */
	idelib_controller_ack_irq(ide);

	while (1) {
		if (backredraw) {
			vga = vga_alpha_ram;
			backredraw = 0;
			redraw = 1;

			for (y=0;y < vga_height;y++) {
				for (x=0;x < vga_width;x++) {
					*vga++ = 0x1E00 + 177;
				}
			}

			vga_moveto(0,0);

			vga_write_color(0x1F);
			vga_write("        IDE controller ");
			sprintf(tmp,"@%X",ide->base_io);
			vga_write(tmp);
			if (ide->alt_io != 0) {
				sprintf(tmp," alt %X",ide->alt_io);
				vga_write(tmp);
			}
			if (ide->irq >= 0) {
				sprintf(tmp," IRQ %d",ide->irq);
				vga_write(tmp);
			}
			vga_write(which ? " Slave" : " Master");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');

			vga_write_color(0xC);
			vga_write("WARNING: This code talks directly to your hard disk controller.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
			vga_write_color(0xC);
			vga_write("         If you value the data on your hard drive do not run this program.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			redraw = 0;

			/* update a string or two: PIO mode */
			if (ide->pio_width == 33)
				drive_main_menustrings[4] = "PIO mode (currently: 32-bit VLB) >>";
			else if (ide->pio_width == 32)
				drive_main_menustrings[4] = "PIO mode (currently: 32-bit) >>";
			else
				drive_main_menustrings[4] = "PIO mode (currently: 16-bit) >>";

			vga_moveto(mbox.ofsx,mbox.ofsy - 2);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE controller main menu");
			while (vga_pos_x < (mbox.width+mbox.ofsx) && vga_pos_x != 0) vga_writec(' ');

			menuboxbound_redraw(&mbox,select);
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1)
				break;

			switch (select) {
				case 0: /* show IDE register taskfile */
					do_common_show_ide_taskfile(ide,which);
					redraw = backredraw = 1;
					break;
				case 1: /*Identify*/
				case 2: /*Identify packet*/
					do_drive_identify_device_test(ide,which,select == 2 ? 0xA1/*Identify packet*/ : 0xEC/*Identify*/);
					redraw = backredraw = 1;
					break;
				case 3: /* power states */
#ifdef POWER_MENU
					do_drive_power_states_test(ide,which);
					redraw = backredraw = 1;
#endif
					break;
				case 4: /* PIO mode */
#ifdef PIO_MODE_MENU
					do_drive_pio_mode(ide,which);
					redraw = backredraw = 1;
#endif
					break;
				case 5: /* NOP */
#ifdef NOP_TEST
					do_ide_controller_drive_nop_test(ide,which);
#endif
					break;
				case 6: /* Tweaks and adjustments */
#ifdef TWEAK_MENU
					do_drive_tweaks_and_adjustments(ide,which);
					redraw = backredraw = 1;
#endif
					break;
				case 7: /* CD-ROM start/stop/eject/load */
					do_drive_cdrom_startstop_test(ide,which);
					redraw = backredraw = 1;
					break;
				case 8: /* CD-ROM reading */
					do_drive_cdrom_reading(ide,which);
					redraw = backredraw = 1;
					break;
				case 9: /* multiple mode */
#ifdef MULTIPLE_MODE_MENU
					do_drive_multiple_mode(ide,which);
					redraw = backredraw = 1;
#endif
					break;
				case 10: /* read/write tests */
					do_drive_readwrite_tests(ide,which);
					redraw = backredraw = 1;
					break;
				case 11: /* misc */
#ifdef MISC_TEST
					do_drive_misc_tests(ide,which);
					redraw = backredraw = 1;
#endif
					break;
			};
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = mbox.item_max;

			redraw = 1;
		}
		else if (c == 0x4B00) { /* left */
			redraw = 1;
		}
		else if (c == 0x4D00) { /* right */
			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > mbox.item_max)
				select = -1;

			redraw = 1;
		}
	}
}

static void (interrupt *my_ide_old_irq)() = NULL;
static struct ide_controller *my_ide_irq_ide = NULL;
static unsigned long ide_irq_counter = 0;
static int my_ide_irq_number = -1;

static void interrupt my_ide_irq() {
	int i;

	_cli();

	/* we CANNOT use sprintf() here. sprintf() doesn't work to well from within an interrupt handler,
	 * and can cause crashes in 16-bit realmode builds. */
	i = vga_width*(vga_height-1);
	vga_alpha_ram[i++] = 0x1F00 | 'I';
	vga_alpha_ram[i++] = 0x1F00 | 'R';
	vga_alpha_ram[i++] = 0x1F00 | 'Q';
	vga_alpha_ram[i++] = 0x1F00 | ':';
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter / 100000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /  10000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /   1000UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /    100UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /     10UL) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ('0' + ((ide_irq_counter /       1L) % 10UL));
	vga_alpha_ram[i++] = 0x1F00 | ' ';
	ide_irq_counter++;

	if (my_ide_irq_ide != NULL) {
		my_ide_irq_ide->irq_fired++;

		/* NTS: This code requires some explanation: On an Intel Core i3 mini-itx motherboard I recently
		 * bought, the SATA controller (in IDE mode) has a problem with IDE interrupts where for reasons
		 * beyond my understanding, once the IRQ fires, the IRQ continues to fire and will not stop no
		 * matter what registers we read or ports we poke. It fires rapidly enough that our busy wait
		 * code cannot proceed and the program "hangs" while the IRQ counter on the screen counts upward
		 * very fast. It is only when exiting back to DOS that the BIOS somehow makes it stop.
		 *
		 * That motherboard is the reason this code was implemented. should any other SATA/IDE controller
		 * have this problem, this code will eventually stop the IRQ flood by masking off the IRQ and
		 * switching the IDE controller struct into polling mode so that the user can continue to use
		 * this program without having to hit the reset button!
		 *
		 * Another Intel Core i3 system (2010) has the same problem, with SATA ports and one IDE port.
		 * This happens even when talking to the IDE port and not the SATA-IDE emulation.
		 *
		 * It seems to be a problem with Intel-based motherboards, 2010 or later.
		 *
		 * Apparently the fix is to chain to the BIOS IRQ handler, which knows how to cleanup the IRQ signal. */

		/* ack IRQ on IDE controller */
		idelib_controller_ack_irq(my_ide_irq_ide);
	}

	if (!opt_irq_chain || my_ide_old_irq == NULL) {
		/* ack PIC */
		if (my_ide_irq_ide->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to previous */
		my_ide_old_irq();
	}

	/* If too many IRQs fired, then stop the IRQ and use polling from now on. */
	if (my_ide_irq_ide != NULL) {
		if (my_ide_irq_ide->irq_fired >= 0xFFFEU) {
			do_ide_controller_emergency_halt_irq(my_ide_irq_ide);
			vga_alpha_ram[i+12] = 0x1C00 | '!';
			my_ide_irq_ide->irq_fired = ~0; /* make sure the IRQ counter is as large as possible */
		}
	}
}

void do_ide_controller_hook_irq(struct ide_controller *ide) {
	if (my_ide_irq_number >= 0 || ide->irq < 0)
		return;

	/* let the IRQ know what IDE controller */
	my_ide_irq_ide = ide;

	/* enable on IDE controller */
	p8259_mask(ide->irq);
	idelib_otr_enable_interrupt(ide,1);
	idelib_controller_ack_irq(ide);

	/* hook IRQ */
	my_ide_old_irq = _dos_getvect(irq2int(ide->irq));
	_dos_setvect(irq2int(ide->irq),my_ide_irq);
	my_ide_irq_number = ide->irq;

	/* enable at PIC */
	p8259_unmask(ide->irq);
}

void do_ide_controller_unhook_irq(struct ide_controller *ide) {
	if (my_ide_irq_number < 0 || ide->irq < 0)
		return;

	/* disable on IDE controller, then mask at PIC */
	p8259_mask(ide->irq);
	idelib_controller_ack_irq(ide);
	idelib_otr_enable_interrupt(ide,0);

	/* restore the original vector */
	_dos_setvect(irq2int(ide->irq),my_ide_old_irq);
	my_ide_irq_number = -1;
	my_ide_old_irq = NULL;
}

void do_ide_controller_emergency_halt_irq(struct ide_controller *ide) {
	/* disable on IDE controller, then mask at PIC */
	if (ide->irq >= 0) p8259_mask(ide->irq);
	idelib_controller_ack_irq(ide);
	idelib_otr_enable_interrupt(ide,0);
}

void do_ide_controller_enable_irq(struct ide_controller *ide,unsigned char en) {
	if (!en || ide->irq < 0 || ide->irq != my_ide_irq_number)
		do_ide_controller_unhook_irq(ide);
	if (en && ide->irq >= 0)
		do_ide_controller_hook_irq(ide);
}

void do_ide_controller(struct ide_controller *ide) {
	struct vga_msg_box vgabox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	char redraw=1;
	int select=-1;
	int c;

	/* we're taking a drive, possibly out from MS-DOS.
	 * make sure SMARTDRV flushes the cache so that it does not attempt to
	 * write to the disk while we're controlling the IDE controller */
	if (smartdrv_version != 0) {
		for (c=0;c < 4;c++) smartdrv_flush();
	}

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c < 0) return;

	/* if the IDE struct says to use interrupts, then do it */
	do_ide_controller_enable_irq(ide,ide->flags.io_irq_enable);

	while (1) {
		if (backredraw) {
			vga = vga_alpha_ram;
			backredraw = 0;
			redraw = 1;

			for (y=0;y < vga_height;y++) {
				for (x=0;x < vga_width;x++) {
					*vga++ = 0x1E00 + 177;
				}
			}

			vga_moveto(0,0);

			vga_write_color(0x1F);
			vga_write("        IDE controller ");
			sprintf(tmp,"@%X",ide->base_io);
			vga_write(tmp);
			if (ide->alt_io != 0) {
				sprintf(tmp," alt %X",ide->alt_io);
				vga_write(tmp);
			}
			if (ide->irq >= 0) {
				sprintf(tmp," IRQ %d",ide->irq);
				vga_write(tmp);
			}
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');

			vga_write_color(0xC);
			vga_write("WARNING: This code talks directly to your hard disk controller.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
			vga_write_color(0xC);
			vga_write("         If you value the data on your hard drive do not run this program.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			redraw = 0;

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Main menu");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');
			y++;

			vga_moveto(8,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("Host Reset");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("Tinker with Master device");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("Tinker with Slave device");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("Currently using ");
			vga_write(ide->flags.io_irq_enable ? "IRQ" : "polling");
			vga_write(", switch to ");
			vga_write((!ide->flags.io_irq_enable) ? "IRQ" : "polling");
			while (vga_pos_x < (vga_width-8) && vga_pos_x != 0) vga_writec(' ');
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1) {
				break;
			}
			else if (select == 0) { /* host reset */
				if (ide->alt_io != 0) {
					vga_msg_box_create(&vgabox,"Host reset in progress",0,0);

					idelib_device_control_set_reset(ide,1);
					t8254_wait(t8254_us2ticks(1000000));
					idelib_device_control_set_reset(ide,0);

					vga_msg_box_destroy(&vgabox);

					/* now wait for not busy */
					do_ide_controller_user_wait_busy_controller(ide);
				}
			}
			else if (select == 1) {
				do_ide_controller_drive(ide,0/*master*/);
				redraw = backredraw = 1;
			}
			else if (select == 2) {
				do_ide_controller_drive(ide,1/*slave*/);
				redraw = backredraw = 1;
			}
			else if (select == 3) {
				if (ide->irq >= 0)
					ide->flags.io_irq_enable = !ide->flags.io_irq_enable;
				else
					ide->flags.io_irq_enable = 0;

				do_ide_controller_enable_irq(ide,ide->flags.io_irq_enable);
				redraw = backredraw = 1;
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 3;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 3)
				select = -1;

			redraw = 1;
		}
	}

	do_ide_controller_enable_irq(ide,0);
	idelib_otr_enable_interrupt(ide,1); /* NTS: Most BIOSes know to unmask the IRQ at the PIC, but there might be some
					        idiot BIOSes who don't clear the nIEN bit in the device control when
						executing INT 13h, so it's probably best to do it for them. */
}

void do_main_menu() {
	char redraw=1;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	struct ide_controller *ide;
	unsigned int x,y,i;
	int select=-1;
	int c;

	while (1) {
		if (backredraw) {
			vga = vga_alpha_ram;
			backredraw = 0;
			redraw = 1;

			for (y=0;y < vga_height;y++) {
				for (x=0;x < vga_width;x++) {
					*vga++ = 0x1E00 + 177;
				}
			}

			vga_moveto(0,0);

			vga_write_color(0x1F);
			vga_write("        IDE controller test program");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');

			vga_write_color(0xC);
			vga_write("WARNING: This code talks directly to your hard disk controller.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
			vga_write_color(0xC);
			vga_write("         If you value the data on your hard drive do not run this program.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			redraw = 0;

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Exit program");
			y++;

			for (i=0;i < MAX_IDE_CONTROLLER;i++) {
				ide = idelib_get_controller(i);
				if (ide != NULL) {
					vga_moveto(8,y++);
					vga_write_color((select == (int)i) ? 0x70 : 0x0F);

					sprintf(tmp,"Controller @ %04X",ide->base_io);
					vga_write(tmp);

					if (ide->alt_io != 0) {
						sprintf(tmp," alt %04X",ide->alt_io);
						vga_write(tmp);
					}

					if (ide->irq >= 0) {
						sprintf(tmp," IRQ %2d",ide->irq);
						vga_write(tmp);
					}
				}
			}
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			if (select == -1) {
				break;
			}
			else if (select >= 0 && select < MAX_IDE_CONTROLLER) {
				ide = idelib_get_controller(select);
				if (ide != NULL) do_ide_controller(ide);
				backredraw = redraw = 1;
			}
		}
		else if (c == 0x4800) {
			if (select <= -1)
				select = MAX_IDE_CONTROLLER - 1;
			else
				select--;

			while (select >= 0 && idelib_get_controller(select) == NULL)
				select--;

			redraw = 1;
		}
		else if (c == 0x5000) {
			select++;
			while (select >= 0 && select < MAX_IDE_CONTROLLER && idelib_get_controller(select) == NULL)
				select++;
			if (select >= MAX_IDE_CONTROLLER)
				select = -1;

			redraw = 1;
		}
	}
}

static void help() {
	printf("test [options]\n");
	printf("\n");
	printf("IDE ATA/ATAPI test program\n");
	printf("(C) 2012-2015 Jonathan Campbell, Hackipedia.org\n");
	printf("\n");
	printf("  /NS             Don't check if SMARTDRV is resident\n");
	printf("  /NOIRQ          Don't use IRQ by default\n");
#ifdef PCI_SCAN
	printf("  /NOPCI          Don't scan PCI bus\n");
#endif
#ifdef ISAPNP
	printf("  /NOISAPNP       Don't scan ISA Plug & Play BIOS\n");
#endif
	printf("  /NOPROBE        Don't probe ISA legacy ports\n");
	printf("  /IRQCHAIN       IRQ should chain to previous handler (default)\n");
	printf("  /IRQNOCHAIN     IRQ should NOT chain to previous handler\n");
}

int parse_argv(int argc,char **argv) {
	char *a;
	int i;

	for (i=1;i < argc;) {
		a = argv[i++];

		if (*a == '/') {
			do { a++; } while (*a == '/');

			if (!strcasecmp(a,"?") || !strcasecmp(a,"h") || !strcasecmp(a,"help")) {
				help();
				return 1;
			}
			else if (!strcasecmp(a,"ns")) {
				opt_ignore_smartdrv = 1;
			}
			else if (!strcasecmp(a,"irqnochain")) {
				opt_irq_chain = 0;
			}
			else if (!strcasecmp(a,"irqchain")) {
				opt_irq_chain = 1;
			}
			else if (!strcasecmp(a,"noirq")) {
				opt_no_irq = 1;
			}
			else if (!strcasecmp(a,"nopci")) {
				opt_no_pci = 1;
			}
			else if (!strcasecmp(a,"noisapnp")) {
				opt_no_isapnp = 1;
			}
			else if (!strcasecmp(a,"noprobe")) {
				opt_no_isa_probe = 1;
			}
			else {
				printf("Unknown switch %s\n",a);
				return 1;
			}
		}
		else {
			help();
			return 1;
		}
	}

	return 0;
}

int main(int argc,char **argv) {
	struct ide_controller *idectrl;
	struct ide_controller *newide;
	int i;

	if (parse_argv(argc,argv))
		return 1;

	if (!opt_ignore_smartdrv) {
		if (smartdrv_detect()) {
			printf("WARNING: SMARTDRV %u.%02u or equivalent disk cache detected!\n",smartdrv_version>>8,smartdrv_version&0xFF);
#ifdef MORE_TEXT
			printf("         Running this program with SMARTDRV enabled is NOT RECOMMENDED,\n");
			printf("         especially when using the snapshot functions!\n");
			printf("         If you choose to test anyway, this program will attempt to flush\n");
			printf("         the disk cache as much as possible to avoid conflict.\n");
#endif
		}
	}

	/* we take a GUI-based approach (kind of) */
	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
	/* the IDE code has some timing requirements and we'll use the 8254 to do it */
	/* I bet that by the time motherboard manufacturers stop implementing the 8254 the legacy DOS support this
	 * program requires to run will be long gone too. */
	if (!probe_8254()) {
		printf("8254 chip not detected\n");
		return 1;
	}
	/* interrupt controller */
	if (!probe_8259()) {
		printf("8259 chip not detected\n");
		return 1;
	}
	if (!init_idelib()) {
		printf("Cannot init IDE lib\n");
		return 1;
	}
#ifdef PCI_SCAN
	if (!opt_no_pci) {
		if (pci_probe(-1/*default preference*/) != PCI_CFG_NONE) {
			uint8_t bus,dev,func,iport;

			printf("PCI bus detected.\n");
			if (pci_bios_last_bus == -1) {
				printf("  Autodetecting PCI bus count...\n");
				pci_probe_for_last_bus();
			}
			printf("  Last bus:                 %d\n",pci_bios_last_bus);
			printf("  Bus decode bits:          %d\n",pci_bus_decode_bits);
			for (bus=0;bus <= pci_bios_last_bus;bus++) {
				for (dev=0;dev < 32;dev++) {
					uint8_t functions = pci_probe_device_functions(bus,dev);
					for (func=0;func < functions;func++) {
						/* make sure something is there before announcing it */
						uint16_t vendor,device,subsystem,subvendor_id;
						struct ide_controller ide={0};
						uint32_t class_code;
						uint8_t revision_id;
						int IRQ_pin,IRQ_n;
						uint32_t reg;

						vendor = pci_read_cfgw(bus,dev,func,0x00); if (vendor == 0xFFFF) continue;
						device = pci_read_cfgw(bus,dev,func,0x02); if (device == 0xFFFF) continue;
						subvendor_id = pci_read_cfgw(bus,dev,func,0x2C);
						subsystem = pci_read_cfgw(bus,dev,func,0x2E);
						class_code = pci_read_cfgl(bus,dev,func,0x08);
						revision_id = class_code & 0xFF;
						class_code >>= 8UL;

						/* must be: class 0x01 (mass storage) 0x01 (IDE controller) */
						if ((class_code&0xFFFF00UL) != 0x010100UL)
							continue;

						/* read the command register. is the device enabled? */
						reg = pci_read_cfgw(bus,dev,func,0x04); /* read Command register */
						if (!(reg&1)) continue; /* if the I/O space bit is cleared, then no */

						/* tell the user! */
						printf("    Found PCI IDE controller %02x:%02x:%02x class=0x%06x\n",bus,dev,func,class_code&0xFFFFFFUL);

						/* enumerate from THAT the primary and secondary IDE */
						for (iport=0;iport < 2;iport++) {
							if (class_code&(0x01 << (iport*2))) { /* bit 0 is set if primary in native, bit 2 if secondary in native */
								/* "native mode" */

								/* read it from the BARs */
								reg = pci_read_cfgl(bus,dev,func,0x10+(iport*8)); /* command block */
								if ((reg&1) && (reg&0xFFFF0000UL) == 0UL) /* copy down IF an I/O resource */
									ide.base_io = reg & 0xFFFC;

								reg = pci_read_cfgl(bus,dev,func,0x14+(iport*8)); /* control block */
								if ((reg&1) && (reg&0xFFFF0000UL) == 0UL) { /* copy down IF an I/O resource */
									/* NTS: This requires some explanation: The PCI I/O resource encoding cannot
									 * represent I/O port ranges smaller than 4 ports, nor can it represent
									 * a 4-port resource unless the base port is a multiple of the I/O port
									 * range length.
									 *
									 * The alt I/O port on legacy systems is 0x3F6/0x376. For a PCI device to
									 * declare the same range, it must effectively declare 0x3F4/0x374 to
									 * 0x3F7/377 and then map the legacy ports from 2 ports in from the base.
									 *
									 * When a newer chipset uses a different base port, the same rule applies:
									 * the I/O resource is 4 ports large, and the last 2 ports (base+2) are
									 * the legacy IDE I/O ports that would be 0x3F6/0x376. */
									ide.alt_io = (reg & 0xFFFC) + 2;
								}

								/* get IRQ number and PCI interrupt (A-D) */
								IRQ_n = pci_read_cfgb(bus,dev,func,0x3C);
								IRQ_pin = pci_read_cfgb(bus,dev,func,0x3D);
								if (IRQ_n != 0 && IRQ_n < 16 && IRQ_pin != 0 && IRQ_pin <= 4)
									ide.irq = IRQ_n;
								else
									ide.irq = -1;

								if (ide.base_io != 0 && (ide.base_io&7) == 0) {
									printf("      PCI IDE%u in native mode, IRQ=%d base=0x%3x alt=0x%3x\n",
											iport,ide.irq,ide.base_io,ide.alt_io);

									if ((newide = idelib_probe(&ide)) == NULL)
										printf("    Warning: probe failed\n");

									/* HACK: An ASUS Intel Core i3 motherboard I own has a SATA controller
									 * that has problems with IDE interrupts (when set to IDE mode).
									 * Once an IDE interrupt fires there's no way to shut it off and
									 * the controller crapfloods the PIC causing our program to "hang"
									 * running through the IRQ handler. */
									if (vendor == 0x8086 && device == 0x8C80) { /* Intel Haswell-based motherboard (2014) */
										idelib_enable_interrupt(newide,0); /* don't bother with interrupts */
									}
								}
							}
							else {
								/* "compatability mode".
								 * this is retarded, why didn't the PCI standards people just come out and
								 * say: guys, if you're a PCI device then frickin' show up as a proper PCI
								 * device and announce what resources you're using in the BARs and IRQ
								 * registers so OSes are not required to guess like this! */
								ide.base_io = iport ? 0x170 : 0x1F0;
								ide.alt_io = iport ? 0x376 : 0x3F6;
								ide.irq = iport ? 15 : 14;

								printf("      PCI IDE%u in compat mode, IRQ=%d base=0x%3x alt=0x%3x\n",
										iport,ide.irq,ide.base_io,ide.alt_io);

								if ((newide = idelib_probe(&ide)) == NULL)
									printf("    Warning: probe failed\n");
							}
						}
					}
				}
			}
		}
	}
#endif
#ifdef ISAPNP
	if (!opt_no_isapnp) {
		if (!init_isa_pnp_bios()) {
			printf("Cannot init ISA PnP\n");
		}
		if (find_isa_pnp_bios()) {
			unsigned int nodesize=0;
			unsigned char node=0,numnodes=0xFF,data[192];

			memset(data,0,sizeof(data));
			printf("ISA PnP BIOS detected\n");
			if (isa_pnp_bios_get_pnp_isa_cfg(data) == 0) {
				struct isapnp_pnp_isa_cfg *nfo = (struct isapnp_pnp_isa_cfg*)data;
				isapnp_probe_next_csn = nfo->total_csn;
				isapnp_read_data = nfo->isa_pnp_port;
			}
			else {
				printf("  ISA PnP BIOS failed to return configuration info\n");
			}

			/* enumerate device nodes reported by the BIOS */
			if (isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize) == 0 && numnodes != 0xFF && nodesize <= sizeof(devnode_raw)) {
				printf("Scanning ISA PnP BIOS devices...\n");
				for (node=0;node != 0xFF;) {
					struct isa_pnp_device_node far *devn;
					unsigned char far *rsc, far *rf;
					unsigned char this_node;
					struct isapnp_tag tag;
					unsigned int ioport1=0;
					unsigned int ioport2=0;
					unsigned int i;
					int irq = -1;

					/* apparently, start with 0. call updates node to
					 * next node number, or 0xFF to signify end */
					this_node = node;
					if (isa_pnp_bios_get_sysdev_node(&node,devnode_raw,ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW) != 0) break;

					devn = (struct isa_pnp_device_node far*)devnode_raw;
					if (devn->type_code[0] == 0x01/*system device, hard disk controller*/ &&
							devn->type_code[1] == 0x01/*Generic ESDI/IDE/ATA controller*/ &&
							devn->type_code[2] == 0x00/*Generic IDE*/) {
						rsc = (unsigned char far*)devn + sizeof(*devn);
						rf = (unsigned char far*)devn + sizeof(devnode_raw);

						do {
							if (!isapnp_read_tag(&rsc,rf,&tag))
								break;
							if (tag.tag == ISAPNP_TAG_END)
								break;

							/* NTS: A Toshiba Satellite 465CDX I own lists the primary IDE controller's alt port range (0x3F6)
							 *      as having length == 1 for some reason. Probably because of the floppy controller. */

							switch (tag.tag) {
								case ISAPNP_TAG_IO_PORT: {
									struct isapnp_tag_io_port far *x = (struct isapnp_tag_io_port far*)tag.data;
									if (ioport1 == 0 && x->length == 8)
										ioport1 = x->min_range;
									else if (ioport2 == 0 && (x->length == 1 || x->length == 2 || x->length == 4))
										ioport2 = x->min_range;
								} break;
							case ISAPNP_TAG_FIXED_IO_PORT: {
								struct isapnp_tag_fixed_io_port far *x = (struct isapnp_tag_fixed_io_port far*)tag.data;
								if (ioport1 == 0 && x->length == 8)
									ioport1 = x->base;
								else if (ioport2 == 0 && (x->length == 1 || x->length == 2 || x->length == 4))
									ioport2 = x->base;
								} break;
							case ISAPNP_TAG_IRQ_FORMAT: {
								struct isapnp_tag_irq_format far *x = (struct isapnp_tag_irq_format far*)tag.data;
								for (i=0;i < 16;i++) {
									if (x->irq_mask & (1U << (unsigned int)i)) { /* NTS: PnP devices usually support odd IRQs like IRQ 9 */
										if (irq < 0) irq = i;
									}
								}
								} break;
							}
						} while (1);

						if (ioport1 != 0) {
							struct ide_controller n;

							printf("  Found PnP IDE controller: base=0x%03x alt=0x%03x IRQ=%d\n",
								ioport1,ioport2,irq);

							memset(&n,0,sizeof(n));
							n.base_io = ioport1;
							n.alt_io = ioport2;
							n.irq = (int8_t)irq; /* -1 is no IRQ */
							if ((newide = idelib_probe(&n)) == NULL) {
								printf("    Warning: probe failed\n");
								/* not filling it in leaves it open for allocation again */
							}
						}
					}
				}
			}
		}
	}
#endif

	if (!opt_no_isa_probe) {
		printf("Probing standard IDE ports...\n");
		for (i=0;(idectrl = (struct ide_controller*)idelib_get_standard_isa_port(i)) != NULL;i++) {
			printf("   %3X/%3X IRQ %d: ",idectrl->base_io,idectrl->alt_io,idectrl->irq); fflush(stdout);

			if ((newide = idelib_probe(idectrl)) != NULL) {
				printf("FOUND: alt=%X irq=%d\n",newide->alt_io,newide->irq);
			}
			else {
				printf("\x0D                             \x0D"); fflush(stdout);
			}
		}
	}

	if (opt_no_irq) {
		unsigned int i;

		for (i=0;i < MAX_IDE_CONTROLLER;i++) {
			struct ide_controller *ide = &ide_controller[i];
			if (idelib_controller_allocated(ide)) idelib_enable_interrupt(ide,0); /* don't bother with interrupts */
		}
	}

	printf("Hit ENTER to continue, ESC to cancel\n");
	i = wait_for_enter_or_escape();
	if (i == 27) {
		smartdrv_close();
		free_idelib();
		return 0;
	}

	if (int10_getmode() != 3) {
		int10_setmode(3);
		update_state_from_vga();
	}

	do_main_menu();
	smartdrv_close();
	free_idelib();
	return 0;
}

