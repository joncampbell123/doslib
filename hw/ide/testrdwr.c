
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
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

void do_ide_controller_drive_rw_test(struct ide_controller *ide,unsigned char which,unsigned char writetest) {
	struct vga_msg_box vgabox;
	char redraw=1;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y,i;
	int select=-1;
	int c;

	if (big_scary_write_test_warning && writetest) {
		vga_msg_box_create(&vgabox,"WARNING!!! WARNING!!! WARNING!!!\n"
			"\n"
			"The write test OVERWRITES data on the selected hard drive!\n"
			"I hope the drive doesn't have anything important!\n"
			"To cancel the test, hit ESC now!"
			,0,0);
		do {
			c = getch();
			if (c == 0) c = getch() << 8;
		} while (!(c == 13 || c == 27));
		vga_msg_box_destroy(&vgabox);
		if (c == 27) return;

		vga_msg_box_create(&vgabox,"WARNING!!! WARNING!!! WARNING!!!\n"
			"\n"
			"The hard drive contents WILL BE OVERWRITTEN BY THIS TEST PROGRAM!\n"
			"If you value your data, even on unrelated drives, hit ESC now!"
			,0,0);
		do {
			c = getch();
			if (c == 0) c = getch() << 8;
		} while (!(c == 13 || c == 27));
		vga_msg_box_destroy(&vgabox);
		if (c == 27) return;

		vga_msg_box_create(&vgabox,"FINAL WARNING!!!\n"
			"\n"
			"This will overwrite the drive's partition table, file system, and data!\n"
			"Last change to hit ESC and save the data, if you value it!"
			,0,0);
		do {
			c = getch();
			if (c == 0) c = getch() << 8;
		} while (!(c == 13 || c == 27));
		vga_msg_box_destroy(&vgabox);
		if (c == 27) return;

		big_scary_write_test_warning = 0;
	}

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
			vga_write(writetest ? " Write" : " Read");
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
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 1) ? 0x70 : 0x0F);
			vga_write("LBA PIO ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 2) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO ");
			vga_write(writetest ? "WRITE (34h)" : "READ (24h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 3) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO MS ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 4) ? 0x70 : 0x0F);
			vga_write("C/H/S PIO TRK ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 5) ? 0x70 : 0x0F);
			vga_write("LBA PIO MS ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 6) ? 0x70 : 0x0F);
			vga_write("LBA PIO 63x ");
			vga_write(writetest ? "WRITE (30h)" : "READ (20h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 7) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO MS ");
			vga_write(writetest ? "WRITE (34h)" : "READ (24h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 8) ? 0x70 : 0x0F);
			vga_write("LBA48 PIO 63x ");
			vga_write(writetest ? "WRITE (34h)" : "READ (24h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 9) ? 0x70 : 0x0F);
			vga_write("LBAMULT PIO MS ");
			vga_write(writetest ? "WRITE (C5h)" : "READ (C4h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 10) ? 0x70 : 0x0F);
			vga_write("LBAMULT PIO 63x ");
			vga_write(writetest ? "WRITE (C5h)" : "READ (C4h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 11) ? 0x70 : 0x0F);
			vga_write("LBA48MULT PIO MS ");
			vga_write(writetest ? "WRITE (39h)" : "READ (29h)");
			while (vga_pos_x < ((vga_width/cols)-4) && vga_pos_x != 0) vga_writec(' ');

			vga_moveto(8,y++);
			vga_write_color((select == 12) ? 0x70 : 0x0F);
			vga_write("LBA48MULT PIO 63 ");
			vga_write(writetest ? "WRITE (39h)" : "READ (29h)");
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
			else if (select == 0 || select == 1 || select == 2 || select == 3 || select == 4 || select == 5 || select == 6 || select == 7 || select == 8 || select == 9 || select == 10 || select == 11 || select == 12) {
				unsigned long long sector = 0;
				unsigned long track;
				unsigned int sectn,head;
				unsigned long tlen = 512; /* one sector */
				unsigned int stop = 0;
				unsigned long tlen_sect = 1;
				unsigned char cmd;
				unsigned int sect_per_block = 1; /* READ/WRITE MULTIPLE */
				unsigned int tracks=0,heads=0,sects=0;

				if (select == 0 || select == 3 || select == 4 || select == 9 || select == 10 || select == 11 || select == 12) {
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

								if (select == 9 || select == 10 || select == 11 || select == 12)
									sect_per_block = ide_info[59] & 0xFF;

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

				/* NTS: The draft ATA-8 spec copy I have says WRITE MULTIPLE is 0xC3. That's WRONG! */
				if (select == 11 || select == 12) /* READ/WRITE MULTIPLE EXT (LBA48) */
					cmd = writetest ? 0x39 : 0x29;
				else if (select == 9 || select == 10) /* READ/WRITE MULTIPLE */
					cmd = writetest ? 0xC5 : 0xC4;
				else if (select == 2 || select == 7 || select == 8) /* READ/WRITE LBA48 */
					cmd = writetest ? 0x34 : 0x24;
				else
					cmd = writetest ? 0x30 : 0x20;

				while (((tracks != 0 && heads != 0 && sects != 0) || select == 1 || select == 2 || select == 5 || select == 6 || select == 7 || select == 8 || select == 9 || select == 10 || select == 11 || select == 12) && !stop) {
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
						else if (select == 5 || select == 7 || select == 9 || select == 11) {
							tlen_sect = 7;
							tlen = 512 * tlen_sect;
						}
						else if (select == 6 || select == 8 || select == 10 || select == 12) {
							tlen_sect = 63;
							tlen = 512 * tlen_sect;
						}

						assert(tlen <= sizeof(cdrom_sector));
						sprintf(tmp,"%s sector %016llu (%016llX) %u sects",writetest?"Writing":"Reading",sector,sector,(unsigned int)tlen_sect);
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

						if (writetest) {
							unsigned int i,l,j;

							for (i=0;i < tlen_sect;i++) {
								memset(cdrom_sector+(i*512),0xAA,512);
								l = (unsigned int)sprintf(cdrom_sector+(i*512)+2,
									"IDE TEST PROGRAM WAS HERE sector %llu (%u/%u)",
									sector+(unsigned long long)i,
									i+1,tlen_sect);
								for (j=l-1;j != ((unsigned int)(-1));j--) {
									cdrom_sector[(j*2)+(i*512)+2+0] = cdrom_sector[j+(i*512)+2];
									cdrom_sector[(j*2)+(i*512)+2+1] = 0x07;
								}
							}
						}
						else {
							memset(cdrom_sector,0,tlen);
						}

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

						if (sect_per_block == 1) {
							/* Traditional PIO: One DRQ+IRQ per sector. */
							for (sectn=0;sectn < tlen_sect;sectn++) {
								/* READ: IRQ will fire when data is ready.
								 * WRITE: Do not wait for IRQ yet */
								if (!writetest && ide->flags.io_irq_enable)
									do_ide_controller_user_wait_irq(ide,1);

								ide->irq_fired = 0; /* <- having waited, reset counter */
								if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
								if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
								x = inp(ide->base_io+7); /* what's the status? */
								if (!(x&1)) { /* if no error, read result from count register */
									if (writetest) {
										if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
											if (ide->pio_width >= 32) {
												if (ide->pio_width == 33) ide_vlb_sync32_pio(ide);

												for (i=0;i < (512/4UL);i++)
													outpd(ide->base_io+0,((uint32_t*)cdrom_sector)[i+(sectn*128)]);
											}
											else {
												for (i=0;i < (512/2UL);i++)
													outpw(ide->base_io+0,((uint16_t*)cdrom_sector)[i+(sectn*256)]);
											}

											/* IRQ will fire when writing complete */
											if (ide->flags.io_irq_enable)
												do_ide_controller_user_wait_irq(ide,1);
										}

										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
									}
									else { /* read */
										if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
											if (ide->pio_width >= 32) {
												if (ide->pio_width == 33) ide_vlb_sync32_pio(ide);

												for (i=0;i < (512/4UL);i++)
													((uint32_t*)cdrom_sector)[i+(sectn*128)] = inpd(ide->base_io+0);
											}
											else {
												for (i=0;i < (512/2UL);i++)
													((uint16_t*)cdrom_sector)[i+(sectn*256)] = inpw(ide->base_io+0);
											}

											y = 1;
										}

										/* wait for controller */
										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
									}
								}

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

								if (x&1) break;
							}
						}
						else {
							/* READ/WRITE MULTIPLE: One DRQ+IRQ every 'sect_per_block' sectors */
							for (sectn=0;sectn < tlen_sect;sectn += sect_per_block) {
								unsigned int rem = tlen_sect - sectn;
								if (rem > sect_per_block) rem = sect_per_block;

								/* READ: IRQ will fire when data is ready.
								 * WRITE: Do not wait for IRQ yet */
								if (!writetest && ide->flags.io_irq_enable)
									do_ide_controller_user_wait_irq(ide,1);

								ide->irq_fired = 0; /* <- having waited, reset counter */
								if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
								if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
								x = inp(ide->base_io+7); /* what's the status? */
								if (!(x&1)) { /* if no error, read result from count register */
									if (writetest) {
										if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
											/* write sector */
											for (i=0;i < (256UL*rem);i++)
												outpw(ide->base_io+0,((uint16_t*)cdrom_sector)[i+(sectn*256)]);

											/* IRQ will fire when writing complete */
											if (ide->flags.io_irq_enable)
												do_ide_controller_user_wait_irq(ide,1);
										}

										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
									}
									else { /* read */
										if (do_ide_controller_user_wait_drive_drq(ide) == 0) {
											/* suck in the data and display it */
											for (i=0;i < (256UL*rem);i++)
												((uint16_t*)cdrom_sector)[i+(sectn*256)] = inpw(ide->base_io+0);

											y = 1;
										}

										/* wait for controller */
										if (do_ide_controller_user_wait_busy_controller(ide) != 0) break;
										if (do_ide_controller_user_wait_drive_ready(ide) != 0) break;
										x = inp(ide->base_io+7); /* what's the status? */
									}
								}

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

								if (x&1) break;
							}
						}

						if (x&1) {
							if (select == 2) {
							}
							else {
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

						if (!stop && y) {
							for (i=0;i < (tlen>>1U);i++)
								vga_alpha_ram[i+(vga_width*2)] = ((uint16_t*)cdrom_sector)[i];

							if (select == 2) {
								if (sector == 0ULL) {
									for (i=0;i < (tlen>>1U);i++)
										vga_alpha_ram[i+(vga_width*(vga_height - 9))] = ((uint16_t*)cdrom_sector)[i];
								}
								else if (sector == 0x1000000ULL) {
									for (i=0;i < (tlen>>1U);i++)
										vga_alpha_ram[i+(vga_width*(vga_height - 5))] = ((uint16_t*)cdrom_sector)[i];
								}

								if (sector == 0ULL || sector == 0x1000000ULL) {
									vga_msg_box_create(&vgabox,"Pausing contents to let you view",0,0);
									do {
										c = getch();
										if (c == 0) c = getch() << 8;
									} while (!(c == 13 || c == 27));
									vga_msg_box_destroy(&vgabox);
								}
							}
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
				select = 12;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > 12)
				select = -1;

			redraw = 1;
		}
	}
}

