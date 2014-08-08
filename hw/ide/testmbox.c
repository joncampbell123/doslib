
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
#include "test.h"

void menuboxbound_redraw(struct menuboxbounds *mbox,int select) {
	int i,x,y;

	for (i=0;i <= mbox->item_max;i++) {
		int col = i / mbox->rows;
		x = mbox->ofsx + (col * mbox->col_width);
		y = mbox->ofsy + (i % mbox->rows);

		vga_moveto(x,y);
		vga_write_color((select == i) ? 0x70 : 0x0F);
		vga_write(mbox->item_string[i]);
		while (vga_pos_x < (x+mbox->col_width) && vga_pos_x != 0) vga_writec(' ');
	}
}

void menuboxbounds_set_def_list(struct menuboxbounds *mbox,unsigned int ofsx,unsigned int ofsy,unsigned int cols) {
	mbox->cols = 1;
	mbox->ofsx = 4;
	mbox->ofsy = 7;
	mbox->rows = vga_height - (mbox->ofsy + 1);
	mbox->width = vga_width - (mbox->ofsx * 2);
	mbox->col_width = mbox->width / mbox->cols;
}

void menuboxbounds_set_item_strings_array(struct menuboxbounds *mbox,const char **str,unsigned int count) {
	mbox->item_string = str;
	mbox->item_max = count - 1;
}

