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

/* WARNING: This code is deliberately designed to NOT fullfill your request if you have
 *          anything but VGA. EGA and CGA monitors are fixed-frequency and this kind of
 *          experimentation is not good for them. You can of course fry a VGA monitor
 *          that way too, but at least we can read back the "safe" values the BIOS
 *          programmed into the hardware */
void vga_write_crtc_mode(struct vga_mode_params *p,unsigned int flags) {
	unsigned char c,c2,syncreset=0;

	if (!(vga_state.vga_flags & VGA_IS_VGA))
		return;

	/* sync disable unless told not to */
	if (!(flags & VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC)) {
		c = vga_read_CRTC(0x17);
		vga_write_CRTC(0x17,c&0x7F);
	}

	c = inp(0x3CC); /* misc out reg */
	/* if changing the clock select bits then we need to do a reset of the sequencer */
	if (p->clock_select != ((c >> 2) & 3)) syncreset=1;
	/* proceed to change bits */
	c &= ~((3 << 2) | (1 << 6) | (1 << 7));
	c |= (p->clock_select&3) << 2;
	c |= p->hsync_neg << 6;
	c |= p->vsync_neg << 7;
	if (syncreset) vga_write_sequencer(0,0x01/*SR=0 AR=1 start synchronous reset*/);
	outp(0x3C2,c); /* misc out */
	if (syncreset) vga_write_sequencer(0,0x03/*SR=1 AR=1 restart sequencer*/);

	vga_write_sequencer(1,
		(p->shift_load_rate << 2) |
		(p->shift4_enable << 4) |
		((p->clock9 ^ 1) << 0) |
		(p->clock_div2 << 3));

	c = 0; /* use 'c' as overflow register */
	c2 = vga_read_CRTC(0x09); /* read max scan line */
	c2 &= ~(1 << 5); /* mask out start vertical blank bit 9 */
	vga_write_CRTC(0x11, /* NTS: we leave bit 7 (protect) == 0 so we can program regs 0-7 in this routine */
		(((p->refresh_cycles_per_scanline == 5) ? 1 : 0) << 6) |
		(p->vertical_end_retrace & 0xF));
	vga_write_CRTC(0x06,(p->vertical_total - 2));
		c |= (((p->vertical_total - 2) >> 8) & 1) << 0;
		c |= (((p->vertical_total - 2) >> 9) & 1) << 5;
	vga_write_CRTC(0x10,p->vertical_start_retrace);
		c |= ((p->vertical_start_retrace >> 8) & 1) << 2;
		c |= ((p->vertical_start_retrace >> 9) & 1) << 7;
	vga_write_CRTC(0x12,p->vertical_display_end - 1);
		c |= (((p->vertical_display_end - 1) >> 8) & 1) << 1;
		c |= (((p->vertical_display_end - 1) >> 9) & 1) << 6;
	vga_write_CRTC(0x15,p->vertical_blank_start - 1);
		c |= (((p->vertical_blank_start - 1) >> 8) & 1) << 3;
		c2|= (((p->vertical_blank_start - 1) >> 9) & 1) << 5;

	/* NTS: this field is 7 bits wide but "Some SVGA chipsets use all 8" as VGADOC says. */
	/*      writing it in this way resolves the partial/full screen blanking problems with Intel 855/915/945 chipsets */
	vga_write_CRTC(0x16,p->vertical_blank_end - 1);

	vga_write_CRTC(0x14, /* NTS we write "underline location == 0" */
		(p->dword_mode << 6) |
		(p->inc_mem_addr_only_every_4th << 5));
	vga_write_CRTC(0x07,c); /* overflow reg */

	c2 &= ~(0x9F);	/* mask out SD + Max scanline */
	c2 |= (p->scan_double << 7);
	c2 |= (p->max_scanline - 1) & 0x1F;
	vga_write_CRTC(0x09,c2);
	vga_write_CRTC(0x13,p->offset);
	vga_write_CRTC(0,(p->horizontal_total - 5));
	vga_write_CRTC(1,(p->horizontal_display_end - 1));
	vga_write_CRTC(2,p->horizontal_blank_start - 1);
	vga_write_CRTC(3,((p->horizontal_blank_end - 1) & 0x1F) | (p->horizontal_start_delay_after_total << 5) | 0x80);
	vga_write_CRTC(4,p->horizontal_start_retrace);
	vga_write_CRTC(5,((((p->horizontal_blank_end - 1) >> 5) & 1) << 7) | (p->horizontal_start_delay_after_retrace << 5) |
		(p->horizontal_end_retrace & 0x1F));

	/* finish by writing reg 0x17 which also enables sync */
	vga_write_CRTC(0x17,
		(p->sync_enable << 7) |
		(vga_read_CRTC(0x17) & 0x10) | /* NTS: one undocumented bit, perhaps best not to change it */
		((p->word_mode^1) << 6) |
		(p->address_wrap_select << 5) |
		(p->memaddr_div2 << 3) |
		(p->scanline_div2 << 2) |
		((p->map14 ^ 1) << 1) |
		((p->map13 ^ 1) << 0));

	/* reinforce write protect */
	c = vga_read_CRTC(0x11);
	vga_write_CRTC(0x11,c|0x80);
}

