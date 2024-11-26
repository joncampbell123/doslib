
#ifdef DSCOLOR
/* draw scanline in 4bpp planar 16-color mode */
static void DSFUNC(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		/* 4KB bank granularity enables placing the window in such a way we can copy a scanline without having to worry about crossing a window boundary */
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_stride);
		const uint8_t bank = (uint16_t)(addr >> (uint32_t)12u);
		const uint16_t bnkaddr = (uint16_t)(addr & 0xFFFu);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + bnkaddr;
#else
		unsigned char far *d = MK_FP(0xA000,bnkaddr);
#endif

		outpw(0x3C4,0x0F02); /* write all bitplanes (map mask) */
		outpw(0x3CE,0x0205); /* write mode 2 (read mode 0) */

		BANKSWITCH(bank);
		vga4pcpy(d,src,pixels);

		outpw(0x3CE,0x0005); /* write mode 0 (read mode 0) */
		outpw(0x3CE,0xFF08); /* bit mask */
	}
}
#endif

#ifdef DSMONO
/* draw scanline in 4bpp planar 16-color mode */
static void DSFUNCM(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_stride);
		const uint8_t bank = (uint16_t)(addr >> (uint32_t)12u);
		const unsigned int bytes = (pixels + 7u) >> 3u;
		const uint16_t bnkaddr = (uint16_t)(addr & 0xFFFu);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + bnkaddr;
#else
		unsigned char far *d = MK_FP(0xA000,bnkaddr);
#endif

		BANKSWITCH(bank);
#if TARGET_MSDOS == 32
		memcpy(d,src,bytes);
#else
		_fmemcpy(d,src,bytes);
#endif
	}
}
#endif

