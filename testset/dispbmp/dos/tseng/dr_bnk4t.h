
#ifdef DSCOLOR
/* draw scanline in 4bpp planar 16-color mode */
static void DSFUNC(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_stride);
		uint8_t bank = (uint16_t)(addr >> (uint32_t)16u);
		uint16_t bnkaddr = (uint16_t)(addr & 0xFFFFu);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + bnkaddr;
#else
		unsigned char far *d = MK_FP(0xA000,bnkaddr);
#endif
		unsigned int cpy;

		outpw(0x3C4,0x0F02); /* write all bitplanes (map mask) */
		outpw(0x3CE,0x0205); /* write mode 2 (read mode 0) */

		BANKSWITCH(bank);
		if (bnkaddr != 0) {
			cpy = 0x10000u - bnkaddr;
			if (cpy > (0xffffu/8u)) cpy = (0xffffu/8u); /* prevent 16-bit overflow */
			cpy *= 8u; /* this is where it would happen */
			if (cpy > pixels) cpy = pixels;
		}
		else {
			cpy = pixels;
		}

		vga4pcpy(d,src,cpy);
		pixels -= cpy;
		if (pixels != 0) {
			bank++;
			src += cpy / 2u;
			BANKSWITCH(bank);

#if TARGET_MSDOS == 32
			d = (unsigned char*)0xA0000;
#else
			d = MK_FP(0xA000,0);
#endif
			vga4pcpy(d,src,pixels);
		}

		outpw(0x3CE,0x0005); /* write mode 0 (read mode 0) */
		outpw(0x3CE,0xFF08); /* bit mask */
	}
}
#endif

#ifdef DSMONO
/* draw scanline in 4bpp planar 16-color mode */
static void DSFUNCM(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		unsigned int bytes = (pixels + 7u) >> 3u;
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_stride);
		uint8_t bank = (uint16_t)(addr >> (uint32_t)16u);
		uint16_t bnkaddr = (uint16_t)(addr & 0xFFFFu);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA0000 + bnkaddr;
#else
		unsigned char far *d = MK_FP(0xA000,bnkaddr);
#endif
		unsigned int cpy;

		BANKSWITCH(bank);
		if (bnkaddr != 0) {
			cpy = 0x10000u - bnkaddr;
			if (cpy > bytes) cpy = bytes;
		}
		else {
			cpy = bytes;
		}

#if TARGET_MSDOS == 32
		memcpy(d,src,cpy);
#else
		_fmemcpy(d,src,cpy);
#endif
		bytes -= cpy;
		if (bytes != 0) {
			bank++;
			src += cpy / 2u;
			BANKSWITCH(bank);

#if TARGET_MSDOS == 32
			d = (unsigned char*)0xA0000;
			memcpy(d,src,cpy);
#else
			d = MK_FP(0xA000,0);
			_fmemcpy(d,src,cpy);
#endif
		}
	}
}
#endif

