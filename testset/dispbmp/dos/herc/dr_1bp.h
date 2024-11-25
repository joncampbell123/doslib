
/* draw scanline in 1bpp hercules mode */
static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const unsigned int bytes = (pixels + 7u) >> 3u;
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xB0000 + ((y >> 2) * img_stride) + ((y & 3) * 0x2000u);
		memcpy(d,src,bytes);
#else
		unsigned char far *d = MK_FP(0xB000,((y >> 2) * img_stride) + ((y & 3) * 0x2000));
		_fmemcpy(d,src,bytes);
#endif
	}
}

