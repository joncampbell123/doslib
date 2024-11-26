
/* draw scanline in 1bpp cga mode */
static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const unsigned int bytes = (pixels + 1u) >> 1u;
#if TARGET_MSDOS == 32
		unsigned char *d = img_vram + ((y >> 2) * img_stride) + ((y & 3) * 0x2000);
		memcpy(d,src,bytes);
#else
		unsigned char far *d = img_vram + ((y >> 2) * img_stride) + ((y & 3) * 0x2000);
		_fmemcpy(d,src,bytes);
#endif
	}
}

