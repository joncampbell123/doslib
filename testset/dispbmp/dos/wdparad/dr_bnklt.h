
/* TEMPLATE! */
static void DSFUNC(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		/* 4KB bank granularity enables placing the window in such a way we can copy a scanline without having to worry about crossing a window boundary */
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_width);
		const uint8_t bank = (uint16_t)(addr >> (uint32_t)12u);
		const uint16_t bnkaddr = (uint16_t)(addr & 0xFFFu);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + bnkaddr;
#else
		unsigned char far *d = MK_FP(0xA000,bnkaddr);
#endif

		BANKSWITCH(bank);
#if TARGET_MSDOS == 32
		memcpy(d,src,pixels);
#else
		_fmemcpy(d,src,pixels);
#endif
	}
}

