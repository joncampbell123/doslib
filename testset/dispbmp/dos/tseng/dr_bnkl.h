
typedef void (*draw_scanline_func_t)(unsigned int y,unsigned char *src,unsigned int pixels);

/* ET3K */
#define BANKSWITCH(bank) outp(0x3CD,bank | (bank << 3u) | 0x40) /* [7:6] we want 64kb banks [5:3] read [2:0] write banks */
static void draw_scanline_et3k(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_width);
		const uint16_t bnkaddr = (uint16_t)(addr & 0xFFFFu);
		uint8_t bank = (uint8_t)(addr >> (uint32_t)16u);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + bnkaddr;
#else
		unsigned char far *d = MK_FP(0xA000,bnkaddr);
#endif
		unsigned int cpy;

		BANKSWITCH(bank);
		if (bnkaddr != 0) {
			cpy = 0x10000u - bnkaddr;
			if (cpy > pixels) cpy = pixels;
		}
		else {
			cpy = pixels;
		}

#if TARGET_MSDOS == 32
		memcpy(d,src,cpy);
#else
		_fmemcpy(d,src,cpy);
#endif
		pixels -= cpy;
		if (pixels != 0) {
			bank++;
			src += cpy;
			BANKSWITCH(bank);

#if TARGET_MSDOS == 32
			d = (unsigned char*)0xA0000;
			memcpy(d,src,pixels);
#else
			d = MK_FP(0xA000,0);
			_fmemcpy(d,src,pixels);
#endif
		}
	}
}
#undef BANKSWITCH

#define BANKSWITCH(bank) outp(0x3CD,bank | (bank << 4u)) /* [7:4] read [3:0] write banks */
static void draw_scanline_et4k(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_width);
		const uint16_t bnkaddr = (uint16_t)(addr & 0xFFFFu);
		uint8_t bank = (uint8_t)(addr >> (uint32_t)16u);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + bnkaddr;
#else
		unsigned char far *d = MK_FP(0xA000,bnkaddr);
#endif
		unsigned int cpy;

		BANKSWITCH(bank);
		if (bnkaddr != 0) {
			cpy = 0x10000u - bnkaddr;
			if (cpy > pixels) cpy = pixels;
		}
		else {
			cpy = pixels;
		}

#if TARGET_MSDOS == 32
		memcpy(d,src,cpy);
#else
		_fmemcpy(d,src,cpy);
#endif
		pixels -= cpy;
		if (pixels != 0) {
			bank++;
			src += cpy;
			BANKSWITCH(bank);

#if TARGET_MSDOS == 32
			d = (unsigned char*)0xA0000;
			memcpy(d,src,pixels);
#else
			d = MK_FP(0xA000,0);
			_fmemcpy(d,src,pixels);
#endif
		}
	}
}
#undef BANKSWITCH

