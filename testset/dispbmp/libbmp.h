
#include <stdint.h>

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
	uint8_t			red_shift;
	uint8_t			red_width;
	uint8_t			green_shift;
	uint8_t			green_width;
	uint8_t			blue_shift;
	uint8_t			blue_width;
	uint8_t			alpha_shift;
	uint8_t			alpha_width;
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

struct BMPFILEREAD *open_bmp(const char *path);
int read_bmp_line(struct BMPFILEREAD *bmp);
void close_bmp(struct BMPFILEREAD **bmp);

