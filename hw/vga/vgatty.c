#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <i86.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/vga/vgatty.h>
#include <hw/vga/vgagui.h>

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

void vga_scroll_up(unsigned char lines) {
	VGA_ALPHA_PTR rd,wr;
	unsigned char row,c;

	if (lines == 0)
		return;
	else if (lines > vga_height)
		lines = vga_height;

	if (lines < vga_height) {
		unsigned char lcopy = vga_height - lines;
		wr = vga_alpha_ram;
		rd = vga_alpha_ram + (lines * vga_stride);
		for (row=0;row < lcopy;row++) {
			for (c=0;c < vga_stride;c++) {
				*wr++ = *rd++;
			}
		}
	}

	wr = vga_alpha_ram + ((vga_height - lines) * vga_stride);
	for (row=0;row < lines;row++) {
		for (c=0;c < vga_stride;c++)
			*wr++ = (vga_color << 8) | 0x20;
	}
}

void vga_cursor_down() {
	if (++vga_pos_y >= vga_height) {
		vga_pos_y = vga_height - 1;
		vga_scroll_up(1);
	}
}

void vga_writec(char c) {
	if (c == '\n') {
		vga_pos_x = 0;
		vga_cursor_down();
	}
	else if (c == '\t') {
		vga_pos_x = (vga_pos_x | 7) + 1;
		if (vga_pos_x >= vga_width) {
			vga_pos_x = 0;
			vga_cursor_down();
		}
	}
	else {
		if (vga_pos_x >= vga_width) {
			vga_pos_x = 0;
			vga_cursor_down();
		}

		vga_alpha_ram[(vga_pos_y * vga_stride) + vga_pos_x] = c | (vga_color << 8);
		vga_pos_x++;
	}
}

void vga_write(const char *msg) {
	while (*msg != 0) vga_writec(*msg++);
}

void vga_write_sync() { /* sync writing pos with BIOS cursor and hardware */
	if (vga_alpha_mode) {
		unsigned int ofs = (vga_pos_y * vga_stride) + vga_pos_x;
		vga_write_CRTC(0xE,ofs >> 8);
		vga_write_CRTC(0xF,ofs);
	}
}

void vga_clear() {
	VGA_ALPHA_PTR wr;
	unsigned char r,c;

	wr = vga_alpha_ram;
	for (r=0;r < vga_height;r++) {
		for (c=0;c < vga_stride;c++) {
			*wr++ = 0x0720;
		}
	}
}

