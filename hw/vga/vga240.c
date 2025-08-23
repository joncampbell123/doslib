
/* TODO: Add two options
 *
 *       1) runtime enable/disable intercept (if you don't want to force to 480 anymore)
 *
 *       2) disable overscan options
 *
 *          I'm noticing that the PEXHDCAP card is mis-detecting the 8-pixel overscan
 *          as part of the active picture, so what I get is a capture that's missing
 *          8 pixels from the right side and capturing 8 pixels of black on the left
 *          (or depending on the DOS game/demo, whatever color was programmed to be
 *          the overscan color).
 *
 *          So what would be useful is if the user could have us disable the overscan
 *          border entirely, or disable overscan on the sides only, or top/bottom only,
 *          or just leave it alone--whatever the user wants.
 *
 *          I'm guessing modern VGA capture cards like the PEXHDCAP are made for Windows
 *          desktops only where overscan color is never used, and that's why DOS overscan
 *          colors confuse it a bit like that.
 *
 *          On the S3 Virge PCI card I have, even for parts of the picture that are
 *          pure black, the card seems to bias the R,G,B output a bit including overscan
 *          and you can clearly see on the output a dark gray where active picture area
 *          and overscan are rendered, which might have something to do with the shifted
 *          image on capture as well.
 *
 * TODO: Reduce memory footprint. 32KB is a bit much, and it crowds out some of the
 *       stuff you want to capture. */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/dos/dos.h>

unsigned short      my_psp = 0;
unsigned char		force89 = 0; /* 0=dont force    8=force 8    9=force 9 */
unsigned char		blanking_fix = 1;
int			blanking_align = 0;
void			(__interrupt __far *old_int10)();

void reinit_int10_mode();

void __interrupt __far new_int10(union INTPACK ip) {
	if (ip.w.ax == 0xDFA0 && ip.w.bx == 0x1AC0) {
		switch (ip.h.cl) {
			case 0: /* install check */
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
				break;
			case 1: /* change force89 option */
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
				force89 = ip.h.ch;
				break;
			case 2: /* change blanking fix option */
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
				blanking_fix = ip.h.ch;
				break;
			case 3: /* change blanking align option */
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
				if (ip.h.ch & 0x80)
					blanking_align = -1;
				else
					blanking_align = ip.h.ch;
				break;
            case 0xFF: /* unhook INT 10h */
                _cli();
                _dos_setvect(0x10,old_int10);
                old_int10 = NULL;
                _sti();
                reinit_int10_mode();
				ip.w.ax = 0xDFAA;
				ip.w.bx = 0xCACA;
                ip.w.cx = my_psp;
                break;
			default:
				ip.w.ax = 0xDFFF;
				ip.w.bx = 0xCACA;
				break;
		};

		return;
	}
	else if (ip.h.ah == 0x00) {
		unsigned int vtotal = 0x20B; /* NTS: two less than actual scanlines */
		unsigned int vretrace_s = 0x1EA;
		unsigned int vretrace_e = 0x1EC;
		unsigned int vdisplay_e = 0x1E0; /* NTS: this value is the display end, actual register value is - 1 */
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

		if ((ip.h.al&0x7F) <= 3 || (ip.h.al&0x7F) == 7) {
			/* force 8-pixel */
			if (force89 == 8) {
				outp(0x3C4,0x01); /* seq, clocking mode reg */
				t = inp(0x3C5);
				outp(0x3C5,t|0x01); /* select 8 dots/char */

				t = inp(0x3CC); /* then select 25MHz clock */
				outp(0x3C2,t&(~0xC));
			}
			/* force 9-pixel */
			else if (force89 == 9) {
				outp(0x3C4,0x01); /* seq, clocking mode reg */
				t = inp(0x3C5);
				outp(0x3C5,t&0xFE); /* select 9 dots/char */

				t = inp(0x3CC); /* then select 28MHz clock */
				outp(0x3C2,(t&(~0xC)) | (1 << 2));
			}
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
			lines++; /* display reg is value - 1 */

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

		/* vertical display end is actually value - 1 */
		vdisplay_e--;

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
	printf("VGA240 [options]\n");
	printf("\n");
	printf("Intercept VGA BIOS calls to force video modes with at least 480 scan lines,\n");
	printf("to improve scan converter output and allow capture/viewing with flat panel or\n");
	printf("VGA capture cards that do not support 350/400-line DOS modes.\n");
	printf("\n");
	printf("If the program is resident, you can run this program with other options to\n");
	printf("change configuration at runtime along with /SET\n");
	printf("\n");
	printf("(C) 2014-2016 Jonathan Campbell\n");
	printf("\n");
	printf("  /INSTALL           Make the program resident in memory.\n");
    printf("  /UNINSTALL         Remove program from memory.\n");
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
        push    cx
		mov	ax,0xDFA0
		mov	bx,0x1AC0
        xor cx,cx               ; CL = 0 install check
		int	10h
		mov	a,ax
		mov	b,bx
        pop cx
		pop	bx
		pop	ax
	}

	return (a == 0xDFAAU && b == 0xCACAU);
}

int unhook(unsigned short far *res_psp) {
	unsigned int a=0,b=0,c=0;

	__asm {
		push	ax
		push	bx
        push    cx
		mov	ax,0xDFA0
		mov	bx,0x1AC0
        mov cx,0x00FF           ; CL = 0xFF unhook INT 10h
		int	10h
		mov	a,ax
		mov	b,bx
        mov c,cx
        pop cx
		pop	bx
		pop	ax
	}

    *res_psp = c;
	return (a == 0xDFAAU && b == 0xCACAU);
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

void reinit_int10_mode() {
    unsigned short cur_cx=0,cur_dx=0;

    /* save cursor position.
     * INT 10h resets cursor to home even if not clearing screen (DOSBox-X behavior too). */
    __asm {
            push    ax
            push    bx
            push    cx
            push    dx

            mov     ah,0x03     ; get cursor position and size
            xor     bx,bx       ; page 0
            int     10h
            mov     cur_cx,cx   ; start/end scanline
            mov     cur_dx,dx   ; row/column position

            pop     dx
            pop     cx
            pop     bx
            pop     ax
    }

    /* NTS: Get the current video mode, then set the mode we just got with bit 7 set to NOT clear contents.
     *      This preserves the screen while forcing 60Hz, so that danooct1 no longer has to explain what
     *      "INT 10h hooked" means (ref https://www.youtube.com/watch?v=Okeyy5jIi9Y) */
    __asm {
            push    ax
            push    bx

            mov     ah,0x0F     ; get video mode
            int     10h

            xor     ah,ah
            xor     bx,bx
            or      al,0x80     ; set bit 7 to tell BIOS not to clear screen contents
            int	    10h         ; AH=0 set video mode AL=mode | 0x80

            pop     bx
            pop	    ax
    }

    /* restore cursor position */
    __asm {
            push    ax
            push    bx
            push    cx
            push    dx

            mov     ah,0x01     ; set cursor shape
            xor     bx,bx       ; page 0
            mov     cx,cur_cx   ; start/end scanline
            int     10h

            mov     ah,0x02     ; set cursor position
            xor     bx,bx       ; page 0
            mov     dx,cur_dx   ; row/column position
            int     10h

            pop     dx
            pop     cx
            pop     bx
            pop     ax
    }

}

int main(int argc,char **argv) {
	unsigned char force89_set=0;
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
			else if (!strcasecmp(a,"u") || !strcasecmp(a,"uninstall")) {
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
				force89_set = 1;
				force89 = 8;
			}
			else if (!strcmp(a,"9")) {
				force89_set = 1;
				force89 = 9;
			}
			else if (!strcmp(a,"N8") || !strcmp(a,"N9")) {
				force89_set = 1;
				force89 = 0;
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
        unsigned short resident_size = 0;

		if (resident()) {
			printf("This program is already resident\n");
			return 1;
		}

        /* auto-detect the size of this EXE by the MCB preceeding the PSP segment */
        /* do it BEFORE hooking in case shit goes wrong, then we can safely abort. */
        /* the purpose of this code is to compensate for Watcom C's lack of useful */
        /* info at runtime or compile time as to how large we are in memory. */
        {
            unsigned short env_seg=0;
            unsigned short psp_seg=0;
            unsigned char far *mcb;

            __asm {
                push    ax
                push    bx
                mov     ah,0x51     ; get PSP segment
                int     21h
                mov     psp_seg,bx
                pop     bx
                pop     ax
            }

            mcb = MK_FP(psp_seg-1,0);

            /* sanity check */
            if (!(*mcb == 'M' || *mcb == 'Z')/*looks like MCB*/ ||
                *((unsigned short far*)(mcb+1)) != psp_seg/*it's MY memory block*/) {
                printf("Can't figure out my resident size, aborting\n");
                return 1;
            }

            my_psp = psp_seg;
            resident_size = *((unsigned short far*)(mcb+3)); /* number of paragraphs */
            if (resident_size < 17) {
                printf("Resident size is too small, aborting\n");
                return 1;
            }

            /* while we're at it, free our environment block as well, we don't need it */
            env_seg = *((unsigned short far*)MK_FP(psp_seg,0x2C));
            if (env_seg != 0) {
                if (_dos_freemem(env_seg) == 0) {
                    *((unsigned short far*)MK_FP(psp_seg,0x2C)) = 0;
                }
                else {
                    printf("WARNING: Unable to free environment block\n");
                }
            }
        }

		_cli();
		old_int10 = _dos_getvect(0x10);
		_dos_setvect(0x10,new_int10);
		_sti();
        reinit_int10_mode();

        printf("INT 10h hooked [VGA240.EXE]. Video modes will be forced to 60Hz.\n");
        printf("Resident size %uKB (%u paragraphs)\n",(resident_size + 0x3FU) >> 6U/*10-4*/,resident_size);

		_dos_keep(0,resident_size);
	}
    else if (tolower(*command) == 'u') {
        unsigned char free_ok=0;
        unsigned short my_psp=0;
        unsigned short res_psp;

        if (!resident()) {
            printf("This program is not resident\n");
            return 1;
        }
        if (!unhook(&res_psp)) {
            printf("Failed to unhook resident portion\n");
            return 1;
        }

        if (res_psp == 0) {
            printf("Unhooked, but unable to locate resident image\n");
            return 1;
        }

        /* ask DOS to free it.
         * temporarily switch current PSP to whatever was given, so we have the permissions */
        __asm {
            push    ax
            push    bx
            push    cx
            push    dx
            push    es

            mov     ah,0x51 ; get current PSP segment (who am I)
            int     21h
            mov     my_psp,bx

            mov     ah,0x50 ; set current PSP segment
            mov     bx,res_psp
            int     21h

            mov     ah,0x49 ; free memory
            mov     es,res_psp ; free resident PSP segment
            int     21h
            jc      freed_not_ok
            mov     free_ok,1
freed_not_ok:

            mov     ah,0x50 ; set current PSP segment
            mov     bx,my_psp
            int     21h

            pop     es
            pop     dx
            pop     cx
            pop     bx
            pop     ax
        }

        if (!free_ok) {
            printf("INT 10h unhooked, but unable to remove resident portion.\n");
            return 1;
        }
    }
	else if (tolower(*command) == 's') {
		if (!resident()) {
			printf("This program is not yet installed\n");
			return 1;
		}

		if (force89_set) {
			if (!res_set_opt8(1,force89)) {
				printf("Failed to set force89\n");
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
			if (blanking_align < 0) blanking_align = 0xFF;
			if (!res_set_opt8(3,(unsigned char)blanking_align)) {
				printf("Failed to set blank align\n");
				return 1;
			}
		}

        reinit_int10_mode();
	}

	return 0;
}

