
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

void printf_video_modeinfo(uint16_t mode,struct vbe_mode_info *mi) {
	printf("Mode 0x%04x:\n",mode);
	printf("    Attributes: 0x%04x ",(unsigned int)mi->mode_attributes);
	if (mi->mode_attributes & VESA_MODE_ATTR_HW_SUPPORTED)
		printf("H/W-SUPP ");
	if (mi->mode_attributes & VESA_MODE_ATTR_VBE_1X_MOREINFO)
		printf("VBE1.Xext ");
	if (mi->mode_attributes & VESA_MODE_ATTR_BIOS_TTY)
		printf("TTY ");
	if (mi->mode_attributes & VESA_MODE_ATTR_COLOR_MODE)
		printf("Color ");
	if (mi->mode_attributes & VESA_MODE_ATTR_GRAPHICS_MODE)
		printf("Graphics ");
	if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE)
		printf("!VGA ");
	if (mi->mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE_WINDOWING)
		printf("!Window ");
	if (mi->mode_attributes & VESA_MODE_ATTR_LINEAR_FRAMEBUFFER_AVAILABLE)
		printf("LinFB ");
	if (mi->mode_attributes & VESA_MODE_ATTR_DOUBLESCAN_AVAILABLE)
		printf("Dblscan ");
	if (mi->mode_attributes & VESA_MODE_ATTR_INTERLACED_AVAILABLE)
		printf("Intlace ");
	if (mi->mode_attributes & VESA_MODE_ATTR_HARDWARE_TRIPLE_BUFFER_SUPPORT)
		printf("TripleBuf ");
	if (mi->mode_attributes & VESA_MODE_ATTR_HARDWARE_STEREO_SUPPORT)
		printf("Stereo ");
	if (mi->mode_attributes & VESA_MODE_ATTR_DUAL_DISPLAY_START_SUPPORT)
		printf("DualDisplay ");
	printf("\n");
	printf("    Win A Attr=0x%02x seg=0x%04x   Win B Attr=0x%02x seg=0x%04x\n",
			mi->win_a_attributes,mi->win_a_segment,
			mi->win_b_attributes,mi->win_b_segment);
	printf("    Window granularity=%uKB size=%uKB function=%04x:%04x bytes/line=%u\n",
			mi->win_granularity,mi->win_size,
			(unsigned int)(mi->window_function>>16),
			(unsigned int)(mi->window_function&0xFFFF),
			mi->bytes_per_scan_line);
	printf("    %u x %u (char %u x %u) %u-plane %ubpp. %u banks. Model %u.\n",
			mi->x_resolution,mi->y_resolution,mi->x_char_size,mi->y_char_size,
			mi->number_of_planes,mi->bits_per_pixel,mi->number_of_banks,mi->memory_model);
	printf("    Model is '%s'. Bank size=%uKB. Image pages=%u\n",vbe_memory_model_to_str(mi->memory_model),
			mi->bank_size,mi->number_of_image_pages);
	printf("    RGBA (size,pos) R=(%u,%u) G=(%u,%u) B=(%u,%u) A=(%u,%u) DCModeInfo=0x%02X\n",
			mi->red_mask_size,	mi->red_field_position,
			mi->green_mask_size,	mi->green_field_position,
			mi->blue_mask_size,	mi->blue_field_position,
			mi->reserved_mask_size,	mi->reserved_field_position,
			mi->direct_color_mode_info);
	printf("    Physical addr: 0x%08lX Linbytes/scan=%u BankPages=%u LinPages=%u\n",(unsigned long)mi->phys_base_ptr,
			mi->lin_bytes_per_line,	mi->bank_number_of_image_pages+1,
			mi->lin_number_of_image_pages);
	printf("    Lin RGBA (size,pos) R=(%u,%u) G=(%u,%u) B=(%u,%u) A=(%u,%u) maxpixelclock=%lu\n",
			mi->lin_red_mask_size,		mi->lin_red_field_position,
			mi->lin_green_mask_size,		mi->lin_green_field_position,
			mi->lin_blue_mask_size,		mi->lin_blue_field_position,
			mi->lin_reserved_mask_size,	mi->lin_reserved_field_position,
			mi->max_pixel_clock);
}

int main() {
	unsigned int entry;
	uint16_t mode;
	char tmp[128];
    int c;

	if (!vbe_probe()) {
		printf("VESA BIOS not found\n");
		return 1;
	}

    if (vbe_info->video_mode_ptr != 0UL) {
		for (entry=0;entry < 128;entry++) {
			struct vbe_mode_info mi={0};

			mode = vbe_read_mode_entry(vbe_info->video_mode_ptr,entry);
			if (mode == 0xFFFF) break;

			if (vbe_read_mode_info(mode,&mi)) {
				printf_video_modeinfo(mode,&mi);

                do {
                    c = getch();
                    if (c == 27) return 1;
                    else if (c == 13 || c == 10) break;
                } while (1);

                sprintf(tmp,"MODESET 0x%04x",mode);
                system(tmp);
            }
		}
	}

    return 0;
}

