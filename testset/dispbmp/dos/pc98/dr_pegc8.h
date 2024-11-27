
/* NEC PC-98 256-color PEGC packed:
 *
 * One 32KB bank switching window at 0xA8000.
 * Another 32KB bank switching window at 0xB0000.
 * PEGC control registers at 0xE0000.
 *
 * There is a planar 256-color mode on a much shorter range of models but we don't care for that here.
 *
 * This uses only the bank switching window, even for 32-bit DOS programs.
 * There is a linear framebuffer at fixed physical address 0xF00000 on 386SX level systems and
 * an alias at 0xFFF00000 on 486/Pentium systems, which helps if the 15MB memory window isn't enabled in the BIOS
 * and the system has 15MB or more system memory. */

static void draw_bankswitch(unsigned int bank) {
	/* NTS: 0xE0000 is the PEGC control region only in 256-color mode.
	 *      For 16-color planar modes, 0xE0000 is the 4th bitplane, and
	 *      that only appears if you switch into 16-color mode. It
	 *      disappears when 8-color mode is active */
	/* WORD 0xE0004 = VRAM bank for 0xA8000-0xAFFFF */
	/* WORD 0xE0006 = VRAM bank for 0xB0000-0xB7FFF */
	/* BYTE 0xE0100 = 0: Packed pixel mode  1: Planar mode */
#if TARGET_MSDOS == 32
	volatile uint16_t *pegc = (volatile uint16_t*)0xE0000;
#else
	volatile uint16_t far *pegc = (volatile uint16_t far *)MK_FP(0xE000,0);
#endif
	pegc[0x0004u>>1u] = bank;
}

static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_stride);
		const uint16_t bnkaddr = (uint16_t)(addr & 0x7FFFul);
		uint8_t bank = (uint8_t)(addr >> (uint32_t)15ul);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)0xA8000 + bnkaddr;
#else
		unsigned char far *d = MK_FP(0xA800,bnkaddr);
#endif
		unsigned int cpy;

		draw_bankswitch(bank);
		if (bnkaddr != 0) {
			cpy = 0x8000u - bnkaddr;
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
			draw_bankswitch(bank);

#if TARGET_MSDOS == 32
			d = (unsigned char*)0xA8000;
			memcpy(d,src,pixels);
#else
			d = MK_FP(0xA800,0);
			_fmemcpy(d,src,pixels);
#endif
		}
	}
}

