
#include <hw/vga/vrl.h>

/* adjust the pixels in a VRL so combined images in one palette are possible.
 * of course this does slow down loading the images, so perhaps by final release the VRLs could be pre-encoded for the
 * palette offset especially if the VRL is one-time-use within the game.
 *
 * WARNING: Make sure "buffer" points to the first byte after the VRL header! */
#if TARGET_MSDOS == 32
void vrl_palrebase(struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs,unsigned char *buffer,const unsigned char adj)
#else
void vrl_palrebase(struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs,unsigned char far *buffer,const unsigned char adj)
#endif
{
    unsigned char run,skip;
#if TARGET_MSDOS == 32
    unsigned char *cdat;
#else
    unsigned char far *cdat;
#endif
    unsigned int i,j;

    for (i=0;i < hdr->width;i++) {
        cdat = buffer + lineoffs[i];

        do {
            run = cdat[0];
            if (run == 0xFF) break;
            skip = cdat[1];

            if (run & 0x80) {
                cdat[2] += adj;
                cdat += 1u + 2u;
            }
            else {
                for (j=0;j < run;j++) cdat[2u+j] += adj;
                cdat += run + 2u;
            }
        } while (1);
    }
}

