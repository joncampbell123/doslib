
#include <stdint.h>

#if TARGET_MSDOS == 16
// headers
# include <i86.h>

// general
# define BMPFAR far

# if defined(TARGET_WINDOWS)
# elif defined(TARGET_OS2)
# else
// MS-DOS real mode
#  include <dos.h>

#  define BMPFILEIMAGE_SEGMENT_BASE
#  define ENABLE_BMPFILEIMAGE
# endif
#else
// general
# define BMPFAR

# define ENABLE_BMPFILEIMAGE
#endif

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
	uint8_t			has_alpha;
};

struct BMPFILEWRITE {
	int			fd;
	unsigned int		width;
	unsigned int		height;
	unsigned int		bpp;
	unsigned int		stride;
	unsigned int		colors;
	unsigned int		colors_used; /* set this nonzero if you use less than the full possible palette */
	struct BMPPALENTRY*	palette;	/* size is "colors" */
	int			current_line;
	int			current_line_add;
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

struct winCIEXYZ {
	uint32_t		ciexyzX;		/* (4) +0x00 +0 */
	uint32_t		ciexyzY;		/* (4) +0x04 +4 */
	uint32_t		ciexyzZ;		/* (4) +0x08 +8 */
};

struct winCIEXYZTRIPLE {
	struct winCIEXYZ	ciexyzRed;		/* (12) +0x00 +0 */
	struct winCIEXYZ	ciexyzGreen;		/* (12) +0x0C +12 */
	struct winCIEXYZ	ciexyzBlue;		/* (12) +0x18 +24 */
};

struct winBITMAPV4HEADER {
	uint32_t		bV4Size;		/* (4) +0x00 +0 */
	int32_t			bV4Width;		/* (4) +0x04 +4 */
	int32_t			bV4Height;		/* (4) +0x08 +8 */
	uint16_t		bV4Planes;		/* (2) +0x0C +12 */
	uint16_t		bV4BitCount;		/* (2) +0x0E +14 */
	uint32_t		bV4V4Compression;	/* (4) +0x10 +16 */
	uint32_t		bV4SizeImage;		/* (4) +0x14 +20 */
	int32_t			bV4XPelsPerMeter;	/* (4) +0x18 +24 */
	int32_t			bV4YPelsPerMeter;	/* (4) +0x1C +28 */
	uint32_t		bV4ClrUsed;		/* (4) +0x20 +32 */
	uint32_t		bV4ClrImportant;	/* (4) +0x24 +36 */
	uint32_t		bV4RedMask;		/* (4) +0x28 +40 */
	uint32_t		bV4GreenMask;		/* (4) +0x2C +44 */
	uint32_t		bV4BlueMask;		/* (4) +0x30 +48 */
	uint32_t		bV4AlphaMask;		/* (4) +0x34 +52 */
	uint32_t		bV4CSType;		/* (4) +0x38 +56 */
	struct winCIEXYZTRIPLE	bV4Endpoints;		/* (36) +0x3C +60 */
	uint32_t		bV4GammaRed;		/* (4) +0x60 +96 */
	uint32_t		bV4GammaGreen;		/* (4) +0x64 +100 */
	uint32_t		bV4GammaBlue;		/* (4) +0x68 +104 */
};							/* (84) =0x6C =108 */

#pragma pack(pop)

struct BMPFILEREAD *open_bmp(const char *path);
int read_bmp_line(struct BMPFILEREAD *bmp);
int resize_bmp_scanline(struct BMPFILEREAD *bmp,unsigned int newsz);
void close_bmp(struct BMPFILEREAD **bmp);

void bitmap_mask2shift(uint32_t mask,uint8_t *shift,uint8_t *width);
void bitmap_memcpy(unsigned char BMPFAR *d,const unsigned char BMPFAR *s,unsigned int l);
void bitmap_memcpy32to24(unsigned char BMPFAR *d24,const unsigned char BMPFAR *s32raw,unsigned int w,const struct BMPFILEREAD *bfr);
unsigned int bitmap_stride_from_bpp_and_w(unsigned int bpp,unsigned int w);
unsigned char bitmap_mkbf8(uint32_t w,const uint8_t fs,const uint8_t fw);

#if defined(ENABLE_BMPFILEIMAGE)
struct BMPFILEIMAGE {
#if defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
	unsigned		bitmap_seg;
#else
	unsigned char BMPFAR*	bitmap;
#endif
	unsigned int		width,height,stride,bpp;
};

struct BMPFILEIMAGE *bmpfileimage_alloc(void);
void bmpfileimage_free_image(struct BMPFILEIMAGE *b);
void bmpfileimage_free(struct BMPFILEIMAGE **b);
int bmpfileimage_alloc_image(struct BMPFILEIMAGE *membmp);
unsigned char BMPFAR *bmpfileimage_row(const struct BMPFILEIMAGE *bfi,unsigned int y);
#endif

struct BMPFILEWRITE *create_write_bmp(void);
int createpalette_write_bmp(struct BMPFILEWRITE *bmp);
int open_write_bmp(struct BMPFILEWRITE *bmp,const char *path);
int write_bmp_line(struct BMPFILEWRITE *bmp,const unsigned char BMPFAR *row,const unsigned int len);
void do_close_write_bmp_file(struct BMPFILEWRITE *bmp);
void do_close_write_bmp(struct BMPFILEWRITE *bmp);
void close_write_bmp(struct BMPFILEWRITE **bmp);

typedef void (*libbmp_conv_scanline_func_t)(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes);

void libbmp_convert_scanline_16bpp555(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels);
void libbmp_convert_scanline_16bpp565(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels);
void libbmp_convert_scanline_16bpp565_to_555(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels);
void libbmp_convert_scanline_32bpp8(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels);
void libbmp_convert_scanline_32to24(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels);
void libbmp_convert_scanline_16_555to24(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels);
void libbmp_convert_scanline_16_565to24(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels);
void libbmp_convert_scanline_none(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels);
