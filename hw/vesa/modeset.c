/* TODO: This code needs to reject what appears to be a junk mode returned by Microsoft Virtual PC 2007
 *       (mode 0x113) which reports itself as 32770 x 36866 CGA graphics mode 16-plane 1 bit/pixel and 8 banks (Ha!)
 *       on the plus side the VESA BIOS doesn't let me set that mode. */

/* bug fixes:
      2011/10/21: Updated CPU detection library not to use 8086, 286, and
                  v8086 detection routines when compiled as 32-bit protected
		  mode code, since obviously if 32-bit code is executing the
		  CPU is at least a 386. This resolves MODESET.EXE causing
		  a system reboot when EMM386.EXE is resident, as the crash
		  was traced to the cpu_detect().
		  
      2011/10/21: Added "single step" mode so crashes like the one described
                  above can be traced to individual functions easier.

      2011/10/21: Added /nd switch to allow debugging whether writing to
                  VRAM is causing the crash. The switch causes MODESET to
		  set the video mode, but not draw anything.

      2011/10/21: Added /nwf switch which allows you to forcibly disable
                  using the bank switch window function indicated by the BIOS,
		  and to use instead INT 10H for bank switching. On some BIOSes,
		  the 32-bit version seems unable to call it correctly and
		  the drawn video ends up bunched up at the top of the screen.
		  Running MODESET with /nwf seems to resolve that issue.
*/

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/vesa/vesa.h>
#include <hw/flatreal/flatreal.h>
#include <hw/dos/doswin.h>

static int sstep = 0; /* single step */

static void help() {
	printf("modeset [options] <MODE>\n");
	printf("    /h /help              This help\n");
	printf("    /l                    Use linear framebuffer\n");
	printf("    /b                    Use banked mode\n");
	printf("    /nwf                  Don't use direct 16-bit window function\n");
	printf("    /fwf                  Force using 16-bit window function\n");
	printf("    /6                    Set DAC to 6-bit wide (16/256-color modes only)\n");
	printf("    /8                    Set DAC to 8-bit wide (16/256-color modes only)\n");
	printf("    /s                    Single-step mode (hit ENTER)\n");
}

static void vbe_mode_test_pattern_svga_planar(struct vbe_mode_decision *md,struct vbe_mode_info *mi) {
	unsigned char *pal = malloc(16*4);
	unsigned long ofs;
	unsigned int x,y;

	/* the planar SVGA modes require VGA-style programming. init the standard VGA library */
	probe_vga();

	vga_write_sequencer(VGA_SC_MAP_MASK,	0xF);	/* map mask register = enable all planes */
	vga_write_GC(VGA_GC_ENABLE_SET_RESET,	0x00);	/* enable set/reset = no on all planes */
	vga_write_GC(VGA_GC_SET_RESET,		0x00);	/* set/reset register = all zero */
	vga_write_GC(VGA_GC_BIT_MASK,		0xFF);	/* all bits modified */

	vga_write_GC(VGA_GC_DATA_ROTATE,	0 | VGA_GC_DATA_ROTATE_OP_NONE); /* rotate=0 op=unmodified */
	vga_write_GC(VGA_GC_MODE,		0x02);	/* 256=0 CGAstyle=0 odd/even=0 readmode=0 writemode=2 (copy CPU bits to each plane) */

	ofs = 0;
	for (y=0;y < mi->y_resolution;y++) {
		ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
		for (x=0;x < mi->x_resolution;x += 8) {
			vesa_writeb(ofs++,(y+x)>>3);
		}
	}

	while (getch() != 13);

	for (x=0;x < 16;x++) {
		pal[x*4+0] = x*4;
		pal[x*4+1] = x*4;
		pal[x*4+2] = x*4;
	}

	/* try to set DAC width */
	if (md->dac8 && (x=vbe_set_dac_width(8)) != 8) {
		vbe_reset_video_to_text();
		printf("Cannot set DAC width to 8 bits (ret=%u)\n",x);
		return;
	}
	else if (!md->dac8) {
		vbe_set_dac_width(6); /* don't care if it fails */
	}

	if (md->dac8) {
		for (x=0;x < 16*4;x++)
			pal[x] <<= 2;
	}

	/* modify the Attribute Controller since it's probably mapping indexes 8-15 to palette colors 0x38-0x3F */
	/* only then will our 16-color palette update show correctly */
	for (x=0;x < 16;x++) vga_write_AC(x,x);
	vga_AC_reenable_screen();

	if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
		vesa_set_palette_data(0,16,pal);
	}
	else {
		outp(0x3C8,0);
		for (y=0;y < 16;y++) {
			outp(0x3C9,pal[y*4+0]);
			outp(0x3C9,pal[y*4+1]);
			outp(0x3C9,pal[y*4+2]);
		}
	}

	while (getch() != 13);

	for (x=0;x < 16;x++) {
		pal[x*4+0] = x*4;
		pal[x*4+1] = 0;
		pal[x*4+2] = 0;
	}

	if (md->dac8) {
		for (x=0;x < 16*4;x++)
			pal[x] <<= 2;
	}

	if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
		vesa_set_palette_data(0,16,pal);
	}
	else {
		outp(0x3C8,0);
		for (y=0;y < 16;y++) {
			outp(0x3C9,pal[y*4+0]);
			outp(0x3C9,pal[y*4+1]);
			outp(0x3C9,pal[y*4+2]);
		}
	}

	while (getch() != 13);

	for (x=0;x < 16;x++) {
		pal[x*4+0] = 0;
		pal[x*4+1] = x*4;
		pal[x*4+2] = 0;
	}

	if (md->dac8) {
		for (x=0;x < 16*4;x++)
			pal[x] <<= 2;
	}

	if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
		vesa_set_palette_data(0,16,pal);
	}
	else {
		outp(0x3C8,0);
		for (y=0;y < 16;y++) {
			outp(0x3C9,pal[y*4+0]);
			outp(0x3C9,pal[y*4+1]);
			outp(0x3C9,pal[y*4+2]);
		}
	}

	while (getch() != 13);

	for (x=0;x < 16;x++) {
		pal[x*4+0] = 0;
		pal[x*4+1] = 0;
		pal[x*4+2] = x*4;
	}

	if (md->dac8) {
		for (x=0;x < 16*4;x++)
			pal[x] <<= 2;
	}

	if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
		vesa_set_palette_data(0,16,pal);
	}
	else {
		outp(0x3C8,0);
		for (y=0;y < 16;y++) {
			outp(0x3C9,pal[y*4+0]);
			outp(0x3C9,pal[y*4+1]);
			outp(0x3C9,pal[y*4+2]);
		}
	}

	free(pal);
	while (getch() != 13);

	if (!vbe_mode_decision_acceptmode(md,NULL)) {
		vbe_reset_video_to_text();
		printf("Failed to un-set up mode\n");
	}
	else {
		vbe_reset_video_to_text();
	}
}

static void vbe_mode_test_pattern_svga_packed(struct vbe_mode_decision *md,struct vbe_mode_info *mi) {
	unsigned int x,y,bypp;
	unsigned long ofs;

	bypp = (mi->bits_per_pixel+7)>>3;
	if (mi->bits_per_pixel == 4 && mi->number_of_planes == 1) {
		/* nonstandard packed 4-bit mode as seen on Toshiba Libretto Chips & Tech 65550 BIOS */
		unsigned char *pal = malloc(16*4);

		memset(pal,0,16*4);
		for (x=0;x < 16;x++) {
			pal[x*4+0] = x*4;
			pal[x*4+1] = x*4;
			pal[x*4+2] = x*4;
		}

		/* try to set DAC width */
		if (md->dac8 && (x=vbe_set_dac_width(8)) != 8) {
			vbe_reset_video_to_text();
			printf("Cannot set DAC width to 8 bits (ret=%u)\n",x);
			return;
		}
		else if (!md->dac8) {
			vbe_set_dac_width(6); /* don't care if it fails */
		}

		if (md->dac8) {
			for (x=0;x < 64;x++)
				pal[x] <<= 2;
		}

		if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
			vesa_set_palette_data(0,16,pal);
		}
		else {
			outp(0x3C8,0);
			for (y=0;y < 16;y++) {
				outp(0x3C9,pal[y*4+0]);
				outp(0x3C9,pal[y*4+1]);
				outp(0x3C9,pal[y*4+2]);
			}
		}

		/* high nibble, low nibble */
		for (y=0;y < 32 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x += 2,ofs++) {
				vesa_writeb(ofs,(x << 4) | (x+1));
			}
		}
		for (y=32;y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x += 2,ofs++) {
				vesa_writeb(ofs,((x^y) << 4) | (((x+1)^y) & 0xF));
			}
		}

		free(pal);
		pal = NULL;
	}
	else if (bypp == 1) {
		unsigned char *pal = malloc(1024);

		memset(pal,0,1024);
		for (x=0;x < 64;x++) {
			pal[x*4+0] = x;
			pal[x*4+1] = x;
			pal[x*4+2] = x;
		}
		for (x=0;x < 64;x++) {
			pal[(x+64)*4+0] = x;
			pal[(x+64)*4+1] = 0;
			pal[(x+64)*4+2] = 0;
		}
		for (x=0;x < 64;x++) {
			pal[(x+128)*4+0] = 0;
			pal[(x+128)*4+1] = x;
			pal[(x+128)*4+2] = 0;
		}
		for (x=0;x < 64;x++) {
			pal[(x+192)*4+0] = 0;
			pal[(x+192)*4+1] = 0;
			pal[(x+192)*4+2] = x;
		}

		/* try to set DAC width */
		if (md->dac8 && (x=vbe_set_dac_width(8)) != 8) {
			vbe_reset_video_to_text();
			printf("Cannot set DAC width to 8 bits (ret=%u)\n",x);
			return;
		}
		else if (!md->dac8) {
			vbe_set_dac_width(6); /* don't care if it fails */
		}

		if (md->dac8) {
			for (x=0;x < 1024;x++)
				pal[x] <<= 2;
		}

		if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
			vesa_set_palette_data(0,256,pal);
		}
		else {
			outp(0x3C8,0);
			for (y=0;y < 256;y++) {
				outp(0x3C9,pal[y*4+0]);
				outp(0x3C9,pal[y*4+1]);
				outp(0x3C9,pal[y*4+2]);
			}
		}

		for (y=0;y < 32 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=bypp) {
				vesa_writeb(ofs,x);
			}
		}
		for (y=32;y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=bypp) {
				vesa_writeb(ofs,x^y);
			}
		}

		free(pal);
		pal = NULL;
	}
	else if (bypp == 2) {
		unsigned int r,g,b;
		for (y=0;y < 16 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=2) {
				r = x & ((1 << mi->red_mask_size) - 1);
				vesa_writew(ofs,(r << mi->red_field_position));
			}
		}
		for (y=16;y < 32 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=2) {
				r = x & ((1 << mi->green_mask_size) - 1);
				vesa_writew(ofs,(r << mi->green_field_position));
			}
		}
		for (y=32;y < 48 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=2) {
				r = x & ((1 << mi->blue_mask_size) - 1);
				vesa_writew(ofs,(r << mi->blue_field_position));
			}
		}
		for (y=48;y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=2) {
				r = x & ((1 << mi->red_mask_size) - 1);
				g = y & ((1 << mi->green_mask_size) - 1);
				b = (((x >> mi->red_mask_size) ^ (y >> mi->green_mask_size)) & 1) ? (r^g) : 0;
				vesa_writew(ofs,
						(r << mi->red_field_position) |
						(g << mi->green_field_position) |
						(b << mi->blue_field_position));
			}
		}
	}
	else if (bypp == 3 || bypp == 4) {
		unsigned int r,g,b;
		for (y=0;y < 16 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=bypp) {
				r = x & ((1UL << (unsigned long)mi->red_mask_size) - 1UL);
				vesa_writed(ofs,((unsigned long)r << (unsigned long)mi->red_field_position));
			}
		}
		for (y=16;y < 32 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=bypp) {
				r = x & ((1UL << (unsigned long)mi->green_mask_size) - 1UL);
				vesa_writed(ofs,((unsigned long)r << mi->green_field_position));
			}
		}
		for (y=32;y < 48 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=bypp) {
				r = x & ((1UL << (unsigned long)mi->blue_mask_size) - 1UL);
				vesa_writed(ofs,((unsigned long)r << mi->blue_field_position));
			}
		}
		for (y=48;y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x++,ofs+=bypp) {
				r = x & ((1UL << (unsigned long)mi->red_mask_size) - 1UL);
				g = y & ((1UL << (unsigned long)mi->green_mask_size) - 1UL);
				b = (((x >> mi->red_mask_size) ^ (y >> mi->green_mask_size)) & 1UL) ? (r^g) : 0UL;
				vesa_writed(ofs,
					((unsigned long)r << (unsigned long)mi->red_field_position) |
					((unsigned long)g << (unsigned long)mi->green_field_position) |
					((unsigned long)b << (unsigned long)mi->blue_field_position));
			}
		}
	}

	while (getch() != 13);

	if (md->dac8 && mi->bits_per_pixel == 8 && (mi->memory_model == 4 || mi->memory_model == 6)) {
		unsigned char *pal = malloc(1024);
		unsigned int x,y;

		for (x=0;x < 256;x++) {
			pal[x*4+0] = x;
			pal[x*4+1] = x;
			pal[x*4+2] = x;
			pal[x*4+3] = 0;
		}

		if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
			vesa_set_palette_data(0,256,pal);
		}
		else {
			outp(0x3C8,0);
			for (y=0;y < 256;y++) {
				outp(0x3C9,pal[y*4+0]);
				outp(0x3C9,pal[y*4+1]);
				outp(0x3C9,pal[y*4+2]);
			}
		}

		while (getch() != 13);

		free(pal);
		pal = NULL;
	}

	if (!vbe_mode_decision_acceptmode(md,NULL)) {
		vbe_reset_video_to_text();
		printf("Failed to un-set up mode\n");
	}
	else {
		vbe_reset_video_to_text();
	}
}

void sstep_wait() {
	char c;

	if (!sstep) return;

	do {
		c = 13;
		read(0,&c,1);
	} while (!(c == 13));
}

int main(int argc,char **argv) {
	struct vbe_mode_decision md;
	struct vbe_mode_info mi={0};
	int i,swi=0,no_draw=0;

	vbe_mode_decision_init(&md);
	for (i=1;i < argc;) {
		const char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			do { a++; } while (*a == '-' || *a == '/');

			if (!strcmp(a,"h") || !strcmp(a,"help") || !strcmp(a,"?")) {
				help();
				return 1;
			}
			else if (!strcmp(a,"nwf")) {
				md.no_wf = 1;
			}
			else if (!strcmp(a,"fwf")) {
				md.force_wf = 1;
			}
			else if (!strcmp(a,"l")) {
				md.lfb = 1;
			}
			else if (!strcmp(a,"s")) {
				sstep = 1;
			}
			else if (!strcmp(a,"b")) {
				md.lfb = 0;
			}
			else if (!strcmp(a,"nd")) {
				no_draw = 1;
			}
			else if (!strcmp(a,"8")) {
				md.dac8 = 1;
			}
			else if (!strcmp(a,"6")) {
				md.dac8 = 0;
			}
			else {
				printf("I don't know what switch '%s' means\n",a);
				return 1;
			}
		}
		else {
			switch (swi++) {
				case 0:
					md.mode = (int)strtol(a,NULL,0);
					break;
			};
		}
	}

	printf("VESA VGA test program\n");
	if (md.mode < 0) {
		help();
		return 1;
	}
	sstep_wait();

	printf("cpu "); fflush(stdout);
	cpu_probe();
	sstep_wait();
	printf("dos "); fflush(stdout);
	probe_dos();
	sstep_wait();
	printf("win "); fflush(stdout);
	detect_windows();
	sstep_wait();

	printf("Probing bios "); fflush(stdout);
	if (!vbe_probe()) {
		printf("VESA BIOS not found\n");
		return 1;
	}
	sstep_wait();

	if (md.no_wf) printf("...not using direct window function\n");

	printf("mode "); fflush(stdout);
	sstep_wait();
	if (!vbe_read_mode_info(md.mode,&mi)) {
		printf("Cannot read mode info\n");
		return 1;
	}
	sstep_wait();
	vbe_fill_in_mode_info(md.mode,&mi);
	printf("OK\n");
	sstep_wait();

	if (!vbe_mode_decision_validate(&md,&mi)) {
		printf(" Parameter validation failed\n");
		return 1;
	}
	sstep_wait();
	if (!vbe_mode_decision_setmode(&md,&mi)) {
		vbe_reset_video_to_text();
		printf(" Failed to set mode\n");
		return 1;
	}
	sstep_wait();
	if (!vbe_mode_decision_acceptmode(&md,&mi)) {
		vbe_reset_video_to_text();
		printf("Failed to set up mode\n");
		return 1;
	}
	sstep_wait();

	if (no_draw) {
		printf("OK--No drawing!\n");
		while (getch() != 13);
		if (!vbe_mode_decision_acceptmode(&md,NULL)) {
			vbe_reset_video_to_text();
			printf("Failed to un-set up mode\n");
			return 1;
		}
		vbe_reset_video_to_text();
	}
	else if (mi.memory_model == 3)
		vbe_mode_test_pattern_svga_planar(&md,&mi);
	else if (mi.memory_model == 4 || mi.memory_model == 6)
		vbe_mode_test_pattern_svga_packed(&md,&mi);
	else {
		while (getch() != 13);
		if (!vbe_mode_decision_acceptmode(&md,NULL)) {
			vbe_reset_video_to_text();
			printf("Failed to un-set up mode\n");
			return 1;
		}
		vbe_reset_video_to_text();
	}

	sstep_wait();
	return 0;
}

