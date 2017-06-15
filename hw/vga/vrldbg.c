/* VRL run-length debugging tool.
 *
 * For sparky4 / Project 16 and anyone else needing to debug the VRL structure */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vrl.h"
#include "vrs.h"
#include "pcxfmt.h"
#include "comshtps.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

static void help() {
	fprintf(stderr,"VRLDBG (C) 2016 Jonathan Campbell\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"vrldbg file\n");
}

int main(int argc,char **argv) {
    unsigned char *base,*raw,*fence;
    struct vrl1_vgax_header *hdr;
    unsigned int x,y,cc;
    size_t rawlen;
    long l;
    int fd;

    if (argc < 2) {
        help();
        return 1;
    }

    fd = open(argv[1],O_RDONLY|O_BINARY);
    if (fd < 0) return 1;
    l = (long)lseek(fd,0,SEEK_END);
    if (l < 16 || l > VRL_MAX_SIZE) return 1;
    rawlen = (size_t)l;

    base = raw = malloc(rawlen);
    if (raw == NULL) return 1;
    if (lseek(fd,0,SEEK_SET) != 0) return 1;
    if (read(fd,raw,rawlen) != rawlen) return 1;
    close(fd);
    fence = raw + rawlen;

    hdr = (struct vrl1_vgax_header*)raw;
    if (memcmp(hdr->vrl_sig,"VRL1",4)) return 1;
    if (memcmp(hdr->fmt_sig,"VGAX",4)) return 1;

#if 0
#pragma pack(push,1)
struct vrl1_vgax_header {
	uint8_t			vrl_sig[4];		// +0x00  "VRL1"
	uint8_t			fmt_sig[4];		// +0x04  "VGAX"
	uint16_t		height;			// +0x08  Sprite height
	uint16_t		width;			// +0x0A  Sprite width
	int16_t			hotspot_x;		// +0x0C  Hotspot offset (X) for programmer's reference
	int16_t			hotspot_y;		// +0x0E  Hotspot offset (Y) for programmer's reference
};							// =0x10
#pragma pack(pop)
#endif
    printf("VRL header:\n");
    printf("  vrl_sig:          \"VRL1\"\n"); // already validated
    printf("  fmt_sig:          \"VGAX\"\n"); // already validated
    printf("  height:           %u pixels\n",hdr->height);
    printf("  width:            %u pixels\n",hdr->width);
    printf("  hotspot_x:        %d pixels\n",hdr->hotspot_x);
    printf("  hotspot_y:        %d pixels\n",hdr->hotspot_y);

    /* strips are encoded in column order, top to bottom.
     * each column ends with a special code, which is a cue to begin the next column and decode more.
     * each strip has a length and a skip count. the skip count is there to allow for sprite
     * transparency by skipping pixels.
     *
     * the organization of this format is optimized for display on VGA hardware in "mode x"
     * unchained 256-color mode (where the planar memory organization of the VGA is exposed) */
    raw = base + sizeof(*hdr);
    for (x=0;x < hdr->width;x++) {/* for each column */
        printf("Begin column x=%u\n",x);
        y=0;

        if (raw >= fence) {
            printf("* unexpected end of data\n");
            break;
        }

        /* each column is a series of vertical strips with a two byte header, until
         * the first occurrence where the first byte is 0xFF. */
        do {
            if (raw >= fence) {
                printf("* unexpected end of data in column x=%u at y=%u\n",x,y);
                break;
            }

            if (*raw == 0xFF) {/* end of column */
                raw++;
                break;
            }

            if (*raw >= 0x80) { /* single-color run */
                if ((raw+3) > fence) {
                    printf("* unexpected end of data in column x=%u at y=%u with %u byte(s) left\n",x,y,(unsigned int)(fence-raw));
                    break;
                }

                {
                    /* <run length + 0x80)> <skip length> <color value> */
                    unsigned char strip_len = (*raw++) - 0x80;
                    unsigned char skip_len = (*raw++);
                    unsigned char color = (*raw++);

                    printf("  y=%u. after skip, y=%u. single-color strip length=%u + skip=%u with color=0x%02x\n",
                        y,y+skip_len,strip_len,skip_len,color);

                    y += strip_len + skip_len;
                }
            }
            else { /* copy strip */
                if ((raw+2) > fence) {
                    printf("* unexpected end of data in column x=%u at y=%u with %u byte(s) left\n",x,y,(unsigned int)(fence-raw));
                    break;
                }

                {
                    /* <run length> <skip length> [strip of pixels] */
                    unsigned char strip_len = (*raw++);
                    unsigned char skip_len = (*raw++);

                    printf("  y=%u. after skip, y=%u. strip length=%u + skip=%u\n",
                        y,y+skip_len,strip_len,skip_len);

                    if ((raw+strip_len) > fence) {
                        printf("* unexpected end of data in strip x=%u at y=%u with %u byte(s) left\n",x,y,(unsigned int)(fence-raw));
                        break;
                    }

                    if (strip_len != 0) {
                        printf("    pixels: ");
                        for (cc=0;cc < strip_len;cc++) printf("0x%02x ",raw[cc]);
                        printf("\n");
                    }

                    y += strip_len + skip_len;
                    raw += strip_len;
                }
            }
        } while(1);

        if (y > hdr->height)
            printf("* warning: y coordinate y=%u overruns height of VRL height=%u\n",(unsigned int)y,(unsigned int)hdr->height);
    }

    if (raw < fence) {
        printf("* warning: %u bytes remain after decoding\n",(unsigned int)(fence-raw));
    }

    free(base);
	return 0;
}

