
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

int main() {
	unsigned int entry;
	uint16_t mode;
	char tmp[128];

	if (!vbe_probe()) {
		printf("VESA BIOS not found\n");
		return 1;
	}

    if (vbe_info->video_mode_ptr != 0UL) {
		for (entry=0;entry < 4096;entry++) {
			struct vbe_mode_info mi={0};

			mode = vbe_read_mode_entry(vbe_info->video_mode_ptr,entry);
			if (mode == 0xFFFF) break;

			if (vbe_read_mode_info(mode,&mi)) {
                sprintf(tmp,"MODESET 0x%04x /i",mode);
                system(tmp);
            }
		}
	}

    return 0;
}

