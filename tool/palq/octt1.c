
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#if defined(LINUX)
# include <time.h>
#endif

#include "libbmp.h"

struct rgb_t {
	unsigned char	r,g,b;
};

unsigned char map2palette(unsigned char r,unsigned char g,unsigned char b,struct rgb_t *pal,unsigned int colors) {
	unsigned int mdst = ~0u,dst,col = 0,i = 0;

	while (i < colors) {
		dst  = (unsigned int)abs((int)r - (int)(pal->r));
		dst += (unsigned int)abs((int)g - (int)(pal->g));
		dst += (unsigned int)abs((int)b - (int)(pal->b));

		if (mdst > dst) {
			mdst = dst;
			col = i;
		}

		pal++;
		i++;
	}

	return col;
}

int main(int argc,char **argv) {
	struct BMPFILEIMAGE *membmp = NULL;
	struct BMPFILEIMAGE *memdst = NULL;
	unsigned int target_colors = 256;
	unsigned char paldepthbits = 8;
	struct rgb_t *palette;
	unsigned int i;

	if (argc < 3)
		return 1;

	membmp = bmpfileimage_alloc();
	if (!membmp)
		return 1;

	memdst = bmpfileimage_alloc();
	if (!memdst)
		return 1;

	if (argc > 3) {
		char *s = argv[3];

		target_colors = strtoul(s,&s,0);
		if (target_colors == 0u || target_colors > 256u)
			return 1;

		if (*s == ':') {
			s++;
			paldepthbits = strtoul(s,&s,0);
			if (paldepthbits == 0 || paldepthbits > 8)
				return 1;
		}
	}

	fprintf(stderr,"Quant to %u colors with %u bits per palette entry\n",target_colors,paldepthbits);

	palette = malloc(sizeof(struct rgb_t) * 256);
	if (!palette)
		return 1;

	{
		struct BMPFILEREAD *bfr;

		bfr = open_bmp(argv[1]);
		if (bfr == NULL) {
			fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
			return 1;
		}
		if (!(bfr->bpp == 24 || bfr->bpp == 32)) {
			fprintf(stderr,"BMP wrong format\n");
			return 1;
		}

		membmp->bpp = 24;
		membmp->width = bfr->width;
		membmp->height = bfr->height;

		if (bmpfileimage_alloc_image(membmp)) {
			fprintf(stderr,"Failed to allocate memory\n");
			return 1;
		}

		while (read_bmp_line(bfr) == 0) {
			unsigned char *dest = bmpfileimage_row(membmp,(unsigned int)bfr->current_line);
			assert(dest != NULL);

			if (bfr->bpp == 32)
				bitmap_memcpy32to24(dest,bfr->scanline,membmp->width,bfr);
			else
				memcpy(dest,bfr->scanline,membmp->stride);
		}

		/* done reading */
		close_bmp(&bfr);
	}

	memdst->bpp = 8;
	memdst->width = membmp->width;
	memdst->height = membmp->height;

	if (bmpfileimage_alloc_image(memdst)) {
		fprintf(stderr,"Failed to allocate memory\n");
		return 1;
	}

	/* test palette, a 6x8x5 color cube */
	for (i=0;i < target_colors;i++) {
		palette[i].r = (unsigned char)(((i % 6u) * 255u) / 5u);
		palette[i].g = (unsigned char)((((i / 6u) % 8u) * 255u) / 7u);
		palette[i].b = (unsigned char)(((i / 6u / 8u) * 255u) / 4u);
	}

	/* quant palette */
	if (paldepthbits < 8) {
		const unsigned char msk = (0xFF00u >> paldepthbits) & 0xFFu;
		for (i=0;i < target_colors;i++) {
			palette[i].r &= msk;
			palette[i].g &= msk;
			palette[i].b &= msk;
		}
	}

	/* convert 24bpp to 8bpp image */
	{
		unsigned char *s24,*d8;
		unsigned int x,y;

		for (y=0;y < membmp->height;y++) {
			s24 = bmpfileimage_row(membmp,y);
			d8 = bmpfileimage_row(memdst,y);
			assert(d8 != NULL && s24 != NULL);

			for (x=0;x < membmp->width;x++) {
				*d8++ = map2palette(s24[2],s24[1],s24[0],palette,target_colors); /* Windows BMPs are BGR order */
				s24 += 3;
			}
		}
	}

	/* write it out */
	{
		struct BMPFILEWRITE *bfw;
		unsigned char *s;
		unsigned int y;

		bfw = create_write_bmp();
		if (!bfw) {
			fprintf(stderr,"Cannot alloc write bmp\n");
			return 1;
		}

		bfw->bpp = 8; // TODO: 2 colors = 1bpp  3-16 colors = 4bpp   anything else 8bpp
		bfw->width = membmp->width;
		bfw->height = membmp->height;
		bfw->colors_used = target_colors;

		if (createpalette_write_bmp(bfw)) {
			fprintf(stderr,"Cannot make write bmp palette\n");
			return 1;
		}

		if (bfw->palette) {
			bfw->colors_used = target_colors;
			for (i=0;i < target_colors;i++) {
				bfw->palette[i].rgbRed = palette[i].r;
				bfw->palette[i].rgbGreen = palette[i].g;
				bfw->palette[i].rgbBlue = palette[i].b;
			}
		}

		if (open_write_bmp(bfw,argv[2])) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		for (y=0;y < membmp->height;y++) {
			s = bmpfileimage_row(memdst,bfw->current_line);
			assert(s != NULL);

			if (write_bmp_line(bfw,s,bfw->stride)) {
				fprintf(stderr,"Scanline write err\n");
				return 1;
			}
		}

		close_write_bmp(&bfw);
	}

	/* free bitmap */
	bmpfileimage_free(&membmp);
	bmpfileimage_free(&memdst);
	free(palette);
	return 0;
}

