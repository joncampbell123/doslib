
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

char tmpline[160];

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

#if defined(SVGA_TSENG)
	int10_9_print("Tseng VGA registers and RAM will be\n");
#else
	int10_9_print("Standard VGA registers and RAM will be\n");
#endif

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

#if defined(SVGA_TSENG)
	sprintf(tmpname,"%s.NFO",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		if (tseng_mode == ET3000)
			fprintf(fp,"Tseng ET3000 specific register dump.\n");
		else if (tseng_mode == ET4000)
			fprintf(fp,"Tseng ET4000 specific register dump.\n");

		fprintf(fp,"\n");
		fprintf(fp,"Additional files in this specific dump:\n");
		fprintf(fp,"  *.CRT          CRTC register dump (as left by BIOS)\n");
		fprintf(fp,"  *.CRU          CRTC register dump after extended unlock.\n");
		fprintf(fp,"  *.CRL          CRTC register dump after extended lock again.\n");
		fprintf(fp,"\n");
		fprintf(fp,"The PL0, PL1, PL2, and PL3 files contain the entire ET4000 memory\n");
		fprintf(fp,"range. All 8 (ET3000) or 16 (ET4000) possible 64KB banks are dumped\n");
		fprintf(fp,"to the file to record how they map to VGA memory. The capture was\n");
		fprintf(fp,"read in 64KB chunks, per 64KB bank.\n");

		fclose(fp);
	}
#endif

	/* ============= Sequencer ============ */
	for (i=0;i < 256;i++) rdump[i] = vga_read_sequencer(i);

	/* ----- write */
	sprintf(tmpname,"%s.SEQ",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,256,1,fp);
		fclose(fp);
	}

	/* ============= GC ============ */
	for (i=0;i < 256;i++) rdump[i] = vga_read_GC(i);

	/* ----- write */
	sprintf(tmpname,"%s.GC",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,256,1,fp);
		fclose(fp);
	}

	/* ============= CRTC ============ */
	for (i=0;i < 256;i++) rdump[i] = vga_read_CRTC(i);

	/* ----- write */
	sprintf(tmpname,"%s.CRT",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,256,1,fp);
		fclose(fp);
	}

#if defined(SVGA_TSENG)
	tseng_et4000_extended_set_lock(/*unlock*/0);

	/* ============= CRTC unlocked ============ */
	for (i=0;i < 256;i++) rdump[i] = vga_read_CRTC(i);

	/* ----- write */
	sprintf(tmpname,"%s.CRU",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,256,1,fp);
		fclose(fp);
	}

	tseng_et4000_extended_set_lock(/*lock*/1);

	/* ============= CRTC locked ============ */
	for (i=0;i < 256;i++) rdump[i] = vga_read_CRTC(i);

	/* ----- write */
	sprintf(tmpname,"%s.CRL",nname);
	if ((fp=fopen(tmpname,"wb")) != NULL) {
		fwrite(rdump,256,1,fp);
		fclose(fp);
	}
#endif

	/* ============= Attribute controller ============ */
	for (i=0;i < 256;i++) rdump[i] = vga_read_AC(i);

	/* ----- write */
	sprintf(tmpname,"%s.AC",nname);
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
		unsigned char ogc6;

		ogc6 = vga_read_GC(6);

		/* for each plane, write to file */
		{
			sprintf(tmpname,"%s.RAM",nname);
			if ((fp=fopen(tmpname,"wb")) != NULL) {
#if defined(SVGA_TSENG)
				unsigned int bank,bmax;

				// TSENG ET3000/ET4000 VGA=========================
				// Capture VGA memory (each 64KB plane) as seen by all 8 or 16 SVGA banks
				vga_write_GC(6,(ogc6 & (~0xC)) + 1); /* we want video RAM to map to 0xA0000-0xAFFFF */

				if (tseng_mode == ET4000)
					bmax = 16;
				else if (tseng_mode == ET3000)
					bmax = 8;
				else
					abort();

				for (bank=0;bank < bmax;bank++) {
					if (tseng_mode == ET4000)
						outp(0x3CD/*Segment Select*/,bank | (bank << 4));
					else if (tseng_mode == ET3000)
						outp(0x3CD/*Segment Select*/,bank | (bank << 3) | 0x40/*64KB configuration*/);

					for (i=0;i < (65536/1024);i++) {
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
				}
				outp(0x3CD/*Segment Select*/,0);
				fclose(fp);
				vga_write_GC(6,ogc6); /* restore */
				// ======================================
#else//=====================================================================================
				// STANDARD VGA=========================
				// Capture VGA planar memory (each 64KB plane)
				vga_write_GC(6,(ogc6 & (~0xC)) + 1); /* we want video RAM to map to 0xA0000-0xAFFFF */

				for (i=0;i < (65536/1024);i++) {
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
				fclose(fp);
				vga_write_GC(6,ogc6); /* restore */
				// ======================================
#endif
			}
		}

		vga_write_GC(6,ogc6);
	}

	/* ============= RAM scan/dump (planar) ============ */
	{
		unsigned char seq4,crt17h,crt14h,pl,ogc4,ogc5,ogc6,seqmask;

		ogc6 = vga_read_GC(6);
		ogc5 = vga_read_GC(5);
		ogc4 = vga_read_GC(4);
		seq4 = vga_read_sequencer(4);
		seqmask = vga_read_sequencer(VGA_SC_MAP_MASK);
		crt14h = vga_read_CRTC(0x14);
		crt17h = vga_read_CRTC(0x17);

		/* switch into planar mode briefly for planar capture */
		vga_write_sequencer(4,0x06);
		vga_write_sequencer(0,0x01);
		vga_write_sequencer(0,0x03);
		vga_write_sequencer(VGA_SC_MAP_MASK,0xF);

		/* for each plane, write to file */
		for (pl=0;pl < 4;pl++) {
			sprintf(tmpname,"%s.PL%u",nname,pl);
			if ((fp=fopen(tmpname,"wb")) != NULL) {
#if defined(SVGA_TSENG)
				unsigned int bank,bmax;

				// TSENG ET3000/ET4000 VGA=========================
				// Capture VGA memory (each 64KB plane) as seen by all 8 or 16 SVGA banks
				vga_write_sequencer(4,0x06);
				vga_write_GC(6,(ogc6 & (~0xF)) + (1 << 2) + 1); /* we want video RAM to map to 0xA0000-0xAFFFF AND we want to temporarily disable alphanumeric mode */
				vga_write_GC(5,(ogc5 & (~0x7B))); /* read mode=0 write mode=0 host o/e=0 */
				vga_write_GC(4,pl); /* read map select */

				if (tseng_mode == ET4000)
					bmax = 16;
				else if (tseng_mode == ET3000)
					bmax = 8;
				else
					abort();

				for (bank=0;bank < bmax;bank++) {
					if (tseng_mode == ET4000)
						outp(0x3CD/*Segment Select*/,bank | (bank << 4));
					else if (tseng_mode == ET3000)
						outp(0x3CD/*Segment Select*/,bank | (bank << 3) | 0x40/*64KB configuration*/);

					for (i=0;i < (65536/1024);i++) {
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
				}
				outp(0x3CD/*Segment Select*/,0);
				fclose(fp);
				vga_write_GC(6,ogc6); /* restore */
				// ======================================
#else//=====================================================================================
				// STANDARD VGA=========================
				// Capture VGA planar memory (each 64KB plane)
				vga_write_sequencer(4,0x06);
				vga_write_GC(6,(ogc6 & (~0xF)) + (1 << 2) + 1); /* we want video RAM to map to 0xA0000-0xAFFFF AND we want to temporarily disable alphanumeric mode */
				vga_write_GC(5,(ogc5 & (~0x7B))); /* read mode=0 write mode=0 host o/e=0 */
				vga_write_GC(4,pl); /* read map select */

				for (i=0;i < (65536/1024);i++) {
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
				fclose(fp);
				vga_write_GC(6,ogc6); /* restore */
				// ======================================
#endif
			}
		}

		vga_write_sequencer(4,seq4);
		vga_write_sequencer(0,0x01);
		vga_write_sequencer(0,0x03);
		vga_write_sequencer(VGA_SC_MAP_MASK,seqmask);
		vga_write_CRTC(0x14,crt14h);
		vga_write_CRTC(0x17,crt17h);
		vga_write_GC(4,ogc4);
		vga_write_GC(5,ogc5);
		vga_write_GC(6,ogc6);
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

