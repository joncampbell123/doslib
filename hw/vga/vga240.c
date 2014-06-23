
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
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

unsigned char		force8 = 0;
unsigned char		blanking_fix = 1;
int			blanking_align = 0;
void			(__interrupt __far *old_int10)();

void __interrupt __far new_int10(union INTPACK ip) {
	if (ip.w.ax == 0xDFA0 && ip.w.bx == 0x1AC0) {
		switch (ip.h.cl) {
			case 0: /* install check */
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
				break;
			case 1: /* change force8 option */
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
				force8 = ip.h.ch;
				break;
			case 2: /* change blanking fix option */
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
				blanking_fix = ip.h.ch;
				break;
			case 3: /* change blanking align option */
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
				blanking_align = (int)((char)ip.h.ch); /* sign extend 0xFF,0x00,0x01 to -1,0,1 */
				break;
			default:
				ip.w.ax = 0xDFFF;
				ip.w.bx = 0xCACA;
				break;
		};

		return;
	}
	else if (ip.h.ah == 0x00) {
		unsigned int vtotal = 0x20B; /* NTS: one less than actual scanlines */
		unsigned int vretrace_s = 0x1EA;
		unsigned int vretrace_e = 0x1EC;
		unsigned int vdisplay_e = 0x1E0;
		unsigned int vblank_s = 0x1E8;
		unsigned int vblank_e = 0x204;
		unsigned int mode = ip.h.al;
		unsigned char t,ov=0;
		unsigned int port;

		__asm {
			push	ds
			mov	ax,seg old_int10
			mov	ds,ax
			mov	ax,mode
			pushf
			callf	dword ptr [old_int10]
			pop	ds
		}

		/* mono or color? */
		port = (inp(0x3CC)&1)?0x3D4:0x3B4;

		/* force 8-pixel */
		if (force8 && (ip.h.al <= 3 || ip.h.al == 7)) {
			outp(0x3C4,0x01); /* seq, clocking mode reg */
			t = inp(0x3C5);
			outp(0x3C5,t|0x01); /* select 8 dots/char */

			t = inp(0x3CC); /* then select 25MHz clock */
			outp(0x3C2,t&(~0xC));
		}

		if (blanking_fix) {
			unsigned int lines;
			unsigned int extra;

			/* read from the card how many active display lines there are */
			outp(port,0x12); /* display end */
			lines = inp(port+1); /* bits 0-7 */
			outp(port,0x07); /* overflow */
			t = inp(port+1);
			lines += (t&0x02)?0x100:0x000;
			lines += (t&0x40)?0x200:0x000;

			/* how many extra lines will there be? */
			if (lines < vdisplay_e)
				extra = vdisplay_e - lines;
			else
				extra = 0;

			/* now use that count to reprogram the blanking area and display params.
			 * use the extra line count to adjust vertical retrace downward so the
			 * shortened image is centered. if we do not do this, then the image is
			 * "stuck" to the bottom of the screen with black at the top. */
			vdisplay_e = lines;
			vblank_s = lines + 8;
			if (blanking_align < 0) {
				/* align image to top */
				vretrace_s = lines + 10 + extra;
				vretrace_e = lines + 12 + extra;
			}
			else if (blanking_align == 0) {
				/* align to center */
				vretrace_s = lines + 10 + (extra/2);
				vretrace_e = lines + 12 + (extra/2);
			}
			else {
				/* align to bottom */
				vretrace_s = lines + 10;
				vretrace_e = lines + 12;
			}
		}

		/* reprogram the CRTC back to a 480 line mode */
		outp(port,0x11); /* retrace end (also clear protect bit, and preserve whatever the "bandwidth" bit contains) */
		t = inp(port+1);
		outp(port+1,(t&0x70)+(vretrace_e&0xF));

		outp(port,0x17);
		t = inp(port+1);
		outp(port+1,t&0x7F);

		/* need to make sure line compare is beyond the end of picture */
		ov |= 0x10; /* bit 8 of line compare */

		outp(port,0x06); /* vertical total */
		outp(port+1,vtotal);
		ov |= (vtotal & 0x100) >> 8;
		ov |= ((vtotal & 0x200) >> ((8+1) - 5));

		outp(port,0x10); /* retrace start */
		outp(port+1,vretrace_s);
		ov |= ((vretrace_s & 0x100) >> (8 - 2));
		ov |= ((vretrace_s & 0x200) >> ((8+1) - 7));

		outp(port,0x12); /* display end */
		outp(port+1,vdisplay_e);
		ov |= ((vdisplay_e & 0x100) >> (8 - 1));
		ov |= ((vdisplay_e & 0x200) >> ((8+1) - 6));

		outp(port,0x15); /* blank start */
		outp(port+1,vblank_s);
		ov |= ((vblank_s & 0x100) >> (8 - 3));
		outp(port,0x09);
		t = inp(port+1);
		outp(port+1,(t&0x9F)+(vblank_s&0x200?0x20:0x00)+0x40/*make sure bit 9 of line compare is set*/);

		outp(port,0x16); /* blank end */
		outp(port+1,vblank_e);

		outp(port,0x07); /* overflow */
		outp(port+1,ov);

		outp(port,0x17);
		t = inp(port+1);
		outp(port+1,t|0x80);

		outp(port,0x11); /* retrace end (also clear protect bit, and preserve whatever the "bandwidth" bit contains) */
		t = inp(port+1);
		outp(port+1,t|0x80);

		return;
	}

	_chain_intr(old_int10);
}

static void help() {
	printf("VGA240 /INSTALL [/NOBLANKFIX]\n");
	printf("\n");
	printf("Intercept VGA BIOS calls to force video modes with at least 480 scan lines,\n");
	printf("which may improve video quality with scan converters or enable capture/viewing\n");
	printf("where devices would normally ignore 400-line output.\n");
	printf("\n");
	printf("If the program is resident, you can run this program with other options to\n");
	printf("change configuration at runtime along with /SET\n");
	printf("\n");
	printf("(C) 2014 Jonathan Campbell\n");
	printf("\n");
	printf("  /INSTALL           Make the program resident in memory.\n");
	printf("  /SET               If the program is resident, update settings.\n");
	printf("  /BLANKFIX          Active display is maintained from original mode,\n");
	printf("                     blanking interval is extended to make 480 lines.\n");
	printf("  /NOBLANKFIX        Active display is extended to 480 lines\n");
	printf("  /BA=N              Adjust vertical retrace to center display area, where\n");
	printf("                     N is -1 (top), 0 (center), 1 (bottom)\n");
	printf("  /8                 Force alphanumeric modes to 8 pixels/char. Use this\n");
	printf("                     option if your VGA scan/capture misdetects the output\n");
	printf("                     as 640x480 instead of 720x480\n");
	printf("  /N8                Don't force to 8 pixels/char\n");
}

int resident() {
	unsigned int a=0,b=0;

	__asm {
		push	ax
		push	bx
		mov	ax,0xDFA0
		mov	bx,0x1AC0
		int	10h
		mov	a,ax
		mov	b,bx
		pop	bx
		pop	ax
	}

	return (a == 0xDFAA && b == 0xCACA);
}

int res_set_opt8(unsigned char func,unsigned char opt) {
	unsigned int a=0,b=0;

	__asm {
		push	ax
		push	bx
		push	cx
		mov	ax,0xDFA0
		mov	bx,0x1AC0
		mov	cl,func
		mov	ch,opt
		int	10h
		mov	a,ax
		mov	b,bx
		pop	cx
		pop	bx
		pop	ax
	}

	return (a == 0xDFAA && b == 0xCACA);
}

void end_of_resident();

int main(int argc,char **argv) {
	unsigned char force8_set=0;
	unsigned char blanking_fix_set=0;
	unsigned char blanking_align_set=0;
	char *a,*command=NULL;
	int i;

#if TARGET_MSDOS != 16
# error you are not supposed to compile this for protected mode!
#endif

	for (i=1;i < argc;) {
		a = argv[i++];
		if (*a == '/' || *a == '-') {
			do { a++; } while (*a == '/' || *a == '-');

			if (!strcasecmp(a,"i") || !strcasecmp(a,"install")) {
				command = a;
			}
			else if (!strcasecmp(a,"s") || !strcasecmp(a,"set")) {
				command = a;
			}
			else if (!strcasecmp(a,"BLANKFIX")) {
				blanking_fix_set=1;
				blanking_fix = 1;
			}
			else if (!strcasecmp(a,"NOBLANKFIX")) {
				blanking_fix_set=1;
				blanking_fix = 0;
			}
			else if (!strncasecmp(a,"BA=",3)) {
				a += 3;
				blanking_align = atoi(a);
				blanking_align_set = 1;
			}
			else if (!strcmp(a,"8")) {
				force8_set = 1;
				force8 = 1;
			}
			else if (!strcmp(a,"N8")) {
				force8_set = 1;
				force8 = 0;
			}
			else {
				fprintf(stderr,"Unknown switch %s\n",a);
				command = NULL;
				break;
			}
		}
		else {
			fprintf(stderr,"Unknown switch %s\n",a);
			command = NULL;
			break;
		}
	}

	if (command == NULL) {
		help();
		return 1;
	}

	if (tolower(*command) == 'i') {
		if (resident()) {
			printf("This program is already resident\n");
			return 1;
		}

		if (!probe_vga()) {
			printf("VGA BIOS not found\n");
			return 1;
		}

		_cli();
		old_int10 = _dos_getvect(0x10);
		_dos_setvect(0x10,new_int10);
		_sti();
		__asm {
			push	ax
			mov	ax,3
			int	10h
			pop	ax
		}
		printf("INT 10h hooked\n"); fflush(stdout);
		_dos_keep(0,(34000)>>4); /* FIXME! */
	}
	else if (tolower(*command) == 's') {
		if (!resident()) {
			printf("This program is not yet installed\n");
			return 1;
		}

		if (force8_set) {
			if (!res_set_opt8(1,force8)) {
				printf("Failed to set force8\n");
				return 1;
			}
		}
		if (blanking_fix_set) {
			if (!res_set_opt8(2,blanking_fix)) {
				printf("Failed to set blank fix\n");
				return 1;
			}
		}
		if (blanking_align_set) {
			if (!res_set_opt8(3,(unsigned char)blanking_align)) {
				printf("Failed to set blank align\n");
				return 1;
			}

		}

		__asm {
			push	ax
			mov	ax,3
			int	10h
			pop	ax
		}
	}

	return 0;
}

void end_of_resident() {
}

