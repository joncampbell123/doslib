
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

char tmp[2048];
char tmpline[160];

void dump_bios_vptables_to_files() {
    /* INT 1Dh video parameter table */
    {
        VGA_RAM_PTR ptr;
        uint16_t pts,pto;
        FILE *fp;

        assert(sizeof(tmp) >= 1024);
#if TARGET_MSDOS == 32
        pto = ((uint16_t*)(0x1Du << 2u))[0];
        pts = ((uint16_t*)(0x1Du << 2u))[1];
        ptr = (VGA_RAM_PTR)(((unsigned long)pts << 4ul) + (unsigned long)pto);
        memcpy(tmp,ptr,1024);
#else
        pto = ((uint16_t far*)(0x1Du << 2u))[0];
        pts = ((uint16_t far*)(0x1Du << 2u))[1];
        ptr = (VGA_RAM_PTR)MK_FP(pts,pto);
        _fmemcpy(tmp,ptr,1024);
#endif

        fp = fopen("INT1DVPT.NFO","wb");
        if (fp != NULL) {
            fprintf(fp,"INT 1Dh table found at %04x:%04x\n",pts,pto);
            fclose(fp);
        }

        fp = fopen("INT1DVPT.BIN","wb");
        if (fp != NULL) {
            fwrite(tmp,1024,1,fp);
            fclose(fp);
        }
    }

    /* EGA/VGA video parameter control block (40:A8 far ptr).
     * According to DOSBox-X source code, the EGA table is 0x40*0x17 large
     * and the VGA table is 0x40*0x1D large. 2048 bytes allows for tables
     * up to 0x40*0x20 large. */
    {
        VGA_RAM_PTR ptr;
        uint16_t pts,pto;
        FILE *fp;

        assert(sizeof(tmp) >= 2048);
#if TARGET_MSDOS == 32
        pto = ((uint16_t*)(0x4A8))[0];
        pts = ((uint16_t*)(0x4A8))[1];
        ptr = (VGA_RAM_PTR)(((unsigned long)pts << 4ul) + (unsigned long)pto);
#else
        pto = ((uint16_t far*)(0x4A8))[0];
        pts = ((uint16_t far*)(0x4A8))[1];
        ptr = (VGA_RAM_PTR)MK_FP(pts,pto);
#endif

        if (ptr != 0) {
#if TARGET_MSDOS == 32
            pto = ((uint16_t*)((uint8_t*)ptr))[0];
            pts = ((uint16_t*)((uint8_t*)ptr))[1];
            ptr = (VGA_RAM_PTR)(((unsigned long)pts << 4ul) + (unsigned long)pto);
            memcpy(tmp,ptr,2048);
#else
            pto = ((uint16_t far*)((uint8_t far*)ptr))[0];
            pts = ((uint16_t far*)((uint8_t far*)ptr))[1];
            ptr = (VGA_RAM_PTR)MK_FP(pts,pto);
            _fmemcpy(tmp,ptr,2048);
#endif

            fp = fopen("40A8VPCT.NFO","wb");
            if (fp != NULL) {
                fprintf(fp,"BDA 40:A8 VPCT table found at %04x:%04x\n",pts,pto);
                fclose(fp);
            }

            fp = fopen("40A8VPCT.BIN","wb");
            if (fp != NULL) {
                fwrite(tmp,2048,1,fp);
                fclose(fp);
            }
        }
    }
}

void bios_cls() {
	VGA_ALPHA_PTR ap;
	VGA_RAM_PTR rp;
	unsigned char m;

	m = int10_getmode();
	if ((rp=vga_state.vga_graphics_ram) != NULL && !(m <= 3 || m == 7)) {
#if TARGET_MSDOS == 16
		unsigned int i,im;

		im = (FP_SEG(vga_state.vga_graphics_ram_fence) - FP_SEG(vga_state.vga_graphics_ram));
		if (im > 0xFFE) im = 0xFFE;
		im <<= 4;
		for (i=0;i < im;i++) vga_state.vga_graphics_ram[i] = 0;
#else
		while (rp < vga_state.vga_graphics_ram_fence) *rp++ = 0;
#endif
	}
	else if ((ap=vga_state.vga_alpha_ram) != NULL) {
#if TARGET_MSDOS == 16
		unsigned int i,im;

		im = (FP_SEG(vga_state.vga_alpha_ram_fence) - FP_SEG(vga_state.vga_alpha_ram));
		if (im > 0x7FE) im = 0x7FE;
		im <<= 4 - 1; /* because ptr is type uint16_t */
		for (i=0;i < im;i++) vga_state.vga_alpha_ram[i] = 0x0720;
#else
		while (ap < vga_state.vga_alpha_ram_fence) *ap++ = 0x0720;
#endif
	}
	else {
		printf("WARNING: bios cls no ptr\n");
	}
}

static unsigned char rdump[4096];

static void int10_9_print(const char *x) {
	while (*x != 0) {
		unsigned short b,cc;

		if (*x == '\n') {
			vga_moveto(0,vga_state.vga_pos_y+1);
			vga_sync_bios_cursor();
			x++;
		}
		else {
			vga_sync_bios_cursor();

			cc = 0x0900 + (((unsigned char)(*x++)) & 0xFF);
			b = 7;

			__asm {
				push	ax
				push	bx
				push	cx
				mov	ax,cc
				mov	bx,b
				mov	cx,1
				int	10h
				pop	cx
				pop	bx
				pop	ax
			}

			vga_moveto(vga_state.vga_pos_x+1,vga_state.vga_pos_y);
			vga_sync_bios_cursor();
		}
	}
}

void dump_to_file(int automated) {
	char tmpname[32];
	char nname[17];
	int c,mode,i;
	FILE *fp;

	mode = int10_getmode();
	sprintf(nname,"MCGADP%02X",mode);

	bios_cls();
	/* position the cursor to home */
	vga_moveto(0,0);
	vga_sync_bios_cursor();

	/* NTS: Older SVGA cards have BIOSes that won't printf to SVGA modes,
	 *      but will apparently support INT 10h AH=0x09. We want this
	 *      text to appear on screen! */

	int10_9_print("Standard MCGA registers and RAM will be\n");

	vga_moveto(0,1);
	vga_sync_bios_cursor();
	if (!automated)
		sprintf(tmpline,"dumped to %s. Hit ENTER.\n",nname);
	else
		sprintf(tmpline,"dumped to %s. Mode 0x%02x.\n",nname,mode);
	int10_9_print(tmpline);

	/* draw colored text for visual reference */
	for (c=0;c < 40;c++) {
		unsigned short b,cc;

		vga_moveto(c,3);
		vga_sync_bios_cursor();
		cc = 0x0930 + (c&0xF);
		b = c;

		__asm {
			push	ax
			push	bx
			push	cx
			mov	ax,cc
			mov	bx,b
			mov	cx,1
			int	10h
			pop	cx
			pop	bx
			pop	ax
		}
	}

	/* ============= BIOS data area ============ */
#if TARGET_MSDOS == 32
	for (i=0;i < 256;i++) rdump[i] = *((uint8_t*)(0x400+i));
#else
	for (i=0;i < 256;i++) rdump[i] = *((uint8_t far*)MK_FP(0x40,i));
#endif

	/* ----- write */
	sprintf(tmpname,"%s.BDA",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,256,1,fp);
		fclose(fp);
	}

    /* resume */
	vga_moveto(0,6);
	vga_sync_bios_cursor();

	/* older VGA BIOSes might not print the text on screen (doesn't support INT 10h teletype functions in SVGA mode).
	 * but they do support the "set pixel" calls. so also draw on the screen with Set Pixel. */
	{
		unsigned short x,y,aa,bb;

		/* FIXME: What do VGA BIOSes do with "setpixel" if the mode we set is a 16bpp/24bpp/32bpp mode?? the "pixel color" field is too small to hold highcolor pixels! */
		for (y=(6*16);y < 8+(6*16);y++) {
			for (x=0;x < 256;x++) {
				aa = 0x0C00 + x;
				bb = 0;

				__asm {
					push	ax
					push	bx
					push	cx
					mov	ax,aa
					mov	bx,bb
					mov	cx,x
					mov	dx,y
					int	10h
					pop	cx
					pop	bx
					pop	ax
				}
			}
		}
	}

	if (!automated) {
		c = getch();
		if (c != 13) return;
	}
	else {
		/* do it before capturing. we don't know if our tricks below would screw up the SVGA mode.
		 * we delay in order to allow the VGA monitor to sync. if appearance of the mode on a monitor
		 * matters this gives a camcorder pointed at the screen and/or VGA capture/scan conversion
		 * time to get a picture of the mode. */

		/* wait 3 sec */
		t8254_wait(t8254_us2ticks(1000000UL));
		t8254_wait(t8254_us2ticks(1000000UL));
		t8254_wait(t8254_us2ticks(1000000UL));
	}

	/* ============= CRTC ============ */
	for (i=0;i < 256;i++) rdump[i] = vga_read_CRTC(i);

	/* ----- write */
	sprintf(tmpname,"%s.CRT",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,256,1,fp);
		fclose(fp);
	}

	/* ============= VGA palette ============ */
	outp(0x3C8,0);
	outp(0x3C7,0);
	for (i=0;i < (2*3*256);i++) rdump[i] = inp(0x3C9);

	/* ----- write */
	sprintf(tmpname,"%s.DAC",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,2*3*256,1,fp);
		fclose(fp);
	}

	/* ============= Reg scan/dump ============ */
	for (i=0;i < 0x30;i++) rdump[i] = inp(0x3B0+i);

	/* ----- write */
	sprintf(tmpname,"%s.RED",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,0x30,1,fp);
		fclose(fp);
	}

	/* ============= RAM scan/dump (normal) ============ */
	{
		/* for each plane, write to file */
		{
			sprintf(tmpname,"%s.RAM",nname);
			if ((fp=fopen(tmpname,"wb")) != NULL) {
				for (i=0;i < (0x10000/1024);i++) {
					unsigned int j;

#if TARGET_MSDOS == 32
					volatile unsigned char *s = ((volatile unsigned char*)0xA0000) + (unsigned int)(i * 1024);
					volatile unsigned char *d = (volatile unsigned char*)rdump;
					for (j=0;j < 1024;j++) d[j] = s[j];
#else
					volatile unsigned char FAR *s = (volatile unsigned char FAR*)MK_FP(0xA000,(i * 1024));
					volatile unsigned char FAR *d = (volatile unsigned char FAR*)rdump;
					for (j=0;j < 1024;j++) d[j] = s[j];
#endif
					fwrite(rdump,1024,1,fp);
				}
				for (i=0;i < (0x10000/1024);i++) {
					unsigned int j;

#if TARGET_MSDOS == 32
					volatile unsigned char *s = ((volatile unsigned char*)0xB0000) + (unsigned int)(i * 1024);
					volatile unsigned char *d = (volatile unsigned char*)rdump;
					for (j=0;j < 1024;j++) d[j] = s[j];
#else
					volatile unsigned char FAR *s = (volatile unsigned char FAR*)MK_FP(0xB000,(i * 1024));
					volatile unsigned char FAR *d = (volatile unsigned char FAR*)rdump;
					for (j=0;j < 1024;j++) d[j] = s[j];
#endif
					fwrite(rdump,1024,1,fp);
				}
				fclose(fp);
				// ======================================
			}
		}
	}

	/* ======================================= */
	printf("Done\n");
	if (!automated) {
		while (getch() != 13);
	}
	else {
		t8254_wait(t8254_us2ticks(1000000UL));
	}
}

int prompt_disk_space() {
    struct diskfree_t df;
    int c;

    do {
        if (_dos_getdiskfree(0/*default*/,&df) == 0) {
            unsigned long fsp,m;

            fsp  = (unsigned long)df.bytes_per_sector;
            fsp *= (unsigned long)df.sectors_per_cluster;
            fsp *= (unsigned long)df.avail_clusters;

#if defined(SVGA_TSENG)
            m  = (65536UL + 128UL);
            if (tseng_mode == ET4000)       m *= 16UL;
            else if (tseng_mode == ET3000)  m *= 8UL;
            m += 256UL * 24UL;
#else
            m  = (65536UL + 128UL) * 5UL;
            m += 256UL * 24UL;
#endif

            if (fsp > m) break;

            printf("Not enough disk space (%luKB). Switch disks and hit ENTER to continue.\n",fsp>>10UL);
        }
        else {
            printf("Unable to determine disk space. Switch disks and hit ENTER to continue.\n");
        }

        do { c = getch();
        } while (!(c == 13 || c == 27));

        if (c == 27) return 0;
    } while (1);

    return 1;
}

void dump_auto_modes_and_bios() {
	unsigned int smode;
	int c;

	bios_cls();
	/* position the cursor to home */
	vga_moveto(0,0);
	vga_sync_bios_cursor();

	printf("I will run through all video modes\n");
	printf("and dump their contents. This may\n");
	printf("harm your monitor if it is old, please\n");
	printf("take caution.\n");
	printf("\n");
	printf("Hit ENTER to continue.\n");

	c = getch();
	if (c != 13) return;

    dump_bios_vptables_to_files();

	int10_setmode(3);

	/* standard modes.
	 * SVGA cards (made either prior to or after VESA BIOS extensions) will have SVGA modes beyond the standard IBM INT 10h modelist */
	for (smode=0;smode < 127/*avoid Paradise special modes [RBIL]*/;smode++) {
		if (smode >= 8 && smode < 13) {
			/* do not enumerate these modes. they don't work, unless you're Tandy/PCjr.
			 * some early 1990's SVGA cards will even crash in the VGA BIOS if you ask for such modes! */
			continue;
		}

        /* wait for free disk space */
        if (!prompt_disk_space()) break;

		/* set the mode. if it didn't take, then skip */
		int10_setmode(smode);
		if (int10_getmode() != smode) continue;

		/* DUMP IT! */
		dump_to_file(/*automated=*/1);

		/* back to a safe default */
		int10_setmode(3);
	}
}

int main() {
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

	if (!(vga_state.vga_flags & VGA_IS_MCGA) || (vga_state.vga_flags & VGA_IS_VGA)) {
        printf("This program is written only for MCGA hardware\n");
		return 1;
	}

    dump_auto_modes_and_bios();

	return 0;
}

