
#if TARGET_MSDOS == 32
static inline void scanline_pack2planar(unsigned char *d,unsigned char *src,unsigned int pixels,const unsigned char shf) {
#else
static inline void scanline_pack2planar(unsigned char far *d,unsigned char *src,const unsigned int pixels,const unsigned char shf) {
#endif
	const unsigned int count = (pixels + 7u) >> 3u;
	unsigned int x;

	for (x=0;x < count;x++) {
		register unsigned char b = 0;

		if (src[0] & (0x10u << shf)) b |= 1u << 7u;
		if (src[0] & (0x01u << shf)) b |= 1u << 6u;
		if (src[1] & (0x10u << shf)) b |= 1u << 5u;
		if (src[1] & (0x01u << shf)) b |= 1u << 4u;
		if (src[2] & (0x10u << shf)) b |= 1u << 3u;
		if (src[2] & (0x01u << shf)) b |= 1u << 2u;
		if (src[3] & (0x10u << shf)) b |= 1u << 1u;
		if (src[3] & (0x01u << shf)) b |= 1u << 0u;

		src += 4;
		*d++ = b;
	}
}

static void draw_scanline(unsigned int y,unsigned char *src,unsigned int pixels) {
	if (y < img_height) {
		const unsigned int addr = y * img_stride;

		{
#if TARGET_MSDOS == 32
			unsigned char * const d = (unsigned char*)0xA8000 + addr;
#else
			unsigned char far * const d = MK_FP(0xA800,addr);
#endif
			scanline_pack2planar(d,src,pixels,0);
		}

		{
#if TARGET_MSDOS == 32
			unsigned char * const d = (unsigned char*)0xB0000 + addr;
#else
			unsigned char far * const d = MK_FP(0xB000,addr);
#endif
			scanline_pack2planar(d,src,pixels,1);
		}

		{
#if TARGET_MSDOS == 32
			unsigned char * const d = (unsigned char*)0xB8000 + addr;
#else
			unsigned char far * const d = MK_FP(0xB800,addr);
#endif
			scanline_pack2planar(d,src,pixels,2);
		}

		{
#if TARGET_MSDOS == 32
			unsigned char * const d = (unsigned char*)0xE0000 + addr;
#else
			unsigned char far * const d = MK_FP(0xE000,addr);
#endif
			scanline_pack2planar(d,src,pixels,3);
		}
	}
}

