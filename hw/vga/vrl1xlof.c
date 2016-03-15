
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
#include <hw/vga/vrl.h>

vrl1_vgax_offset_t *vrl_genlineoffsets(struct vrl1_vgax_header *hdr,unsigned char *data,unsigned int datasz) {
	unsigned int x = 0;
	unsigned char run,skip;
	vrl1_vgax_offset_t *list;
	unsigned char *fence = data + datasz,*s;

	if (hdr->width == 0U || hdr->width >= ((~0U) / sizeof(vrl1_vgax_offset_t)))
		return NULL;

	list = malloc(hdr->width * sizeof(vrl1_vgax_offset_t));
	if (list == NULL) return NULL;

	s = data;
	while (s < fence) {
		list[x++] = (vrl1_vgax_offset_t)(s - data);
		while (s < fence) {
			run = *s++;
			if (run == 0xFF) break;
			skip = *s++;

			if (run&0x80)
				s++;
			else
				s += run;
		}

		if (x >= hdr->width) break;
	}

	return list;
}

