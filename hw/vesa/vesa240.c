
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

static void help() {
	printf("VESA240 /INSTALL\n");
	printf("\n");
	printf("Intercept VESA BIOS calls to force video modes with at least 480 scan lines,\n");
	printf("which may improve video quality with scan converters or enable capture/viewing\n");
	printf("where devices would normally ignore 400-line output.\n");
	printf("(C) 2014 Jonathan Campbell\n");
}

void end_of_resident();

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

	_cli();
	old_int10 = _dos_getvect(0x10);
	_dos_setvect(0x10,new_int10);
	_sti();
	printf("INT 10h hooked\n"); fflush(stdout);
	_dos_keep(0,(34000+sizeof(remap))>>4); /* FIXME! */
	return 0;
}

void end_of_resident() {
}

