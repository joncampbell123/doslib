
#include <stdio.h>
#include <stdint.h>

#include <hw/vga/vrl.h>

/*---------------------------------------------------------------------------*/
/* VRL image structure                                                       */
/*---------------------------------------------------------------------------*/

struct vrl_image {
    struct vrl1_vgax_header*        vrl_header;
    vrl1_vgax_offset_t*             vrl_lineoffs;
    unsigned char*                  buffer;
    unsigned int                    bufsz;
};

void free_vrl(struct vrl_image *img);
int load_vrl(struct vrl_image *img,const char *path);

