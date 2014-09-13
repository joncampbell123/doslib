
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
#include "testrdts.h"
#include "test.h"

#include "testnop.h"
#include "testpwr.h"

const char *drive_readwrite_test_modes[] = {
	"Mode: C/H/S",
	"Mode: C/H/S MULTIPLE",
	"Mode: LBA",
	"Mode: LBA MULTIPLE",
	"Mode: LBA48",
	"Mode: LBA48 MULTIPLE"
};

struct drive_rw_test_info		drive_rw_test_nfo;

char					drive_readwrite_test_geo[128];
char					drive_readwrite_test_chs[128];
char					drive_readwrite_test_numsec[128];
char					drive_readwrite_test_mult[128];

int drive_rw_test_mode_supported(struct drive_rw_test_info *nfo) {
	if (!drive_rw_test_nfo.can_do_multiple &&
		(nfo->mode == DRIVE_RW_MODE_CHSMULTIPLE ||
		 nfo->mode == DRIVE_RW_MODE_LBAMULTIPLE ||
		 nfo->mode == DRIVE_RW_MODE_LBA48_MULTIPLE))
		return 0;

	if (!drive_rw_test_nfo.can_do_lba &&
		(nfo->mode == DRIVE_RW_MODE_LBA ||
		 nfo->mode == DRIVE_RW_MODE_LBAMULTIPLE ||
		 nfo->mode == DRIVE_RW_MODE_LBA48 ||
		 nfo->mode == DRIVE_RW_MODE_LBA48_MULTIPLE))
		return 0;

	if (!drive_rw_test_nfo.can_do_lba48 &&
		(nfo->mode == DRIVE_RW_MODE_LBA48 ||
		 nfo->mode == DRIVE_RW_MODE_LBA48_MULTIPLE))
		return 0;

	return 1;
}

void do_drive_readwrite_edit_chslba(struct ide_controller *ide,unsigned char which,struct drive_rw_test_info *nfo,unsigned char editgeo) {
	uint64_t lba,tmp;
	int cyl,head,sect;
	struct vga_msg_box box;
	unsigned char redraw=1;
	unsigned char ok=1;
	char temp_str[64];
	int select=0;
	int c,i=0;

	if (editgeo) {
		cyl = nfo->num_cylinder;
		sect = nfo->num_sector;
		head = nfo->num_head;
		lba = nfo->max_lba;
	}
	else {
		cyl = nfo->cylinder;
		sect = nfo->sector;
		head = nfo->head;
		lba = nfo->lba;
	}

	vga_msg_box_create(&box,editgeo ?
		"Edit disk geometry:             " :
		"Edit position:                  ",
		2+4,0);
	while (1) {
		char recalc = 0;
		char rekey = 0;

		if (redraw) {
			redraw = 0;

			vga_moveto(box.x+2,box.y+2+1 + 0);
			vga_write_color(0x1E);
			vga_write("Cylinder:  ");
			vga_write_color(select == 0 ? 0x70 : 0x1E);
			i=sprintf(temp_str,"%u",cyl);
			while (i < (box.w-4-11)) temp_str[i++] = ' ';
			temp_str[i] = 0;
			vga_write(temp_str);

			vga_moveto(box.x+2,box.y+2+1 + 1);
			vga_write_color(0x1E);
			vga_write("Head:      ");
			vga_write_color(select == 1 ? 0x70 : 0x1E);
			i=sprintf(temp_str,"%u",head);
			while (i < (box.w-4-11)) temp_str[i++] = ' ';
			temp_str[i] = 0;
			vga_write(temp_str);

			vga_moveto(box.x+2,box.y+2+1 + 2);
			vga_write_color(0x1E);
			vga_write("Sector:    ");
			vga_write_color(select == 2 ? 0x70 : 0x1E);
			i=sprintf(temp_str,"%u",sect);
			while (i < (box.w-4-11)) temp_str[i++] = ' ';
			temp_str[i] = 0;
			vga_write(temp_str);

			vga_moveto(box.x+2,box.y+2+1 + 3);
			vga_write_color(0x1E);
			vga_write("LBA:       ");
			vga_write_color(select == 3 ? 0x70 : 0x1E);
			i=sprintf(temp_str,"%llu",lba);
			while (i < (box.w-4-11)) temp_str[i++] = ' ';
			temp_str[i] = 0;
			vga_write(temp_str);
		}

		c = getch();
		if (c == 0) c = getch() << 8;

nextkey:	if (c == 27) {
			ok = 0;
			break;
		}
		else if (c == 13) {
			ok = 1;
			break;
		}
		else if (c == 0x4800) {
			if (--select < 0) select = 3;
			redraw = 1;
		}
		else if (c == 0x5000 || c == 9/*tab*/) {
			if (++select > 3) select = 0;
			redraw = 1;
		}

		else if (c == 0x4B00) { /* left */
			switch (select) {
				case 0:
					if (cyl == 0) cyl = editgeo ? 16383 : (nfo->num_cylinder - 1);
					else cyl--;
					break;
				case 1:
					if (head == 0) head = editgeo ? 16 : (nfo->num_head - 1);
					else head--;
					break;
				case 2:
					if (sect <= 1) sect = editgeo ? 256 : nfo->num_sector;
					else sect--;
					break;
				case 3:
					if (lba > 0ULL) lba--;
					break;
			};

			recalc = 1;
			redraw = 1;
		}
		else if (c == 0x4D00) { /* right */
			switch (select) {
				case 0:
					if ((++cyl) >= (editgeo ? 16384 : nfo->num_cylinder)) cyl = 0;
					break;
				case 1:
					if ((++head) >= (editgeo ? 17 : nfo->num_head)) head = 0;
					break;
				case 2:
					if ((++sect) >= (editgeo ? 257 : (nfo->num_sector+1))) sect = 1;
					break;
				case 3:
					lba++;
					break;
			};

			recalc = 1;
			redraw = 1;
		}

		else if (c == 8 || isdigit(c)) {
			unsigned int sy = box.y+2+1 + select;
			unsigned int sx = box.x+2+11;

			switch (select) {
				case 0:	sprintf(temp_str,"%u",cyl); break;
				case 1:	sprintf(temp_str,"%u",head); break;
				case 2:	sprintf(temp_str,"%u",sect); break;
				case 3:	sprintf(temp_str,"%llu",lba); break;
			}

			if (c == 8) {
				i = strlen(temp_str) - 1;
				if (i < 0) i = 0;
				temp_str[i] = 0;
			}
			else {
				i = strlen(temp_str);
				if (i == 1 && temp_str[0] == '0') i--;
				if ((i+2) < sizeof(temp_str)) {
					temp_str[i++] = (char)c;
					temp_str[i] = 0;
				}
			}

			redraw = 1;
			while (1) {
				if (redraw) {
					redraw = 0;
					vga_moveto(sx,sy);
					vga_write_color(0x70);
					vga_write(temp_str);
					while (vga_pos_x < (box.x+box.w-4) && vga_pos_x != 0) vga_writec(' ');
				}

				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 8) {
					if (i > 0) {
						temp_str[--i] = 0;
						redraw = 1;
					}
				}
				else if (isdigit(c)) {
					if ((i+2) < sizeof(temp_str)) {
						temp_str[i++] = (char)c;
						temp_str[i] = 0;
						redraw = 1;
					}
				}
				else {
					break;
				}
			}

			switch (select) {
				case 0:	tmp=strtoull(temp_str,NULL,0); cyl=(tmp >  16383ULL ? 16383ULL : tmp); break;
				case 1:	tmp=strtoull(temp_str,NULL,0); head=(tmp >    16ULL ?    16ULL : tmp); break;
				case 2:	tmp=strtoull(temp_str,NULL,0); sect=(tmp >   256ULL ?   256ULL : tmp); break;
				case 3:	lba=strtoull(temp_str,NULL,0); break;
			}

			rekey = 1;
			recalc = 1;
		}

		if (recalc) {
			recalc = 0;
			if (sect == 0) sect = 1;
			if (cyl > 16383) cyl = 16383;

			if (editgeo) {
				if (cyl < 1) cyl = 1;
				if (head < 1) head = 1;
				if (head > 16) head = 16;
				if (sect > 256) sect = 256;

				if (select == 3) {
					if (lba >= (16383ULL * 16ULL * 63ULL)) {
						sect = 63;
						head = 16;
						cyl = 16383;
					}
					else {
						cyl = (int)(lba / (unsigned long long)sect / (unsigned long long)head);
						if (cyl < 0) cyl = 1;
						if (cyl > 16383) cyl = 16383;
					}
				}
				else if (cyl < 16383) {
					lba = (unsigned long long)cyl * (unsigned long long)head * (unsigned long long)sect;
				}
			}
			else {
				if (cyl < 0) cyl = 0;
				if (head < 0) head = 0;
				if (head > 15) head = 15;
				if (sect > 255) sect = 255;

				if (select == 3) {
					sect = (int)(lba % (unsigned long long)nfo->num_sector) + 1;
					head = (int)((lba / (unsigned long long)nfo->num_sector) % (unsigned long long)nfo->num_head);
					if (lba >= (16384ULL * (unsigned long long)nfo->num_sector * (unsigned long long)nfo->num_head)) {
						cyl = 16383;
						sect = nfo->num_sector;
						head = nfo->num_head - 1;
					}
					else {
						cyl = (int)((lba / (unsigned long long)nfo->num_sector) / (unsigned long long)nfo->num_head);
					}
				}
				else {
					lba  = (unsigned long long)cyl * (unsigned long long)nfo->num_sector * (unsigned long long)nfo->num_head;
					lba += (unsigned long long)head * (unsigned long long)nfo->num_sector;
					lba += (unsigned long long)sect - 1ULL;
				}
			}

			redraw = 1;
		}

		if (rekey) {
			rekey = 0;
			goto nextkey;
		}
	}
	vga_msg_box_destroy(&box);

	if (ok) {
		if (editgeo) {
			nfo->num_cylinder = cyl;
			nfo->num_sector = sect;
			nfo->num_head = head;
			nfo->max_lba = lba;
		}
		else {
			nfo->cylinder = cyl;
			nfo->sector = sect;
			nfo->head = head;
			nfo->lba = lba;
		}
	}
}

void do_drive_readwrite_test_choose_mode(struct ide_controller *ide,unsigned char which,struct drive_rw_test_info *nfo) {
	int select=drive_rw_test_nfo.mode;
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_readwrite_test_modes);

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
			vga_write("        IDE r/w test mode ");
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

			sprintf(tmp," lba=%u lba48=%u multiple=%u",
				drive_rw_test_nfo.can_do_lba?1:0,
				drive_rw_test_nfo.can_do_lba48?1:0,
				drive_rw_test_nfo.can_do_multiple?1:0);
			vga_write(tmp);

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

			vga_moveto(mbox.ofsx,mbox.ofsy - 2);
			vga_write_color((select == -1) ? 0x70 : 0x0F);
			vga_write("Back");
			while (vga_pos_x < (mbox.width+mbox.ofsx) && vga_pos_x != 0) vga_writec(' ');

			menuboxbound_redraw(&mbox,select);
		}

		c = getch();
		if (c == 0) c = getch() << 8;

		if (c == 27) {
			break;
		}
		else if (c == 13) {
			struct vga_msg_box vgabox;

			if (!drive_rw_test_nfo.can_do_multiple &&
				(select == DRIVE_RW_MODE_CHSMULTIPLE ||
				select == DRIVE_RW_MODE_LBAMULTIPLE ||
				select == DRIVE_RW_MODE_LBA48_MULTIPLE)) {

				vga_msg_box_create(&vgabox,
					"The IDE device doesn't seem to support SET MULTIPLE/READ MULTIPLE.\n"
					"Are you sure you want to choose this mode?\n"
					"\n"
					"Hit ENTER to proceed, ESC to cancel"
					,0,0);
				do {
					c = getch();
					if (c == 0) c = getch() << 8;
				} while (!(c == 13 || c == 27));
				vga_msg_box_destroy(&vgabox);
				if (c == 27) continue;
			}

			if (!drive_rw_test_nfo.can_do_lba &&
				(select == DRIVE_RW_MODE_LBA ||
				select == DRIVE_RW_MODE_LBAMULTIPLE ||
				select == DRIVE_RW_MODE_LBA48 ||
				select == DRIVE_RW_MODE_LBA48_MULTIPLE)) {

				vga_msg_box_create(&vgabox,
					"The IDE device doesn't seem to support LBA mode.\n"
					"Are you sure you want to choose this mode?\n"
					"\n"
					"Hit ENTER to proceed, ESC to cancel"
					,0,0);
				do {
					c = getch();
					if (c == 0) c = getch() << 8;
				} while (!(c == 13 || c == 27));
				vga_msg_box_destroy(&vgabox);
				if (c == 27) continue;
			}

			if (!drive_rw_test_nfo.can_do_lba48 &&
				(select == DRIVE_RW_MODE_LBA48 ||
				select == DRIVE_RW_MODE_LBA48_MULTIPLE)) {

				vga_msg_box_create(&vgabox,
					"The IDE device doesn't seem to support 48-bit LBA.\n"
					"Are you sure you want to choose this mode?\n"
					"\n"
					"Hit ENTER to proceed, ESC to cancel"
					,0,0);
				do {
					c = getch();
					if (c == 0) c = getch() << 8;
				} while (!(c == 13 || c == 27));
				vga_msg_box_destroy(&vgabox);
				if (c == 27) continue;
			}

			if (select >= 0)
				drive_rw_test_nfo.mode = select;

			break;
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = mbox.item_max;

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > mbox.item_max)
				select = -1;

			redraw = 1;
		}
	}
}

/*-----------------------------------------------------------------*/

void do_hdd_drive_read_verify_test(struct ide_controller *ide,unsigned char which,unsigned char continuous,struct drive_rw_test_info *nfo) {
	unsigned char user_esc = 0;
	struct ide_taskfile *tsk;
	unsigned long tlen_sect;
	unsigned int cleared=0;
	int c;

	if (nfo->read_sectors == 0) return;
	if (nfo->multiple_sectors == 0) return;

	idelib_controller_reset_irq_counter(ide); /* IRQ will fire after command completion */
	tsk = idelib_controller_get_taskfile(ide,-1/*selected drive*/);

again:	/* jump point: send execution back here for another sector */
	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;
	idelib_controller_ack_irq(ide); /* <- make sure to ack IRQ */

	tlen_sect = nfo->read_sectors;
	/* C/H/S continuous: limit reads to within track, don't cross. we can't assume the drive will do that. */
	if (continuous && (nfo->mode == DRIVE_RW_MODE_CHS || nfo->mode == DRIVE_RW_MODE_CHSMULTIPLE)) {
		if ((nfo->sector + tlen_sect) > nfo->num_sector)
			tlen_sect = (nfo->num_sector + 1 - nfo->sector);
	}

	if (nfo->mode == DRIVE_RW_MODE_LBA48 || nfo->mode == DRIVE_RW_MODE_LBA48_MULTIPLE) {
		tsk->sector_count = tlen_sect;
		tsk->lba0_3 = nfo->lba & 0xFF;
		tsk->lba1_4 = (nfo->lba >> 8) & 0xFF;
		tsk->lba2_5 = (nfo->lba >> 16) & 0xFF;
		tsk->lba0_3 |= ((nfo->lba >> 24) & 0xFF) << 8;
		tsk->lba1_4 |= ((nfo->lba >> 32) & 0xFF) << 8;
		tsk->lba2_5 |= ((nfo->lba >> 40) & 0xFF) << 8;
		tsk->head_select = (which << 4) | 0x40;

		tsk->command = 0x42; /* READ VERIFY EXT */
		if (idelib_controller_apply_taskfile(ide,0xFC/*base_io+2-7*/,IDELIB_TASKFILE_LBA48_UPDATE|IDELIB_TASKFILE_LBA48/*set LBA48*/) < 0)
			return;
	}
	else {
		if (tsk->sector_count > 256) return;
		tsk->sector_count = (tlen_sect&0xFF);
		if (nfo->mode == DRIVE_RW_MODE_CHS || nfo->mode == DRIVE_RW_MODE_CHSMULTIPLE) {
			tsk->chs_sector = nfo->sector;
			tsk->chs_cylinder_low = nfo->cylinder & 0xFF;
			tsk->chs_cylinder_high = (nfo->cylinder >> 8) & 0xFF;
			tsk->head_select = (nfo->head & 0xF) | (which << 4) | 0xA0;
		}
		else if (nfo->mode == DRIVE_RW_MODE_LBA || nfo->mode == DRIVE_RW_MODE_LBAMULTIPLE) {
			tsk->lba0_3 = nfo->lba & 0xFF;
			tsk->lba1_4 = (nfo->lba >> 8) & 0xFF;
			tsk->lba2_5 = (nfo->lba >> 16) & 0xFF;
			tsk->head_select = ((nfo->lba >> 24) & 0xF) | (which << 4) | 0xE0;
		}

		tsk->command = 0x40; /* READ VERIFY */
		if (idelib_controller_apply_taskfile(ide,0xFC/*base_io+2-7*/,IDELIB_TASKFILE_LBA48_UPDATE/*clear LBA48*/) < 0)
			return;
	}

	if (ide->flags.io_irq_enable) { /* NOW we wait for the IRQ */
		if (do_ide_controller_user_wait_irq(ide,1) < 0)
			return;
		idelib_controller_ack_irq(ide); /* <- or else it won't fire again */
	}

	if (do_ide_controller_user_wait_busy_controller(ide) != 0 || do_ide_controller_user_wait_drive_ready(ide) < 0)
		return;

	/* ---- draw contents on the screen ---- */
	vga_write_color(0x0E);
	if (!cleared) {
		vga_clear();
		vga_moveto(0,0);
		vga_write("Sector verification:\n");
		cleared = 1;
	}

	if (nfo->mode == DRIVE_RW_MODE_CHS || nfo->mode == DRIVE_RW_MODE_CHSMULTIPLE) {
		sprintf(tmp,"CHS %u/%u/%u",nfo->cylinder,nfo->head,nfo->sector); vga_write(tmp);
	}
	else {
		sprintf(tmp,"%llu-%llu",nfo->lba,nfo->lba+(unsigned long long)tlen_sect-1ULL); vga_write(tmp);
	}

	if (!idelib_controller_is_error(ide)) { /* OK. success. now read the data */
		vga_write_color(0x0A);
		vga_write(" PASSED\n");
	}
	else {
		vga_write_color(0x0C);
		vga_write(" FAILED\n");
	}


	if (continuous && !user_esc) {
		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;
		}
		else {
			c = 0;
		}
	}
	else {
		if ((c=wait_for_enter_or_escape()) == 27)
			return; /* allow user to exit early by hitting ESC */
	}

	if (c != 27 && !user_esc) {
		/* if the user hit ENTER, then read another sector and display that too */
		if (nfo->mode == DRIVE_RW_MODE_CHS || nfo->mode == DRIVE_RW_MODE_CHSMULTIPLE) {
			nfo->sector += (unsigned long long)tlen_sect;
			while (nfo->sector > nfo->num_sector) {
				nfo->sector -= nfo->num_sector;
				nfo->head++;
			}
			while (nfo->head >= nfo->num_head) {
				nfo->head -= nfo->num_head;
				nfo->cylinder++;
			}

			if (nfo->cylinder >= 16384) {
				nfo->cylinder = 16383;
				nfo->sector = nfo->num_sector;
				nfo->head = nfo->num_head - 1;
			}

			nfo->lba  = (unsigned long long)nfo->cylinder * (unsigned long long)nfo->num_sector * (unsigned long long)nfo->num_head;
			nfo->lba += (unsigned long long)nfo->head * (unsigned long long)nfo->num_sector;
			nfo->lba += (unsigned long long)nfo->sector - 1ULL;
		}
		else {
			nfo->lba += (unsigned long long)tlen_sect;

			nfo->sector = (int)(nfo->lba % (unsigned long long)nfo->num_sector) + 1;
			nfo->head = (int)((nfo->lba / (unsigned long long)nfo->num_sector) % (unsigned long long)nfo->num_head);
			if (nfo->lba >= (16384ULL * (unsigned long long)nfo->num_sector * (unsigned long long)nfo->num_head)) {
				nfo->cylinder = 16383;
				nfo->sector = nfo->num_sector;
				nfo->head = nfo->num_head - 1;
			}
			else {
				nfo->cylinder = (int)((nfo->lba / (unsigned long long)nfo->num_sector) / (unsigned long long)nfo->num_head);
			}
		}

		goto again;
	}
}

const char *drive_read_verify_test_menustrings[] = {
	"Show IDE register taskfile",		/* 0 */
	"*mode*",				/* 1 */ /* rewritten (CHS, LBA, CHS MULTI, etc) */
	drive_readwrite_test_geo,
	drive_readwrite_test_chs,
	drive_readwrite_test_numsec,
	"Read verify sectors",
	"Read verify sectors continuously"
};

void do_drive_read_verify_test(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_read_verify_test_menustrings);

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
			vga_write("        IDE r/w tests ");
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

			sprintf(tmp," lba=%u lba48=%u multiple=%u",
				drive_rw_test_nfo.can_do_lba?1:0,
				drive_rw_test_nfo.can_do_lba48?1:0,
				drive_rw_test_nfo.can_do_multiple?1:0);
			vga_write(tmp);

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

			drive_read_verify_test_menustrings[1] = drive_readwrite_test_modes[drive_rw_test_nfo.mode];

			sprintf(drive_readwrite_test_geo,"Geometry: C/H/S %u/%u/%u LBA %llu",
				drive_rw_test_nfo.num_cylinder,
				drive_rw_test_nfo.num_head,
				drive_rw_test_nfo.num_sector,
				drive_rw_test_nfo.max_lba);

			sprintf(drive_readwrite_test_chs,"Position: C/H/S %u/%u/%u LBA %llu",
				drive_rw_test_nfo.cylinder,
				drive_rw_test_nfo.head,
				drive_rw_test_nfo.sector,
				drive_rw_test_nfo.lba);

			sprintf(drive_readwrite_test_numsec,"Number of sectors per read: %u",
				drive_rw_test_nfo.read_sectors);

			sprintf(drive_readwrite_test_mult,"Multiple mode: %u sectors (max=%u)",
				drive_rw_test_nfo.multiple_sectors,
				drive_rw_test_nfo.max_multiple_sectors);

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
				case 1: /* Mode */
					do_drive_readwrite_test_choose_mode(ide,which,&drive_rw_test_nfo);
					redraw = backredraw = 1;
					break;
				case 2: /* Edit geometry/max LBA */
					do_drive_readwrite_edit_chslba(ide,which,&drive_rw_test_nfo,/*editgeo*/1);
					redraw = 1;
					break;
				case 3: /* Edit position */
					do_drive_readwrite_edit_chslba(ide,which,&drive_rw_test_nfo,/*editgeo*/0);
					redraw = 1;
					break;
				case 4: /* Number of sectors */
					c = prompt_sector_count();
					if (c >= 1 && c <= 256) {
						drive_rw_test_nfo.read_sectors = c;
						redraw = 1;
					}
					break;
				case 5: /*Read sectors*/
				case 6: /*Read sectors continuously*/
					do_hdd_drive_read_verify_test(ide,which,/*continuous=*/(select==6),&drive_rw_test_nfo);
					redraw = backredraw = 1;
					break;
			};
		}
		else if (c == 0x4800) {
			if (--select < -1)
				select = mbox.item_max;

			redraw = 1;
		}
		else if (c == 0x4B00) { /* left */
			switch (select) {
				case 1: /* Mode */
					do {
						if (drive_rw_test_nfo.mode == 0)
							drive_rw_test_nfo.mode = DRIVE_RW_MODE_MAX-1;
						else
							drive_rw_test_nfo.mode--;
					} while (!drive_rw_test_mode_supported(&drive_rw_test_nfo));
					break;
				case 4: /* Number of sectors */
					if (drive_rw_test_nfo.read_sectors > 1)
						drive_rw_test_nfo.read_sectors--;
					break;
			};

			redraw = 1;
		}
		else if (c == 0x4D00) { /* right */
			switch (select) {
				case 1: /* Mode */
					do {
						if (++drive_rw_test_nfo.mode >= DRIVE_RW_MODE_MAX)
							drive_rw_test_nfo.mode = 0;
					} while (!drive_rw_test_mode_supported(&drive_rw_test_nfo));
					break;
				case 4: /* Number of sectors */
					if (drive_rw_test_nfo.read_sectors < 256)
						drive_rw_test_nfo.read_sectors++;
					break;
			};

			redraw = 1;
		}
		else if (c == 0x5000) {
			if (++select > mbox.item_max)
				select = -1;

			redraw = 1;
		}
	}
}

/*-----------------------------------------------------------------*/

const char *drive_readwrite_tests_menustrings[] = {
	"Show IDE register taskfile",		/* 0 */
	"Reading tests >>",
	"Writing tests >>",
	"Read verify tests"
};

void do_drive_readwrite_tests(struct ide_controller *ide,unsigned char which) {
	struct menuboxbounds mbox;
	char backredraw=1;
	VGA_ALPHA_PTR vga;
	unsigned int x,y;
	int select=-1;
	char redraw=1;
	int c;

	/* get geometry and max sector count *NOW* then the user can tweak them later */
	memset(&drive_rw_test_nfo,0,sizeof(drive_rw_test_nfo));
	{
		uint16_t info[256];

		c = do_ide_identify((unsigned char*)info,sizeof(info),ide,which,0xEC/*ATA IDENTIFY DEVICE*/);
		if (c < 0) return;

		drive_rw_test_nfo.can_do_lba = (info[49] & 0x200) ? 1 : 0;
		drive_rw_test_nfo.can_do_multiple = ((info[47] & 0xFF) != 0) ? 1 : 0;
		drive_rw_test_nfo.can_do_lba48 = drive_rw_test_nfo.can_do_lba && ((info[83] & 0x400) ? 1 : 0);

		/* NTS: Never mind the thousands of OSes out there still using these fields, ATA-8 marks them "obsolete".
		 *      Thanks. You guys realize this is the same logic behind the infuriating number of APIs in Windows
		 *      that are "obsolete" yet everything relies on them?
		 *
		 *      This is why you keep OLDER copies of standards around, guys! */
		drive_rw_test_nfo.num_cylinder = info[54];	/* number of current logical cylinders */
		drive_rw_test_nfo.num_head = info[55];		/* number of current logical heads */
		drive_rw_test_nfo.num_sector = info[56];	/* number of current logical sectors */
		if (drive_rw_test_nfo.num_cylinder == 0 && drive_rw_test_nfo.num_head == 0 && drive_rw_test_nfo.num_sector == 0) {
			drive_rw_test_nfo.num_cylinder = info[1]; /* number of logical cylinders */
			drive_rw_test_nfo.num_head = info[3];	/* number of logical heads */
			drive_rw_test_nfo.num_sector = info[6]; /* number of logical sectors */
		}

		if (drive_rw_test_nfo.can_do_lba48)
			drive_rw_test_nfo.max_lba = ((uint64_t)info[103] << 48ULL) + ((uint64_t)info[102] << 32ULL) +
				((uint64_t)info[101] << 16ULL) + ((uint64_t)info[100]);
		if (drive_rw_test_nfo.max_lba == 0)
			drive_rw_test_nfo.max_lba = ((uint64_t)info[61] << 16ULL) + ((uint64_t)info[60]);
		if (drive_rw_test_nfo.max_lba == 0)
			drive_rw_test_nfo.max_lba = ((uint64_t)info[58] << 16ULL) + ((uint64_t)info[57]);

		drive_rw_test_nfo.sector = 1;
		drive_rw_test_nfo.read_sectors = 1;
		drive_rw_test_nfo.mode = DRIVE_RW_MODE_CHS;
		if (drive_rw_test_nfo.can_do_multiple && (info[59]&0x100))
			drive_rw_test_nfo.multiple_sectors = info[59]&0xFF;
		if (drive_rw_test_nfo.can_do_multiple && (info[47]&0xFF) != 0)
			drive_rw_test_nfo.max_multiple_sectors = info[47]&0xFF;
	}

	/* UI element vars */
	menuboxbounds_set_def_list(&mbox,/*ofsx=*/4,/*ofsy=*/7,/*cols=*/1);
	menuboxbounds_set_item_strings_arraylen(&mbox,drive_readwrite_tests_menustrings);

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
			vga_write("        IDE controller read/write tests ");
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
				case 1: /* Read tests */
					do_drive_read_test(ide,which);
					redraw = backredraw = 1;
					break;
				case 2: /* Write tests */
					redraw = backredraw = 1;
					break;
				case 3: /* Read verify tests */
					do_drive_read_verify_test(ide,which);
					redraw = backredraw = 1;
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

