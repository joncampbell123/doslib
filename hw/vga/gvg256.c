
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

#if defined(TARGET_PC98)
/*nothing*/
#else

#include <hw/vga/gvg256.h>

struct v320x200x256_VGA_state v320x200x256_VGA_state = {0};
struct vga_mode_params v320x200x256_VGA_crtc_state = {0};
struct vga_mode_params v320x200x256_VGA_crtc_state_init = {0}; // initial state after setting mode via BIOS
struct vga_mode_params v320x200x256_VGA_crtc_state_basemode = {0}; // initial state after setting mode

void v320x200x256_VGA_init() {
	/* the VGA display is 4:3. No question there. We allow the calling program to overwrite this later in case the DOS program
	 * is intended for 16:9 modern flat panel displays */
	v320x200x256_VGA_state.dar_n = 4;
	v320x200x256_VGA_state.dar_d = 3;
}

void v320x200x256_VGA_setmode(unsigned int flags) {
	if (!(flags & v320x200x256_VGA_setmode_FLAG_DONT_USE_INT10))
		int10_setmode(0x13);

	v320x200x256_VGA_update_from_CRTC_state();
	v320x200x256_VGA_crtc_state_basemode = v320x200x256_VGA_crtc_state_init = v320x200x256_VGA_crtc_state;

	/* Normal VGA/SVGA modes implement VGA "chained mode" by mapping 4 bytes every 16 bytes
	 * and read/write the same. DWORD and WORD bits are on. */
	vga_set_memory_map(1/*0xA0000-0xAFFFF*/);
	v320x200x256_VGA_crtc_state.word_mode = 1;
	v320x200x256_VGA_crtc_state.dword_mode = 1;

	vga_write_crtc_mode(&v320x200x256_VGA_crtc_state,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
	v320x200x256_VGA_update_from_CRTC_state();
}

double v320x200x256_VGA_get_hsync_rate() {
	double t = (double)vga_clock_rates[v320x200x256_VGA_crtc_state.clock_select];
	t /= (uint32_t)v320x200x256_VGA_crtc_state.horizontal_total *
		(v320x200x256_VGA_crtc_state.clock_div2 ? (uint32_t)2 : (uint32_t)1) *
		(v320x200x256_VGA_crtc_state.clock9 ? (uint32_t)9 : (uint32_t)8);
	return t;
}

double v320x200x256_VGA_get_refresh_rate() {
	double t = (double)vga_clock_rates[v320x200x256_VGA_crtc_state.clock_select];
	t /= (uint32_t)v320x200x256_VGA_crtc_state.horizontal_total *
		(uint32_t)v320x200x256_VGA_crtc_state.vertical_total *
		(v320x200x256_VGA_crtc_state.clock_div2 ? (uint32_t)2 : (uint32_t)1) *
		(v320x200x256_VGA_crtc_state.clock9 ? (uint32_t)9 : (uint32_t)8);
	return t;
}

void v320x200x256_VGA_update_par() {
	/* VGA monitors tend to vertically stretch the picture to fill the screen no matter what the scan line count, as long as the horizontal and
	 * vertical sync timing is within the monitor's supported ranges of course. */
	v320x200x256_VGA_state.par_n = v320x200x256_VGA_crtc_state.vertical_total * 2;
	v320x200x256_VGA_state.par_d = v320x200x256_VGA_crtc_state.horizontal_total * 4 * v320x200x256_VGA_state.scan_height_div;
}

void v320x200x256_VGA_update_from_CRTC_state() {
	update_state_from_vga();
	vga_read_crtc_mode(&v320x200x256_VGA_crtc_state);

	v320x200x256_VGA_state.vram_size = 0x10000UL; /* 64KB */
	v320x200x256_VGA_state.stride_shift = v320x200x256_VGA_crtc_state.dword_mode ? 2 : (v320x200x256_VGA_crtc_state.word_mode ? 1 : 0);

	v320x200x256_VGA_state.stride =
		v320x200x256_VGA_state.virt_width = vga_state.vga_stride << v320x200x256_VGA_state.stride_shift;
	v320x200x256_VGA_state.width = v320x200x256_VGA_crtc_state.horizontal_display_end * 4;
	v320x200x256_VGA_state.scan_height_div = v320x200x256_VGA_crtc_state.max_scanline;
	v320x200x256_VGA_state.scan_height = v320x200x256_VGA_crtc_state.vertical_display_end;
	v320x200x256_VGA_state.height = (v320x200x256_VGA_crtc_state.vertical_display_end +
		v320x200x256_VGA_crtc_state.max_scanline - 1) / v320x200x256_VGA_crtc_state.max_scanline; /* <- NTS: Modern Intel chipsets however ignore the partial last scanline! */
	v320x200x256_VGA_state.virt_height = v320x200x256_VGA_state.vram_size / v320x200x256_VGA_state.stride;
	v320x200x256_VGA_state.vis_offset = v320x200x256_VGA_crtc_state.offset << v320x200x256_VGA_state.stride_shift;
	v320x200x256_VGA_state.hpel = (vga_read_AC(0x13) >> 1) & 7;
	v320x200x256_VGA_update_draw_ptr();
	v320x200x256_VGA_update_vis_ptr();
	v320x200x256_VGA_update_par();
}

void v320x200x256_VGA_setvirtwidth(int w) {
	w = (w + (1 << v320x200x256_VGA_state.stride_shift)) >> (1 + v320x200x256_VGA_state.stride_shift);
	if (w < 1) w = 1;
	if (w > 255) w = 255;
	vga_write_CRTC(0x13,w);
	v320x200x256_VGA_update_from_CRTC_state();
}

int v320x200x256_VGA_setvideomode(int width,int height,double hsync,double vsync) {
	if (width < 8) width = 320;
	if (height < 8) height = height;
	if (width > (255*4)) width = (255*4);
	if (height > 1023) height = 1023;
	width = (width + 2) & (~3);

	v320x200x256_VGA_state.scan_height_div = 1;
	while ((height*v320x200x256_VGA_state.scan_height_div) < 350)
		v320x200x256_VGA_state.scan_height_div++;

	/* NTS: This code assumes whatever state the BIOS left the CRTC in is 320x200 70Hz */
	v320x200x256_VGA_crtc_state = v320x200x256_VGA_crtc_state_init;
	v320x200x256_VGA_crtc_state.max_scanline = v320x200x256_VGA_state.scan_height_div;
	v320x200x256_VGA_crtc_state.clock_select = 0;
	v320x200x256_VGA_crtc_state.scan_double = 0;
	v320x200x256_VGA_crtc_state.clock_div2 = 0;
	v320x200x256_VGA_crtc_state.clock9 = 0; /* 25MHz */
	if ((height*v320x200x256_VGA_state.scan_height_div) > 410) {
		/* program 480-line parameters (60Hz) */
		v320x200x256_VGA_crtc_state.vertical_total = 525;
		v320x200x256_VGA_crtc_state.vertical_blank_end = 517;
		v320x200x256_VGA_crtc_state.vertical_blank_start = 488;
		v320x200x256_VGA_crtc_state.vertical_start_retrace = 490;
		v320x200x256_VGA_crtc_state.vertical_end_retrace = 492;
		v320x200x256_VGA_crtc_state.vertical_display_end = 480;
	}
	if (width > 320) {
		/* bump up the clock to gain more horizontal resolution */
		v320x200x256_VGA_crtc_state.clock_select = 1; /* 28MHz */
		v320x200x256_VGA_crtc_state.horizontal_total = 112;
		v320x200x256_VGA_crtc_state.horizontal_blank_end = 111;
		v320x200x256_VGA_crtc_state.horizontal_blank_start = 91;
		v320x200x256_VGA_crtc_state.horizontal_start_retrace = 94;
		v320x200x256_VGA_crtc_state.horizontal_end_retrace = 108;
		v320x200x256_VGA_crtc_state.horizontal_display_end = 90;
		/* vertical adjust for clock difference */
		v320x200x256_VGA_crtc_state.vertical_blank_end += 2;
		v320x200x256_VGA_crtc_state.vertical_total += 2;
	}

	v320x200x256_VGA_crtc_state_basemode = v320x200x256_VGA_crtc_state;
	vga_write_crtc_mode(&v320x200x256_VGA_crtc_state,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
	v320x200x256_VGA_setwindow((int)0x8000,(int)0x8000,width,height*v320x200x256_VGA_crtc_state.max_scanline,width);
	return 1;
}

void v320x200x256_VGA_setwindow(int x,int y,int w,int h,int vwidth) {
	int minx,miny,maxx,maxy,maxw,maxh,vmy;

	x = (x + 2) & (~3);
	w = (w + 2) & (~3);
	if (w < 4) w = 4;
	if (w > ((255+5)*4)) w = (255+5)*4;
	if (h < 1) h = 1;
	if (h > (1023 + 2)) h = 1023 + 2;

	v320x200x256_VGA_update_from_CRTC_state();

	minx = (v320x200x256_VGA_crtc_state.horizontal_end_retrace + 1 - v320x200x256_VGA_crtc_state.horizontal_total) * 4;
	miny = v320x200x256_VGA_crtc_state.vertical_end_retrace + 1 - v320x200x256_VGA_crtc_state.vertical_total;
	maxx = (v320x200x256_VGA_crtc_state.horizontal_start_retrace - 1) * 4;
	maxy = v320x200x256_VGA_crtc_state.vertical_start_retrace - 1;
	maxw = (v320x200x256_VGA_crtc_state.horizontal_total - (v320x200x256_VGA_crtc_state.horizontal_end_retrace + 2 - v320x200x256_VGA_crtc_state.horizontal_start_retrace)) * 4;
	maxh = v320x200x256_VGA_crtc_state.vertical_total - (v320x200x256_VGA_crtc_state.vertical_end_retrace + 2 - v320x200x256_VGA_crtc_state.vertical_start_retrace);

	if (vwidth <= 0) vwidth = v320x200x256_VGA_state.virt_width;
	{
		unsigned int d = 2 << v320x200x256_VGA_state.stride_shift;
		unsigned int i = (vwidth + (d>>1)) / d;

		if (i == 0) i = 1;
		else if (i > 255) i = 255;
		vga_state.vga_stride = vga_state.vga_draw_stride = vga_state.vga_draw_stride_limit = i * 2;
		v320x200x256_VGA_crtc_state.offset = i;
		v320x200x256_VGA_state.stride = v320x200x256_VGA_state.virt_width = vga_state.vga_stride << v320x200x256_VGA_state.stride_shift;
	}

	if (maxw > v320x200x256_VGA_state.stride) maxw = v320x200x256_VGA_state.stride;

	vmy = (int)(((unsigned long)v320x200x256_VGA_state.vram_size / (unsigned long)v320x200x256_VGA_state.stride) * (unsigned long)v320x200x256_VGA_state.scan_height_div);
	if (maxh > vmy) maxh = vmy;

	if (x == (int)0x8000)
		x = ((((int)v320x200x256_VGA_crtc_state.horizontal_display_end * 4) - w) / 2) & (~3);
	if (y == (int)0x8000)
		y = ((int)v320x200x256_VGA_crtc_state.vertical_display_end - h) / 2;

	if (x < minx) x = minx;
	if (y < miny) y = miny;
	if (x > maxx) x = maxx;
	if (y > maxy) y = maxy;
	if ((x+w) > maxx) w = maxx - x;
	if ((y+h) > maxy) h = maxy - y;
	if (w > maxw) w = maxw;
	if (h > maxh) h = maxh;
	if (w < 4) w = 4;
	if (h < 1) h = 1;

	v320x200x256_VGA_crtc_state.horizontal_start_retrace =
		v320x200x256_VGA_crtc_state_basemode.horizontal_start_retrace - (x / 4);
	v320x200x256_VGA_crtc_state.horizontal_blank_start =
		v320x200x256_VGA_crtc_state_basemode.horizontal_blank_start - (x / 4);
	v320x200x256_VGA_crtc_state.horizontal_end_retrace =
		v320x200x256_VGA_crtc_state_basemode.horizontal_end_retrace - (x / 4);
	v320x200x256_VGA_crtc_state.vertical_start_retrace =
		v320x200x256_VGA_crtc_state_basemode.vertical_start_retrace - y;
	v320x200x256_VGA_crtc_state.vertical_blank_start =
		v320x200x256_VGA_crtc_state_basemode.vertical_blank_start - y;
	v320x200x256_VGA_crtc_state.vertical_end_retrace =
		v320x200x256_VGA_crtc_state_basemode.vertical_end_retrace - y;
	v320x200x256_VGA_crtc_state.horizontal_display_end =
		w / 4;
	v320x200x256_VGA_crtc_state.vertical_display_end =
		h;

	if (v320x200x256_VGA_crtc_state.horizontal_blank_end < (v320x200x256_VGA_crtc_state.horizontal_end_retrace+1))
		v320x200x256_VGA_crtc_state.horizontal_blank_end = (v320x200x256_VGA_crtc_state.horizontal_end_retrace+1);
	if (v320x200x256_VGA_crtc_state.vertical_blank_end < (v320x200x256_VGA_crtc_state.vertical_end_retrace+1))
		v320x200x256_VGA_crtc_state.vertical_blank_end = (v320x200x256_VGA_crtc_state.vertical_end_retrace+1);

	vga_write_crtc_mode(&v320x200x256_VGA_crtc_state,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
	v320x200x256_VGA_update_from_CRTC_state();
}

#endif

