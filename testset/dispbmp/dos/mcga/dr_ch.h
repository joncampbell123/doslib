
/* draw scanline in normal chained 256-color mode */
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

