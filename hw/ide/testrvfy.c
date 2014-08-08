
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
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

void do_ide_controller_drive_readverify_test(struct ide_controller *ide,unsigned char which) {
	struct vga_msg_box vgabox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y,i;
	char redraw=1;
	int select=-1;
	int c;

	/* most of the commands assume a ready controller. if it's stuck,
	 * we'd rather the user have a visual indication that it's stuck that way */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* select the drive we want */
	idelib_controller_drive_select(ide,which,/*head*/0,IDELIB_DRIVE_SELECT_MODE_CHS);

	/* in case the IDE controller is busy for that time */
	c = do_ide_controller_user_wait_busy_controller(ide);
	if (c != 0) return;

	/* it might be a CD-ROM drive, which in some cases might not raise the Drive Ready bit */
	do_ide_controller_atapi_device_check_post_host_reset(ide);

	/* wait for the drive to indicate readiness */
	c = do_ide_controller_user_wait_drive_ready(ide);
	if (c != 0) return;

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
			vga_write(" Read verify");
			vga_write(" test");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');

			vga_write_color(0xC);
			vga_write("WARNING: This code talks directly to your hard disk controller.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
			vga_write_color(0xC);
			vga_write("         If you value the data on your hard drive do not run this program.");
			while (vga_pos_x < vga_width && vga_pos_x != 0) vga_writec(' ');
		}

		if (redraw) {
			const int cols = 2;

			redraw = 0;

			y = 5;
			vga_moveto(8,y++);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back to IDE drive menu");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			y = 7;
			vga_moveto(8,y++);
			vga_write_color((select == 0) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("LBA PIO ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO ");
			vga_write("READ (42h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO MS ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 4) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO TRK ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 5) ? 0x70 : 0x0F);
			vga_write("LBA PIO MS ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 6) ? 0x70 : 0x0F);
			vga_write("LBA PIO 63x ");
			vga_write("READ (40h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 7) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO MS ");
			vga_write("READ (42h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 8) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO 63x ");
			vga_write("READ (42h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');
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
			else if (select == 0 || select == 1 || select == 2 || select == 3 || select == 4 || select == 5 || select == 6 || select == 7 || select == 8) {
				unsigned long long sector = 0;
				unsigned long track;
				unsigned int sectn,head;
				unsigned long tlen = 512; /* one sector */
				unsigned int stop = 0;
				unsigned long tlen_sect = 1;
				unsigned char cmd;
				unsigned int sect_per_block = 1; /* READ/WRITE MULTIPLE */
				unsigned int tracks=0,heads=0,sects=0;

				if (select == 0 || select == 3 || select == 4) {
					/* ask for C/H/S geometry or MULTIPLE block size */
					if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
						do_ide_controller_user_wait_drive_ready(ide) == 0) {
						ide->irq_fired = 0;
						outp(ide->base_io+7,0xEC); /* <- identify device */
						if (ide->flags.io_irq_enable)
							do_ide_controller_user_wait_irq(ide,1);

						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
						x = inp(ide->base_io+7); /* what's the status? */
						if (!(x&1)) { /* if no error, read result from count register */
							if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
								/* wait for Data Request */
								for (i=0;i < 256;i++)
									ide_info[i] = inpw(ide->base_io+0); /* read 16-bit word from data port, PIO */

								tracks = ide_info[1];
								heads = ide_info[3] & 0xFF;
								sects = ide_info[6] & 0xFF;

								sprintf(tmp,"C/H/S %u/%u/%u MULTBLK=%u",tracks,heads,sects,sect_per_block);
								vga_msg_box_create(&vgabox,tmp,0,0);
								do {
									c = getch();
									if (c == 0) c = getch() << 8;
								} while (!(c == 13 || c == 27));
								vga_msg_box_destroy(&vgabox);

								if (c == 27) {
									tracks = heads = sects = 0;
								}
							}
						}

						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
					}
				}

				if (sect_per_block == 0)
					sect_per_block = 1;

				if (select == 2 || select == 7 || select == 8) /* READ LBA48 */
					cmd = 0x42; /* FIXME */
				else
					cmd = 0x40;

				while (((tracks != 0 && heads != 0 && sects != 0) || select == 1 || select == 2 || select == 5 || select == 6 || select == 7 || select == 8) && !stop) {
					if (do_ide_controller_user_wait_busy_controller(ide) == 0 &&
						do_ide_controller_user_wait_drive_ready(ide) == 0) {
						if (select == 0 || select == 3 || select == 4) { /* CHS */
							sectn = 1 + ((unsigned long)sector % (unsigned long)sects);
							head = ((unsigned long)sector / (unsigned long)sects) % (unsigned long)heads;
							track = (unsigned long)sector / (unsigned long)sects / (unsigned long)heads;

							if (select == 3) { /* multisector read with CHS but don't cross track boundaries */
								tlen_sect = 7;
								if ((sectn+tlen_sect) > sects) tlen_sect = (sects - sectn) + 1;
								tlen = 512 * tlen_sect;
							}
							else if (select == 4) { /* read to end of track */
								tlen_sect = (sects - sectn) + 1;
								if (tlen_sect > sects) tlen_sect = sects;
								tlen = 512 * tlen_sect;
							}
						}
						else if (select == 5 || select == 7) {
							tlen_sect = 7;
							tlen = 512 * tlen_sect;
						}
						else if (select == 6 || select == 8) {
							tlen_sect = 63;
							tlen = 512 * tlen_sect;
						}

						assert(tlen <= sizeof(cdrom_sector));
						sprintf(tmp,"%s sector %016llu (%016llX) %u sects","Reading",sector,sector,(unsigned int)tlen_sect);
						vga_moveto(0,0);
						vga_write_color(0x0E);
						vga_write(tmp);
						if (select == 0 || select == 3 || select == 4) {
							sprintf(tmp," CHS %lu/%lu/%lu",
								(unsigned long)track,
								(unsigned long)head,
								(unsigned long)sectn);
							vga_write(tmp);
						}
						vga_write("           ");

						memset(cdrom_sector,0,tlen);

						if (kbhit()) {
							c = getch();
							if (c == 0) c = getch() << 8;
							if (c == 27) {
								stop = 1;
								break;
							}
						}

						y = 0;
						if (select == 0 || select == 3 || select == 4) { /* CHS */
							idelib_controller_drive_select(ide,which,head,IDELIB_DRIVE_SELECT_MODE_CHS);
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);
							outp(ide->base_io+2,tlen_sect); /* number of sectors (NOTE: 0 means 256 sectors) */
							outp(ide->base_io+3,sectn); /* sector # */
							outp(ide->base_io+4,track); /* track */
							outp(ide->base_io+5,track >> 8UL);
						}
						else if (select == 2 || select == 7 || select == 8 || select == 11 || select == 12) { /* LBA48 */
							vga_write("LBA48");

							outp(ide->base_io+6,0x40 | (which << 4));
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);

							outp(ide->base_io+2,tlen_sect >> 8); /* number of sectors hi byte */
							outp(ide->base_io+3,sector >> 24ULL);
							outp(ide->base_io+4,sector >> 32ULL);
							outp(ide->base_io+5,sector >> 40ULL);

							outp(ide->base_io+2,tlen_sect); /* number of sectors lo byte */
							outp(ide->base_io+3,sector);
							outp(ide->base_io+4,sector >> 8ULL);
							outp(ide->base_io+5,sector >> 16ULL);
						}
						else { /* LBA */
							outp(ide->base_io+6,0xE0 | (which << 4) | ((sector >> 24UL) & 0xFUL));
							if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
							if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
							outp(ide->base_io+1,0x00);
							outp(ide->base_io+2,tlen_sect); /* number of sectors (NOTE: 0 means 256 sectors) */
							outp(ide->base_io+3,sector); /* sector # */
							outp(ide->base_io+4,sector >> 8ULL); /* track */
							outp(ide->base_io+5,sector >> 16ULL);
						}
						ide->irq_fired = 0;
						outp(ide->base_io+7,cmd); /* command */

						/* unlike the actual read command, it does not raise DRQ, and does not fire an interrupt
						 * until the whole sector verify is complete. */
						if (ide->flags.io_irq_enable)
							do_ide_controller_user_wait_irq(ide,1);

						ide->irq_fired = 0; /* <- having waited, reset counter */
						if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
						if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
						x = inp(ide->base_io+7); /* what's the status? */

						/* we want to know (for debugging) if the IRQ occurs multiple times */
						if (ide->irq_fired > 1) {
							sprintf(tmp,"Device fired too many IRQs (%u)",ide->irq_fired);
							vga_msg_box_create(&vgabox,tmp,0,0);
							do {
								c = getch();
								if (c == 0) c = getch() << 8;
							} while (!(c == 13 || c == 27));
							vga_msg_box_destroy(&vgabox);
						}

						if (x&1) {
							stop = 1;
							sprintf(tmp,"Device rejected with error %02X",x);
							vga_msg_box_create(&vgabox,tmp,0,0);
							do {
								c = getch();
								if (c == 0) c = getch() << 8;
							} while (!(c == 13 || c == 27));
							vga_msg_box_destroy(&vgabox);
						}
					}
					else {
						stop = 1;
					}

					if (!stop) {
						if (select == 2 && (sector & 0xFFFFFFULL) == 0ULL) {
							if (sector < 0x1000000ULL)
								sector += 0x1000000ULL;
							else if (sector < 0x10000000ULL)
								sector += 0x10000000ULL - 0x1000000ULL;
							else if (sector < 0x100000000ULL)
								sector += 0x100000000ULL - 0x10000000ULL;
							else
								sector = (sector + 1ULL) - 0x100000000ULL;
						}
						else {
							sector += tlen_sect;
						}
					}
					redraw = backredraw = 1;
				}
			}
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = 8;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 8)
				select = -1;

			redraw = 1;
		}
	}
}

