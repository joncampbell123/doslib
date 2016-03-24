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
#include <hw/dos/doswin.h>

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

void vga_read_crtc_mode(struct vga_mode_params *p) {
	unsigned char c,c2;

	if (!(vga_state.vga_flags & VGA_IS_VGA))
		return;

	c = inp(0x3CC);	/* misc out reg */
	p->clock_select = (c >> 2) & 3;
	p->hsync_neg = (c >> 6) & 1;
	p->vsync_neg = (c >> 7) & 1;

	c = vga_read_sequencer(1);
	p->clock9 = (c & 1) ^ 1;
	p->clock_div2 = (c >> 3) & 1;
	p->shift4_enable = (c >> 4) & 1;
	p->shift_load_rate = (c >> 2) & 1;

	p->sync_enable = (vga_read_CRTC(0x17) >> 7) & 1;
	p->word_mode = ((vga_read_CRTC(0x17) >> 6) & 1) ^ 1;
	p->address_wrap_select = (vga_read_CRTC(0x17) >> 5) & 1;
	p->memaddr_div2 = (vga_read_CRTC(0x17) >> 3) & 1;
	p->scanline_div2 = (vga_read_CRTC(0x17) >> 2) & 1;
	p->map14 = ((vga_read_CRTC(0x17) >> 1) & 1) ^ 1;
	p->map13 = ((vga_read_CRTC(0x17) >> 0) & 1) ^ 1;

	p->dword_mode = (vga_read_CRTC(0x14) >> 6) & 1;
	p->horizontal_total = vga_read_CRTC(0) + 5;
	p->horizontal_display_end = vga_read_CRTC(1) + 1;
	p->horizontal_blank_start = vga_read_CRTC(2) + 1;
	p->horizontal_blank_end = ((vga_read_CRTC(3) & 0x1F) | ((vga_read_CRTC(5) >> 7) << 5) |
		((p->horizontal_blank_start - 1) & (~0x3F))) + 1;
	if (p->horizontal_blank_start >= p->horizontal_blank_end)
		p->horizontal_blank_end += 0x40;
	p->horizontal_start_retrace = vga_read_CRTC(4);
	p->horizontal_end_retrace = (vga_read_CRTC(5) & 0x1F) |
		(p->horizontal_start_retrace & (~0x1F));
	if ((p->horizontal_start_retrace&0x1F) >= (p->horizontal_end_retrace&0x1F))
		p->horizontal_end_retrace += 0x20;
	p->horizontal_start_delay_after_total = (vga_read_CRTC(3) >> 5) & 3;
	p->horizontal_start_delay_after_retrace = (vga_read_CRTC(5) >> 5) & 3;

	c = vga_read_CRTC(7); /* c = overflow reg */
	c2 = vga_read_CRTC(9);

	p->scan_double = (c2 >> 7) & 1;
	p->max_scanline = (c2 & 0x1F) + 1;
	p->offset = vga_read_CRTC(0x13);
	p->vertical_total = (vga_read_CRTC(6) | ((c & 1) << 8) | (((c >> 5) & 1) << 9)) + 2;
	p->vertical_start_retrace = (vga_read_CRTC(0x10) | (((c >> 2) & 1) << 8) | (((c >> 7) & 1) << 9));
	p->vertical_end_retrace = (vga_read_CRTC(0x11) & 0xF) |
		(p->vertical_start_retrace & (~0xF));
	if ((p->vertical_start_retrace&0xF) >= (p->vertical_end_retrace&0xF))
		p->vertical_end_retrace += 0x10;
	p->refresh_cycles_per_scanline = ((vga_read_CRTC(0x11) >> 6) & 1) ? 5 : 3;
	p->inc_mem_addr_only_every_4th = (vga_read_CRTC(0x14) >> 5) & 1;
	p->vertical_display_end = ((vga_read_CRTC(0x12) | (((c >> 1) & 1) << 8) | (((c >> 6) & 1) << 9))) + 1;
	p->vertical_blank_start = ((vga_read_CRTC(0x15) | (((c >> 3) & 1) << 8) | (((c2 >> 5) & 1) << 9))) + 1;
	p->vertical_blank_end = ((vga_read_CRTC(0x16) & 0x7F) | ((p->vertical_blank_start - 1) & (~0x7F))) + 1;
	if (p->vertical_blank_start >= p->vertical_blank_end)
		p->vertical_blank_end += 0x80;
}

