
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

/* this one's for you, Sparky4. Non-mode-X PCX drawing routine. Not fast, but shows you how PCX compression works. --J.C. */
#pragma pack(push,1)
typedef struct pcx_header {
	uint8_t			manufacturer;		// +0x00  always 0x0A
	uint8_t			version;		// +0x01  0, 2, 3, or 5
	uint8_t			encoding;		// +0x02  always 0x01 for RLE
	uint8_t			bitsPerPlane;		// +0x03  bits per pixel in each color plane (1, 2, 4, 8, 24)
	uint16_t		Xmin,Ymin,Xmax,Ymax;	// +0x04  window (image dimensions). Pixel count in each dimension is Xmin <= x <= Xmax, Ymin <= y <= Ymax i.e. INCLUSIVE
	uint16_t		VertDPI,HorzDPI;	// +0x0C  vertical/horizontal resolution in DPI
	uint8_t			palette[48];		// +0x10  16-color or less color palette
	uint8_t			reserved;		// +0x40  reserved, set to zero
	uint8_t			colorPlanes;		// +0x41  number of color planes
	uint16_t		bytesPerPlaneLine;	// +0x42  number of bytes to read for a single plane's scanline (uncompressed, apparently)
	uint16_t		palType;		// +0x44  palette type (1 = color)
	uint16_t		hScrSize,vScrSize;	// +0x46  scrolling?
	uint8_t			pad[54];		// +0x4A  padding
};							// =0x80
#pragma pack(pop)

static void v320x200x256_VGA_menu_setpixel_drawpcx(unsigned int sx,unsigned int sy,const char *path) {
	struct pcx_header *pcx;
	unsigned int buffersz;
	char *buffer,*fence,*s;
	unsigned char b,count;
	unsigned int x,y,rx;
	unsigned int w,h;
	int fd,c;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	/* load PCX into memory */
	fd = open(path,O_RDONLY|O_BINARY);
	if (fd < 0) {
		printf("Can't open '%s'\n",path);
		return;
	}

	{
		unsigned long fsz = lseek(fd,0,SEEK_END);
		if (fsz <= 128UL/* too small to be PCX */ || fsz > 32000UL/*too large for this demo*/) {
			close(fd);
			return;
		}

		buffersz = (unsigned int)fsz; /* WARNING: unsigned long (32-bit) truncation to unsigned int (16-bit) in real-mode MS-DOS */
		buffer = malloc(buffersz);
		if (buffer == NULL) {
			close(fd);
			return;
		}
		fence = buffer + buffersz;
		lseek(fd,0,SEEK_SET);
		read(fd,buffer,buffersz);
	}
	close(fd);

	/* PCX is loaded. process it from memory buffer. */
	pcx = (struct pcx_header*)buffer; /* PCX header */
	w = pcx->Xmax + 1 - pcx->Xmin;
	h = pcx->Ymax + 1 - pcx->Ymin;
	if (w == 0 || h == 0 || w > 320 || h > 240 || pcx->bitsPerPlane != 8 || pcx->colorPlanes != 1) {
		printf("Don't like the PCX: w=%u h=%u bpp=%u colorplanes=%u\n",
			w,h,pcx->bitsPerPlane,pcx->colorPlanes);
		free(buffer);
		return;
	}

	/* VGA palette is at the end */
	if (buffersz >= (128+769)) {
		s = buffer + buffersz - 769;
		if (*s == 0x0C) {
			s++;
			vga_palette_lseek(0);
			for (c=0;c < 256;c++) {
				vga_palette_write(s[0] >> 2,s[1] >> 2,s[2] >> 2);
				s += 3;
			}

			fence -= 769; /* don't decode VGA palette as RLE! */
		}
	}

	/* DRAW! */
	rx=0;
	x=sx;y=sy;
	s = buffer+128; /* start after PCX header */
	do {
		b = *s++;
		if ((b&0xC0) == 0xC0) {
			count = b&0x3F;
			if (s >= fence) break;
			b = *s++;
			while (count > 0) {
				v320x200x256_VGA_setpixel(x++,y,b);
				if ((++rx) >= pcx->bytesPerPlaneLine) {
					x=sx;
					y++;
					rx=0;
				}
				count--;
			}
		}
		else {
			v320x200x256_VGA_setpixel(x++,y,b);
			if ((++rx) >= pcx->bytesPerPlaneLine) {
				x=sx;
				y++;
				rx=0;
			}
		}
	} while (s < fence);

	/* we're done. free the buffer */
	free(buffer);
	buffer = fence = s = NULL;
}

#pragma pack(push,1)
struct vrl_header {
	uint8_t			vrl_sig[4];		// +0x00  "VRL1"
	uint8_t			fmt_sig[4];		// +0x04  "VGAX"
	uint16_t		height;			// +0x08  Sprite height
	uint16_t		width;			// +0x0A  Sprite width
	int16_t			hotspot_x;		// +0x0C  Hotspot offset (X) for programmer's reference
	int16_t			hotspot_y;		// +0x0E  Hotspot offset (Y) for programmer's reference
};							// =0x10
#pragma pack(pop)

static void v320x200x256_VGA_menu_setpixel_drawvrl(unsigned int sx,unsigned int sy,const char *path) {
	struct vrl_header *vrl;
	unsigned int buffersz;
	char *buffer,*fence,*s;
	unsigned char b,run,skip;
	unsigned int x,y,rx;
	unsigned int w;
	int fd;

	if ((vga_flags & VGA_IS_VGA) == 0)
		return;

	/* load PCX into memory */
	fd = open(path,O_RDONLY|O_BINARY);
	if (fd < 0) {
		printf("Can't open '%s'\n",path);
		return;
	}

	{
		unsigned long fsz = lseek(fd,0,SEEK_END);
		if (fsz <= 20UL || fsz > 64000UL) {
			close(fd);
			return;
		}

		buffersz = (unsigned int)fsz; /* WARNING: unsigned long (32-bit) truncation to unsigned int (16-bit) in real-mode MS-DOS */
		buffer = malloc(buffersz);
		if (buffer == NULL) {
			close(fd);
			return;
		}
		fence = buffer + buffersz;
		lseek(fd,0,SEEK_SET);
		read(fd,buffer,buffersz);
	}
	close(fd);

	/* VRL is loaded. process it from memory buffer. */
	vrl = (struct vrl_header*)buffer; /* VRL header */
	if (memcmp(vrl->vrl_sig,"VRL1",4) || memcmp(vrl->fmt_sig,"VGAX",4)) {
		fprintf(stderr,"Dont like the VRL\n");
		free(buffer);
		return;
	}

	/* DRAW! */
	rx=0;x=sx;y=sy;
	s = buffer+sizeof(*vrl); /* start after VRL header */
	w = vrl->width;
	while ((s+2) <= fence && rx < vrl->width) {
		y=sy;
		while ((s+2) <= fence) {
			run = *s++;
			if (run == 0xFF) break;
			skip = *s++;
			y += skip;
			if (run & 0x80) {
				if (s >= fence) break;
				b = *s++;

				while (run > 0x80) {
					v320x200x256_VGA_setpixel(x,y++,b);
					run--;
				}
			}
			else {
				while (s < fence && run > 0) {
					v320x200x256_VGA_setpixel(x,y++,*s++);
					run--;
				}
			}
		}

		rx++;
		x++;
	}

	/* we're done. free the buffer */
	free(buffer);
	buffer = fence = s = NULL;
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
			printf("A=320x200 B=320x240 C=352x200 D=352x240\n");
			printf("E=320x400 F=320x480 G=352x400 H=352x480\n");
			printf("I=360x400 J=360x480\n");
		}

		c = getch();
		if (c == 27)
			break;
		else if (c == 'A') {
			v320x200x256_VGA_setvideomode(320,200,-1,-1);
			redraw = 1;
		}
		else if (c == 'B') {
			v320x200x256_VGA_setvideomode(320,240,-1,-1);
			redraw = 1;
		}
		else if (c == 'C') {
			v320x200x256_VGA_setvideomode(352,200,-1,-1);
			redraw = 1;
		}
		else if (c == 'D') {
			v320x200x256_VGA_setvideomode(352,240,-1,-1);
			redraw = 1;
		}
		else if (c == 'E') {
			v320x200x256_VGA_setvideomode(320,400,-1,-1);
			redraw = 1;
		}
		else if (c == 'F') {
			v320x200x256_VGA_setvideomode(320,480,-1,-1);
			redraw = 1;
		}
		else if (c == 'G') {
			v320x200x256_VGA_setvideomode(352,400,-1,-1);
			redraw = 1;
		}
		else if (c == 'H') {
			v320x200x256_VGA_setvideomode(352,480,-1,-1);
			redraw = 1;
		}
		else if (c == 'I') {
			v320x200x256_VGA_setvideomode(360,400,-1,-1);
			redraw = 1;
		}
		else if (c == 'J') {
			v320x200x256_VGA_setvideomode(360,480,-1,-1);
			redraw = 1;
		}
		else if (c == 'v') {
			if (v320x200x256_VGA_state.stride > (2 << v320x200x256_VGA_state.stride_shift)) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,
					v320x200x256_VGA_state.scan_height,v320x200x256_VGA_state.stride - (2 << v320x200x256_VGA_state.stride_shift));
				redraw = 1;
			}
		}
		else if (c == 'V') {
			if (v320x200x256_VGA_state.stride < ((2*255) << v320x200x256_VGA_state.stride_shift)) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,
					v320x200x256_VGA_state.scan_height,v320x200x256_VGA_state.stride + (2 << v320x200x256_VGA_state.stride_shift));
				redraw = 1;
			}
		}
		else if (c == 'c') {
			posx = posy = (int)0x8000;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height,-1);
			redraw = 1;
		}
		else if (c == 'x') {
			if (posx == (int)0x8000)
				posx = 0;
			else
				posx -= 4;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height,-1);
			redraw = 1;
		}
		else if (c == 'X') {
			if (posx == (int)0x8000)
				posx = 0;
			else
				posx += 4;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height,-1);
			redraw = 1;
		}
		else if (c == 'y') {
			if (posy == (int)0x8000)
				posy = 0;
			else
				posy--;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height,-1);
			redraw = 1;
		}
		else if (c == 'Y') {
			if (posy == (int)0x8000)
				posy = 0;
			else
				posy++;
			v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height,-1);
			redraw = 1;
		}
		else if (c == 'w') {
			if (v320x200x256_VGA_state.width > 4) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width-4,v320x200x256_VGA_state.scan_height,-1);
				redraw = 1;
			}
		}
		else if (c == 'W') {
			if (v320x200x256_VGA_state.width < 4096) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width+4,v320x200x256_VGA_state.scan_height,-1);
				redraw = 1;
			}
		}
		else if (c == 'h') {
			if (v320x200x256_VGA_state.height > 0) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height-1,-1);
				redraw = 1;
			}
		}
		else if (c == 'H') {
			if (v320x200x256_VGA_state.height < 4096) {
				v320x200x256_VGA_setwindow(posx,posy,v320x200x256_VGA_state.width,v320x200x256_VGA_state.scan_height+1,-1);
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
			printf("H. PCX draw            I. VRL draw\n");
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
		else if (c == 'h' || c == 'H') {
			v320x200x256_VGA_menu_setpixel_drawpcx(0,0,"megaman.pcx");
			v320x200x256_VGA_menu_setpixel_drawpcx(0,100,"megaman.pcx");

			while (1) {
				c = getch();
				if (c == 27 || c == 13)
					break;
			}

			/* Test PCX shamelessly copied from Sparky4's project */
			v320x200x256_VGA_menu_setpixel_drawpcx(0,0,"46113319.pcx");

			while (1) {
				c = getch();
				if (c == 27 || c == 13)
					break;
			}

			/* Another test PCX shamelessly copied from Sparky4's project */
			v320x200x256_VGA_menu_setpixel_drawpcx(0,0,"chikyuu.pcx");

			while (1) {
				c = getch();
				if (c == 27 || c == 13)
					break;
			}

			/* Yet another test PCX shamelessly copied from Sparky4's project */
			v320x200x256_VGA_menu_setpixel_drawpcx(0,0,"ed2.pcx");

			while (1) {
				c = getch();
				if (c == 27 || c == 13)
					break;
			}

			redraw = 1;
		}
		else if (c == 'i' || c == 'I') {
			unsigned int x,y;

			v320x200x256_VGA_menu_setpixel_drawpcx(0,0,"megaman.pcx");
			for (y=0;y < 200;y++) {
				for (x=0;x < 320;x++) {
					v320x200x256_VGA_setpixel(x,y,x^y);
				}
			}
			v320x200x256_VGA_menu_setpixel_drawvrl(0,0,"megaman.vrl");
			v320x200x256_VGA_menu_setpixel_drawvrl(0,100,"megaman.vrl");

			while (1) {
				c = getch();
				if (c == 27 || c == 13)
					break;
			}

			/* Test PCX shamelessly copied from Sparky4's project */
			v320x200x256_VGA_menu_setpixel_drawpcx(0,0,"46113319.pcx");
			for (y=0;y < 200;y++) {
				for (x=0;x < 320;x++) {
					v320x200x256_VGA_setpixel(x,y,x^y);
				}
			}
			v320x200x256_VGA_menu_setpixel_drawvrl(0,0,"46113319.vrl");

			while (1) {
				c = getch();
				if (c == 27 || c == 13)
					break;
			}

			/* Another test PCX shamelessly copied from Sparky4's project */
			v320x200x256_VGA_menu_setpixel_drawpcx(0,0,"chikyuu.pcx");
			for (y=0;y < 200;y++) {
				for (x=0;x < 320;x++) {
					v320x200x256_VGA_setpixel(x,y,x^y);
				}
			}
			v320x200x256_VGA_menu_setpixel_drawvrl(0,0,"chikyuu.vrl");

			while (1) {
				c = getch();
				if (c == 27 || c == 13)
					break;
			}

			/* Yet another test PCX shamelessly copied from Sparky4's project */
			v320x200x256_VGA_menu_setpixel_drawpcx(0,0,"ed2.pcx");
			for (y=0;y < 200;y++) {
				for (x=0;x < 320;x++) {
					v320x200x256_VGA_setpixel(x,y,x^y);
				}
			}
			v320x200x256_VGA_menu_setpixel_drawvrl(0,0,"ed2.vrl");

			while (1) {
				c = getch();
				if (c == 27 || c == 13)
					break;
			}

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

