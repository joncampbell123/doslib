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
    struct vrl1_vgax_header *hdr;
    unsigned char *raw;
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

    raw = malloc(rawlen);
    if (raw == NULL) return 1;
    if (lseek(fd,0,SEEK_SET) != 0) return 1;
    if (read(fd,raw,rawlen) != rawlen) return 1;
    close(fd);

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

    free(raw);
	return 0;
}

