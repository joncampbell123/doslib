
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <conio.h>
#include <stdio.h>
#include <errno.h>
#include <i86.h>

static const char bmpfile[] = "200l8v.bmp";
static const unsigned int img_width = 320;
static const unsigned int img_height = 200;

#pragma pack(push,1)
struct BMPPALENTRY {
	uint8_t			rgbBlue;	/* +0 */
	uint8_t			rgbGreen;	/* +1 */
	uint8_t			rgbRed;		/* +2 */
	uint8_t			rgbReserved;	/* +3 */
};						/* =4 */
#pragma pack(pop)

struct BMPFILEREAD {
	int			fd;
	unsigned int		width;
	unsigned int		height;
	unsigned int		bpp;
	unsigned int		stride;
	unsigned int		colors;
	unsigned long		size;
	unsigned int		dib_offset;
	unsigned char*		scanline;
	unsigned int		scanline_size;
	int			current_line;
	int			current_line_add;
	struct BMPPALENTRY*	palette;	/* size is "colors" */
};

#pragma pack(push,1)
struct winBITMAPFILEHEADER {
	uint16_t		bfType;		/* +0  'BM' */
	uint32_t		bfSize;		/* +2 */
	uint16_t		bfReserved1;	/* +6 */
	uint16_t		bfReserved2;	/* +8 */
	uint32_t		bfOffBits;	/* +10 */
};						/* =14 */

struct winBITMAPINFOHEADER {
	uint32_t		biSize;			/* +0 */
	int32_t			biWidth;		/* +4 */
	int32_t			biHeight;		/* +8 */
	uint16_t		biPlanes;		/* +12 */
	uint16_t		biBitCount;		/* +14 */
	uint32_t		biCompression;		/* +16 */
	uint32_t		biSizeImage;		/* +20 */
	int32_t			biXPelsPerMeter;	/* +24 */
	int32_t			biYPelsPerMeter;	/* +28 */
	uint32_t		biClrUsed;		/* +32 */
	uint32_t		biClrImportant;		/* +36 */
};							/* =40 */
#pragma pack(pop)

struct BMPFILEREAD *open_bmp(const char *path) {
	struct BMPFILEREAD *bmp = (struct BMPFILEREAD*)calloc(1,sizeof(struct BMPFILEREAD));
	unsigned char *temp;

	if (bmp == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	temp = malloc(256); /* for temporary loading only */
	if (temp == NULL) {
		errno = ENOMEM;
		free(bmp);
		return NULL;
	}

	bmp->fd = open(path,O_RDONLY | O_BINARY); /* sets errno */
	if (bmp->fd < 0) goto fail_fileopen;

	/* file header */
	if (lseek(bmp->fd,0,SEEK_SET) != 0) goto fail;
	if (read(bmp->fd,temp,14) != 14) goto fail;
	{
		struct winBITMAPFILEHEADER *fh = (struct winBITMAPFILEHEADER*)temp;
		if (fh->bfType != 0x4D42) goto fail;
		bmp->dib_offset = fh->bfOffBits;
	}

	/* then the BITMAPINFOHEADER */
	if (read(bmp->fd,temp,4) != 4) goto fail;
	{
		struct winBITMAPINFOHEADER *bi = (struct winBITMAPINFOHEADER*)temp;
		/* we just read the first 4 bytes, the biSize field */
		/* we don't support the old BITMAPCOREINFO that OS/2 uses, sorry */
		/* nothing is defined that is so large to need more than 256 bytes */
		if (bi->biSize < 40 || bi->biSize > 256) goto fail;
		/* now read the rest of it */
		if (read(bmp->fd,temp+4,bi->biSize-4) != (bi->biSize-4)) goto fail;

		if (bi->biWidth <= 0 || bi->biHeight == 0 || bi->biWidth > 0xFFFF || bi->biHeight > 0xFFFF || bi->biBitCount == 0 || bi->biPlanes > 1) goto fail;

		/* bitmaps are upside down by default, Windows 95 and later make the field signed and negative means topdown */
		if (bi->biHeight < 0) {
			bmp->current_line = -1;
			bmp->current_line_add = 1;
		}
		else {
			bmp->current_line = (int)bi->biHeight;
			bmp->current_line_add = -1;
		}

		bmp->width = (unsigned int)(bi->biWidth);
		bmp->height = (unsigned int)labs(bi->biHeight);
		bmp->bpp = bi->biBitCount;

		if (bi->biCompression == 0) {
			bmp->stride = (((bmp->width * bmp->bpp) + 31u) & (~31u)) >> 3u;
			if (bmp->stride == 0) goto fail;
			bmp->scanline_size = bmp->stride;
			bmp->scanline = malloc(bmp->scanline_size + 8u);
			if (bmp->scanline == NULL) goto fail;
			bmp->size = (unsigned long)bmp->stride * (unsigned long)bmp->height;

			if (bmp->bpp <= 8) {
				bmp->colors = 1u << bmp->bpp;
				if (bi->biClrUsed != 0 && bmp->colors > bi->biClrUsed) bmp->colors = bi->biClrUsed;
				if (bmp->colors == 0) goto fail;

				bmp->palette = (struct BMPPALENTRY*)calloc(bmp->colors,sizeof(struct BMPPALENTRY));
				if (bmp->palette == NULL) goto fail;

				/* palette follows struct */
				if (read(bmp->fd,bmp->palette,bmp->colors * sizeof(struct BMPPALENTRY)) != (bmp->colors * sizeof(struct BMPPALENTRY))) goto fail;
			}
		}
		else {
			goto fail;
		}
	}

	/* then prepare for reading the bitmap */
	if (lseek(bmp->fd,bmp->dib_offset,SEEK_SET) != bmp->dib_offset) goto fail;

	free(temp);
	return bmp;
fail:
	close(bmp->fd);
fail_fileopen:
	if (bmp->scanline) free(bmp->scanline);
	if (bmp->palette) free(bmp->palette);
	free(temp);
	free(bmp);
	return NULL;
}

static void do_close_bmp(struct BMPFILEREAD *bmp) {
	if (bmp->palette) free(bmp->palette);
	bmp->palette = NULL;
	if (bmp->scanline) free(bmp->scanline);
	bmp->scanline = NULL;
	if (bmp->fd >= 0) close(bmp->fd);
	bmp->fd = -1;
	free(bmp);
}

void close_bmp(struct BMPFILEREAD **bmp) {
	if (bmp && *bmp) {
		do_close_bmp(*bmp);
		*bmp = NULL;
	}
}

int read_bmp_line(struct BMPFILEREAD *bmp) {
	if (bmp == NULL) return -1;
	if (bmp->fd < 0) return -1;
	if (bmp->scanline == NULL || bmp->scanline_size == 0) return -1;
	if (bmp->stride == 0 || bmp->stride > bmp->scanline_size) return -1;

	bmp->current_line += bmp->current_line_add;
	if (bmp->current_line < 0 || (unsigned int)bmp->current_line >= bmp->height) return -1;

	if (read(bmp->fd,bmp->scanline,bmp->stride) != bmp->stride) return -1;

	return 0;
}

static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + (y * img_width);
		memcpy(d,src,pixels);
#else
		unsigned char far *d = MK_FP(0xA000,y * img_width);
		_fmemcpy(d,src,pixels);
#endif
	}
}

int main() {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp != 8 || bfr->colors == 0 || bfr->colors > 256 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* set 320x200x256 VGA */
	__asm {
		mov	ax,0x0013	; AH=0x00 AL=0x13
		int	0x10
	}

	/* set palette */
	outp(0x3C8,0);
	for (i=0;i < bfr->colors;i++) {
		outp(0x3C9,bfr->palette[i].rgbRed >> 2);
		outp(0x3C9,bfr->palette[i].rgbGreen >> 2);
		outp(0x3C9,bfr->palette[i].rgbBlue >> 2);
	}

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0)
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);

	/* done reading */
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	/* restore text */
	__asm {
		mov	ax,0x0003	; AH=0x00 AL=0x03
		int	0x10
	}
	return 0;
}

