
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/vga/vgatty.h>
#include <hw/8254/8254.h>
#include <hw/dos/doswin.h>

#if defined(TARGET_WINDOWS)
# error WRONG
#endif

void bios_cls() {
	VGA_ALPHA_PTR ap;
	VGA_RAM_PTR rp;
	unsigned char m;

	m = int10_getmode();
	if ((rp=vga_graphics_ram) != NULL && !(m <= 3 || m == 7)) {
#if TARGET_MSDOS == 16
		unsigned int i,im;

		im = (FP_SEG(vga_graphics_ram_fence) - FP_SEG(vga_graphics_ram));
		if (im > 0xFFE) im = 0xFFE;
		im <<= 4;
		for (i=0;i < im;i++) vga_graphics_ram[i] = 0;
#else
		while (rp < vga_graphics_ram_fence) *rp++ = 0;
#endif
	}
	else if ((ap=vga_alpha_ram) != NULL) {
#if TARGET_MSDOS == 16
		unsigned int i,im;

		im = (FP_SEG(vga_alpha_ram_fence) - FP_SEG(vga_alpha_ram));
		if (im > 0x7FE) im = 0x7FE;
		im <<= 4 - 1; /* because ptr is type uint16_t */
		for (i=0;i < im;i++) vga_alpha_ram[i] = 0x0720;
#else
		while (ap < vga_alpha_ram_fence) *ap++ = 0x0720;
#endif
	}
	else {
		printf("WARNING: bios cls no ptr\n");
	}
}

/*===================================================================================*/
static struct v320x200x256_VGA_state {
	uint16_t		stride;			// bytes per scanline
	uint16_t		width,virt_width;	// video width vs virtual width
	uint16_t		height,virt_height;	// video height vs virtual height
	uint16_t		draw_offset;		// draw offset
	uint16_t		vis_offset;		// visual offset
	uint32_t		vram_size;		// size of VRAM
	VGA_RAM_PTR		draw_ptr;
} v320x200x256_VGA_state = {0};

void v320x200x256_VGA_update_from_CRTC_state() {
	struct vga_mode_params p;

	vga_read_crtc_mode(&p);
	v320x200x256_VGA_state.vram_size = 0x10000UL; /* assume 64KB. this mode support code doesn't assume the ability to do Mode X tricks */
	v320x200x256_VGA_state.stride = v320x200x256_VGA_state.virt_width = p.offset * 2 * 4; /* assume doubleword size */
	v320x200x256_VGA_state.width = p.horizontal_display_end * 4;
	v320x200x256_VGA_state.height = (p.vertical_display_end + p.max_scanline - 1) / p.max_scanline; /* <- NTS: Modern Intel chipsets however ignore the partial last scanline! */
	v320x200x256_VGA_state.virt_height = v320x200x256_VGA_state.vram_size / v320x200x256_VGA_state.stride;
	v320x200x256_VGA_state.vis_offset = vga_get_start_location() << 2; /* this will lose the upper 2 bits, by design. also does not pay attention to hpel */
	v320x200x256_VGA_state.draw_ptr = vga_graphics_ram + v320x200x256_VGA_state.draw_offset;
}

static inline uint8_t v320x200x256_VGA_getpixelnc(const unsigned int x,const unsigned int y) {
	return v320x200x256_VGA_state.draw_ptr[(y*v320x200x256_VGA_state.width)+x];
}

static inline uint8_t v320x200x256_VGA_getpixel(const unsigned int x,const unsigned int y) {
	if (x >= v320x200x256_VGA_state.width || y >= v320x200x256_VGA_state.height) return 0;
	return v320x200x256_VGA_getpixelnc(x,y);
}

static inline void v320x200x256_VGA_setpixelnc(const unsigned int x,const unsigned int y,const uint8_t v) {
	v320x200x256_VGA_state.draw_ptr[(y*v320x200x256_VGA_state.width)+x] = v;
}

static inline void v320x200x256_VGA_setpixel(const unsigned int x,const unsigned int y,const uint8_t v) {
	if (x >= v320x200x256_VGA_state.width || y >= v320x200x256_VGA_state.height) return;
	v320x200x256_VGA_setpixelnc(x,y,v);
}

void v320x200x256_VGA_menu_setpixel_box1() {
	unsigned int x,y,xm,ym;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	xm = v320x200x256_VGA_state.width/2;
	ym = v320x200x256_VGA_state.height/2;
	if (xm == 0 || ym == 0) return;

	for (y=1;y < (ym-1);y++)
		for (x=1;x < (xm-1);x++)
			v320x200x256_VGA_setpixel(x,y,x+y);

	for (x=0;x < xm;x++)
		v320x200x256_VGA_setpixel(x,0,1);
	for (x=0;x < xm;x++)
		v320x200x256_VGA_setpixel(x,ym-1,1);
	for (y=1;y < (ym-1);y++)
		v320x200x256_VGA_setpixel(0,y,1);
	for (y=1;y < (ym-1);y++)
		v320x200x256_VGA_setpixel(xm-1,y,1);

	while (1) {
		c = getch();
		if (c == 27 || c == 13)
			break;
	}
}

void v320x200x256_VGA_menu_setpixel_box1b() {
	unsigned int x,y,xm,ym;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	xm = v320x200x256_VGA_state.width/2;
	ym = v320x200x256_VGA_state.height/2;
	if (xm == 0 || ym == 0) return;

	for (y=1;y < (ym-1);y++)
		for (x=1+(y&3);x < (xm-1);x += 4)
			v320x200x256_VGA_setpixel(x,y,x+y);

	for (x=0;x < xm;x++)
		v320x200x256_VGA_setpixel(x,0,1);
	for (x=0;x < xm;x++)
		v320x200x256_VGA_setpixel(x,ym-1,1);
	for (y=1;y < (ym-1);y++)
		v320x200x256_VGA_setpixel(0,y,1);
	for (y=1;y < (ym-1);y++)
		v320x200x256_VGA_setpixel(xm-1,y,1);

	while (1) {
		c = getch();
		if (c == 27 || c == 13)
			break;
	}
}

void v320x200x256_VGA_menu_setpixel_box2overdraw() {
	unsigned int x,y,xm,ym;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	xm = v320x200x256_VGA_state.width/2;
	ym = v320x200x256_VGA_state.height/2;
	if (xm == 0 || ym == 0) return;

	for (y=1;y < (ym-1);y++)
		for (x=1;x < (xm-1);x++)
			v320x200x256_VGA_setpixel(x,y,x^y);

	for (y=1;y < (ym-1);y++)
		for (x=1;x < (xm-1);x++)
			v320x200x256_VGA_setpixel(x,y,x+y);

	for (x=0;x < xm;x++)
		v320x200x256_VGA_setpixel(x,0,1);
	for (x=0;x < xm;x++)
		v320x200x256_VGA_setpixel(x,ym-1,1);
	for (y=1;y < (ym-1);y++)
		v320x200x256_VGA_setpixel(0,y,1);
	for (y=1;y < (ym-1);y++)
		v320x200x256_VGA_setpixel(xm-1,y,1);

	while (1) {
		c = getch();
		if (c == 27 || c == 13)
			break;
	}
}

void v320x200x256_VGA_menu_setpixel_box3inv1() {
	unsigned int x,y,xm,ym;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	xm = v320x200x256_VGA_state.width/2;
	ym = v320x200x256_VGA_state.height/2;
	if (xm == 0 || ym == 0) return;

	for (y=0;y < ym;y++)
		for (x=0;x < xm;x++)
			v320x200x256_VGA_setpixel(x,y,v320x200x256_VGA_getpixel(x,y)^15);

	while (1) {
		c = getch();
		if (c == 27 || c == 13)
			break;
	}
}

void v320x200x256_VGA_menu_setpixel_box3rw() {
	unsigned int x,y,xm,ym;
	unsigned char p;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	printf("This code is reading, writing, then\n");
	printf("restoring each pixel. If the code\n");
	printf("works correctly, no rectangle should\n");
	printf("be visible.\n");

	xm = v320x200x256_VGA_state.width/2;
	ym = v320x200x256_VGA_state.height/2;
	if (xm == 0 || ym == 0) return;

	for (y=0;y < ym;y++) {
		for (x=0;x < xm;x++) {
			p = v320x200x256_VGA_getpixel(x,y);
			v320x200x256_VGA_setpixel(x,y,p^3);
			v320x200x256_VGA_setpixel(x,y,p^7);
			v320x200x256_VGA_setpixel(x,y,p^15);
			v320x200x256_VGA_setpixel(x,y,p^31);
			v320x200x256_VGA_setpixel(x,y,p);
		}
	}

	while (1) {
		c = getch();
		if (c == 27 || c == 13)
			break;
	}
}

void v320x200x256_VGA_menu_setpixel_box3rwdisp() {
	unsigned int x,y,xm,ym;
	unsigned char p;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	xm = v320x200x256_VGA_state.width/2;
	ym = v320x200x256_VGA_state.height/2;
	if (xm == 0 || ym == 0) return;

	for (y=(ym/2);y < ym;y++) {
		for (x=(xm/2);x < xm;x++) {
			v320x200x256_VGA_setpixel(x,y,x+y);
		}
	}

	for (y=0;y < ym;y++) {
		for (x=0;x < xm;x++) {
			p = v320x200x256_VGA_getpixel(x,y);
			v320x200x256_VGA_setpixel(x+xm,y,p);
		}
	}

	for (y=0;y < ym;y++) {
		for (x=0;x < xm;x++) {
			p = v320x200x256_VGA_getpixel(x,y);
			v320x200x256_VGA_setpixel(x,y+ym,p);
		}
	}

	for (y=0;y < ym;y++) {
		for (x=0;x < xm;x++) {
			p = v320x200x256_VGA_getpixel(x,y);
			v320x200x256_VGA_setpixel(x+xm,y+ym,p);
		}
	}

	while (1) {
		c = getch();
		if (c == 27 || c == 13)
			break;
	}
}

void v320x200x256_VGA_menu_setpixel_box3rwzoom() {
	unsigned int x,y,xm,ym;
	unsigned char p;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	xm = v320x200x256_VGA_state.width/2;
	ym = v320x200x256_VGA_state.height/2;
	if (xm == 0 || ym == 0) return;

	for (y=(ym/4);y < ym;y++) {
		for (x=(xm/4);x < xm;x++) {
			v320x200x256_VGA_setpixel(x,y,x+y);
		}
	}

	for (y=0;y < ym;y++) {
		for (x=0;x < xm;x++) {
			p = v320x200x256_VGA_getpixel(x>>1,y>>1);
			v320x200x256_VGA_setpixel(x+xm,y,p);
		}
	}

	for (y=0;y < ym;y++) {
		for (x=0;x < xm;x++) {
			p = v320x200x256_VGA_getpixel(x>>1,y>>1);
			v320x200x256_VGA_setpixel(x,y+ym,p);
		}
	}

	for (y=0;y < ym;y++) {
		for (x=0;x < xm;x++) {
			p = v320x200x256_VGA_getpixel(x>>1,y>>1);
			v320x200x256_VGA_setpixel(x+xm,y+ym,p);
		}
	}

	while (1) {
		c = getch();
		if (c == 27 || c == 13)
			break;
	}
}

void v320x200x256_VGA_menu_setpixel() {
	unsigned char redraw;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	redraw = 1;
	while (1) {
		if (redraw) {
			/* position the cursor to home */
			bios_cls();
			vga_moveto(0,0);
			vga_sync_bios_cursor();
			redraw = 0;

			printf("ESC  Top menu          A. Box1\n");
			printf("B. Box1b               C. Box2overdraw\n");
			printf("D. Box3inv1            E. Box3rw\n");
			printf("F. Box3rw displace     G. Box3rwzoom\n");
		}

		c = getch();
		if (c == 27)
			break;
		else if (c == 'a' || c == 'A') {
			v320x200x256_VGA_menu_setpixel_box1();
			redraw = 1;
		}
		else if (c == 'b' || c == 'B') {
			v320x200x256_VGA_menu_setpixel_box1b();
			redraw = 1;
		}
		else if (c == 'c' || c == 'C') {
			v320x200x256_VGA_menu_setpixel_box2overdraw();
			redraw = 1;
		}
		else if (c == 'd' || c == 'D') {
			v320x200x256_VGA_menu_setpixel_box3inv1();
			redraw = 1;
		}
		else if (c == 'e' || c == 'E') {
			v320x200x256_VGA_menu_setpixel_box3rw();
			redraw = 1;
		}
		else if (c == 'f' || c == 'F') {
			v320x200x256_VGA_menu_setpixel_box3rwdisp();
			redraw = 1;
		}
		else if (c == 'g' || c == 'G') {
			v320x200x256_VGA_menu_setpixel_box3rwzoom();
			redraw = 1;
		}
	}
}

void v320x200x256_VGA_menu() {
	unsigned char redraw;
	int c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	int10_setmode(0x13);
	update_state_from_vga();
	v320x200x256_VGA_update_from_CRTC_state();

	redraw = 1;
	while (1) {
		if (redraw) {
			/* position the cursor to home */
			bios_cls();
			vga_moveto(0,0);
			vga_sync_bios_cursor();
			redraw = 0;

			printf("stride=%u w=%u/%u h=%u/%u\nvram=%lu\n\n",
				v320x200x256_VGA_state.stride,
				v320x200x256_VGA_state.width,
				v320x200x256_VGA_state.virt_width,
				v320x200x256_VGA_state.height,
				v320x200x256_VGA_state.virt_height,
				(unsigned long)v320x200x256_VGA_state.vram_size);

			printf("ESC  Main menu         A. SetPixel\n");
		}

		c = getch();
		if (c == 27)
			break;
		else if (c == 'a' || c == 'A') {
			v320x200x256_VGA_menu_setpixel();
			redraw = 1;
		}
	}

	int10_setmode(3);
	update_state_from_vga();
}
/*===================================================================================*/

int main() {
	unsigned char redraw;
	int c;

	probe_dos();
	detect_windows();

	if (!probe_8254()) {
		printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
		return 1;
	}

	if (!probe_vga()) {
		printf("VGA probe failed\n");
		return 1;
	}

	int10_setmode(3);
	update_state_from_vga();

	redraw = 1;
	while (1) {

		if (redraw) {
			/* position the cursor to home */
			vga_moveto(0,0);
			vga_sync_bios_cursor();

			printf("ESC  Exit to DOS       A. 320x200x256 VGA\n");
		}

		c = getch();
		if (c == 27)
			break;
		else if (c == 'a' || c == 'A')
			v320x200x256_VGA_menu();
	}

	return 0;
}

