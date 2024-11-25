
/* draw scanline in 2bpp cga mode */
static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const unsigned int bytes = (pixels + 3u) >> 2u;
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xB8000 + ((y >> 1) * img_stride) + ((y & 1) * 0x2000);
		memcpy(d,src,bytes);
#else
		unsigned char far *d = MK_FP(0xB800,((y >> 1) * img_stride) + ((y & 1) * 0x2000));
		_fmemcpy(d,src,bytes);
#endif
	}
}

