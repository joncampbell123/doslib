
#include <hw/vga/vrl.h>

void draw_vrl1_vgax_modexclip(unsigned int x,unsigned int y,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz);
void draw_vrl1_vgax_modexclip_hflip(unsigned int x,unsigned int y,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz);

