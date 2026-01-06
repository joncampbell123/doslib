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

static int vga_setbmode = -1;
static int info = 0; /* show info */
static int sstep = 0; /* single step */

#if TARGET_MSDOS == 32
static unsigned char *font8x8 = NULL;
#else
static unsigned char FAR *font8x8 = NULL;
#endif

static char info_txt[1024];

static void help() {
	printf("modeset [options] <MODE>\n");
	printf("    /h /help              This help\n");
	printf("    /l                    Use linear framebuffer\n");
	printf("    /b                    Use banked mode\n");
	printf("    /nwf                  Don't use direct 16-bit window function\n");
	printf("    /fwf                  Force using 16-bit window function\n");
	printf("    /npw                  Don't use protected mode 32-bit window function VBE 2\n");
	printf("    /fpw                  Force using protected mode 32-bit window function VBE 2\n");
	printf("    /6                    Set DAC to 6-bit wide (16/256-color modes only)\n");
	printf("    /8                    Set DAC to 8-bit wide (16/256-color modes only)\n");
	printf("    /s                    Single-step mode (hit ENTER)\n");
	printf("    /i                    Print info on screen\n");
}

static void vbe_mode_test_pattern_svga_text(struct vbe_mode_decision *md,struct vbe_mode_info *mi) {
	unsigned long ofs;
	unsigned int x,y;
    unsigned int c;

	probe_vga();

    for (c=0;c < 16;c++) {
        ofs = 0;
        for (y=0;y < mi->y_resolution;y++) {
            ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
            for (x=0;x < mi->x_resolution;x++) {
                vesa_writeb(ofs++,(x+y)&0xFF);
                vesa_writeb(ofs++,y + (c << 4));
            }
        }

        if (c == 0 && info) {
            unsigned int w=0,r=0;
            char *s = info_txt;
            char c;

            while (c = *s++) {
                if (c != '\n') {
                    vesa_writeb(w+0,(unsigned char)c);
                    vesa_writeb(w+1,0x0F);
                    w += 2;
                }
                else {
                    w = (r += mi->bytes_per_scan_line);
                }
            }
        }

        while (getch() != 13);
    }

	if (!vbe_mode_decision_acceptmode(md,NULL)) {
		vbe_reset_video_to_text();
		printf("Failed to un-set up mode\n");
	}
	else {
		vbe_reset_video_to_text();
	}
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

    if (info) {
        unsigned int w=0,r=0,fo;
        char *s = info_txt;
        char c;

        vga_write_GC(VGA_GC_MODE,		0x00);	/* write mode 0 */

        while (c = *s++) {
            if (c != '\n') {
                fo = ((unsigned char)c) * 8;
                ofs = w;
                w++;

                for (y=0;y < 8;y++) {
			        vesa_writeb(ofs,font8x8[fo++]);
                    ofs += mi->bytes_per_scan_line;
                }
            }
            else {
                w = (r += (mi->bytes_per_scan_line * 8));
            }
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

static const unsigned char dither1bpp[8][4] = {
	{0x00,0x00,0x00,0x00},
	{0xAA,0x00,0x00,0x00},
	{0xAA,0x00,0xAA,0x00},
	{0xAA,0x55,0xAA,0x00},
	{0xAA,0x55,0xAA,0x55},
	{0xFF,0x55,0xAA,0x55},
	{0xFF,0x55,0xFF,0x55},
	{0xFF,0xFF,0xFF,0xFF}
};

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

#if 0 // FIXME: Does this 4bpp packed mode support 8-bit DAC features? If the Libretto BIOS does not, then we should not try and DOSBox-X should not allow it.
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
#endif

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
				vesa_writeb(ofs,(x << 4) | ((x+1) & 0xF));
			}
		}
		for (y=32;y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x += 2,ofs++) {
				vesa_writeb(ofs,((x^y) << 4) | (((x+1)^y) & 0xF));
			}
		}

		if (info) {
			unsigned int fo;
			unsigned long w=0,r=0;
			char *s = info_txt;
			char c;

			while (c = *s++) {
				if (c != '\n') {
					fo = ((unsigned char)c) * 8;
					ofs = w;
					w += 8/2;

					for (y=0;y < 8;y++) {
						unsigned char b = font8x8[fo++];

						for (x=0;x < 8;x += 2) {
							unsigned char fb;

							fb  = (b & 0x80) ? 0xF0 : 0x00;
							fb += (b & 0x40) ? 0x0F : 0x00;

							vesa_writeb(ofs+(x>>1u),fb);
							b <<= 2;
						}

						ofs += mi->bytes_per_scan_line;
					}
				}
				else {
					w = (r += (mi->bytes_per_scan_line * 8));
				}
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
		pal = NULL;
	}
	else if (mi->bits_per_pixel == 1 && mi->number_of_planes == 1) {
		/* nonstandard 1bpp mode in DOSBox-X machine=svga_dosbox */
		unsigned char *pal = malloc(2*4);

		memset(pal,0,2*4);
		for (x=0;x < 2;x++) {
			pal[x*4+0] = x*63;
			pal[x*4+1] = x*63;
			pal[x*4+2] = x*63;
		}

		if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
			vesa_set_palette_data(0,2,pal);
		}
		else {
			outp(0x3C8,0);
			for (y=0;y < 2;y++) {
				outp(0x3C9,pal[y*4+0]);
				outp(0x3C9,pal[y*4+1]);
				outp(0x3C9,pal[y*4+2]);
			}
		}

		/* high nibble, low nibble */
		for (y=0;y < 32 && y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x += 8,ofs++) {
				vesa_writeb(ofs,(y&1)?0x55:0xAA);
			}
		}
		for (y=32;y < mi->y_resolution;y++) {
			ofs = ((unsigned long)y * (unsigned long)mi->bytes_per_scan_line);
			for (x=0;x < mi->x_resolution;x += 8,ofs++) {
				unsigned char c = ((x>>3) ^ (y>>3)) & 7;
				vesa_writeb(ofs,dither1bpp[c][y&3]);
			}
		}

		if (info) {
			unsigned int fo;
			unsigned long w=0,r=0;
			char *s = info_txt;
			char c;

			while (c = *s++) {
				if (c != '\n') {
					fo = ((unsigned char)c) * 8;
					ofs = w;
					w += 1;

					for (y=0;y < 8;y++) {
						unsigned char b = font8x8[fo++];
						vesa_writeb(ofs,b);
						ofs += mi->bytes_per_scan_line;
					}
				}
				else {
					w = (r += (mi->bytes_per_scan_line * 8));
				}
			}
		}

		while (getch() != 13);

		for (x=0;x < 2;x++) {
			pal[x*4+0] = x*63;
			pal[x*4+1] = 0;
			pal[x*4+2] = 0;
		}

		if (md->dac8) {
			for (x=0;x < 2*4;x++)
				pal[x] <<= 2;
		}

		if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
			vesa_set_palette_data(0,2,pal);
		}
		else {
			outp(0x3C8,0);
			for (y=0;y < 2;y++) {
				outp(0x3C9,pal[y*4+0]);
				outp(0x3C9,pal[y*4+1]);
				outp(0x3C9,pal[y*4+2]);
			}
		}

		while (getch() != 13);

		for (x=0;x < 2;x++) {
			pal[x*4+0] = 0;
			pal[x*4+1] = x*63;
			pal[x*4+2] = 0;
		}

		if (md->dac8) {
			for (x=0;x < 2*4;x++)
				pal[x] <<= 2;
		}

		if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
			vesa_set_palette_data(0,2,pal);
		}
		else {
			outp(0x3C8,0);
			for (y=0;y < 2;y++) {
				outp(0x3C9,pal[y*4+0]);
				outp(0x3C9,pal[y*4+1]);
				outp(0x3C9,pal[y*4+2]);
			}
		}

		while (getch() != 13);

		for (x=0;x < 2;x++) {
			pal[x*4+0] = 0;
			pal[x*4+1] = 0;
			pal[x*4+2] = x*4;
		}

		if (md->dac8) {
			for (x=0;x < 2*4;x++)
				pal[x] <<= 2;
		}

		if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) {
			vesa_set_palette_data(0,2,pal);
		}
		else {
			outp(0x3C8,0);
			for (y=0;y < 2;y++) {
				outp(0x3C9,pal[y*4+0]);
				outp(0x3C9,pal[y*4+1]);
				outp(0x3C9,pal[y*4+2]);
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

		if (info) {
			unsigned int fo;
			unsigned long w=0,r=0;
			char *s = info_txt;
			char c;

			while (c = *s++) {
				if (c != '\n') {
					fo = ((unsigned char)c) * 8;
					ofs = w;
					w += 8;

					for (y=0;y < 8;y++) {
						unsigned char b = font8x8[fo++];

						for (x=0;x < 8;x++) {
							vesa_writeb(ofs+x,(b & 0x80) ? 63 : 0);
							b <<= 1;
						}

						ofs += mi->bytes_per_scan_line;
					}
				}
				else {
					w = (r += (mi->bytes_per_scan_line * 8));
				}
			}
		}
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

		if (info) {
			unsigned int fo;
			unsigned long w=0,r=0;
			char *s = info_txt;
			char c;

			while (c = *s++) {
				if (c != '\n') {
					fo = ((unsigned char)c) * 8;
					ofs = w;
					w += 8*2;

					for (y=0;y < 8;y++) {
						unsigned char b = font8x8[fo++];

						for (x=0;x < 8;x++) {
							vesa_writew(ofs+(x*2),(b & 0x80) ? 0xFFFF : 0x0000);
							b <<= 1;
						}

						ofs += mi->bytes_per_scan_line;
					}
				}
				else {
					w = (r += (mi->bytes_per_scan_line * 8));
				}
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

		if (info) {
			unsigned int fo;
			unsigned long w=0,r=0;
			char *s = info_txt;
			char c;

			while (c = *s++) {
				if (c != '\n') {
					fo = ((unsigned char)c) * 8;
					ofs = w;
					w += 8*bypp;

					for (y=0;y < 8;y++) {
						unsigned char b = font8x8[fo++];

						for (x=0;x < 8;x++) {
							vesa_writed(ofs+(x*bypp),(b & 0x80) ? 0xFFFFFFFF : 0x00000000);
							b <<= 1;
						}

						ofs += mi->bytes_per_scan_line;
					}
				}
				else {
					w = (r += (mi->bytes_per_scan_line * 8));
				}
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
			else if (!strcmp(a,"npw")) {
				md.no_p32wf = 1;
			}
			else if (!strcmp(a,"fpw")) {
				md.force_p32wf = 1;
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
			else if (!strcmp(a,"i")) {
				info = 1;
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
            else if (!strcmp(a,"vb")) {
                vga_setbmode = 0;//byte mode
            }
            else if (!strcmp(a,"vw")) {
                vga_setbmode = 1;//word mode
            }
            else if (!strcmp(a,"vd")) {
                vga_setbmode = 2;//dword mode
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

    if (info) {
#if TARGET_MSDOS == 32
        struct dpmi_realmode_call rc={0};

        rc.eax = 0x1130;
        rc.ebx = 0x0300;
        vbe_realint(&rc);

        font8x8 = (unsigned char*)((rc.es << 4) + (rc.ebp & 0xFFFF));
#else
        unsigned short s=0,o=0;

        __asm {
            push    ax
            push    bx
            push    es
            push    bp
            mov     ax,1130h
            mov     bh,3h
            int     10h
            mov     ax,bp
            pop     bp
            mov     s,es
            mov     o,ax
            pop     es
            pop     bx
            pop     ax
        }

        font8x8 = MK_FP(s,o);
#endif
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
#if TARGET_MSDOS == 32
	vbe2_pm_probe();
#endif

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

    if (info) {
        char *w = info_txt;
        char *f = info_txt + sizeof(info_txt) - 1;

        w += sprintf(w,"Mode 0x%04x:\n",md.mode);
        if (mi.mode_attributes & VESA_MODE_ATTR_LINEAR_FRAMEBUFFER_AVAILABLE)
            w += sprintf(w,"LinFB ");
        if (mi.mode_attributes & VESA_MODE_ATTR_DOUBLESCAN_AVAILABLE)
            w += sprintf(w,"Dblscan ");
        if (mi.mode_attributes & VESA_MODE_ATTR_INTERLACED_AVAILABLE)
            w += sprintf(w,"Intlace ");
        if (md.lfb)
            w += sprintf(w,"UsingLFB ");
        w += sprintf(w,"\n");

        w += sprintf(w,"Win A Attr=0x%02x seg=0x%04x\n"
                       "    Win B Attr=0x%02x seg=0x%04x\n",
                mi.win_a_attributes,mi.win_a_segment,
                mi.win_b_attributes,mi.win_b_segment);
        w += sprintf(w,"Window granularity=%uKB size=%uKB\n"
                       "    function=%04x:%04x bytes/line=%u\n",
                mi.win_granularity,mi.win_size,
                (unsigned int)(mi.window_function>>16),
                (unsigned int)(mi.window_function&0xFFFF),
                mi.bytes_per_scan_line);
        w += sprintf(w,"%u x %u (char %u x %u) %u-plane\n"
                       "    %ubpp. %u banks. Model %u.\n",
                mi.x_resolution,mi.y_resolution,mi.x_char_size,mi.y_char_size,
                mi.number_of_planes,mi.bits_per_pixel,mi.number_of_banks,mi.memory_model);
        w += sprintf(w,"RGBA (size,pos) R=(%u,%u) G=(%u,%u)\n"
                       "    B=(%u,%u) A=(%u,%u) DCModeInfo=0x%02X\n",
                mi.red_mask_size,	mi.red_field_position,
                mi.green_mask_size,	mi.green_field_position,
                mi.blue_mask_size,	mi.blue_field_position,
                mi.reserved_mask_size,	mi.reserved_field_position,
                mi.direct_color_mode_info);
        w += sprintf(w,"Phys: 0x%08lX Lnbyte/scan=%u\n"
                       "    BankPages=%u LinPages=%u\n",(unsigned long)mi.phys_base_ptr,
                mi.lin_bytes_per_line,	mi.bank_number_of_image_pages+1,
                mi.lin_number_of_image_pages);
        w += sprintf(w,"Lin RGBA (size,pos) R=(%u,%u) G=(%u,%u)\n"
                       "    B=(%u,%u) A=(%u,%u) maxpixelclock=%lu\n",
                mi.lin_red_mask_size,		mi.lin_red_field_position,
                mi.lin_green_mask_size,		mi.lin_green_field_position,
                mi.lin_blue_mask_size,		mi.lin_blue_field_position,
                mi.lin_reserved_mask_size,	mi.lin_reserved_field_position,
                mi.max_pixel_clock);

        assert(w <= f);
        *w = 0;
    }

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

    if (vga_setbmode >= 0) {
        /* Test whether SVGA modes are immune or not to the BYTE/WORD/DWORD bits */
        unsigned char un,mc;

        if (!probe_vga())
            return 1;

        un = vga_read_CRTC(0x14);
        mc = vga_read_CRTC(0x17);

        if (vga_setbmode >= 2)//DW BIT
            un |=  0x40;
        else
            un &= ~0x40;

        if (vga_setbmode >= 1)//W BIT (set to 0 for word mode)
            mc &= ~0x40;
        else
            mc |=  0x40;

        vga_write_CRTC(0x14,un);
        vga_write_CRTC(0x17,mc);
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
    else if (mi.memory_model == 0)
		vbe_mode_test_pattern_svga_text(&md,&mi);
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

