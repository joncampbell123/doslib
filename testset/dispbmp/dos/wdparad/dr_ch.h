
/* draw scanline in normal chained 256-color mode, except the ET4000 allows access to the first 128KB through A0000-BFFFF */
static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + (y * img_width);
		memcpy(d,src,pixels);
#else
		const uint32_t adrofs = (uint32_t)y * (uint32_t)img_width;
		unsigned char far *d = MK_FP(0xA000 + (uint16_t)(adrofs >> (uint32_t)4),(uint16_t)(adrofs & 0xF));
		_fmemcpy(d,src,pixels);
#endif
	}
}

