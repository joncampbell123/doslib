
/* TODO: Make the 320x200x256-color override hack optional (and off by default).
 *       
 *       On an S3 Virge PCI card I own, substituting a VESA BIOS mode for mode
 *       13h causes additional compatability problems because things like
 *       horizontal pel and other standard VGA tricks don't work at all.
 *       Considering you just finished the Standard VGA version of this hack,
 *       the mode 13h hack is no longer necessary and it should be disabled by
 *       default though available as an option. */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
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
#include <hw/vesa/vesa.h>

struct mode_remap {
	uint16_t	source_mode;
	uint16_t	target_mode;
};

#define MAX_REMAP	256

unsigned short      my_psp = 0;
void			(__interrupt __far *old_int10)();
struct mode_remap	remap[MAX_REMAP];
int			remap_count=0;
union REGS		remap_rr;

void __interrupt __far new_int10(union INTPACK ip) {
	if (ip.h.ah == 0x00) {
		unsigned int i,j;

		for (i=0;i < remap_count;i++) {
			if (remap[i].source_mode == (ip.h.al&0x7F)) {
				j = remap[i].target_mode | ((ip.h.al&0x80)?0x8000:0x0000);
				__asm {
					push	ds
					mov	ax,seg old_int10
					mov	ds,ax
					mov	ax,0x4F02
					mov	bx,j
					pushf
					callf	dword ptr [old_int10]
					pop	ds
				}

				return;
			}
		}
	}
	else if (ip.w.ax == 0x4F02) {
		unsigned int i;

		for (i=0;i < remap_count;i++) {
			if (remap[i].source_mode == (ip.w.bx&0x3FFF)) {
				ip.w.bx = remap[i].target_mode | (ip.w.bx & 0xC000);
				break;
			}
		}
	}

	_chain_intr(old_int10);
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

static void help() {
	printf("VESA240 /INSTALL\n");
	printf("\n");
	printf("Intercept VESA BIOS calls to force video modes with at least 480 scan lines,\n");
	printf("which may improve video quality with scan converters or enable capture/viewing\n");
	printf("where devices would normally ignore 400-line output.\n");
	printf("(C) 2014-2016 Jonathan Campbell\n");
}

void install_320x200x256_remap() {
	struct vbe_mode_info nmi;
	uint16_t nmode,tnmode;
	int nentry;

	nmode=0;
	for (nentry=0;nentry < 1024;nentry++) {
		tnmode = vbe_read_mode_entry(vbe_info->video_mode_ptr,nentry);
		if (tnmode == 0xFFFF) break;
		memset(&nmi,0,sizeof(nmi));
		if (!vbe_read_mode_info(tnmode,&nmi)) continue;
		if (!(nmi.mode_attributes & VESA_MODE_ATTR_HW_SUPPORTED)) continue;
		if (!(nmi.mode_attributes & VESA_MODE_ATTR_GRAPHICS_MODE)) continue;
		if (nmi.x_resolution != 320) continue;
		if (nmi.y_resolution != 240) continue;
		if (nmi.number_of_planes != 1) continue;
		if (nmi.bits_per_pixel != 8) continue;
		if (!(nmi.memory_model == 5/*non-chain 4 256-color*/ || nmi.memory_model == 4/*packed*/ || nmi.memory_model == 3/*4-plane*/)) continue;
		nmode = tnmode;
		break;
	}

	if (nmode != 0) {
		printf("  Remapping mode 0x13 to 0x%03x (%ux%ux%u)\n",nmode,
				nmi.x_resolution,nmi.y_resolution,nmi.bits_per_pixel);

		if (remap_count < MAX_REMAP) {
			remap[remap_count].source_mode = 0x13;
			remap[remap_count].target_mode = nmode&0x3FFF;
			remap_count++;
		}
	}
	else {
		printf("  No mapping available for mode 0x13\n");
	}
}

int main(int argc,char **argv) {
	char *a,*command=NULL;
        unsigned short resident_size = 0;
	unsigned int entry;
	uint16_t mode;
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
	if (!vbe_probe() || vbe_info == NULL) {
		printf("VESA BIOS not found\n");
		return 1;
	}
	if (vbe_info->video_mode_ptr == 0UL) {
		printf("VBE bios does not provide a mode list\n");
		return 1;
	}

	/* also find a mapping from mode 0x13 (320x200x256) to VESA BIOS */
	install_320x200x256_remap();

	for (entry=0;entry < 1024;entry++) {
		struct vbe_mode_info mi={0},nmi;
		uint16_t nmode,tnmode;
		int nentry;

		mode = vbe_read_mode_entry(vbe_info->video_mode_ptr,entry);
		if (mode == 0xFFFF) break;
		if (!vbe_read_mode_info(mode,&mi)) continue;
		if (!(mi.mode_attributes & VESA_MODE_ATTR_HW_SUPPORTED)) continue;
		if (!(mi.mode_attributes & VESA_MODE_ATTR_GRAPHICS_MODE)) continue;

		if (mi.y_resolution < 240 || (mi.y_resolution > 240 && mi.y_resolution < 480)) {
			printf("Mode 0x%03x (%ux%ux%u) has less than 480 lines\n",mode,
				mi.x_resolution,mi.y_resolution,mi.bits_per_pixel);

			/* look for a target mode to remap to */
			nmode=0;
			for (nentry=0;nentry < 1024;nentry++) {
				tnmode = vbe_read_mode_entry(vbe_info->video_mode_ptr,nentry);
				if (tnmode == 0xFFFF) break;
				memset(&nmi,0,sizeof(nmi));
				if (!vbe_read_mode_info(tnmode,&nmi)) continue;
				if (!(nmi.mode_attributes & VESA_MODE_ATTR_HW_SUPPORTED)) continue;
				if (!(nmi.mode_attributes & VESA_MODE_ATTR_GRAPHICS_MODE)) continue;
				if (nmi.x_resolution != mi.x_resolution) continue;
				if (nmi.number_of_planes != mi.number_of_planes) continue;
				if (nmi.bits_per_pixel != mi.bits_per_pixel) continue;
				if (nmi.number_of_banks != mi.number_of_banks) continue;
				if (nmi.memory_model != mi.memory_model) continue;

				if (mi.y_resolution < 240 && nmi.y_resolution == 240) {
					/* found one: should remap 320x200 -> 320x240 */
					nmode = tnmode;
					break;
				}
				else if (mi.y_resolution >= 240 && nmi.y_resolution == 480) {
					/* found one: should remap 320/640x350/400 to 320/640x480 */
					nmode = tnmode;
					break;
				}
			}

			if (nmode != 0) {
				printf("  Remap to 0x%03x (%ux%ux%u)\n",nmode,
					nmi.x_resolution,nmi.y_resolution,nmi.bits_per_pixel);
			}
			else {
				printf("  Remap to nothing\n");
			}

			if (remap_count >= MAX_REMAP) break;
			remap[remap_count].source_mode = mode&0x3FFF;
			remap[remap_count].target_mode = nmode&0x3FFF;
			remap_count++;
		}
	}

	if (remap_count == 0) {
		printf("Nothing to remap\n");
		return 0;
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

        printf("INT 10h hooked [VESA240.EXE]. Video modes will be forced to 60Hz.\n");
        printf("Resident size %uKB (%u paragraphs)\n",(resident_size + 0x3FU) >> 6U/*10-4*/,resident_size);

		_dos_keep(0,resident_size);
	return 0;
}

