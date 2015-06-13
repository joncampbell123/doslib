
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

char tmpline[80];

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

unsigned int common_prompt_number() {
	unsigned int nm;

	tmpline[0] = 0;
	fgets(tmpline,sizeof(tmpline),stdin);
	if (isdigit(tmpline[0]))
		nm = (unsigned int)strtoul(tmpline,NULL,0);
	else
		nm = ~0U;

	return nm;
}

void help_main() {
	int c;

	bios_cls();
	vga_moveto(0,0);
	vga_sync_bios_cursor();

	printf("VGA library state:\n");
	printf("\n");
	printf(" VGA=        (1) VGA detected\n");
	printf(" EGA=        (1) EGA detected/compat\n");
	printf(" CGA=        (1) CGA detected/compat\n");
	printf(" MDA=        (1) MDA detected/compat\n");
	printf(" MCGA=       (1) MCGA detected/compat\n");
	printf(" HGC=        Hercules detected & what\n");
	printf(" Tandy/PCjr= Tandy or PCjr detected\n");
	printf(" Amstrad=    Amstrad detected\n");
	printf(" IO=         Base I/O port (3B0/3D0)\n");
	printf(" ALPHA=      (1) Alphanumeric mode\n");
	printf(" RAM         VGA RAM mapping\n");
	printf(" 9W=         (1) VGA lib 9-pixel mode\n");
	printf(" INT10=      Active INT10h video mode\n");
	printf("\n");
	printf("ESC to return, ENTER/SPACE for more...\n");

	c = getch();
	if (c == 27) return;

	bios_cls();
	vga_moveto(0,0);
	vga_sync_bios_cursor();

	printf("VGA CRTC state:\n");
	printf("\n");
	printf(" Clock=      Dot clock selection (0-3)\n");
	printf(" div2=       (1) Divide dot clock by 2\n");
	printf(" pix/clk=    pixels/char clk (8 or 9)\n");
	printf(" Word=       (1) Word mode\n");
	printf(" Dword=      (1) Doubleword mode\n");
	printf(" Hsync       -/+ Hsync polarity\n");
	printf(" Vsync       -/+ Vsync polarity\n");
	printf(" mem4=       Mem clock divide by 4\n");
	printf(" SLR=        Shift/load rate\n");
	printf(" S4=         Shift 4 enable\n");
	printf(" memdv2=     (1) Div memaddr by 2\n");
	printf(" scandv2=    (1) Div scanlineclk by 2\n");
	printf(" awrap=      (1) Address wrap select\n");
	printf(" map14=      (1) Map MA14 = bit 14\n");
	printf(" map13=      (1) Map MA13 = bit 13\n");
	printf(" ref/scanline= Refresh cycles/scanline\n");
	printf(" hrate/vrate= Horz./Vert. refresh rate\n");
	printf(" offset=     offset (unit per scanline)\n");
	printf("\n");
	printf("ESC to return, ENTER/SPACE for more...\n");

	c = getch();
	if (c == 27) return;

	bios_cls();
	vga_moveto(0,0);
	vga_sync_bios_cursor();

	printf("VGA CRTC state (cont):\n");
	printf("\n");
	printf(" V:tot=      Vertical total\n");
	printf(" disp=       .. active display\n");
	printf(" retr=       .. retrace as inequality\n");
	printf(" blnk=       .. blanking as inequality\n");
	printf(" H:tot=      Horizontal total (chars)\n");
	printf(" disp=       .. active display\n");
	printf(" retr=       .. retrace as inequality\n");
	printf(" blnk=       .. blanking as inequality\n");
	printf(" sdtot=      .. start delay after total\n");
	printf(" sdretr=     .. start delay aft retrace\n");
	printf(" scan2x=     Scan double bit\n");
	printf(" maxscanline= Max scanline per cell or row\n");
	printf("\n");
	printf("ESC to return, ENTER/SPACE for more...\n");

	c = getch();
	if (c == 27) return;
}

static unsigned char rdump[4096];

void dump_to_file() {
	char tmpname[32];
	char nname[17];
	int c,mode,i;
	FILE *fp;

	mode = int10_getmode();
	sprintf(nname,"VGADMP%02X",mode);

	bios_cls();
	/* position the cursor to home */
	vga_moveto(0,0);
	vga_sync_bios_cursor();

	printf("Standard VGA registers and RAM will be\n");
	printf("dumped to %s. Hit ENTER to proceed.\n",nname);
	printf("\n");
	printf("\n");
	printf("\n");

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
	vga_moveto(0,5);
	vga_sync_bios_cursor();

	c = getch();
	if (c != 13) return;

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

	/* ============= RAM scan/dump ============ */
	{
		unsigned char seq4,crt17h,crt14h,pl,ogc5,ogc6,seqmask;

		ogc6 = vga_read_GC(6);
		ogc5 = vga_read_GC(5);
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
			}
		}

		vga_write_sequencer(4,seq4);
		vga_write_sequencer(0,0x01);
		vga_write_sequencer(0,0x03);
		vga_write_sequencer(VGA_SC_MAP_MASK,seqmask);
		vga_write_CRTC(0x14,crt14h);
		vga_write_CRTC(0x17,crt17h);
		vga_write_GC(5,ogc5);
		vga_write_GC(6,ogc6);
	}

	/* ======================================= */
	printf("Done\n");
	while (getch() != 13);
}

/* utility function to flash VGA color palette registers */
void flash_vga_pal() {
	unsigned char palidx,palold[3],palflash=1,w[3],redraw=1,flashwait=0,vga_mode;
	int c;

	bios_cls();
	/* position the cursor to home */
	vga_moveto(0,0);
	vga_sync_bios_cursor();

	vga_mode = int10_getmode();

	printf("Flashing VGA color palette entry.\n");
	printf("ESC to quit, use Left/Right arrow keys\n");
	printf("to select palette entry, Up/Down to\n");
	printf("change values.\n");
	printf("\n");

	palidx = 40;
	for (c=0;c < palidx;c++) {
		unsigned short b,cc;

		vga_moveto(c,5);
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
	printf("\n");
	printf("\n");

	/* 320x200x256 mode: draw all 256 colors for ref */
	if (vga_mode == 19 && vga_graphics_ram != NULL) {
		VGA_RAM_PTR draw;
		unsigned int x,y;

		for (y=0;y < 8;y++) {
			draw = vga_graphics_ram + (320 * (200 - 1 - y));
			for (x=0;x < 256;x++) *draw++ = x;
		}
	}

	palidx = 7; /* start with color 7 (the text we print) */
	vga_read_PAL(palidx,palold,1);

	while (1) {
		t8254_wait(0x10000UL); /* 9 / 18.2Hz = 0.5 second (about). no faster. don't give users seizures. */

		if (redraw) {
			printf("\x0D" "idx=0x%02x val=0x%02x%02x%02x",
				palidx,palold[0],palold[1],palold[2]);
			fflush(stdout);
			redraw = 0;
		}

		if (kbhit()) {
			c = getch();
			if (c == 27)
				break;
			else if (c == 0) {
				c = getch();
				if (c == 0x4B) { /* left */
					vga_write_PAL(palidx,palold,1);
					palidx--;
					vga_read_PAL(palidx,palold,1);
					flashwait = 9 - 1;
					palflash = 1;
					redraw = 1;
				}
				else if (c == 0x4D) { /* right */
					vga_write_PAL(palidx,palold,1);
					palidx++;
					vga_read_PAL(palidx,palold,1);
					flashwait = 9 - 1;
					palflash = 1;
					redraw = 1;
				}
				else if (c == 0x48) { /* up */
					palold[0]++;
					palold[1]++;
					palold[2]++;
					vga_write_PAL(palidx,palold,1);
					vga_read_PAL(palidx,palold,1);
					flashwait = 255 - 20;
					palflash = 0;
					redraw = 1;
				}
				else if (c == 0x50) { /* down */
					palold[0]--;
					palold[1]--;
					palold[2]--;
					vga_write_PAL(palidx,palold,1);
					vga_read_PAL(palidx,palold,1);
					flashwait = 255 - 20;
					palflash = 0;
					redraw = 1;
				}
			}
		}

		if (++flashwait == 9) {
			flashwait = 0;
			w[0] = palflash ? (palold[0] + 0x18) : palold[0];
			w[1] = palflash ? (palold[1] + 0x18) : palold[1];
			w[2] = palflash ? (palold[2] + 0x18) : palold[2];
			vga_write_PAL(palidx,w,1);
			palflash = !palflash;
		}
	}

	vga_write_PAL(palidx,palold,1);
}

/* utility function to flash Attribute Controller Palette indexes */
void flash_acp() {
	unsigned char palidx,palold,palflash=1,w,redraw=1,flashwait=0,vga_mode;
	int c;

	bios_cls();
	/* position the cursor to home */
	vga_moveto(0,0);
	vga_sync_bios_cursor();

	vga_mode = int10_getmode();

	printf("Flashing Attribute Controller entry.\n");
	printf("ESC to quit, use Left/Right arrow keys\n");
	printf("to select palette entry, Up/Down to\n");
	printf("change values.\n");
	printf("\n");

	palidx = 40;
	for (c=0;c < palidx;c++) {
		unsigned short b,cc;

		vga_moveto(c,5);
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
	printf("\n");
	printf("\n");

	/* 320x200x256 mode: draw all 256 colors for ref */
	if (vga_mode == 19 && vga_graphics_ram != NULL) {
		VGA_RAM_PTR draw;
		unsigned int x,y;

		for (y=0;y < 8;y++) {
			draw = vga_graphics_ram + (320 * (200 - 1 - y));
			for (x=0;x < 256;x++) *draw++ = x;
		}
	}

	palidx = 7; /* start with color 7 (the text we print) */
	palold = vga_read_AC(palidx);

	while (1) {
		t8254_wait(0x10000UL); /* 9 / 18.2Hz = 0.5 second (about). no faster. don't give users seizures. */

		if (redraw) {
			printf("\x0D" "idx=0x%02x val=0x%02x RGB(%u,%u,%u)",
				palidx,palold,
				((palold&4)>>1) | ((palold&32)>>5),
				 (palold&2)     | ((palold&16)>>4),
				((palold&1)<<1) | ((palold& 8)>>3));
			fflush(stdout);
			redraw = 0;
		}

		if (kbhit()) {
			c = getch();
			if (c == 27)
				break;
			else if (c == 0) {
				c = getch();
				if (c == 0x4B) { /* left */
					vga_write_AC(palidx,palold);
					vga_write_AC(palidx | VGA_AC_ENABLE,palold);
					palidx = (palidx-1)&0xF;
					palold = vga_read_AC(palidx);
					flashwait = 9 - 1;
					palflash = 1;
					redraw = 1;
				}
				else if (c == 0x4D) { /* right */
					vga_write_AC(palidx,palold);
					vga_write_AC(palidx | VGA_AC_ENABLE,palold);
					palidx = (palidx+1)&0xF;
					palold = vga_read_AC(palidx);
					flashwait = 9 - 1;
					palflash = 1;
					redraw = 1;
				}
				else if (c == 0x48) { /* up */
					palold++;
					vga_write_AC(palidx,palold);
					vga_write_AC(palidx | VGA_AC_ENABLE,palold);
					palold = vga_read_AC(palidx);
					flashwait = 255 - 20;
					palflash = 0;
					redraw = 1;
				}
				else if (c == 0x50) { /* down */
					palold--;
					vga_write_AC(palidx,palold);
					vga_write_AC(palidx | VGA_AC_ENABLE,palold);
					palold = vga_read_AC(palidx);
					flashwait = 255 - 20;
					palflash = 0;
					redraw = 1;
				}
			}
		}

		if (++flashwait == 9) {
			flashwait = 0;
			w = palflash ? (palold ^ 0x3F) : palold;
			vga_write_AC(palidx,w);
			vga_write_AC(palidx | VGA_AC_ENABLE,w);
			palflash = !palflash;
		}
	}

	vga_write_AC(palidx,palold);
	vga_write_AC(palidx | VGA_AC_ENABLE,palold);
}

int main() {
	unsigned char redraw,vga_mode;
	struct vga_mode_params mp;
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

	if (!(vga_flags & VGA_IS_VGA)) {
		printf("Modesetting + readback of CRTC registers is only supported on VGA\n");
		return 1;
	}

	redraw = 1;
	while (1) {
		update_state_from_vga();

		if (redraw) {
			unsigned long rate;
			double hrate,vrate;
			unsigned char gfx_misc;

			redraw = 0;
			vga_mode = int10_getmode();
			vga_read_crtc_mode(&mp);

			rate = vga_clock_rates[mp.clock_select];
			if (mp.clock_div2) rate >>= 1;
			hrate = (double)rate / ((mp.clock9 ? 9 : 8) * mp.horizontal_total);
			vrate = hrate / mp.vertical_total;

			bios_cls();
			/* position the cursor to home */
			vga_moveto(0,0);
			vga_sync_bios_cursor();

			/* also read GC misc */
			gfx_misc = vga_read_GC(6);

			printf("VGA=%u EGA=%u CGA=%u MDA=%u MCGA=%u HGC=%u(%u)\n",
					(vga_flags & VGA_IS_VGA) ? 1 : 0,
					(vga_flags & VGA_IS_EGA) ? 1 : 0,
					(vga_flags & VGA_IS_CGA) ? 1 : 0,
					(vga_flags & VGA_IS_MDA) ? 1 : 0,
					(vga_flags & VGA_IS_MCGA) ? 1 : 0,
					(vga_flags & VGA_IS_HGC) ? 1 : 0,vga_hgc_type);
			printf("Tandy/PCjr=%u Amstrad=%u IO=%03xh ALPHA=%u\n",
					(vga_flags & VGA_IS_TANDY) ? 1 : 0,
					(vga_flags & VGA_IS_AMSTRAD) ? 1 : 0,
					vga_base_3x0,
					vga_alpha_mode);

			printf("RAM 0x%05lx-0x%05lx 9W=%u INT10=%u(0x%02x)\n",
					(unsigned long)vga_ram_base,
					(unsigned long)vga_ram_base+vga_ram_size-1UL,
					vga_9wide,vga_mode,vga_mode);

			printf("Clock=%u div2=%u (%luHZ) pix/clk=%u\n",
					mp.clock_select,
					mp.clock_div2,
					vga_clock_rates[mp.clock_select] >> (mp.clock_div2 ? 1 : 0),
					mp.clock9 ? 9 : 8);

			printf("Word=%u DWord=%u Hsync%c Vsync%c mem4=%u\n",
					mp.word_mode,
					mp.dword_mode,
					mp.hsync_neg?'-':'+',
					mp.vsync_neg?'-':'+',
					mp.inc_mem_addr_only_every_4th);

			printf("SLR=%u S4=%u memdv2=%u scandv2=%u awrap=%u\n",
					mp.shift_load_rate,
					mp.shift4_enable,
					mp.memaddr_div2,
					mp.scanline_div2,
					mp.address_wrap_select);

			printf("map14=%u map13=%u ref/scanline=%u o/e=%u\n",
					mp.map14,
					mp.map13,
					mp.refresh_cycles_per_scanline,
					(gfx_misc&2)?1:0);

			printf("hrate=%.3fKHz vrate=%.3fHz\n",
					hrate / 1000,
					vrate);

			printf("V:tot=%u disp=%u retr=%u <=x< %u\n",
					mp.vertical_total,
					mp.vertical_display_end,
					mp.vertical_start_retrace,
					mp.vertical_end_retrace);

			printf(" blnk=%u <=x< %u alpha=%u\n",
					mp.vertical_blank_start,
					mp.vertical_blank_end,
					((gfx_misc&1)^1)?1:0); /* the bit is called "alpha disable" */

			printf("H:tot=%u disp=%u retr=%u <=x< %u\n",
					mp.horizontal_total,
					mp.horizontal_display_end,
					mp.horizontal_start_retrace,
					mp.horizontal_end_retrace);

			printf(" blnk=%u <=x< %u sdtot=%u sdretr=%u\n",
					mp.horizontal_blank_start,
					mp.horizontal_blank_end,
					mp.horizontal_start_delay_after_total,
					mp.horizontal_start_delay_after_retrace);

			printf("scan2x=%u maxscanline=%u offset=%u\n",
					mp.scan_double,
					mp.max_scanline,
					mp.offset);

			printf("\n");

			printf("ESC  Exit to DOS       ? Explain this\n");
			printf("m    Set mode          c Change clock\n");
			printf("9    Toggle 8/9        w Word/Dword\n");
			printf("-    H/Vsync polarity  $ mem4 toggle\n");
			printf("4    toggle shift4     5 toggle SLR\n");
			printf("2    toggle more...    M max scanline\n");
			printf("o    offset register   x Mode-X\n");
			printf("z    palette tinkering d Dump regs\n");
			printf("h    panning/hpel\n");
		}

		c = getch();
		if (c == 27) break;
		else if (c == 13) {
			redraw = 1;
		}
		else if (c == '=') {
			vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			redraw = 1;
		}
		else if (c == 'h') {
			unsigned int nm;

			bios_cls();
			/* position the cursor to home */
			vga_moveto(0,0);
			vga_sync_bios_cursor();

			printf("\n");
			printf(" o   Play with offset register (CRTC)\n");
			printf(" p   Play with hpel register (AC)\n");

			c = getch();
			if (c == 'o') {
				printf("\n");
				printf("New offset (can be decimal or hexadecimal)? "); fflush(stdout);
				nm = common_prompt_number();
				vga_set_start_location(nm);
			}
			else if (c == 'p') {
				printf("\n");
				printf("New hpel? "); fflush(stdout);
				nm = common_prompt_number();
				if (nm <= 255) vga_write_AC(0x13 | VGA_AC_ENABLE,nm);
			}

			redraw = 1;
		}
		else if (c == 'd') {
			bios_cls();
			/* position the cursor to home */
			vga_moveto(0,0);
			vga_sync_bios_cursor();

			printf("\n");
			printf(" f   Dump VGA state to file\n");

			c = getch();
			if (c == 'f')
				dump_to_file();

			redraw = 1;
		}
		else if (c == 'z') {
			unsigned int nm;

			bios_cls();
			/* position the cursor to home */
			vga_moveto(0,0);
			vga_sync_bios_cursor();

			printf("\n");
			printf(" a   Flash attrib control palette\n");
			printf(" p   Flash VGA color palette\n");
			printf(" o   Change overscan color\n");

			c = getch();
			if (c == 'a')
				flash_acp();
			else if (c == 'p')
				flash_vga_pal();
			else if (c == 'o') {
				printf("\n");
				printf("New color code (hex or dec)? "); fflush(stdout);
				nm = common_prompt_number();
				if (nm <= 255) vga_write_AC(0x11 | VGA_AC_ENABLE,nm);
			}

			redraw = 1;
		}
		else if (c == 'x') {
			bios_cls();
			/* position the cursor to home */
			vga_moveto(0,0);
			vga_sync_bios_cursor();

			printf("\n");
			printf(" c   CRTC configuration\n");
			printf(" s   Sequencer configuration\n");
			printf(" C   CRTC anti-configuration\n");
			printf(" S   Sequencer anti-configuration\n");

			c = getch();
			if (c == 'c') {
				/* CRTC-side configuration of Mode-X */
				mp.word_mode = 0;
				mp.dword_mode = 0;
				mp.address_wrap_select = 1;
				mp.inc_mem_addr_only_every_4th = 0;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == 'C') {
				mp.word_mode = 1;
				mp.dword_mode = 1;
				mp.address_wrap_select = 1;
				mp.inc_mem_addr_only_every_4th = 0;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == 's') {
				/* CRTC-side configuration of Mode-X */
				vga_write_sequencer(0,0x01);
				vga_write_sequencer(4,0x06);
				vga_write_sequencer(0,0x03);
				vga_write_sequencer(VGA_SC_MAP_MASK,0xF);
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == 'S') {
				vga_write_sequencer(0,0x01);
				vga_write_sequencer(4,0x0E);
				vga_write_sequencer(0,0x03);
				vga_write_sequencer(VGA_SC_MAP_MASK,0xF);
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}

			redraw = 1;
		}
		else if (c == 'o') {
			unsigned int nm;

			printf("\n");
			printf("New value? "); fflush(stdout);
			nm = common_prompt_number();
			if (nm <= 255) {
				mp.offset = nm;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			redraw = 1;
		}
		else if (c == 'M') {
			unsigned int nm;

			printf("\n");
			printf("New value? "); fflush(stdout);
			nm = common_prompt_number();
			if (nm > 0 && nm <= 32) {
				mp.max_scanline = nm;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			redraw = 1;
		}
		else if (c == '2') {
			bios_cls();
			/* position the cursor to home */
			vga_moveto(0,0);
			vga_sync_bios_cursor();

			printf("\n");
			printf(" m   toggle memaddr div2\n");
			printf(" s   toggle scanline div2\n");
			printf(" a   toggle address wrap select\n");
			printf(" r   toggle refresh cycles/scan\n");
			printf(" 3   toggle map13\n");
			printf(" 4   toggle map14\n");
			printf(" d   toggle scan double\n");
			printf(" o   toggle odd/even\n");
			printf(" p   toggle alpha disable\n");

			c = getch();
			if (c == 'd') {
				mp.scan_double = !mp.scan_double;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == 'o') {
				vga_write_GC(6,vga_read_GC(6)^2);
			}
			else if (c == 'p') {
				vga_write_GC(6,vga_read_GC(6)^1);
			}
			else if (c == 'm') {
				mp.memaddr_div2 = !mp.memaddr_div2;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == 's') {
				mp.scanline_div2 = !mp.scanline_div2;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == 'a') {
				mp.address_wrap_select = !mp.address_wrap_select;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == 'r') {
				mp.refresh_cycles_per_scanline = !mp.refresh_cycles_per_scanline;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == '3') {
				mp.map13 = !mp.map13;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}
			else if (c == '4') {
				mp.map14 = !mp.map14;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			}

			redraw = 1;
		}
		else if (c == '4') {
			mp.shift4_enable = !mp.shift4_enable;
			vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			redraw = 1;
		}
		else if (c == '5') {
			mp.shift_load_rate = !mp.shift_load_rate;
			vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			redraw = 1;
		}
		else if (c == '?') {
			redraw = 1;
			help_main();
		}
		else if (c == '$') {
			mp.inc_mem_addr_only_every_4th = !mp.inc_mem_addr_only_every_4th;
			vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			redraw = 1;
		}
		else if (c == '-') {
			unsigned int nm;

			printf("\n");
			printf(" 0: h + v +\n");
			printf(" 1: h - v +\n");
			printf(" 2: h + v -\n");
			printf(" 3: h - v -\n");
			printf("Mode? "); fflush(stdout);
			nm = common_prompt_number();
			if (nm != ~0 && nm < 4) {
				mp.hsync_neg = (nm&1);
				mp.vsync_neg = (nm&2)?1:0;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
				redraw = 1;
			}
		}
		else if (c == '9') {
			mp.clock9 = !mp.clock9;
			vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
			redraw = 1;
		}
		else if (c == 'm') {
			unsigned int nm;

			printf("\n");
			printf("Mode? "); fflush(stdout);
			nm = common_prompt_number();
			if (nm != (~0U)) {
				if (nm < 0x100) {
					int10_setmode(nm);
				}
				else {
					uint16_t a;

					a = nm;
					/* VESA BIOS extensions or no, try to set a mode */
					__asm {
						push	ax
						push	bx
						mov	ax,0x4F02
						mov	bx,a
						int	0x10
						pop	bx
						pop	ax
					}
				}
			}
			redraw = 1;
		}
		else if (c == 'c') {
			unsigned int nm;

			printf("\n");
			for (nm=0;nm < 4;nm++) printf(" [%u] = %-8luHz   [%u] = %luHz\n",
				nm,vga_clock_rates[nm],
				nm+4,vga_clock_rates[nm]>>1);
			printf("Clock? "); fflush(stdout);
			nm = common_prompt_number();
			if (nm != ~0 && nm < 8) {
				mp.clock_select = nm&3;
				mp.clock_div2 = (nm&4)?1:0;
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
				redraw = 1;
			}
		}
		else if (c == 'w') {
			unsigned int nm;

			printf("\n");
			printf(" 0: byte (w=0 d=0)\n");
			printf(" 1: word (w=1 d=0)\n");
			printf(" 2: dword (w=1 d=1)\n");
			printf(" 3: dword !word (w=0 d=1)\n");
			printf("Mode? "); fflush(stdout);
			nm = common_prompt_number();
			if (nm != ~0 && nm < 4) {
				mp.dword_mode = (nm&2)?1:0;
				mp.word_mode = (nm&1)^mp.dword_mode;	/* 0 1 2 3 > 00 01 11 10 */
				vga_write_crtc_mode(&mp,VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC);
				redraw = 1;
			}
		}
	}

	return 0;
}

