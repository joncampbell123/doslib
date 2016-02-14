
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
#include <hw/8254/8254.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

#include <hw/vga/gvg256.h>
#include <hw/vga/tvg256.h>

static void bios_cls() {
	VGA_RAM_PTR rp;

	if ((rp=vga_graphics_ram) != NULL) {
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
}

static void v320x200x256_VGA_menu_setpixel_box1() {
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

static void v320x200x256_VGA_menu_setpixel_box1b() {
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

static void v320x200x256_VGA_menu_setpixel_box2overdraw() {
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

static void v320x200x256_VGA_menu_setpixel_box3inv1() {
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

static void v320x200x256_VGA_menu_setpixel_box3rw() {
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

static void v320x200x256_VGA_menu_setpixel_box3rwdisp() {
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

static void v320x200x256_VGA_menu_setpixel_box3rwzoom() {
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

static void v320x200x256_VGA_menu_windowing() {
	int posx = (int)0x8000,posy = (int)0x8000;
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

			printf("w/W/h/H=dim x/X/y/Y/c=pos\n");
			printf("  pos=(");
			if (posx == (int)0x8000) printf("NA");
			else printf("%d",posx);
			printf(",");
			if (posy == (int)0x8000) printf("NA");
			else printf("%d",posy);
			printf(") ");

			printf("vis=(%d,%d) ",
				v320x200x256_VGA_state.width,
				v320x200x256_VGA_state.height);

			printf("\n");
		}

		c = getch();
		if (c == 27)
			break;
		else if (c == 'c') {
			posx = posy = (int)0x8000;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height);
			redraw = 1;
		}
		else if (c == 'x') {
			if (posx == (int)0x8000)
				posx = 0;
			else
				posx -= 4;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height);
			redraw = 1;
		}
		else if (c == 'X') {
			if (posx == (int)0x8000)
				posx = 0;
			else
				posx += 4;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height);
			redraw = 1;
		}
		else if (c == 'y') {
			if (posy == (int)0x8000)
				posy = 0;
			else
				posy--;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height);
			redraw = 1;
		}
		else if (c == 'Y') {
			if (posy == (int)0x8000)
				posy = 0;
			else
				posy++;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height);
			redraw = 1;
		}
		else if (c == 'w') {
			if (v320x200x256_VGA_state.width > 4) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width-4,v320x200x256_VGA_state.scan_height);
				redraw = 1;
			}
		}
		else if (c == 'W') {
			if (v320x200x256_VGA_state.width < 4096) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width+4,v320x200x256_VGA_state.scan_height);
				redraw = 1;
			}
		}
		else if (c == 'h') {
			if (v320x200x256_VGA_state.height > 0) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height-1);
				redraw = 1;
			}
		}
		else if (c == 'H') {
			if (v320x200x256_VGA_state.height < 4096) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height+1);
				redraw = 1;
			}
		}
	}
}

static void v320x200x256_VGA_menu_setpixel() {
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

	v320x200x256_VGA_init();
	v320x200x256_VGA_setmode(0);

	redraw = 1;
	while (1) {
		if (redraw) {
			/* position the cursor to home */
			bios_cls();
			vga_moveto(0,0);
			vga_sync_bios_cursor();
			redraw = 0;

			printf("stride=%u w=%u/%u h=%u/%u\nvram=%lu par=%u:%u (%.3f)\nhsync=%.3fKHz fps=%.3f\n\n",
				v320x200x256_VGA_state.stride,
				v320x200x256_VGA_state.width,
				v320x200x256_VGA_state.virt_width,
				v320x200x256_VGA_state.height,
				v320x200x256_VGA_state.virt_height,
				(unsigned long)v320x200x256_VGA_state.vram_size,
				v320x200x256_VGA_state.par_n,
				v320x200x256_VGA_state.par_d,
				(double)v320x200x256_VGA_state.par_n / v320x200x256_VGA_state.par_d,
				(double)v320x200x256_VGA_get_hsync_rate() / 1000, /* Hz -> KHz */
				(double)v320x200x256_VGA_get_refresh_rate());

			printf("ESC  Main menu         A. SetPixel\n");
			printf("B. Windowing\n");
		}

		c = getch();
		if (c == 27)
			break;
		else if (c == 'a' || c == 'A') {
			v320x200x256_VGA_menu_setpixel();
			redraw = 1;
		}
		else if (c == 'b' || c == 'B') {
			v320x200x256_VGA_menu_windowing();
			redraw = 1;
		}
	}

	int10_setmode(3);
	update_state_from_vga();
}

