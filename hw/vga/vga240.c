
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

unsigned char		blanking_fix = 1;
void			(__interrupt __far *old_int10)();

void __interrupt __far new_int10(union INTPACK ip) {
	if (ip.h.ah == 0x00) {
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

		if (blanking_fix) {
			unsigned int lines;

			/* read from the card how many active display lines there are */
			outp(port,0x12); /* display end */
			lines = inp(port+1); /* bits 0-7 */
			outp(port,0x07); /* overflow */
			t = inp(port+1);
			lines += (t&0x02)?0x100:0x000;
			lines += (t&0x40)?0x200:0x000;

			/* now use that count to reprogram the blanking area and display params */
			vdisplay_e = lines;
			vblank_s = lines + 8;
			vretrace_s = lines + 10;
			vretrace_e = lines + 12;
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
	printf("(C) 2014 Jonathan Campbell\n");
	printf("\n");
	printf("  /NOBLANKFIX        If given, active display is extended, which may reveal\n");
	printf("                     offscreen rendering or random data.\n");
}

void end_of_resident();

int main(int argc,char **argv) {
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
			else if (!strcasecmp(a,"NOBLANKFIX")) {
				blanking_fix = 0;
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
	return 0;
}

void end_of_resident() {
}

