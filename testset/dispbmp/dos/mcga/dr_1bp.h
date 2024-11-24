
/* draw scanline in 4bpp planar 16-color mode */
static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const unsigned int bytes = (pixels + 7u) >> 3u;
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + (y * img_stride);
		memcpy(d,src,bytes);
#else
		unsigned char far *d = MK_FP(0xA000,y * img_stride);
		_fmemcpy(d,src,bytes);
#endif
	}
}

